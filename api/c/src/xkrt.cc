#include <xkrt/runtime.h>
#include <xkrt/xkrt.h>
#include <assert.h>
#include <stdlib.h>

XKRT_NAMESPACE_USE;

#ifdef __cplusplus
extern "C" {
#endif

int
xkrt_init(
    xkrt_runtime_t * runtime
) {
    assert(runtime);
    runtime_t * rt = (runtime_t *) malloc(sizeof(runtime_t));
    assert(rt);
    *runtime = rt;
    return rt->init();
}

int
xkrt_deinit(
    xkrt_runtime_t * runtime
) {
    assert(runtime && *runtime);
    runtime_t * rt = (runtime_t *)(*runtime);
    int ret = rt->deinit();
    free(rt);
    *runtime = NULL;
    return ret;
}

void
xkrt_reset(
    xkrt_runtime_t * runtime
) {
    assert(runtime && *runtime);
    runtime_t * rt = (runtime_t *)(*runtime);
    rt->reset();
}

int
xkrt_memory_register(
    xkrt_runtime_t * runtime,
    void * ptr,
    size_t size
) {
    assert(runtime && *runtime);
    runtime_t * rt = (runtime_t *)(*runtime);
    return rt->memory_register(ptr, size);
}

int
xkrt_memory_unregister(
    xkrt_runtime_t * runtime,
    void * ptr,
    size_t size
) {
    assert(runtime && *runtime);
    runtime_t * rt = (runtime_t *)(*runtime);
    return rt->memory_unregister(ptr, size);
}

int
xkrt_memory_register_async(
    xkrt_runtime_t * runtime,
    void * ptr,
    size_t size
) {
    assert(runtime && *runtime);
    runtime_t * rt = (runtime_t *)(*runtime);
    return rt->memory_register_async(ptr, size);
}

int
xkrt_memory_unregister_async(
    xkrt_runtime_t * runtime,
    void * ptr,
    size_t size
) {
    assert(runtime && *runtime);
    runtime_t * rt = (runtime_t *)(*runtime);
    return rt->memory_unregister_async(ptr, size);
}

int
xkrt_file_read_async(
    xkrt_runtime_t * runtime,
    int fd,
    void * buffer,
    size_t n,
    unsigned int nchunks
) {
    assert(runtime && *runtime);
    runtime_t * rt = (runtime_t *)(*runtime);
    return rt->file_read_async(fd, buffer, n, nchunks);
}

int
xkrt_file_write_async(
    xkrt_runtime_t * runtime,
    int fd,
    void * buffer,
    size_t n,
    unsigned int nchunks
) {
    assert(runtime && *runtime);
    runtime_t * rt = (runtime_t *)(*runtime);
    return rt->file_write_async(fd, buffer, n, nchunks);
}

void *
xkrt_memory_device_allocate(
    xkrt_runtime_t * runtime,
    device_global_id_t device,
    size_t size
) {
    assert(runtime && *runtime);
    runtime_t * rt = (runtime_t *)(*runtime);
    return (void *)rt->memory_device_allocate(device, size);
}

void
xkrt_memory_device_deallocate(
    xkrt_runtime_t * runtime,
    device_global_id_t device,
    void * chunk
) {
    assert(runtime && *runtime);
    runtime_t * rt = (runtime_t *)(*runtime);
    rt->memory_device_deallocate(device, (area_chunk_t*)chunk);
}

void *
xkrt_memory_host_allocate(
    xkrt_runtime_t * runtime,
    device_global_id_t device,
    size_t size
) {
    assert(runtime && *runtime);
    runtime_t * rt = (runtime_t *)(*runtime);
    return rt->memory_host_allocate(device, size);
}

void
xkrt_memory_host_deallocate(
    xkrt_runtime_t * runtime,
    device_global_id_t device,
    void * ptr,
    size_t size
) {
    assert(runtime && *runtime);
    runtime_t * rt = (runtime_t *)(*runtime);
    rt->memory_host_deallocate(device, ptr, size);
}

void *
xkrt_memory_unified_allocate(
    xkrt_runtime_t * runtime,
    device_global_id_t device,
    size_t size
) {
    assert(runtime && *runtime);
    runtime_t * rt = (runtime_t *)(*runtime);
    return rt->memory_unified_allocate(device, size);
}

void
xkrt_memory_unified_deallocate(
    xkrt_runtime_t * runtime,
    device_global_id_t device,
    void * ptr,
    size_t size
) {
    assert(runtime && *runtime);
    runtime_t * rt = (runtime_t *)(*runtime);
    rt->memory_unified_deallocate(device, ptr, size);
}

xkrt_driver_t *
xkrt_driver_get(
    xkrt_runtime_t * runtime,
    xkrt_driver_type_t type
) {
    assert(runtime && *runtime);
    runtime_t * rt = (runtime_t *)(*runtime);
    return (xkrt_driver_t *) rt->driver_get(type);
}

xkrt_device_t *
xkrt_device_get(
    xkrt_runtime_t * runtime,
    device_global_id_t device
) {
    assert(runtime && *runtime);
    runtime_t * rt = (runtime_t *)(*runtime);
    return (xkrt_device_t *) rt->device_get(device);
}

unsigned int
xkrt_get_ndevices(
    xkrt_runtime_t * runtime
) {
    assert(runtime && *runtime);
    runtime_t * rt = (runtime_t *)(*runtime);
    return rt->get_ndevices();
}

unsigned int
xkrt_get_ndevices_max(
    xkrt_runtime_t * runtime
) {
    assert(runtime && *runtime);
    runtime_t * rt = (runtime_t *)(*runtime);
    return rt->get_ndevices_max();
}

// ---------------------------
// TASKING
// ---------------------------

void
xkrt_task_commit(
    xkrt_runtime_t * runtime,
    xkrt_task_t * task
) {
    assert(runtime && *runtime);
    runtime_t * rt = (runtime_t *)(*runtime);
    rt->task_commit((task_t *) task);
}

void
xkrt_task_complete(
    xkrt_runtime_t * runtime,
    xkrt_task_t * task
) {
    assert(runtime && *runtime);
    runtime_t * rt = (runtime_t *)(*runtime);
    rt->task_complete((task_t *) task);
}

void
xkrt_task_detachable_incr(
    xkrt_runtime_t * runtime,
    xkrt_task_t * task
) {
    assert(runtime && *runtime);
    runtime_t * rt = (runtime_t *)(*runtime);
    rt->task_detachable_incr((task_t *) task);
}

void
xkrt_task_detachable_decr(
    xkrt_runtime_t * runtime,
    xkrt_task_t * task
) {
    assert(runtime && *runtime);
    runtime_t * rt = (runtime_t *)(*runtime);
    rt->task_detachable_decr((task_t *) task);
}

void
xkrt_task_enqueue(
    xkrt_runtime_t * runtime,
    xkrt_task_t * task
) {
    assert(runtime && *runtime);
    runtime_t * rt = (runtime_t *)(*runtime);
    rt->task_enqueue((task_t *) task);
}

/* TASK SPAWN */
void
xkrt_task_spawn(
    xkrt_runtime_t * runtime,
    xkrt_task_func_t func,
    void * user_data
) {
    assert(runtime && *runtime);
    runtime_t * rt = (runtime_t *)(*runtime);

    auto wrapper = [func, user_data](runtime_t * runtime, device_t * device, task_t * task) {
        func((xkrt_runtime_t *) &runtime, (xkrt_device_t *) device, (xkrt_task_t *) task, user_data);
    };

    return rt->task_spawn(wrapper);
}

void
xkrt_task_wait(xkrt_runtime_t * runtime)
{
    assert(runtime && *runtime);
    runtime_t * rt = (runtime_t *)(*runtime);
    return rt->task_wait();
}

// ---------------------------
// TEAM UTILITIES
// ---------------------------

xkrt_team_t *
xkrt_team_get(
    xkrt_runtime_t * runtime,
    xkrt_driver_type_t type
) {
    assert(runtime && *runtime);
    runtime_t * rt = (runtime_t *)(*runtime);
    return (xkrt_team_t *) rt->team_get(type);
}

xkrt_team_t *
xkrt_team_get_any(
    xkrt_runtime_t * runtime,
    xkrt_driver_type_bitfield_t types
) {
    assert(runtime && *runtime);
    runtime_t * rt = (runtime_t *)(*runtime);
    return (xkrt_team_t *) rt->team_get_any(types);
}

// ---------------------------
// TEAM TASK SPAWN (C CALLBACKS)
// ---------------------------

void
xkrt_team_task_spawn(
    xkrt_runtime_t * runtime,
    xkrt_team_t * team,
    xkrt_task_func_t func,
    void * user_data
) {
    assert(runtime && *runtime);
    runtime_t * rt = (runtime_t *)(*runtime);

    auto wrapper = [func, user_data](runtime_t * runtime, device_t * device, task_t * task) {
        func((xkrt_runtime_t *) &runtime, (xkrt_device_t *) &device, (xkrt_task_t *) task, user_data);
    };

    rt->team_task_spawn((team_t *) team, wrapper);
}

void
xkrt_team_task_spawn_with_accesses(
    xkrt_runtime_t * runtime,
    xkrt_team_t * team,
    xkrt_task_set_accesses_func_t set_accesses,
    xkrt_task_func_t func,
    void * user_data
) {
    assert(runtime && *runtime);
    runtime_t * rt = (runtime_t *)(*runtime);

    auto set_wrapper = [set_accesses, user_data](task_t * task, access_t * access) {
        if (set_accesses) set_accesses((xkrt_task_t *) task, (xkrt_access_t *) access, user_data);
    };

    auto func_wrapper = [func, user_data](runtime_t * runtime, device_t * device, task_t * task) {
        func((xkrt_runtime_t *) &runtime, (xkrt_device_t *) &device, (xkrt_task_t *) task, user_data);
    };

    rt->team_task_spawn<0, true, false>((team_t *) team, set_wrapper, nullptr, func_wrapper);
}

#ifdef __cplusplus
} // extern "C"
#endif

