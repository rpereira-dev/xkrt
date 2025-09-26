#ifndef __XKRT_C_H__
#define __XKRT_C_H__

#include <xkrt/consts.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* Types */
typedef void * xkrt_runtime_t;
typedef void * xkrt_task_t;
typedef void * xkrt_access_t;
typedef void * xkrt_team_t;
typedef void * xkrt_driver_t;
typedef void * xkrt_device_t;

/* Runtime init */
int  xkrt_init  (xkrt_runtime_t * runtime);
int  xkrt_deinit(xkrt_runtime_t * runtime);
void xkrt_reset (xkrt_runtime_t * runtime);

/* Memory registration */
int  xkrt_memory_register        (xkrt_runtime_t * runtime, void * ptr, size_t size);
int  xkrt_memory_unregister      (xkrt_runtime_t * runtime, void * ptr, size_t size);
int  xkrt_memory_register_async  (xkrt_runtime_t * runtime, void * ptr, size_t size);
int  xkrt_memory_unregister_async(xkrt_runtime_t * runtime, void * ptr, size_t size);

/* File operations */
int  xkrt_file_read_async (xkrt_runtime_t * runtime, int fd, void * buffer, size_t n, unsigned int nchunks);
int  xkrt_file_write_async(xkrt_runtime_t * runtime, int fd, void * buffer, size_t n, unsigned int nchunks);

/* Device memory allocation */
void * xkrt_memory_device_allocate   (xkrt_runtime_t * runtime, xkrt_device_global_id_t device, size_t size);
void   xkrt_memory_device_deallocate (xkrt_runtime_t * runtime, xkrt_device_global_id_t device, void * chunk);
void * xkrt_memory_host_allocate     (xkrt_runtime_t * runtime, xkrt_device_global_id_t device, size_t size);
void   xkrt_memory_host_deallocate   (xkrt_runtime_t * runtime, xkrt_device_global_id_t device, void * ptr, size_t size);
void * xkrt_memory_unified_allocate  (xkrt_runtime_t * runtime, xkrt_device_global_id_t device, size_t size);
void   xkrt_memory_unified_deallocate(xkrt_runtime_t * runtime, xkrt_device_global_id_t device, void * ptr, size_t size);

/* Drivers/devices */
xkrt_driver_t * xkrt_driver_get(xkrt_runtime_t * runtime, xkrt_driver_type_t type);
xkrt_device_t * xkrt_device_get(xkrt_runtime_t * runtime, xkrt_device_global_id_t device);

unsigned int xkrt_get_ndevices    (xkrt_runtime_t * runtime);
unsigned int xkrt_get_ndevices_max(xkrt_runtime_t * runtime);

/* TASKING */
void xkrt_task_commit  (xkrt_runtime_t * runtime, xkrt_task_t * task);
void xkrt_task_complete(xkrt_runtime_t * runtime, xkrt_task_t * task);

void xkrt_task_detachable_incr(xkrt_runtime_t * runtime, xkrt_task_t * task);
void xkrt_task_detachable_decr(xkrt_runtime_t * runtime, xkrt_task_t * task);

void xkrt_task_enqueue(xkrt_runtime_t * runtime, xkrt_task_t * task);

/* TASK SPAWN */
typedef void (*xkrt_task_func_t)(xkrt_task_t * task, void * user_data);

void xkrt_task_spawn(
    xkrt_runtime_t * runtime,
    xkrt_task_func_t func,
    void * user_data
);

void xkrt_task_wait(xkrt_runtime_t * runtime);

/* TEAM UTILITIES */
xkrt_team_t * xkrt_team_get    (xkrt_runtime_t * runtime, xkrt_driver_type_t type);
xkrt_team_t * xkrt_team_get_any(xkrt_runtime_t * runtime, xkrt_driver_type_bitfield_t types);

/* TEAM TASK SPAWN */
void xkrt_team_task_spawn(
    xkrt_runtime_t * runtime,
    xkrt_team_t * team,
    xkrt_task_func_t func,
    void * user_data
);

typedef void (*xkrt_task_set_accesses_func_t)(xkrt_task_t * task, xkrt_access_t * accesses, void * user_data);

void xkrt_team_task_spawn_with_accesses(
    xkrt_runtime_t * runtime,
    xkrt_team_t * team,
    xkrt_task_set_accesses_func_t set_accesses,
    xkrt_task_func_t func,
    void * user_data
);

#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */

#endif /* __XKRT_C_H__ */

