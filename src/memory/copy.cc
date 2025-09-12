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

XKRT_NAMESPACE_BEGIN

typedef struct  copy_args_t
{
    // the runtime
    runtime_t * runtime;

    // the device responsible to perform the copy
    device_global_id_t device_global_id;

    // pointers
    device_global_id_t dst_device_global_id;
    uintptr_t dst_device_mem;

    device_global_id_t src_device_global_id;
    uintptr_t src_device_mem;

    // size of the copy
    size_t size;

}               copy_args_t;

static void
body_memory_copy_callback(void * vargs [XKRT_CALLBACK_ARGS_MAX])
{
    task_t * task = (task_t *) vargs[0];
    assert(task);

    copy_args_t * args = (copy_args_t *) TASK_ARGS(task);
    args->runtime->task_detachable_decr(task);
}

static void
body_memory_copy(task_t * task)
{
    assert(task);
    copy_args_t * args = (copy_args_t *) TASK_ARGS(task);

    callback_t callback;
    callback.func    = body_memory_copy_callback;
    callback.args[0] = task;

    args->runtime->copy(
        args->device_global_id,
        args->size,
        args->dst_device_global_id,
        args->dst_device_mem,
        args->src_device_global_id,
        args->src_device_mem,
        callback
    );
}

/**
 *  Create a detachable task to be scheduled onto a thread of the device 'device_global_id'.
 *  The task will intiate the copy
 */
extern "C"
void
memory_copy_async(
    runtime_t * runtime,
    const device_global_id_t device_global_id,
    const device_global_id_t dst_device_global_id,
    const uintptr_t dst_device_mem,
    const device_global_id_t src_device_global_id,
    const uintptr_t src_device_mem,
    const size_t size
) {
    thread_t * thread = thread_t::get_tls();
    assert(thread);

    constexpr task_flag_bitfield_t flags = TASK_FLAG_DETACHABLE | TASK_FLAG_DEVICE;
    constexpr size_t task_size = task_compute_size(flags, 0);
    constexpr size_t args_size = sizeof(copy_args_t);

    task_t * task = thread->allocate_task(task_size + args_size);
    new (task) task_t(runtime->formats.copy_async, flags);

    task_dev_info_t * dev = TASK_DEV_INFO(task);
    new (dev) task_dev_info_t(device_global_id, UNSPECIFIED_TASK_ACCESS);

    task_det_info_t * det = TASK_DET_INFO(task);
    new (det) task_det_info_t();

    copy_args_t * args = (copy_args_t *) TASK_ARGS(task, task_size);
    args->runtime = runtime;
    args->device_global_id = device_global_id;
    args->dst_device_global_id = dst_device_global_id;
    args->dst_device_mem = dst_device_mem;
    args->src_device_global_id = src_device_global_id;
    args->src_device_mem = src_device_mem;
    args->size = size;

    # ifndef NDEBUG
    snprintf(task->label, sizeof(task->label), "copy");
    # endif /* NDEBUG */

    runtime->task_detachable_incr(task);
    runtime->task_commit(task);
}

void
memory_copy_async_register_format(runtime_t * runtime)
{
    task_format_t format;
    memset(format.f, 0, sizeof(format.f));
    format.f[TASK_FORMAT_TARGET_HOST] = (task_format_func_t) body_memory_copy;
    snprintf(format.label, sizeof(format.label), "memory_copy");
    runtime->formats.copy_async = task_format_create(&(runtime->formats.list), &format);
}

void
runtime_t::copy(
    const device_global_id_t      device_global_id,
    const memory_view_t         & host_view,
    const device_global_id_t      dst_device_global_id,
    const memory_replica_view_t & dst_device_view,
    const device_global_id_t      src_device_global_id,
    const memory_replica_view_t & src_device_view,
    const callback_t            & callback
) {
    device_t * device = this->device_get(device_global_id);
    stream_instruction_t * instr = device->offloader_stream_instruction_submit_copy<memory_view_t, memory_replica_view_t>(
        host_view,
        dst_device_global_id,
        dst_device_view,
        src_device_global_id,
        src_device_view
    );
    instr->push_callback(callback);
}

void
runtime_t::copy(
    const device_global_id_t   device_global_id,
    const size_t               size,
    const device_global_id_t   dst_device_global_id,
    const uintptr_t            dst_device_addr,
    const device_global_id_t   src_device_global_id,
    const uintptr_t            src_device_addr,
    const callback_t         & callback
) {
    device_t * device = this->device_get(device_global_id);
    // TODO: create 1x instruction per pinned segment, and callback
    stream_instruction_t * instr = device->offloader_stream_instruction_submit_copy<size_t, uintptr_t>(
        size,
        dst_device_global_id,
        dst_device_addr,
        src_device_global_id,
        src_device_addr
    );
    instr->push_callback(callback);
}

XKRT_NAMESPACE_END
