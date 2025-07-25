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

# include <xkrt/memory/access/blas/memory-tree.hpp>
# include <xkrt/xkrt.h>
# include <xkrt/runtime.h>
# include <xkrt/driver/device.hpp>
# include <xkrt/driver/driver.h>
# include <xkrt/driver/stream.h>
# include <xkrt/logger/logger.h>
# include <xkrt/logger/bits-to-str.h>
# include <xkrt/logger/todo.h>
# include <xkrt/sync/mem.h>
# include <xkrt/stats/stats.h>
# include <xkrt/task/task.hpp>

# include <cassert>
# include <cstring>
# include <cerrno>

# ifndef _GNU_SOURCE
#  define _GNU_SOURCE
# endif /* _GNU_SOURCE */
# include <sched.h> /* getcpu */

/////////////////////////
//  DEVICE PROGRESSION //
/////////////////////////

static inline void
__task_moldable_split(
    xkrt_runtime_t * runtime,
    task_t * task,
    access_t * accesses,
    task_dep_info_t * dep,
    task_mol_info_t * mol
) {
    // right now, all task accesses must have the same type
    const access_type_t type = (accesses + 0)->type;

    # ifndef NDEBUG
    for (task_access_counter_t i = 1 ; i < dep->ac ; ++i)
        assert(type == (accesses + i)->type);
    # endif /* atm, only support moldable tasks with accesses of same type */

    switch (type)
    {
        case (ACCESS_TYPE_INTERVAL):
        {
            // dupplicate the task
            task_t * dup_task = runtime->task_dup(task);
            assert(dup_task);

            // split accesses and refine dependencies
            access_t * dup_accesses = TASK_ACCESSES(dup_task);
            assert(dup_accesses);

            // for each access
            for (task_access_counter_t i = 0 ; i < dep->ac ; ++i)
            {
                access_t * access     = accesses     + i;
                access_t * dup_access = dup_accesses + i;

                assert(access->task     == task);
                assert(dup_access->task == task);
                dup_access->task = dup_task;
                assert(access->task     == task);
                assert(dup_access->task == dup_task);

                // split access
                access_t::split(access, dup_access, dup_task, ACCESS_SPLIT_MODE_HALVES);

                // for each original successor
                for (access_t * succ_access : access->successors)
                {
                    // if new access conflicts
                    if (access_t::conflicts(dup_access, succ_access))
                    {
                        // set a dependency
                        __access_precedes(dup_access, succ_access);
                    }
                    else
                    {
                        // nothing to do
                    }

                    // if shrinked access still conflicts
                    if (access_t::conflicts(access, succ_access))
                    {
                        // nothing to do, we recycle task and the access,
                        // so the dependency is already set
                    }
                    else
                    {
                        // TODO: unset the dependency
                        LOGGER_FATAL("TODO: should unref once the successor");
                    }
                }
            }

            // submit the dupplicated task
            assert(dup_task->parent);
            assert(dup_task->parent == task->parent);

            # pragma message(TODO "This is quite ugly, can we have tasks go through a more regular transition path ? At that point, the dupplicated task is in the 'ready' state already, as its original task was ready")
            ++dup_task->parent->cc;
            xkrt_runtime_submit_task(runtime, dup_task);

            break ;
        }

        default:
            LOGGER_FATAL("Not supported");
    }
}

static inline void
__device_prepare_task(
    xkrt_runtime_t * runtime,
    xkrt_device_t * device,
    task_t * task
) {
    assert(device);
    assert(task);
    assert(task->state.value == TASK_STATE_READY);

    /* if that's a device task, then fetches to the device. Else, fetch to the host */
    xkrt_device_global_id_t device_global_id = (task->flags & TASK_FLAG_DEVICE) ? device->global_id : HOST_DEVICE_GLOBAL_ID;
    LOGGER_DEBUG("Preparing task `%s` of format `%d` on device `%d` - on a thread of device `%d`",
            task->label, task->fmtid, device_global_id, device->global_id);

    if (task->flags & TASK_FLAG_DEPENDENT)
    {
        task_dep_info_t * dep = TASK_DEP_INFO(task);
        assert(TASK_DEP_INFO(task)->wc == 0);

        /* if there is at least one access */
        if (dep->ac > 0)
        {
            /* retrieve accesses */
            access_t * accesses = TASK_ACCESSES(task);

            /////////////////////////
            // MOLDABLE TASK SPLIT //
            /////////////////////////

            // TODO : move that to another function

            /* if the task is moldable */
            if (task->flags & TASK_FLAG_MOLDABLE)
            {
                /* check split condition, and split, or execute normally */
                task_mol_info_t * mol = TASK_MOL_INFO(task);
                assert(mol->split_condition);

                /* if the moldable task must split */
                if (mol->split_condition(task, accesses))
                {
                    // shrink the moldable task, and resubmit the original task
                    // whose accesses got shrinked
                    __task_moldable_split(runtime, task, accesses, dep, mol);
                    return xkrt_runtime_submit_task(runtime, task);
                }
                else
                {
                    // mothing to do
                }
            }
            else
            {
                // nothing to do
            }

            ////////////////////
            // FETCH ACCESSES //
            ////////////////////

            /* increase task 'fetching' counter so it does not get ready early
             * (eg before we processed all accesses bellow) */
            __task_fetching(1, task);

            /* for each access */
            assert(dep->ac <= TASK_MAX_ACCESSES);
            for (task_access_counter_t i = 0 ; i < dep->ac ; ++i)
            {
                access_t * access = accesses + i;
                if (access->mode & ACCESS_MODE_V)
                    continue ;

                assert(task == access->task);
                MemoryCoherencyController * mcc = task_get_memory_controller(runtime, task->parent, access);
                if (mcc)
                    mcc->fetch(access, device_global_id);
            }

            /* decrease the task 'fetching' counter to detect early-fetch completion */
            __task_fetched(1, task, xkrt_device_task_execute, runtime, device);
            /* else the task will be launched in a callback while all accesses were fetched */
        }
    }
    else
    {
        xkrt_device_task_execute(runtime, device, task);
    }
}

/* main loop for the thread responsible the passed device */
static inline int
xkrt_device_thread_main_loop(
    xkrt_runtime_t * runtime,
    xkrt_device_t * device,
    xkrt_thread_t * thread,
    uint8_t device_tid
) {
    assert(thread == xkrt_thread_t::get_tls());

    task_t * task = NULL;

    auto test = [&] (void) {
        if (task)                                                                   return false;
        if ((task = thread->deque.pop()) != NULL)                                   return false;
        if (!device->offloader_streams_are_empty(device_tid, XKRT_STREAM_TYPE_ALL)) return false;
        if (device->state != XKRT_DEVICE_STATE_COMMIT)                              return false;
        return true;
    };

    while (device->state == XKRT_DEVICE_STATE_COMMIT)
    {
        ///////////////////////////////////////////////////////////////////////////////////
        // sleep until a task or an instruction is ready, or until the runtime must stop
        ///////////////////////////////////////////////////////////////////////////////////

        thread->pause(test);

        // if the runtime must stop, break
        if (device->state != XKRT_DEVICE_STATE_COMMIT)
        {
            assert(device->state == XKRT_DEVICE_STATE_STOP);
            break ;
        }

        // if there is a task, run it
        if (task)
        {
            __device_prepare_task(runtime, device, task);
            task = NULL;
        }

        // is there are instructions to proress, progress them
        if (!device->offloader_streams_are_empty(device_tid, XKRT_STREAM_TYPE_ALL))
            device->offloader_poll(device_tid);
    }

    return EINTR;
}

///////////
//  MAIN //
///////////

/* Main entry thread created per device */
void *
xkrt_device_thread_main(
    xkrt_team_t * team,
    xkrt_thread_t * thread
) {
    // unpack args
    xkrt_device_team_args_t * args = (xkrt_device_team_args_t *) team->desc.args;
    assert(args);

    // get the id of that thread in the args->devices list
    // attach threads in a compact manner, similarly to how the team is
    // created, so the thread hits a device topologically close
    int id = thread->tid % args->ndevices;

    // unpack args runtime
    xkrt_runtime_t * runtime        = args->runtime;
    xkrt_driver_type_t driver_type  = args->devices[id].driver_type;
    uint8_t device_driver_id        = args->devices[id].device_driver_id;

    // get the driver
    xkrt_driver_t * driver = runtime->driver_get(driver_type);
    int is_device_main_thread = thread->tid < args->ndevices;

    // create device
    if (is_device_main_thread)
    {
        assert(driver->f_device_create);

        // create the device
        xkrt_device_t * device = driver->f_device_create(driver, device_driver_id);
        if (device == NULL)
            LOGGER_FATAL("Could not create a device");

        runtime->drivers.devices.n.fetch_add(1, std::memory_order_relaxed);

        // initialize device attributes
        device->state       = XKRT_DEVICE_STATE_CREATE;
        device->driver_type = driver_type;
        device->driver_id   = device_driver_id;
        device->conf        = &(runtime->conf.device);
        device->global_id   = (driver_type == XKRT_DRIVER_TYPE_HOST) ? 0 : runtime->drivers.devices.next_id.fetch_add(1, std::memory_order_relaxed);

        // register device to the global list
        runtime->drivers.devices.list[device->global_id] = device;

        // register device to the driver list
        driver->devices[device_driver_id] = device;

        // init device by the driver
        driver->f_device_init(device->driver_id);

        char buffer[512];
        driver->f_device_info(device_driver_id, buffer, sizeof(buffer));
        LOGGER_INFO("  global id = %2u | %s", device->global_id, buffer);

        /* get total memory and allocate chunk0 */
        if (driver->f_memory_device_info)
        {
            driver->f_memory_device_info(device->driver_id, device->memories, &device->nmemories);
            assert(device->nmemories > 0);
            for (int i = 0 ; i < device->nmemories ; ++i)
            {
                xkrt_device_memory_info_t * info = device->memories + i;
                LOGGER_INFO("Found memory `%s` of capacity %zuGB", info->name, info->capacity/(size_t)1e9);
                info->allocated = 0;
                XKRT_MUTEX_INIT(info->area.lock);
            }
        }

        assert(device->state == XKRT_DEVICE_STATE_CREATE);
        device->state = XKRT_DEVICE_STATE_INIT;
    }

    // wait for all devices to be in the 'init' state and for all threads to join
    pthread_barrier_wait(&driver->barrier);

    // register the device thread
    xkrt_device_t * device = driver->devices[device_driver_id];
    assert(device);
    uint8_t device_tid = device->nthreads.fetch_add(1, std::memory_order_relaxed);
    device->threads[device_tid] = thread;

    // print thread
    unsigned int cpu, node;
    getcpu(&cpu, &node);
    LOGGER_INFO("Starting thread for %s device (device_driver_id=%d, device_global_id=%d) on cpu %d of node %d",
            driver->f_get_name(), device_driver_id, device->global_id, cpu, node);

    // 'commit' all devices
    if (is_device_main_thread)
    {
        // commit
        assert(driver->f_device_commit);
        xkrt_device_global_id_bitfield_t * affinity = &(runtime->router.affinity[device->global_id][0]);
        memset(affinity, 0, sizeof(runtime->router.affinity[device->global_id]));
        int err = driver->f_device_commit(device->driver_id, affinity);
        if (err)
            LOGGER_FATAL("Commit fail device %d of driver %s", device->driver_id, driver->f_get_name());
        assert(device->state == XKRT_DEVICE_STATE_INIT);
        device->state = XKRT_DEVICE_STATE_COMMIT;
        ++driver->ndevices_commited;

        // can only have 1 host device, that is the device 0
        assert(driver_type != XKRT_DRIVER_TYPE_HOST || driver->ndevices_commited == 1);

        // print affinity
        for (int i = 0 ; i < XKRT_DEVICES_PERF_RANK_MAX ; ++i)
        {
            xkrt_device_global_id_bitfield_t bf = affinity[i];
            constexpr int nbytes = sizeof(xkrt_device_global_id_bitfield_t);
            char buffer[8*nbytes + 1];
            xkrt_bits_to_str(buffer, (unsigned char *) &bf, nbytes);
            LOGGER_DEBUG("Device `%2u` affinity mask for perf `%2u` is `%s`", device->global_id, i, buffer);
        }

        // init offloader
        device->offloader_init(driver->f_stream_suggest);
    }

    // wait for all devices to be in the 'commit' state with the offloader init
    pthread_barrier_wait(&driver->barrier);

    // initialize offloader thread to initialize streams
    device->offloader_init_thread(device_tid, driver->f_stream_create);

    // wait for all threads to have streams initialized
    pthread_barrier_wait(&driver->barrier);
    // cannot use 'args->barrier' after this point

    /* infinite loop with the device context */
    int err = xkrt_device_thread_main_loop(runtime, device, thread, device_tid);
    assert((err==0) || (err==EINTR));

    // delete streams
    if (driver->f_stream_delete)
        for (uint8_t j = 0 ; j < XKRT_STREAM_TYPE_ALL ; ++j)
            for (int k = 0 ; k < device->count[j] ; ++k)
                driver->f_stream_delete(device->streams[device_tid][j][k]);

    // wait for all thread to delete their streams
    pthread_barrier_wait(&driver->barrier);

    /* deinitialize driver */
    if (is_device_main_thread)
    {
        // release memory
        if (driver->f_memory_device_deallocate)
        {
            for (int j = 0 ; j < device->nmemories ; ++j)
            {
                if (device->memories[j].allocated)
                {
                    xkrt_area_t * area = &(device->memories[j].area);
                    driver->f_memory_device_deallocate(device->driver_id, (void *) area->chunk0.ptr, area->chunk0.size, j);
                }
            }
        }
        else
            LOGGER_WARN("Driver `%u` is missing `f_device_memory_deallocate`", driver_type);

        // delete device
        if (driver->f_device_destroy)
            driver->f_device_destroy(device->driver_id);
        else
            LOGGER_WARN("Driver `%u` is missing `f_device_destroy`", driver_type);
    }

    /* wait for all the main thread to deinit */
    pthread_barrier_wait(&driver->barrier);

    return NULL;
}

void
xkrt_runtime_t::task_thread_enqueue(
    xkrt_thread_t * thread,
    task_t * task
) {
    thread->deque.push(task);

    // TODO: this is quite ugly, but the thread may be sleeping in two places:
    //  - within its condition
    //  - within a team barrier (thus, the broadcast)
    thread->wakeup();
    if (thread->team)
        pthread_cond_signal(&thread->team->priv.barrier.cond);
}

void
xkrt_runtime_t::task_team_enqueue(
    xkrt_team_t * team,
    task_t * task
) {
    // start at a random thread
    xkrt_thread_t * tls = xkrt_thread_t::get_tls();
    int start = tls->rng() % team->priv.nthreads;

    // find one that is not already working
    for (int i = 0 ; i < team->priv.nthreads ; ++i)
    {
        xkrt_thread_t * thread = team->priv.threads + ((start + i) % team->priv.nthreads);
        bool busy = !thread->sleep.sleeping;
        if (busy)
            continue ;

        // assign it the task
        return this->task_thread_enqueue(thread, task);
    }

    // all threads are working, assigning on the first random one
    xkrt_thread_t * thread = team->priv.threads + start;
    return this->task_thread_enqueue(thread, task);
}

