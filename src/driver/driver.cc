/*
** Copyright 2024,2025 INRIA
**
** Contributors :
** Thierry Gautier, thierry.gautier@inrialpes.fr
** Joao Lima joao.lima@inf.ufsm.br
** Romain PEREIRA, romain.pereira@inria.fr + rpereira@anl.gov
**
** This software is a computer program whose purpose is to execute
** blas subroutines on multi-GPUs system.
**
** This software is governed by the CeCILL-C license under French law and
** abiding by the rules of distribution of free software.  You can  use,
** modify and/ or redistribute the software under the terms of the CeCILL-C
** license as circulated by CEA, CNRS and INRIA at the following URL
** "http://www.cecill.info".

** As a counterpart to the access to the source code and  rights to copy,
** modify and redistribute granted by the license, users are provided only
** with a limited warranty  and the software's author,  the holder of the
** economic rights,  and the successive licensors  have only  limited
** liability.

** In this respect, the user's attention is drawn to the risks associated
** with loading,  using,  modifying and/or developing or reproducing the
** software by the user in light of its specific status of free software,
** that may mean  that it is complicated to manipulate,  and  that  also
** therefore means  that it is reserved for developers  and  experienced
** professionals having in-depth computer knowledge. Users are therefore
** encouraged to load and test the software's suitability as regards their
** requirements in conditions enabling the security of their systems and/or
** data to be ensured and,  more generally, to use and operate it in the
** same conditions as regards security.

** The fact that you are presently reading this means that you have had
** knowledge of the CeCILL-C license and that you accept its terms.
**/

# include <xkrt/support.h>
# include <xkrt/runtime.h>
# include <xkrt/driver/driver.h>
# include <xkrt/logger/logger.h>
# include <xkrt/utils/min-max.h>
# include <xkrt/sync/spinlock.h>
# include <xkrt/thread/thread.h>

# include <cassert>
# include <cstring>
# include <cerrno>
# include <climits>

XKRT_NAMESPACE_BEGIN;

/* initialize drivers and create 1 thread per gpu starting on the passed routine */
void
drivers_init(runtime_t * runtime)
{
    # pragma message(TODO "Dynamic driver loading not implemented (with dlopen). Only supporting built-in drivers")

    // PARAMETERS
    unsigned int ndevices_requested  = runtime->conf.device.ngpus + 1;                      // host device + ngpus
    bool use_p2p            = runtime->conf.device.use_p2p;
    assert(ndevices_requested < XKRT_DEVICES_MAX);

    // SET MEMBERS
    memset(runtime->drivers.list, 0, sizeof(runtime->drivers.list));
    memset(runtime->drivers.devices.list, 0, sizeof(runtime->drivers.devices.list));
    runtime->drivers.devices.next_id = 1;                                                   // host device is always 0
    runtime->drivers.devices.n = 0;

    // LOAD DRIVERS
    driver_t * (*creators[XKRT_DRIVER_TYPE_MAX])(void);
    memset(creators, 0, sizeof(creators));

    extern driver_t * XKRT_DRIVER_TYPE_HOST_create_driver(void);
    creators[XKRT_DRIVER_TYPE_HOST] = XKRT_DRIVER_TYPE_HOST_create_driver;
    static_assert(XKRT_DRIVER_TYPE_HOST == 0);
    static_assert(HOST_DEVICE_GLOBAL_ID == 0);

    char support[512];
    strcpy(support, "host");

# if XKRT_SUPPORT_CUDA
    extern driver_t * XKRT_DRIVER_TYPE_CU_create_driver(void);
    creators[XKRT_DRIVER_TYPE_CUDA] = XKRT_DRIVER_TYPE_CU_create_driver;
    strcat(support, ", cuda");
# endif /* XKRT_SUPPORT_CUDA */

# if XKRT_SUPPORT_ZE
    extern driver_t * XKRT_DRIVER_TYPE_ZE_create_driver(void);
    creators[XKRT_DRIVER_TYPE_ZE] = XKRT_DRIVER_TYPE_ZE_create_driver;
    strcat(support, ", ze");
# endif /* XKRT_SUPPORT_ZE */

# if XKRT_SUPPORT_CL
    extern driver_t * XKRT_DRIVER_TYPE_CL_create_driver(void);
    creators[XKRT_DRIVER_TYPE_CL] = XKRT_DRIVER_TYPE_CL_create_driver;
    strcat(support, ", opencl");
# endif /* XKRT_SUPPORT_CL */

# if XKRT_SUPPORT_HIP
    extern driver_t * XKRT_DRIVER_TYPE_HIP_create_driver(void);
    creators[XKRT_DRIVER_TYPE_HIP] = XKRT_DRIVER_TYPE_HIP_create_driver;
    strcat(support, ", hip");
# endif /* XKRT_SUPPORT_HIP */

# if XKRT_SUPPORT_SYCL
    extern driver_t * XKRT_DRIVER_TYPE_SYCL_create_driver(void);
    creators[XKRT_DRIVER_TYPE_SYCL] = XKRT_DRIVER_TYPE_SYCL_create_driver;
    strcat(support, ", sycl");
# endif /* XKRT_SUPPORT_SYCL */

    LOGGER_INFO("Built with support for `%s`", support);

    // number of devices
    uint8_t ndevices = 0;

    // FOR EACH DRIVER
    for (uint8_t driver_type = 0 ;
            driver_type < XKRT_DRIVER_TYPE_MAX && ndevices < ndevices_requested ;
            ++driver_type)
    {
        // if the driver is enabled
        driver_t * (*creator)(void) = creators[driver_type];
        conf_driver_t * driver_conf = runtime->conf.drivers.list + driver_type;
        if (driver_conf->used && creator)
        {
            // create it
            driver_t * driver = creator();
            runtime->drivers.list[driver_type] = driver;
            if (driver == NULL)
                continue ;
            driver->type = (driver_type_t) driver_type;

            // number of threads per device of that driver
            int nthreads_per_device = driver_conf->nthreads_per_device;
            assert(nthreads_per_device > 0);

            // load devices
            const char * driver_name = driver->f_get_name ? driver->f_get_name() : "(null)";
            LOGGER_INFO("Loading driver `%s`", driver_name);

            driver->ndevices_commited = 0;
            if (driver->f_init == NULL || driver->f_init(ndevices_requested - ndevices, use_p2p))
            {
                LOGGER_WARN("Failed to load");
                continue ;
            }

            assert(driver->f_get_ndevices_max);
            unsigned int ndevices_max = driver->f_get_ndevices_max();
            LOGGER_DEBUG("Driver has up to %u devices", ndevices_max);
            if (ndevices_max == 0)
                continue ;

            unsigned int ndevices_for_driver = MIN(ndevices_requested - ndevices, ndevices_max);
            assert(ndevices_for_driver);

            // allocate driver thread places
            thread_place_t * places = (thread_place_t *) malloc(sizeof(thread_place_t) * ndevices_for_driver);
            assert(places);

            // ARGS PASSED TO FORKED THREADS
            device_team_args_t args = {
                .runtime = runtime,
                .devices = {},
                .ndevices = 0
            };

            // get cpuset for the device
            for (device_driver_id_t device_driver_id = 0; device_driver_id < ndevices_for_driver ; ++device_driver_id)
            {
                assert(driver->f_device_cpuset);
                int err = driver->f_device_cpuset(runtime->topology, places + device_driver_id, device_driver_id);
                if (err)
                    LOGGER_WARN("Invalid cpuset returned for device %d - using default cpuset", device_driver_id);
                else
                {
                    args.devices[args.ndevices].driver_type      = (driver_type_t) driver_type;
                    args.devices[args.ndevices].device_driver_id = device_driver_id;
                    ++ndevices;
                    ++args.ndevices;
                }
            }

            // create the team
            driver->team.desc.routine             = device_thread_main;
            driver->team.desc.args                = &args;
            driver->team.desc.nthreads            = args.ndevices * nthreads_per_device;
            driver->team.desc.binding.mode        = XKRT_TEAM_BINDING_MODE_COMPACT;
            driver->team.desc.binding.places      = XKRT_TEAM_BINDING_PLACES_EXPLICIT;
            driver->team.desc.binding.places_list = places;
            driver->team.desc.binding.nplaces     = args.ndevices;
            driver->team.desc.binding.flags       = XKRT_TEAM_BINDING_FLAG_NONE;

            // prepare the barrier for the devices team
            pthread_barrier_t * barrier = &driver->barrier;
            if (pthread_barrier_init(barrier, NULL, driver->team.desc.nthreads + 1))
                LOGGER_FATAL("Couldnt initialized pthread_barrier_t");

            // create a team of thread
            runtime->team_create(&driver->team);

            // wait for all devices to be created
            pthread_barrier_wait(barrier);    // init
            pthread_barrier_wait(barrier);    // commit
            pthread_barrier_wait(barrier);    // offloader queues

            // set devices bitfield
            driver->devices_bitfield = 0;
            for (int i = 0 ; i < driver->ndevices_commited ; ++i)
                driver->devices_bitfield |= (1 << driver->devices[i]->global_id);
        }
        // driver disabled or couldnt init
        else
            runtime->drivers.list[driver_type] = NULL;
    }
    assert(ndevices <= ndevices_requested);

    // DEBUG OUTPUT
    if (ndevices == 0)
    {
        LOGGER_WARN("No devices found :-(");
        return ;
    }
    LOGGER_INFO("Found %d devices (with %d requested)", ndevices, ndevices_requested);
}

void
drivers_deinit(runtime_t * runtime)
{
    // notify each thread to stop
    for (device_global_id_t device_global_id = 0 ;
            device_global_id < runtime->drivers.devices.n ;
            ++device_global_id)
    {
        device_t * device = runtime->drivers.devices.list[device_global_id];
        assert(device);
        device->state = XKRT_DEVICE_STATE_STOP;

        int nthreads = device->nthreads.load(std::memory_order_acq_rel);
        for (int i = 0 ; i < nthreads ; ++i)
            device->threads[i]->wakeup();
    }

    // finalize each driver
    for (uint8_t driver_type = 0 ; driver_type < XKRT_DRIVER_TYPE_MAX ; ++driver_type)
    {
        driver_t * driver = runtime->drivers.list[driver_type];
        if (driver)
        {
            if (driver->ndevices_commited.load())
            {
                // wait for threads queue deletion
                pthread_barrier_wait(&driver->barrier);

                // wait for main thread driver deinitialization
                pthread_barrier_wait(&driver->barrier);

                // can destroy the barrier now
                pthread_barrier_destroy(&driver->barrier);
            }

            // join threads
            runtime->team_join(&driver->team);
            free(driver->team.desc.binding.places_list);

            if (driver->f_finalize)
                driver->f_finalize();
            else
                LOGGER_WARN("Driver `%u` is missing `f_finalize`", driver_type);
        }
    }
}

const char *
driver_name(driver_type_t driver_type)
{
    switch (driver_type)
    {
        case (XKRT_DRIVER_TYPE_HOST):   return "host";
        case (XKRT_DRIVER_TYPE_CUDA):   return "cuda";
        case (XKRT_DRIVER_TYPE_HIP):    return "hip";
        case (XKRT_DRIVER_TYPE_ZE):     return "ze";
        case (XKRT_DRIVER_TYPE_CL):     return "cl";
        case (XKRT_DRIVER_TYPE_SYCL):   return "sycl";
        default:                        return "(null)";
    }
}

driver_type_t
driver_type_from_name(const char * name)
{
    for (int i = 0 ; i < XKRT_DRIVER_TYPE_MAX ; ++i)
        if (strcmp(name, driver_name((driver_type_t) i)) == 0)
            return (driver_type_t) i;
    return XKRT_DRIVER_TYPE_MAX;
}

int
support_driver(driver_type_t driver_type)
{
    switch (driver_type)
    {
        case (XKRT_DRIVER_TYPE_HOST):   return 1;
        case (XKRT_DRIVER_TYPE_CUDA):   return XKRT_SUPPORT_CUDA;
        case (XKRT_DRIVER_TYPE_HIP):    return XKRT_SUPPORT_HIP;
        case (XKRT_DRIVER_TYPE_ZE):     return XKRT_SUPPORT_ZE;
        case (XKRT_DRIVER_TYPE_CL):     return XKRT_SUPPORT_CL;
        case (XKRT_DRIVER_TYPE_SYCL):   return XKRT_SUPPORT_SYCL;
        default:                        return 0;
    }
}

device_t *
driver_device_get(driver_t * driver, device_driver_id_t device_driver_id)
{
    assert(device_driver_id >= 0);
    assert(device_driver_id < driver->ndevices_commited);
    return driver->devices[device_driver_id];
}

XKRT_NAMESPACE_END;
