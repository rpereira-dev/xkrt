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

static inline xkrt_device_global_id_t
__task_device(
    task_t * task
) {
    // if that task must execute on a device
    if (task->flags & TASK_FLAG_DEVICE)
    {
        task_dev_info_t * dev = TASK_DEV_INFO(task);
        assert(dev);

        // if it has a targeted device set, return it
        if (dev->targeted_device_id != UNSPECIFIED_DEVICE_GLOBAL_ID)
            return dev->targeted_device_id;

        // if it has an OCR, we may already know which device will own most
        // bytes once all predecessor tasks finished their execution, but its
        // not trivial to implement at that point. So it is not supported yet.
        // This should:
        //  - for each access of the task
        //      - get predecessor writers
        //      - get place of execution of writers
        //      - if they are all known,
        //          - return the device with most bytes written
        //      - else
        //          - if amongst known devices, one owns more than half of the bytes,
        //              - return this device
        //          - else
        //              - we dont know yet, return UNSPECIFIED_DEVICE
        if (dev->ocr_access_index)
        {
            # pragma message(TODO "See file and comment: implement pre-fetching for tasks with OCR")
        }

        // could not find the device to execute
        return UNSPECIFIED_DEVICE_GLOBAL_ID;
    }
    // else, it executes on the host
    else
        return HOST_DEVICE_GLOBAL_ID;
}

/**
 *  - transition the task to completed
 *  - initiate memory prefetching for successors whose place of execution is known
 *  - enqueue all ready successors
 */
static inline void
__task_complete(
    xkrt_runtime_t * runtime,
    task_t * task
) {
    // TODO: instead, can we have a counter per thread, to reduce the number of
    // updates on the 'parent' counter ?
    task->parent->cc.fetch_sub(1, std::memory_order_relaxed);

    // assertions
    assert(
        task->state.value == TASK_STATE_DATA_FETCHED ||
        task->state.value == TASK_STATE_READY
    );
    if (task->flags & (TASK_FLAG_DETACHABLE | TASK_FLAG_DEPENDENT))
    {
        assert(
            ((task->flags & TASK_FLAG_DEPENDENT)  && (TASK_DEP_INFO(task)->wc.load() == 0)) ||
            ((task->flags & TASK_FLAG_DETACHABLE) && (TASK_DET_INFO(task)->wc.load() == 2))
        );
    }

    // transition the task
    SPINLOCK_LOCK(task->state.lock);
    {
        task->state.value = TASK_STATE_COMPLETED;
        LOGGER_DEBUG_TASK_STATE(task);
    }
    SPINLOCK_UNLOCK(task->state.lock);
    assert(task->parent);

    // if the task has successors, that dependency is now satisfied
    if (task->flags & TASK_FLAG_DEPENDENT)
    {
        task_dep_info_t * dep = TASK_DEP_INFO(task);
        access_t * accesses = TASK_ACCESSES(task);
        for (task_access_counter_t i = 0 ; i < dep->ac ; ++i)
        {
            access_t * access = accesses + i;

            // detached access, not my responsibility to fulfill this dependency
            if (access->mode & ACCESS_MODE_D)
                continue ;

            for (access_t * succ_access : access->successors)
            {
                // get successor task
                task_t * succ = succ_access->task;

                ////////////////////////
                // MEMORY PREFETCHING //
                ////////////////////////

                if (runtime->conf.enable_prefetching)
                {
                    // if the succ access is not being fetched, or got fetched already
                    if (succ_access->state == ACCESS_STATE_INIT)
                    {
                        // if the pred access wrote memory
                        if (access->mode & ACCESS_MODE_W)
                        {
                            // if successor device can already be known
                            const xkrt_device_global_id_t device_global_id = __task_device(succ);
                            if (device_global_id != UNSPECIFIED_DEVICE_GLOBAL_ID)
                            {
                                // then we can prefetch memory
                                MemoryCoherencyController * mcc = task_get_memory_controller(
                                        runtime, succ->parent, succ_access);
                                if (mcc)
                                    mcc->fetch(access, device_global_id);
                            }
                        }
                    }
                }

                //////////////////////////////////
                // RELEASE TASK DEPENPENDENCIES //
                //////////////////////////////////

                assert(succ->flags & TASK_FLAG_DEPENDENT);
                task_dep_info_t * sdep = TASK_DEP_INFO(succ);

                // task may be ready now
                if (sdep->wc.fetch_sub(1, std::memory_order_seq_cst) == 1)
                    __task_ready(succ, xkrt_runtime_submit_task, runtime);
            }
        }
    }
    XKRT_STATS_INCR(runtime->stats.tasks[task->fmtid].completed, 1);
}

/* decrease detachable ref counter by 1, and call F(..., succ) foreach task
 * 'succ' that became ready */
static inline void
__task_detachable_post(
    xkrt_runtime_t * runtime,
    task_t * task
) {
    assert(task->flags & TASK_FLAG_DETACHABLE);
    task_det_info_t * det = TASK_DET_INFO(task);
    if (det->wc.fetch_add(1, std::memory_order_relaxed) == 1)
        __task_complete(runtime, task);
}

/* transition the task to the state 'executed' - and eventually to 'completed' or 'detached' */
static inline void
__task_executed(
    xkrt_runtime_t * runtime,
    task_t * task
) {
    if (task->flags & TASK_FLAG_DETACHABLE)
        __task_detachable_post(runtime, task);
    else
        __task_complete(runtime, task);
}

static void
xkrt_device_task_executed_callback(
    const void * args[XKRT_CALLBACK_ARGS_MAX]
) {
    xkrt_runtime_t * runtime = (xkrt_runtime_t *) args[0];
    assert(runtime);

    task_t * task = (task_t *) args[1];
    assert(task);

    __task_executed(runtime, task);
}

/**
 * Must be called once all task accesses were fetched, to queue the task kernel for execution
 *  - driver - the driver to use for executing the kernel
 *  - device - the device to use for executing the kernel
 *  - task   - the task
 */
void
xkrt_device_task_execute(
    xkrt_runtime_t * runtime,
    xkrt_device_t * device,
    task_t * task
) {
    xkrt_thread_t * thread = xkrt_thread_t::get_tls();
    assert(thread);

    task_format_t * format;

    /* running an empty task */
    if (task->fmtid == TASK_FORMAT_NULL)
    {
        __task_executed(runtime, task);
    }
    else
    {
        /* retrieve task format */
        format = task_format_get(&(runtime->formats.list), task->fmtid);
        assert(format);

        // convert device driver type to task format target
        task_format_target_t targetfmt;
        switch (device->driver_type)
        {
            # define CASE(X)                            \
                case (XKRT_DRIVER_TYPE_##X):            \
                    targetfmt = TASK_FORMAT_TARGET_##X; \
                    break ;

            CASE(HOST)
            CASE(CUDA)
            CASE(ZE)
            CASE(CL)
            CASE(HIP)
            CASE(SYCL)

            default:
                LOGGER_FATAL("Invalid device driver type");
        }

        /* if there is a format */
        if (format)
        {
            /* if there is a body to execute */
            if (format->f[targetfmt] == NULL)
                targetfmt = TASK_FORMAT_TARGET_HOST;

            if (format->f[targetfmt])
            {
                /* if its a host task */
                if (targetfmt == TASK_FORMAT_TARGET_HOST)
                {
                    task_t * current = thread->current_task;
                    thread->current_task = task;
                    ((void (*)(task_t *)) format->f[TASK_FORMAT_TARGET_HOST])(task);
                    thread->current_task = current;

                    /* if the task yielded, requeue it */
                    if (task->flags & TASK_FLAG_REQUEUE)
                    {
                        task->flags = task->flags & ~(TASK_FLAG_REQUEUE);
                        runtime->task_thread_enqueue(thread, task);
                    }
                    /* else, it executed entirely */
                    else
                        __task_executed(runtime, task);
                }
                /* else, if its a device task */
                else
                {
                    /* the task will complete in the callback called asynchronously on kernel completion */
                    xkrt_callback_t callback;
                    callback.func    = xkrt_device_task_executed_callback;
                    callback.args[0] = runtime;
                    callback.args[1] = task;
                    assert(XKRT_CALLBACK_ARGS_MAX >= 2);

                    /* submit kernel launch instruction */
                    device->offloader_stream_instruction_submit_kernel(
                        (void (*)(void *, void *, xkrt_stream_instruction_counter_t)) format->f[targetfmt],
                        task,
                        callback
                    );
                }
            }
            else
                LOGGER_FATAL("Task format for `%p` has no impl for device `%u`", task, device->global_id);
        }
        else
            LOGGER_FATAL("Invalid format for task `%p`", task);
    }
}

static void
body_host_capture(task_t * task)
{
    assert(task);

    std::function<void(task_t *)> * f = (std::function<void(task_t *)> *) TASK_ARGS(task);
    (*f)(task);
}

void
xkrt_task_host_capture_register_format(xkrt_runtime_t * runtime)
{
    task_format_t format;
    memset(format.f, 0, sizeof(format.f));
    format.f[TASK_FORMAT_TARGET_HOST] = (task_format_func_t) body_host_capture;
    snprintf(format.label, sizeof(format.label), "host_capture");
    runtime->formats.host_capture = task_format_create(&(runtime->formats.list), &format);
}

void
xkrt_runtime_t::task_commit(task_t * task)
{
    xkrt_thread_t * thread = xkrt_thread_t::get_tls();
    assert(thread);

    thread->commit(task, xkrt_runtime_submit_task, this);
    XKRT_STATS_INCR(this->stats.tasks[task->fmtid].commited, 1);
}

void
xkrt_runtime_t::task_detachable_post(task_t * task)
{
    assert(task);
    assert(task->flags & TASK_FLAG_DETACHABLE);
    __task_detachable_post(this, task);
}

void
xkrt_runtime_t::task_complete(task_t * task)
{
    assert(task);
    assert(!(task->flags & TASK_FLAG_DETACHABLE));

    __task_complete(this, task);
}

void
xkrt_runtime_t::task_run(
    xkrt_team_t * team,
    xkrt_thread_t * thread,
    task_t * task
) {
    assert(team);
    assert(thread);
    assert(task);

    assert(thread == xkrt_thread_t::get_tls());
    if (task->fmtid != TASK_FORMAT_NULL)
    {
        task_format_t * format = this->formats.list.list + task->fmtid;
        assert(format->f[TASK_FORMAT_TARGET_HOST]);
        task_t * current = thread->current_task;
        thread->current_task = task;
        void (*f)(task_t *) = (void (*)(task_t *)) format->f[TASK_FORMAT_TARGET_HOST];
        f(task);
        thread->current_task = current;
    }
    __task_executed(this, task);
}

/** duplicate a moldable task */
task_t *
xkrt_runtime_t::task_dup(
    const task_t * task
) {
    assert(task->flags & TASK_FLAG_MOLDABLE);

    task_mol_info_t * mol = TASK_MOL_INFO(task);
    assert(mol);

    const size_t args_size = mol->args_size;
    const size_t task_size = TASK_SIZE(task);

    xkrt_thread_t * tls = xkrt_thread_t::get_tls();
    task_t * dup = tls->allocate_task(task_size + args_size);
    assert(dup);

    // TODO: probably not C++ standard, but should work ?
    memcpy(dup, task, task_size + args_size);

    # ifndef NDEBUG
    snprintf(dup->label, sizeof(dup->label), "%s-dup", task->label);
    # endif

    return dup;
}

/////////////////////
// TASK SUBMISSION //
/////////////////////

static inline void
submit_task_host(
    xkrt_runtime_t * runtime,
    task_t * task
) {
    xkrt_thread_t * tls = xkrt_thread_t::get_tls();
    assert(tls);

    if (tls->team == NULL)
    {
        xkrt_driver_t * driver = runtime->drivers.list[XKRT_DRIVER_TYPE_HOST];
        assert(driver->ndevices_commited == 1);
        runtime->task_team_enqueue(&driver->team, task);
    }
    else
    {
        runtime->task_thread_enqueue(tls, task);
    }
}

static inline void
submit_task_device(
    xkrt_runtime_t * runtime,
    task_t * task
) {
    // task must be a device task
    assert(task->flags & TASK_FLAG_DEVICE);
    task_dev_info_t * dev = TASK_DEV_INFO(task);

    // Find the worker to offload the task
    xkrt_device_t * device = NULL;
    xkrt_device_global_id_t device_id = UNSPECIFIED_DEVICE_GLOBAL_ID;

    // if an ocr parameter is set, retrieve the device accordingly
    if (dev->ocr_access_index != UNSPECIFIED_TASK_ACCESS)
    {
        // if an ocr is set, task must be a dependent task (i.e. with some accesses)
        assert(task->flags & TASK_FLAG_DEPENDENT);

        // retrieve the access
        task_dep_info_t * dep = TASK_DEP_INFO(task);
        assert(dev->ocr_access_index >= 0 && dev->ocr_access_index < dep->ac);
        access_t * access = TASK_ACCESSES(task) + dev->ocr_access_index;

        // looking for the device that owns the data
        MemoryCoherencyController * memcontroller = task_get_memory_controller(runtime, task->parent, access);
        assert(memcontroller);
        const xkrt_device_global_id_bitfield_t owners = memcontroller->who_owns(access);
        if (owners)
            device_id = (xkrt_device_global_id_t) (__random_set_bit(owners) - 1);
    }

    // if a target device is set
    if (device_id == UNSPECIFIED_DEVICE_GLOBAL_ID && dev->targeted_device_id != UNSPECIFIED_DEVICE_GLOBAL_ID)
        device_id = dev->targeted_device_id;

    // fallback to round robin if no devices found
    if (device_id == UNSPECIFIED_DEVICE_GLOBAL_ID)
    {
        // must have at least one non-host device
        if (runtime->drivers.devices.n > 1)
        {
            while (1)
            {
                device_id = runtime->drivers.devices.round_robin_device_global_id.fetch_add(1, std::memory_order_relaxed);
                device_id = (xkrt_device_global_id_t) (1 + (device_id % (runtime->drivers.devices.n - 1)));

                xkrt_device_t * device = runtime->drivers.devices.list[device_id];
                if (device)
                    break ;
            }
        }
        else
            LOGGER_FATAL("No device available to execute the task");
    }

    // only coherent async are supported onto the host device yet
    if (device_id == HOST_DEVICE_GLOBAL_ID)
        return submit_task_host(runtime, task);

    assert((device_id >= 0 && device_id < runtime->drivers.devices.n));
    device = runtime->drivers.devices.list[device_id];
    assert(device);

    LOGGER_DEBUG("Enqueuing task `%s` to device %d", task->label, device_id);

    /* push a task to a thread of the device */
    uint8_t tid = device->thread_next.fetch_add(1, std::memory_order_relaxed) % device->nthreads;
    xkrt_thread_t * thread = device->threads[tid];
    runtime->task_thread_enqueue(thread, task);
}

void
xkrt_runtime_submit_task(
    xkrt_runtime_t * runtime,
    task_t * task
) {
    assert(task->state.value == TASK_STATE_READY);
    if (task->flags & TASK_FLAG_DEVICE)
        submit_task_device(runtime, task);
    else
        submit_task_host(runtime, task);
}

void
xkrt_runtime_t::task_enqueue(task_t * task)
{
    xkrt_runtime_submit_task(this, task);
}
