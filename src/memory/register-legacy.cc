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

# include <xkrt/logger/todo.h>
# include <xkrt/runtime.h>

///////////////////////////////////
//  ORIGINAL KAAPI 1.0 INTERFACE //
///////////////////////////////////

//  General idea of these interfaces
//      - Nvidia GPUs serializes memory pinning anyway
//      - We have a single thread dedicated to pinning memory to any device
//      - it schedule 'pinning tasks' that are tasks with read/write access on the memory


//  Some ideas:
//      - can we pin memory independently of CUDA / HIP / ... via 'mlock' and
//      notify the driver that the memory is pinned ? Would be better in case
//      of server with different GPU vendors...


# pragma message(TODO "The current implementation spawn independent tasks. Maybe make it dependent to a specific type of access")

extern "C"
int
xkrt_memory_register(
    xkrt_runtime_t * runtime,
    void * ptr,
    size_t size
) {
    for (uint8_t driver_id = 0 ; driver_id < XKRT_DRIVER_TYPE_MAX; ++driver_id)
    {
        xkrt_driver_t * driver = runtime->driver_get((xkrt_driver_type_t) driver_id);
        if (!driver)
            continue ;
        if (!driver->f_memory_host_register)
            LOGGER_DEBUG("Driver `%u` does not implement memory register", driver_id);
        else if (driver->f_memory_host_register(ptr, size))
            LOGGER_ERROR("Could not register memory for driver `%s`", driver->f_get_name());
    }

    # if XKRT_MEMORY_REGISTER_OVERFLOW_PROTECTION
    /* save in the registered map, for later accesses */
    runtime->registered_memory[(uintptr_t)ptr] = size;
    # endif /* XKRT_MEMORY_REGISTER_OVERFLOW_PROTECTION */

    # if XKRT_SUPPORT_STATS
    runtime->stats.memory.registered += size;
    # endif /* XKRT_SUPPORT_STATS */

    return 0;
}

extern "C"
int
xkrt_memory_unregister(xkrt_runtime_t * runtime, void * ptr, size_t size)
{
    for (uint8_t driver_id = 0 ; driver_id < XKRT_DRIVER_TYPE_MAX; ++driver_id)
    {
        xkrt_driver_t * driver = runtime->driver_get((xkrt_driver_type_t) driver_id);
        if (!driver)
            continue ;
        if (!driver->f_memory_host_unregister)
            LOGGER_DEBUG("Driver `%u` does not implement memory unregister", driver_id);
        else if (driver->f_memory_host_unregister(ptr, size))
            LOGGER_ERROR("Could not unregister memory for driver `%s`", driver->f_get_name());
    }

    # if XKRT_MEMORY_REGISTER_OVERFLOW_PROTECTION
    /* remove from the registered map */
    runtime->registered_memory.erase((uintptr_t)ptr);
    # endif /* XKRT_MEMORY_REGISTER_OVERFLOW_PROTECTION */

    # if XKRT_SUPPORT_STATS
    runtime->stats.memory.unregistered += size;
    # endif /* XKRT_SUPPORT_STATS */

    return 0;
}

extern "C"
size_t
xkrt_memory_register_async(xkrt_runtime_t * runtime, void * ptr, size_t size)
{
    xkrt_driver_t * driver = runtime->driver_get(XKRT_DRIVER_TYPE_HOST);
    assert(driver);

    xkrt_team_t * team = &driver->team;

    // TODO : could be optimized using a custom format for register tasks
    runtime->team_task_spawn(
        team,
        [runtime, ptr, size] (task_t * task) {
            (void) task;
            xkrt_memory_register(runtime, ptr, size);
        }
    );

    return 0;
}

extern "C"
int
xkrt_memory_unregister_async(xkrt_runtime_t * runtime, void * ptr, size_t size)
{
    xkrt_driver_t * driver = runtime->driver_get(XKRT_DRIVER_TYPE_HOST);
    assert(driver);

    xkrt_team_t * team = &driver->team;

    // TODO : could be optimized using a custom format for unregister tasks
    runtime->team_task_spawn(
        team,
        [runtime, ptr, size] (task_t * task) {
            (void) task;
            xkrt_memory_unregister(runtime, ptr, size);
        }
    );
    return 0;
}

extern "C"
int
xkrt_memory_register_waitall(xkrt_runtime_t * runtime)
{
    // atm, waits for all children tasks
    // instead, we probably want to wait only on register tasks

    runtime->task_wait();
    return 0;
}
