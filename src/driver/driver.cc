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
# include <xkrt/internals.h>
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

static void
bits_to_str(char * buffer, unsigned char * mem, size_t nbytes)
{
    buffer[8*nbytes] = 0;
    size_t k = 8*nbytes - 1;
    for (int i = (int)nbytes - 1 ; i >= 0 ; --i)
        for (int j = 0 ; j < 8 ; ++j)
            buffer[k--] = (mem[i] & (1 << j)) ? '1' : '0';
}

/* entry point for threads in the driver's team */
static void *
driver_thread_main(
    runtime_t * runtime,
    team_t * team,
    thread_t * thread
) {
    // driver type
    driver_t * driver = (driver_t *) team->desc.args;
    assert(driver);

    // device driver id is the thread tid
    device_driver_id_t device_driver_id = (device_driver_id_t) thread->tid;
    assert(device_driver_id >= 0);
    assert(device_driver_id < driver->devices.n);

    // device global id
    device_global_id_t device_global_id = driver->devices.global_ids[device_driver_id];

    ///////////////////////
    // create the device //
    ///////////////////////

    assert(driver->f_device_create);

    // create the device
    device_t * device = driver->f_device_create(driver, device_driver_id);
    if (device == NULL)
        LOGGER_FATAL("Could not create a device");

    // initialize device attributes
    device->state       = XKRT_DEVICE_STATE_CREATE;
    device->driver_type = driver->type;
    device->driver_id   = device_driver_id;
    device->conf        = &(runtime->conf.device);
    device->global_id   = device_global_id;

    // register device to the global list
    runtime->drivers.devices.list[device_global_id] = device;

    // register device to the driver list
    driver->devices.list[device_driver_id] = device;

    // init device by the driver
    driver->f_device_init(device->driver_id);

    char buffer[512];
    driver->f_device_info(device_driver_id, buffer, sizeof(buffer));
    LOGGER_INFO("  global id = %2u | %s", device_global_id, buffer);

    /* get total memory and allocate chunk0 */
    if (driver->f_memory_device_info)
    {
        driver->f_memory_device_info(device->driver_id, device->memories, &device->nmemories);
        assert(device->nmemories > 0);
        for (int i = 0 ; i < device->nmemories ; ++i)
        {
            device_memory_info_t * info = device->memories + i;
            LOGGER_INFO("Found memory `%s` of capacity %zuGB", info->name, info->capacity/(size_t)1e9);
            info->allocated = 0;
            XKRT_MUTEX_INIT(info->area.lock);
        }
    }

    // wait for all devices of that driver to be in the 'init' state
    assert(device->state == XKRT_DEVICE_STATE_CREATE);
    device->state = XKRT_DEVICE_STATE_INIT;
    pthread_barrier_wait(&driver->barrier);

    // commit
    assert(driver->f_device_commit);
    device_global_id_bitfield_t * affinity = &(runtime->router.affinity[device->global_id][0]);
    memset(affinity, 0, sizeof(runtime->router.affinity[device->global_id]));
    int err = driver->f_device_commit(device->driver_id, affinity);
    if (err)
        LOGGER_FATAL("Commit fail device %d of driver %s", device->driver_id, driver->f_get_name());
    assert(device->state == XKRT_DEVICE_STATE_INIT);
    device->state = XKRT_DEVICE_STATE_COMMIT;

    // can only have 1 host device, that is the device 0
    assert(driver->type != XKRT_DRIVER_TYPE_HOST || driver->devices.n == 1);

    // print affinity
    for (int i = 0 ; i < XKRT_DEVICES_PERF_RANK_MAX ; ++i)
    {
        device_global_id_bitfield_t bf = affinity[i];
        constexpr int nbytes = sizeof(device_global_id_bitfield_t);
        char buffer[8*nbytes + 1];
        bits_to_str(buffer, (unsigned char *) &bf, nbytes);
        LOGGER_DEBUG("Device `%2u` affinity mask for perf `%2u` is `%s`", device->global_id, i, buffer);
    }

    // init offloader
    device->offloader_init(driver->f_queue_suggest);

    // wait for all devices to be in the 'commit' state with the offloader init
    pthread_barrier_wait(&driver->barrier);

    //////////////////////////////////////////////////
    // Fork a team of worker thread for that device //
    //////////////////////////////////////////////////

    // number of thread per device
    conf_driver_t * driver_conf = runtime->conf.drivers.list + driver->type;
    int nthreads_per_device = driver_conf->nthreads_per_device;
    assert(nthreads_per_device > 0);

    // the device team args
    device_team_args_t args = {
        .driver = driver,
        .device_global_id = device_global_id,
        .device_driver_id = device_driver_id,
        .barrier = {}
    };

    // prepare a barrier, to synchronize threads of that device
    if (pthread_barrier_init(&args.barrier, NULL, nthreads_per_device))
        LOGGER_FATAL("Couldnt initialized pthread_barrier_t");

    // create the device team
    device->team = driver->devices.teams + device_driver_id;

    device->team->desc.args                = &args;
    device->team->desc.binding.flags       = XKRT_TEAM_BINDING_FLAG_NONE;
    device->team->desc.binding.mode        = XKRT_TEAM_BINDING_MODE_COMPACT;
    device->team->desc.binding.nplaces     = 1;
    device->team->desc.binding.places      = XKRT_TEAM_BINDING_PLACES_EXPLICIT;
    device->team->desc.binding.places_list = team->desc.binding.places_list + device_driver_id;
    device->team->desc.master_is_member    = true;
    device->team->desc.nthreads            = nthreads_per_device;
    device->team->desc.routine             = (team_routine_t) device_thread_main;

    runtime->team_create(device->team);     // return from the 'device team'
    runtime->team_join(device->team);

    /////////////////////
    // Teardown driver //
    /////////////////////

    // release memory
    if (driver->f_memory_device_deallocate)
    {
        for (int j = 0 ; j < device->nmemories ; ++j)
        {
            if (device->memories[j].allocated)
            {
                area_t * area = &(device->memories[j].area);
                driver->f_memory_device_deallocate(device->driver_id, (void *) area->chunk0.ptr, area->chunk0.size, j);
            }
        }
    }
    else
        LOGGER_WARN("Driver `%u` is missing `f_device_memory_deallocate`", driver->type);

    // delete device
    if (driver->f_device_destroy)
        driver->f_device_destroy(device->driver_id);
    else
        LOGGER_WARN("Driver `%u` is missing `f_device_destroy`", driver->type);

    return NULL; // return from the 'driver team'
}

/* initialize drivers and create 1 thread per gpu starting on the passed routine */
void
drivers_init(runtime_t * runtime)
{
    # pragma message(TODO "Dynamic driver loading not implemented (with dlopen). Only supporting built-in drivers")

    // PARAMETERS
    device_global_id_t ndevices_requested  = runtime->conf.device.ngpus + 1; // host device + ngpus
    bool use_p2p = runtime->conf.device.use_p2p;
    assert(ndevices_requested < XKRT_DEVICES_MAX);

    // SET MEMBERS
    memset(runtime->drivers.list, 0, sizeof(runtime->drivers.list));
    memset(runtime->drivers.devices.list, 0, sizeof(runtime->drivers.devices.list));
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

    // TODO: currently sequentially initializing driver.
    // Maybe we should initialize them in parallel, but that'd break XKAAPI_NGPUS semantic

    ////////////////////////////////////////////////
    // First, figure-out how many drivers we have //
    ////////////////////////////////////////////////

    int ndrivers = 0;

    // for each driver
    for (uint8_t driver_type = 0 ; driver_type < XKRT_DRIVER_TYPE_MAX && runtime->drivers.devices.n < ndevices_requested ; ++driver_type)
    {
        // if the driver is enabled
        driver_t * (*creator)(void) = creators[driver_type];
        conf_driver_t * driver_conf = runtime->conf.drivers.list + driver_type;
        if (driver_conf->used && creator)
        {
            // instanciate it
            driver_t * driver = creator();
            runtime->drivers.list[driver_type] = driver;
            if (driver == NULL)
                continue ;
            driver->type = (driver_type_t) driver_type;

            // instanciate devices
            const char * driver_name = driver->f_get_name ? driver->f_get_name() : "(null)";
            LOGGER_INFO("Loading driver `%s`", driver_name);

            driver->devices.n = 0;
            if (driver->f_init == NULL || driver->f_init(ndevices_requested - runtime->drivers.devices.n, use_p2p))
            {
                LOGGER_WARN("Failed to load");
                continue ;
            }

            // number of devices for that driver
            assert(driver->f_get_ndevices_max);
            unsigned int ndevices_max = driver->f_get_ndevices_max();
            LOGGER_DEBUG("Driver has up to %u devices", ndevices_max);

            if (ndevices_max == 0)
                continue ;

            driver->devices.n = (device_driver_id_t) MIN(ndevices_requested - runtime->drivers.devices.n, ndevices_max);
            assert(driver->devices.n);

            ++ndrivers;

            ////////////////////////////////////////////////////////////////////
            // create a team with 1 thread per device                         //
            // each thread then forks a new team of threads on that device    //
            ////////////////////////////////////////////////////////////////////

            // generate places and global ids
            team_thread_place_t * places = (team_thread_place_t *) malloc(sizeof(team_thread_place_t) * driver->devices.n);
            assert(places);

            driver->devices.bitfield = 0;

            for (device_driver_id_t device_driver_id = 0; device_driver_id < driver->devices.n ; ++device_driver_id)
            {
                assert(driver->f_device_cpuset);
                int err = driver->f_device_cpuset(runtime->topology, places + device_driver_id, device_driver_id);
                if (err)
                {
                    LOGGER_WARN("Invalid cpuset returned for device %d - using default cpuset", device_driver_id);
                    CPU_ZERO(places + device_driver_id);
                    long nproc = sysconf(_SC_NPROCESSORS_CONF);
                    for (int i = 0; i < nproc; ++i)
                        CPU_SET(i, places + device_driver_id);
                }

                // set the device global id
                static_assert(XKRT_DRIVER_TYPE_HOST == 0);
                static_assert(HOST_DEVICE_GLOBAL_ID == 0);
                const device_global_id_t device_global_id = runtime->drivers.devices.n++;
                driver->devices.global_ids[device_driver_id] = device_global_id;
                driver->devices.bitfield |= (1 << device_global_id);
            }

            // create the driver team
            driver->team.desc.args                = driver;
            driver->team.desc.binding.flags       = XKRT_TEAM_BINDING_FLAG_NONE;
            driver->team.desc.binding.mode        = XKRT_TEAM_BINDING_MODE_COMPACT;
            driver->team.desc.binding.nplaces     = driver->devices.n;
            driver->team.desc.binding.places      = XKRT_TEAM_BINDING_PLACES_EXPLICIT;
            driver->team.desc.binding.places_list = places;
            driver->team.desc.master_is_member    = false;
            driver->team.desc.nthreads            = driver->devices.n;
            driver->team.desc.routine             = (team_routine_t) driver_thread_main;

            // prepare a barrier, to synchronize devices of that driver
            if (pthread_barrier_init(&driver->barrier, NULL, driver->devices.n))
                LOGGER_FATAL("Couldnt initialized pthread_barrier_t");
        }
        else
        {
            runtime->drivers.list[driver_type] = NULL;
        }
    }
    assert(runtime->drivers.devices.n <= ndevices_requested);

    // prepare a barrier, to synchronize drivers
    if (pthread_barrier_init(&runtime->drivers.barrier, NULL, ndrivers + 1))
        LOGGER_FATAL("Couldnt initialized pthread_barrier_t");

    ////////////////////////////////////////////////
    // Second, init device of each driver         //
    ////////////////////////////////////////////////

    // for each driver
    for (uint8_t driver_type = 0 ; driver_type < XKRT_DRIVER_TYPE_MAX ; ++driver_type)
    {
        // if the driver is enable, create a team of thread
        driver_t * driver = runtime->driver_get((driver_type_t) driver_type);
        if (driver && driver->devices.n)
            runtime->team_create(&driver->team);
    }

    // wait for all devices to be created
    pthread_barrier_wait(&runtime->drivers.barrier);

    // DEBUG OUTPUT
    if (runtime->drivers.devices.n == 0)
        LOGGER_WARN("No devices found :-(");
    else
        LOGGER_INFO("Found %d devices (with %d requested)", runtime->drivers.devices.n, ndevices_requested);
}

void
drivers_deinit(runtime_t * runtime)
{
    // notify each thread to stop
    for (device_global_id_t device_global_id = 0 ; device_global_id < runtime->drivers.devices.n ; ++device_global_id)
    {
        device_t * device = runtime->drivers.devices.list[device_global_id];
        assert(device);
        device->state = XKRT_DEVICE_STATE_STOP;
        device->team->wakeup();
    }

    // finalize each driver
    for (uint8_t driver_type = 0 ; driver_type < XKRT_DRIVER_TYPE_MAX ; ++driver_type)
    {
        // join threads
        driver_t * driver = runtime->drivers.list[driver_type];
        if (driver && driver->devices.n)
        {
            runtime->team_join(&driver->team);
            free(driver->team.desc.binding.places_list);
        }

        // finalize driver
        if (driver)
        {
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
    assert(device_driver_id < driver->devices.n);
    return driver->devices.list[device_driver_id];
}

XKRT_NAMESPACE_END;
