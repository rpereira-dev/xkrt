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

# include <xkrt/runtime.h>
# include <xkrt/conf/conf.h>
# include <xkrt/logger/logger.h>
# include <xkrt/driver/driver.h>
# include <xkrt/memory/alignedas.h>
# include <xkrt/sync/spinlock.h>

# include <atomic>
# include <stdlib.h>
# include <string.h>
# include <signal.h>

# include <hwloc.h>

//////////////////////////////
//  Runtime initialization  //
//////////////////////////////

static inline void
task_format_register(xkrt_runtime_t * runtime)
{
    task_formats_init(&(runtime->formats.list));
    xkrt_memory_copy_async_register_format(runtime);
    xkrt_task_host_capture_register_format(runtime);
    xkrt_memory_async_register_format(runtime);
    xkrt_file_async_register_format(runtime);
}

extern "C"
int
xkrt_init(xkrt_runtime_t * runtime)
{
    LOGGER_INFO("Initializing XKRT");

    // set TLS
    xkrt_team_t * team = NULL;
    int tid = 0;
    xkrt_device_global_id_t device_global_id = HOST_DEVICE_GLOBAL_ID;
    xkrt_thread_place_t place;
    xkrt_runtime_t::thread_getaffinity(place);
    xkrt_thread_t * thread = new xkrt_thread_t(team, tid, pthread_self(), device_global_id, place);
    assert(thread);
    xkrt_thread_t::push_tls(thread);

    # if XKRT_SUPPORT_STATS
    memset(&runtime->stats, 0, sizeof(runtime->stats));
    # endif /* XKRT_SUPPORT_STATS */

    // set affinities to 0
    memset(&runtime->router.affinity, 0, sizeof(runtime->router.affinity));

    // create topology
    hwloc_topology_init(&runtime->topology);
    hwloc_topology_load(runtime->topology);

    // load
    xkrt_init_conf(&(runtime->conf));
    task_format_register(runtime);

    // the '+1' is to enforce the host device, always
    xkrt_drivers_init(runtime);

    # if XKRT_MEMORY_REGISTER_OVERFLOW_PROTECTION
    // initialize the registered memory map
    if (runtime->conf.protect_registered_memory_overflow)
    {
        new (&runtime->registered_memory) std::map<uintptr_t, size_t>();
    }
    else
    {
        LOGGER_WARN("Compiled with `MEMORY_REGISTER_OVERFLOW_PROTECTION` but `XKAAPI_MEMORY_REGISTER_PROTECT_OVERFLOW` environment variable is not set");
    }
    # endif /* XKRT_MEMORY_REGISTER_OVERFLOW_PROTECTION */

    // mark runtime as initialized
    runtime->state = XKRT_RUNTIME_INITIALIZED;

    return 0;
}

extern "C"
int
xkrt_deinit(xkrt_runtime_t * runtime)
{
    LOGGER_INFO("Deinitializing XKRT");
    assert(runtime);
    assert(runtime->state == XKRT_RUNTIME_INITIALIZED);

    # if XKRT_SUPPORT_STATS
    if (runtime->conf.report_stats_on_deinit)
        xkrt_runtime_stats_report(runtime);
    # endif /* XKRT_SUPPORT_STATS */

    runtime->state = XKRT_RUNTIME_DEINITIALIZED;
    xkrt_drivers_deinit(runtime);
    hwloc_topology_destroy(runtime->topology);

    return 0;
}

//////////////////////////////
//  Runtime synchronize     //
//////////////////////////////

extern "C"
int
xkrt_sync(xkrt_runtime_t * runtime)
{
    assert(runtime);
    runtime->task_wait();

    return 0;
}

///////////////
// UTILITIES //
///////////////

extern "C"
int
xkrt_get_ndevices_max(xkrt_runtime_t * runtime, int * count)
{
    assert(count);

    *count = 0;
    for (int i = 0 ; i < XKRT_DRIVER_TYPE_MAX ; ++i)
    {
        if (i != XKRT_DRIVER_TYPE_HOST)
        {
            xkrt_driver_t * driver = runtime->driver_get((xkrt_driver_type_t) i);
            if (driver && driver->f_get_ndevices_max)
                *count += driver->f_get_ndevices_max();
        }
    }
    return 0;
}

unsigned int
xkrt_runtime_t::get_ndevices(void)
{
    return this->drivers.devices.n;
}
