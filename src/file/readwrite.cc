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

typedef struct  file_args_t
{
    xkrt_runtime_t * runtime;
    int fd;
    size_t offset;
    void * buffer;
    size_t size;
    # if 0
    unsigned int nchunks;
    std::atomic<unsigned int> nchunks_completed;
    # endif
}               file_args_t;

/* the task completes once all segment completed */
constexpr task_flag_bitfield_t flags = TASK_FLAG_DEPENDENT | TASK_FLAG_DETACHABLE;
constexpr unsigned int ac  = 1;
constexpr size_t task_size = task_compute_size(flags, ac);
constexpr size_t args_size = sizeof(file_args_t);

static void
body_file_async_callback(const void * vargs [XKRT_CALLBACK_ARGS_MAX])
{
    task_t * task = (task_t *) vargs[0];
    assert(task);

    /* retrieve task args */
    file_args_t * args = (file_args_t *) TASK_ARGS(task, task_size);

    # if 1
    args->runtime->task_detachable_post(task);
    # else
    LOGGER_WARN("TODO: Implement partitioned accesses");
    // if all read/write completed, complete the task
    if (args->nchunks_completed.fetch_add(1, std::memory_order_relaxed) == args->nchunks - 1)
        args->runtime->task_detachable_post(task);
    # endif
}

template<xkrt_stream_instruction_type_t T>
static void
body_file_async(task_t * task)
{
    assert(task);

    xkrt_callback_t callback;
    callback.func    = body_file_async_callback;
    callback.args[0] = task;

    file_args_t * args = (file_args_t *) TASK_ARGS(task, task_size);

    xkrt_device_t * device = args->runtime->device_get(HOST_DEVICE_GLOBAL_ID);
    assert(device);

    # if 1

    device->offloader_stream_instruction_submit_file<T>(
            args->fd, args->buffer, args->size, args->offset, callback);

    # else
    // compute chunk size
    const size_t chunksize = args->n / args->nchunks;
    assert(chunksize > 0);

    const uintptr_t ptr = (const uintptr_t) args->buffer;
    for (size_t i = 0 ; i < args->nchunks ; ++i)
    {
        callback.args[1] = (void *) i;

        device->offloader_stream_instruction_submit_file<T>(
            args->fd,
            (void *) (ptr + i * chunksize),
            (i == args->nchunks - 1) ? (args->n - i*chunksize) : chunksize,
            callback
        );
    }
    # endif
}

// TODO: reimplement using partitionned dependencies
template<xkrt_stream_instruction_type_t T>
static inline int
file_async(
    xkrt_runtime_t * runtime,
    int fd,
    void * buffer,
    size_t n,
    unsigned int nchunks
) {
    # if 1

    static_assert(T == XKRT_STREAM_INSTR_TYPE_FD_READ || T == XKRT_STREAM_INSTR_TYPE_FD_WRITE);
    assert(nchunks > 0);

    // compute number of instructions to spawn
    if (n < nchunks)
       nchunks = (unsigned int) n;

    // compute chunk size
    const size_t chunksize = n / nchunks;
    assert(chunksize > 0);

    // create the task that submit the i/o instruction
    xkrt_thread_t * thread = xkrt_thread_t::get_tls();
    assert(thread);

    // get task format
    const task_format_id_t fmtid = (T == XKRT_STREAM_INSTR_TYPE_FD_READ) ? runtime->formats.file_read_async : runtime->formats.file_write_async;

    const uintptr_t p = (const uintptr_t) buffer;

    for (unsigned int i = 0 ; i < nchunks ; ++i)
    {
        task_t * task = thread->allocate_task(task_size + args_size);
        new(task) task_t(fmtid, flags);

        // copy arguments
        file_args_t * args = (file_args_t *) TASK_ARGS(task, task_size);
        args->runtime = runtime;
        args->fd = fd;
        args->offset = i * chunksize;
        args->buffer = (void *) (p + args->offset);
        args->size   = (i == nchunks - 1) ? (n - args->offset) : chunksize;

        # if 0 /* no need to init det info if initializing dep info */
        task_det_info_t * det = TASK_DET_INFO(task);
        new (det) task_det_info_t();
        # endif

        task_dep_info_t * dep = TASK_DEP_INFO(task);
        new (dep) task_dep_info_t(ac);

        # ifndef NDEBUG
        snprintf(task->label, sizeof(task->label), T == XKRT_STREAM_INSTR_TYPE_FD_READ ? "fread" : "fwrite");
        # endif

        const uintptr_t a = (const uintptr_t) args->buffer;
        const uintptr_t b = a + args->size;

        // detached virtual write onto the memory segment
        access_t * accesses = TASK_ACCESSES(task, flags);
        constexpr access_mode_t mode = (access_mode_t) (ACCESS_MODE_W | ACCESS_MODE_V);
        new(accesses + 0) access_t(task, a, b, mode);
        thread->resolve<1>(task, accesses);

        // commit
        runtime->task_commit(task);
    }

    return 0;

    # else
    // OLD IMPLEMENTATION WITH 1 single task, use that once we have partitioned dependencies
    LOGGER_FATAL("TODO: reimplement to create 'n' tasks");

    static_assert(T == XKRT_STREAM_INSTR_TYPE_FD_READ || T == XKRT_STREAM_INSTR_TYPE_FD_WRITE);
    assert(nchunks > 0);

    // compute number of instructions to spawn
    if (n < nchunks)
       nchunks = (unsigned int) n;

    // create the task that submit the i/o instruction
    xkrt_thread_t * thread = xkrt_thread_t::get_tls();
    assert(thread);

    // get task format
    const task_format_id_t fmtid = (T == XKRT_STREAM_INSTR_TYPE_FD_READ) ? runtime->formats.file_read_async : runtime->formats.file_write_async;

    task_t * task = thread->allocate_task(task_size + args_size);
    new(task) task_t(fmtid, flags);

    // copy arguments
    file_args_t * args = (file_args_t *) TASK_ARGS(task, task_size);
    args->runtime = runtime;
    args->fd = fd;
    args->buffer = buffer;
    args->n = n;
    args->nchunks = nchunks;
    args->nchunks_completed.store(0);

    # if 0 /* no need to init det info if initializing dep info */
    task_det_info_t * det = TASK_DET_INFO(task);
    new (det) task_det_info_t();
    # endif

    task_dep_info_t * dep = TASK_DEP_INFO(task);
    new (dep) task_dep_info_t(ac);

    # ifndef NDEBUG
    snprintf(task->label, sizeof(task->label), T == XKRT_STREAM_INSTR_TYPE_FD_READ ? "fread" : "fwrite");
    # endif

    // detached virtual write onto the memory segment
    access_t * accesses = TASK_ACCESSES(task, flags);
    // constexpr access_mode_t mode = (access_mode_t) (ACCESS_MODE_W | ACCESS_MODE_V | ACCESS_MODE_D);
    constexpr access_mode_t mode = (access_mode_t) (ACCESS_MODE_W | ACCESS_MODE_V);
    LOGGER_WARN("Right now, fulfilling all dependencies on task completion... implemented detached accesses and fix me");
    const uintptr_t ptr = (const uintptr_t) buffer;
    new(accesses + 0) access_t(task, ptr, ptr + n, mode);
    thread->resolve<1>(task, accesses);

    // commit
    runtime->task_commit(task);

    return 0;
    # endif
}

int
xkrt_runtime_t::file_read_async(
    int fd,
    void * buffer,
    size_t n,
    unsigned int nchunks
) {
    return file_async<XKRT_STREAM_INSTR_TYPE_FD_READ>(this, fd, buffer, n, nchunks);
}

int
xkrt_runtime_t::file_write_async(
    int fd,
    void * buffer,
    size_t n,
    unsigned int nchunks
) {
    return file_async<XKRT_STREAM_INSTR_TYPE_FD_WRITE>(this, fd, buffer, n, nchunks);
}

void
xkrt_file_async_register_format(xkrt_runtime_t * runtime)
{
    {
        task_format_t format;
        memset(format.f, 0, sizeof(format.f));
        format.f[TASK_FORMAT_TARGET_HOST] = (task_format_func_t) body_file_async<XKRT_STREAM_INSTR_TYPE_FD_READ>;
        snprintf(format.label, sizeof(format.label), "file_read_async");
        runtime->formats.file_read_async = task_format_create(&(runtime->formats.list), &format);
    }

    {
        task_format_t format;
        memset(format.f, 0, sizeof(format.f));
        format.f[TASK_FORMAT_TARGET_HOST] = (task_format_func_t) body_file_async<XKRT_STREAM_INSTR_TYPE_FD_WRITE>;
        snprintf(format.label, sizeof(format.label), "file_write_async");
        runtime->formats.file_write_async = task_format_create(&(runtime->formats.list), &format);
    }
}
