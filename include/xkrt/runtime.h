/*
** Copyright 2024,2025 INRIA
**
** Contributors :
** Thierry Gautier, thierry.gautier@inrialpes.fr
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

#ifndef __XKRT_RUNTIME_H__
# define __XKRT_RUNTIME_H__

# include <xkrt/conf/conf.h>
# include <xkrt/driver/driver.h>
# include <xkrt/thread/thread.h>
# include <xkrt/memory/access/coherency-controller.hpp>
# include <xkrt/memory/routing/router-affinity.hpp>
# include <xkrt/stats/stats.h>
# include <xkrt/sync/spinlock.h>
# include <xkrt/task/task.hpp>

# include <hwloc.h>

# include <map>

typedef enum    xkrt_runtime_state_t : uint8_t
{
    XKRT_RUNTIME_DEINITIALIZED = 0,
    XKRT_RUNTIME_INITIALIZED,
}               xkrt_runtime_state_t;

typedef struct  xkrt_runtime_t
{
    /* runtime state */
    std::atomic<xkrt_runtime_state_t> state;

    /* driver list */
    xkrt_drivers_t drivers;

    /* task formats */
    struct {
        task_formats_t list;
        task_format_id_t copy_async;
        task_format_id_t host_capture;
        task_format_id_t memory_touch_async;
    } formats;

    /* user conf */
    xkrt_conf_t conf;

    /* memory router */
    RouterAffinity router;

    # if XKRT_MEMORY_REGISTER_OVERFLOW_PROTECTION
    /* registered memory segments, map: addr -> size */
    std::map<uintptr_t, size_t> registered_memory;
    # endif /* XKRT_MEMORY_REGISTER_OVERFLOW_PROTECTION */

    /* hwloc topology, read only, initialized at init */
    hwloc_topology_t topology;

    //////////////////////////////////////////////////////////////////////////////////////////////
    // UTILITIES FOR THE MEMORY COHERENCY TREE - THIS SHOULD GTFO AND BE ABSTRACTED IN THE TREE //
    //////////////////////////////////////////////////////////////////////////////////////////////
    /// It currently only serves as a glue between the tree 'xkrt_device_global_id_t' representation
    /// and the runtime 'xkrt_device_t' structures
    //////////////////////////////////////////////////////////////////////////////////////////////

    ////////////////////
    // DATA MOVEMENTS //
    ////////////////////

    /* Submit a copy instruction to a stream of the device */
    void copy(
        const xkrt_device_global_id_t   device_global_id,
        const size_t                    size,
        const xkrt_device_global_id_t   dst_device_global_id,
        const uintptr_t                 dst_device_addr,
        const xkrt_device_global_id_t   src_device_global_id,
        const uintptr_t                 src_device_addr,
        const xkrt_callback_t         & callback
    );

    /* Submit a copy instruction to a stream of the device */
    void copy(
        const xkrt_device_global_id_t   device_global_id,
        const memory_view_t           & host_view,
        const xkrt_device_global_id_t   dst_device_global_id,
        const memory_replicate_view_t & dst_device_view,
        const xkrt_device_global_id_t   src_device_global_id,
        const memory_replicate_view_t & src_device_view,
        const xkrt_callback_t         & callback
    );

    ////////////
    // MEMORY //
    ////////////

    /* allocate the chunk0 on the device if not allocated already */
    void memory_device_preallocate_ensure(const xkrt_device_global_id_t device_global_id, const int memory_id);

    /* allocate memory onto chunk0 of the given device memory index */
    xkrt_area_chunk_t * memory_device_allocate_on(const xkrt_device_global_id_t device_global_id, const size_t size, const int memory_id);

    /* allocate memory onto chunk0 of the given device */
    xkrt_area_chunk_t * memory_device_allocate(const xkrt_device_global_id_t device_global_id, const size_t size);

    /* deallocate the given chunk */
    void memory_device_deallocate(const xkrt_device_global_id_t device_global_id, xkrt_area_chunk_t * chunk);

    /* dealloacte all memory previously allocated on the device */
    void memory_device_deallocate_all(const xkrt_device_global_id_t device_global_id);

    /* allocate memory onto the host using the driver given device */
    void * memory_host_allocate(const xkrt_device_global_id_t device_global_id, const size_t size);

    /* deallocate memory onto the host using the driver of the given device */
    void memory_host_deallocate(const xkrt_device_global_id_t device_global_id, void * mem, const size_t size);

    /* allocate unified memory using the driver of the given device */
    void * memory_unified_allocate(const xkrt_device_global_id_t device_global_id, const size_t size);

    /* deallocate unified memory using the driver of the given device */
    void memory_unified_deallocate(const xkrt_device_global_id_t device_global_id, void * mem, const size_t size);

    /////////////////////////
    // MEMORY REGISTRATION //
    /////////////////////////

    /**
     *  Create 'n' tasks so that each task i in [0..n-1]:
     *      - access - commutative write on [ptr + i*chunk_size,  ptr + (i+1)*chunk_size]
     *      - routine - touches memory pages in [ptr + i*chunk_size,  ptr + (i+1)*chunk_size]
     */
    int memory_touch_async(xkrt_team_t * team, void * ptr, const size_t chunk_size, int n);

    /**
     *  Create 'n' tasks so that each task i in [0..n-1]
     *      - access - commutative write on [ptr + i*chunk_size,  ptr + (i+1)*chunk_size]
     *      - routine - register a chunk of memory [ptr + i*chunk_size,  ptr + (i+1)*chunk_size].
     *
     *  Note: each task may run several 'cuMemRegister' several time on a single chunk
     */
    int memory_register_async(xkrt_team_t * team, void * ptr, const size_t chunk_size, int n);
    int memory_unregister_async(xkrt_team_t * team, void * ptr, const size_t chunk_size, int n);

    // TODO - this was a preliminary design. In the end, it got moved to the
    // memory coherency controller when fetching data on a
    // ACCESS_MODE_PIN/ACCESS_MODE_UNPIN
    # if 0
    /**
     *  Create tasks to copy memory from/to the host.
     *  The number of tasks spawned depend on the number and the state of segment previously submitted.
     *  Each 'transfer' task
     *      - access - a sequential write on a sub-segment
     *      - routine - submit a copy instruction to the device stream
     *      - is detachable - and is fulfilled once the copy completed
     *
     *  Discussions
     *
     *  This routine supports concurrency with `memory_touch_async` `memory_register_async` `memory_unregister_async` :
     *      if there is a task currently touching/registering/unregistering a
     *      sub-segment, then a dependent transfer task will spawn for that sub-segment
     *
     *      Similarly, if there is an un-going transfer on a segment,
     *      future registering/unregistering tasks will depend on it, and
     *      touching tasks will not touch the sub-segment, as the copy already touched it
     *
     *  Said differently, these interfaces put in competition memory
     *  touching/registering/unregistering/transfer with the heuristic that:
     *      - better initiating transfer early on unregistered memory over waiting for registration
     *
     *  For instance,
     *
     *      Legend: R  = registered memory
     *              U  = unregistered memory
     *              R' = registering memory
     *
     * (1)
     *      host_addr                               host_addr + size
     *          [ R  R  R  R  R  R  R  R  R  R  R  R  R  R  R ]
     *
     *  Then a single transfer task is spawned and mark ready immediatly.
     *
     *
     * (2)
     *      host_addr            X           Y        host_addr + size
     *          [ R  R  R  R  R  U  U  U  U  R  R  R  R  R  R ]
     *
     *  Then 3x transfer tasks spawns and are marked ready immediatly.
     *      [host_addr..X[  ,  [X..Y[  , [Y..host_addr + size[
     *
     *
     * (3)
     *      host_addr            X           Y        host_addr + size
     *          [ R  R  R  R  R  U  U  U  U  R' R' R' R' R' R']
     *
     *  Then 3x transfer tasks spawns,
     *      [host_addr..X[  ,  [X..Y[  are marked ready immediatly.
     *      [Y..host_addr + size[      is made dependent of the task currently registering the segment
     *
     */
    int memory_transfer_async(
        const xkrt_device_global_id_t device_global_id,
        const uintptr_t               device_addr,
        const uintptr_t               host_addr,
        const size_t                  size,
        const bool                    h2d
    );
    # endif

    /////////////////////
    // SYNCHRONIZATION //
    /////////////////////

    /////////////
    // TASKING //
    /////////////

    /* Enqueue a ready task to the given device */
    void task_submit(const xkrt_device_global_id_t device_global_id, task_t * task);

    /* Commit a task - so it may be schedule from now once its dependences completed */
    void task_commit(task_t * task);

    /* Notify once a detachable task */
    void task_detachable_post(task_t * task);

    /* Complete a task */
    void task_complete(task_t * task);

    /* wait for children tasks of the current task to complete */
    void task_wait(void);

    /* schedule a ready task, and return 1 if one task was found, 0 otherwise */
    int task_schedule(void);

    /* spawn a task in the currently executing thread team */
    void task_spawn(const std::function<void(task_t*)> & f);

    ///////////////
    // THREADING //
    ///////////////

    /* Retrieve the cpuset of the calling thread */
    static void thread_getaffinity(cpu_set_t & cpuset);

    /* Bind the calling thread to the given cpu set */
    static void thread_setaffinity(cpu_set_t & cpuset);

    /* create a new thread team */
    void team_create(xkrt_team_t * team);

    /* wait until all threads called the barrier */
    template<bool worksteal = false>
    void team_barrier(xkrt_team_t * team, xkrt_thread_t * thread = NULL);

    /* wait until all threads finished and destroy the team */
    void team_join(xkrt_team_t * team);

    /* start a critical section */
    void team_critical_begin(xkrt_team_t * team);

    /* end a critical section */
    void team_critical_end(xkrt_team_t * team);

    /* blocking parallel_for region */
    void team_parallel_for(xkrt_team_t * team, xkrt_team_parallel_for_func_t func);

    /* spawn a task in the passed team */
    void team_task_spawn(xkrt_team_t * team, const std::function<void(task_t*)> & f);

    /* retrieve the team of thread of the specific driver */
    xkrt_team_t * team_get(const xkrt_driver_type_t type);

    /* retrieve the first non-null driver' team from the passed bitfield */
    xkrt_team_t * team_get_any(const xkrt_driver_type_bitfield_t types);

    ////////////
    // ENERGY //
    ////////////

    /* start recording energy usage */
    void power_start(const xkrt_device_global_id_t device_global_id, xkrt_power_t * pwr);

    /* stop recording and return energy usage */
    void power_stop(const xkrt_device_global_id_t device_global_id, xkrt_power_t * pwr);

    ///////////////
    // UTILITIES //
    ///////////////

    /* get driver */
    xkrt_driver_t * driver_get(const xkrt_driver_type_t type);

    /* get device */
    xkrt_device_t * device_get(const xkrt_device_global_id_t device_global_id);

    /* get number of commited devices */
    unsigned int get_ndevices(void);

    //////////////////////////////////////
    // linked list for freeing on crash //
    //////////////////////////////////////
    struct xkrt_runtime_t * prev;
    struct xkrt_runtime_t * next;

     # if XKRT_SUPPORT_STATS
     struct {
         struct {
             stats_int_t submitted;
             stats_int_t commited;
             stats_int_t completed;
         } tasks[TASK_FORMAT_MAX];
     } stats;
     # endif /* XKRT_SUPPORT_STATS */

}               xkrt_runtime_t;

/* submit a ready task */
void xkrt_runtime_submit_task(xkrt_runtime_t * runtime, task_t * task);

/* memory async thread management */
void xkrt_memory_copy_async_register_format(xkrt_runtime_t * runtime);

/* host capture task format */
void xkrt_task_host_capture_register_format(xkrt_runtime_t * runtime);

/* register v2 format */
void xkrt_memory_touch_async_register_format(xkrt_runtime_t * runtime);
void xkrt_memory_register_async_register_format(xkrt_runtime_t * runtime);
void xkrt_memory_unregister_async_register_format(xkrt_runtime_t * runtime);
void xkrt_memory_transfer_async_register_format(xkrt_runtime_t * runtime);

/* Main entry thread created per device */
void * xkrt_device_thread_main(xkrt_team_t * team, xkrt_thread_t * thread);

/* initialize drivers */
void xkrt_drivers_init(xkrt_runtime_t * runtime);

/* deinitialize drivers */
void xkrt_drivers_deinit(xkrt_runtime_t * runtime);

/* must be call once task accesses were all fetched */
void xkrt_device_task_execute(
    xkrt_runtime_t * runtime,
    xkrt_device_t * device,
    task_t * task
);

/* enqueue the task in the thread of the team */
void xkrt_team_thread_task_enqueue(xkrt_runtime_t * runtime, xkrt_team_t * team, xkrt_thread_t * thread, task_t * task);

/* enqueue the task to a random thread of the team */
void xkrt_team_task_enqueue(xkrt_runtime_t * runtime, xkrt_team_t * team, task_t * task);

/* submit a task to the given device */
void xkrt_device_task_submit(
    xkrt_runtime_t * runtime,
    xkrt_device_global_id_t device_global_id,
    task_t * task
);

/* report some stats about the runtime */
void xkrt_runtime_stats_report(xkrt_runtime_t * runtime);

/* arguments passed to the device team */
typedef struct  xkrt_device_thread_args_t
{
    xkrt_driver_type_t driver_type;
    uint8_t device_driver_id;
}               xkrt_device_thread_args_t;

typedef struct  xkrt_device_team_args_t
{
    xkrt_runtime_t * runtime;
    xkrt_device_thread_args_t devices[XKRT_DEVICES_MAX];
    int ndevices;
}               xkrt_device_team_args_t;

MemoryCoherencyController * task_get_memory_controller(
    xkrt_runtime_t * runtime,
    task_t * task,
    const access_t * access
);

#endif /* __XKRT_RUNTIME_H__ */
