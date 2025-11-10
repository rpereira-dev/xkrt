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

# include <xkrt/support.h>
# include <xkrt/types.h>
# include <xkrt/conf/conf.h>
# include <xkrt/distribution/distribution.h>
# include <xkrt/driver/driver.h>
# include <xkrt/thread/team.h>
# include <xkrt/thread/thread.h>
# include <xkrt/memory/access/coherency-controller.hpp>
# include <xkrt/memory/access/common/interval-set.hpp>
# include <xkrt/memory/register.h>
# include <xkrt/memory/routing/router-affinity.hpp>
# include <xkrt/stats/stats.h>
# include <xkrt/sync/spinlock.h>
# include <xkrt/task/task.hpp>

# include <hwloc.h>

# include <map>

XKRT_NAMESPACE_BEGIN

typedef enum    runtime_state_t : uint8_t
{
    XKRT_RUNTIME_DEINITIALIZED = 0,
    XKRT_RUNTIME_INITIALIZED,
}               runtime_state_t;

struct  runtime_t
{
    /* runtime state */
    std::atomic<runtime_state_t> state;

    /* driver list */
    drivers_t drivers;

    /* task formats */
    struct {
        task_formats_t list;
        task_format_id_t host_capture;
        task_format_id_t memory_copy_async;
        task_format_id_t memory_touch_async;
        task_format_id_t memory_register_async;
        task_format_id_t memory_unregister_async;
        task_format_id_t file_read_async;
        task_format_id_t file_write_async;
    } formats;

    /* user conf */
    conf_t conf;

    /* memory router */
    RouterAffinity router;

    # if XKRT_MEMORY_REGISTER_OVERFLOW_PROTECTION
    /* registered memory segments, map: addr -> size */
    std::map<uintptr_t, size_t> registered_memory;
    # endif /* XKRT_MEMORY_REGISTER_OVERFLOW_PROTECTION */

    # if XKRT_MEMORY_REGISTER_ASSISTED
    IntervalSet<uintptr_t> registered_pages;
    # endif /* XKRT_MEMORY_REGISTER_ASSISTED */

    /* hwloc topology, read only, initialized at init */
    hwloc_topology_t topology;

    //////////////////////////////////////////////////////////////////////////////////////////////
    // PUBLIC INTERFACES //
    //////////////////////////////////////////////////////////////////////////////////////////////

    ////////////////
    // Management //
    ////////////////

    /* initialize the runtime: load drivers and create task formats */
    int init(void);

    /* deinitialize the runtime */
    int deinit(void);

    /* deallocate all memory replicas and tasks */
    void reset(void);

    ////////////////////
    // DATA MOVEMENTS //
    ////////////////////

    /* Submit a copy command to a queue of the device */
    void copy(
        const device_global_id_t   device_global_id,
        const size_t               size,
        const device_global_id_t   dst_device_global_id,
        const uintptr_t            dst_device_addr,
        const device_global_id_t   src_device_global_id,
        const uintptr_t            src_device_addr,
        const callback_t         & callback
    );

    /* Submit a copy command to a queue of the device */
    void copy(
        const device_global_id_t      device_global_id,
        const memory_view_t         & host_view,
        const device_global_id_t      dst_device_global_id,
        const memory_replica_view_t & dst_device_view,
        const device_global_id_t      src_device_global_id,
        const memory_replica_view_t & src_device_view,
        const callback_t            & callback
    );

    /**
     * Spawn 'n' tasks so that each task i in [0..n-1]
     *      oi = i*size/n
     *      ai = ptr + oi
     *      bi = MIN(ai+size/n, size)
     *      si = bi - ai
     *
     *      - access    - virtual read on Interval(src.ai, src.bi)
     *      - routine   - submit a copy command
     */
    void memory_copy_async(
        const device_global_id_t   device_global_id,
        const size_t               size,
        const device_global_id_t   dst_device_global_id,
        const uintptr_t            dst_device_addr,
        const device_global_id_t   src_device_global_id,
        const uintptr_t            src_device_addr,
        int                        n
    );

    ///////////////////////
    // DATA DISTRIBUTION //
    ///////////////////////

    /* distribute the passed memory segment uniformly and continuously across all devices */
    void distribute_async(
        distribution_type_t type,
        void * ptr, size_t size,
        size_t nb,
        size_t h
    );

    /* distribute array of segment across all devices */
    void distribute_async(
        distribution_type_t type,
        void ** ptr,
        size_t chunk_size,
        unsigned int n
    );

    /* distribute matrix across all devices */
    void distribute_async(
        distribution_type_t type,
        matrix_storage_t storage,
        void * ptr, size_t ld,
        size_t m, size_t n,
        size_t mb, size_t nb,
        size_t sizeof_type,
        size_t hx, size_t hy
    );

    /////////////////////
    // I/O FILESYSTEM //
    /////////////////////

    /**
     * Spawn 'n' tasks so that each task i in [0..n-1]
     *      oi = i*size/n
     *      ai = ptr + oi
     *      bi = MIN(ai+size/n, size)
     *      si = bi - ai
     *
     *      - access    - write on Interval(ai, bi)
     *      - routine   - submit a file IO read/write command that fseek 'fd' to 'oi' and reads 'si' bytes to [ai..bi]
     */

    int file_read_async (int fd, void * buffer, const size_t size, int n);
    int file_write_async(int fd, void * buffer, const size_t size, int n);

    /** Iterate for each task of the segment */
    inline void foreach(
        const uintptr_t p,
        const size_t size,
        int n,
        const std::function<void(const int, const uintptr_t, const uintptr_t)> & func)
    {
        // compute number of commands to spawn
        if (size < (size_t) n)
            n = (int) size;

        // compute chunk size
        const size_t chunksize = size / n;
        assert(chunksize > 0);

        for (int i = 0; i < n; ++i) {
            const uintptr_t a = p + i * chunksize;
            const uintptr_t b = (i == n - 1) ? p + size : p + (i + 1) * chunksize;
            func(i, a, b);
        }
    }

    ///////////////////////
    // MEMORY ALLOCATION //
    ///////////////////////

    /* allocate the chunk0 on the device if not allocated already */
    void memory_device_preallocate_ensure(const device_global_id_t device_global_id, const int memory_id);

    /* allocate memory onto chunk0 of the given device memory index */
    area_chunk_t * memory_device_allocate_on(const device_global_id_t device_global_id, const size_t size, const int memory_id);

    /* allocate memory onto chunk0 of the given device */
    area_chunk_t * memory_device_allocate(const device_global_id_t device_global_id, const size_t size);

    /* deallocate the given chunk */
    void memory_device_deallocate(const device_global_id_t device_global_id, area_chunk_t * chunk);

    /* dealloacte all memory previously allocated on the device */
    void memory_device_deallocate_all(const device_global_id_t device_global_id);

    /* allocate memory onto the host using the driver given device */
    void * memory_host_allocate(const device_global_id_t device_global_id, const size_t size);

    /* deallocate memory onto the host using the driver of the given device */
    void memory_host_deallocate(const device_global_id_t device_global_id, void * mem, const size_t size);

    /* allocate unified memory using the driver of the given device */
    void * memory_unified_allocate(const device_global_id_t device_global_id, const size_t size);

    /* deallocate unified memory using the driver of the given device */
    void memory_unified_deallocate(const device_global_id_t device_global_id, void * mem, const size_t size);

    /////////////////////
    // MEMORY MOVEMENT //
    /////////////////////

    /* synchronous allocation of a device noncoherent replica */
    void memory_noncoherent_alloc(device_global_id_t device_global_id, void * ptr, size_t size);
    void memory_noncoherent_alloc(device_global_id_t device_global_id, matrix_storage_t storage, void * ptr, size_t ld, size_t m, size_t n, size_t sizeof_type);

    /* spawn empty tasks to make the replica coherent on the passed device */
    void memory_coherent_async(device_global_id_t device_global_id, void * ptr, size_t size);
    void memory_coherent_async(device_global_id_t device_global_id, matrix_storage_t storage, void * ptr, size_t ld, size_t m, size_t n, size_t sizeof_type);

    /* hint the unified memory system that the given memory will be used by the device */
    int memory_unified_advise  (const device_global_id_t device_global_id, const void * addr, const size_t size);
    /* tell the unified memory system to move the memory so it is coherent on the given device */
    int memory_unified_prefetch(const device_global_id_t device_global_id, const void * addr, const size_t size);

    /////////////////////////
    // MEMORY REGISTRATION //
    /////////////////////////

    /**
     *  Memory registration
     */
    int memory_register  (void * ptr, size_t size);
    int memory_unregister(void * ptr, size_t size);

    /**
     * Spawn an independent task to register the passed memory
     */
    int memory_register_async  (void * ptr, size_t size);
    int memory_unregister_async(void * ptr, size_t size);

    /**
     *  TODO: is it 'legal' to have concurrently
     *      - host thread A writing to memory [a..b]
     *      - host thread B pinning memory [a..b]
     * ? Can't find any doc on it, so for now, assuming it is not
     *
     *  Spawn 'n' tasks so that each task i in [0..n-1]
     *      ai = ptr + i*size/n
     *      bi = MIN(ai+size/n, size)
     *
     *      - access    - write on Interval(ai, bi)
     *                  - virtual commutative write on NULL
     *
     *      - routine - register/unregister/touch [ai..bi]
     */
    int memory_register_async  (team_t * team, void * ptr, const size_t size, int n);
    int memory_unregister_async(team_t * team, void * ptr, const size_t size, int n);
    int memory_touch_async     (team_t * team, void * ptr, const size_t size, int n);

    int memory_register_async(void * ptr, const size_t size, int n)
    {
        team_t * team = this->team_get(XKRT_DRIVER_TYPE_HOST, 0);
        assert(team);
        return this->memory_register_async(team, ptr, size, n);
    }

    int memory_unregister_async(void * ptr, const size_t size, int n)
    {
        team_t * team = this->team_get(XKRT_DRIVER_TYPE_HOST, 0);
        assert(team);
        return this->memory_register_async(team, ptr, size, n);
    }

    int memory_touch_async(void * ptr, const size_t size, int n)
    {
        team_t * team = this->team_get(XKRT_DRIVER_TYPE_HOST, 0);
        assert(team);
        return this->memory_touch_async(team, ptr, size, n);
    }

    /////////////////////
    // SYNCHRONIZATION //
    /////////////////////

    /////////////
    // TASKING //
    /////////////

    /* Commit a task - so it may be schedule from now once its dependences
     * completed. The task will be pushed to a device team */
    void task_commit(task_t * task);

    /* Decrease the ref counter of a detachable task, and complete it if it reaches 0 */
    void task_detachable_decr(task_t * task);

    /* Increase the ref counter of a detachable task */
    void task_detachable_incr(task_t * task);

    /* Complete a task */
    void task_complete(task_t * task);

    /* wait for children tasks of the current task to complete */
    void task_wait(void);

    /* enqueue a task to :
     *  - the current thread if its within a team
     *  - or the host driver team if the current thread has no team
     */
    void task_enqueue(task_t * task);
    static inline void
    task_enqueue(runtime_t * runtime, task_t * task)
    {
        runtime->task_enqueue(task);
    }

    /* enqueue a task to the given thread */
    void task_thread_enqueue(thread_t * thread, task_t * task);
    static inline void task_thread_enqueue(runtime_t * runtime, thread_t * thread, task_t * task)
    {
        runtime->task_thread_enqueue(thread, task);
    }

    /* enqueue a task to the given team */
    void task_team_enqueue(team_t * team, task_t * task);
    static inline void
    task_team_enqueue(runtime_t * runtime, team_t * team, task_t * task)
    {
        return runtime->task_team_enqueue(team, task);
    }

    /* duplicate a moldable task (do not use unless you know what you're doing) */
    task_t * task_dup(const task_t * task);

    /* instanciate a new task (do not use unless you know what you're doing,
     * you may want to use `task_spawn` instead) */
    inline task_t *
    task_instanciate(
        const std::function<void(task_t *, access_t *)> & set_accesses,
        const std::function<void(runtime_t *, device_t *, task_t *)> & f,
        const int naccesses
    ) {
        assert(naccesses > 0);
        assert(set_accesses);

        // retrieve tls
        thread_t * tls = thread_t::get_tls();
        assert(tls);

        // create the task
        constexpr task_flag_bitfield_t flags = TASK_FLAG_DEPENDENT;
        const     size_t task_size = task_compute_size(flags, naccesses);
        constexpr size_t args_size = sizeof(f);

        task_t * task = tls->allocate_task(task_size + args_size);
        new (task) task_t(this->formats.host_capture, flags);

        std::function<void(runtime_t *, device_t *, task_t *)> * fcpy = (std::function<void(runtime_t *, device_t *, task_t *)> *) TASK_ARGS(task, task_size);
        new (fcpy) std::function<void(runtime_t *, device_t *, task_t *)>(f);

        task_dep_info_t * dep = TASK_DEP_INFO(task);
        new (dep) task_dep_info_t(naccesses);

        access_t * accesses = TASK_ACCESSES(task, flags);
        set_accesses(task, accesses);
        tls->resolve(accesses, naccesses);

        # if XKRT_SUPPORT_DEBUG
        snprintf(task->label, sizeof(task->label), "capture-dynamic-access");
        # endif

        return task;
    }

    /* instanciate a new task (do not use unless you know what you're doing,
     * you may want to use `task_spawn` instead) */
    template <task_access_counter_t ac, bool has_set_accesses, bool has_split_condition>
    inline task_t *
    task_instanciate(
        const std::function<void(task_t *, access_t *)> & set_accesses,
        const std::function<bool(task_t *, access_t *)> & split_condition,
        const std::function<void(runtime_t *, device_t *, task_t *)> & f
    ) {
        static_assert(ac == 0 || has_set_accesses);     // must have both or none
        static_assert(!has_split_condition || ac > 0);  // cannot split if task has no accesses

        assert(has_set_accesses    == (set_accesses    != nullptr));
        assert(has_split_condition == (split_condition != nullptr));

        // retrieve tls
        thread_t * tls = thread_t::get_tls();
        assert(tls);

        // create the task
        constexpr task_flag_bitfield_t depflag = ac                  ? TASK_FLAG_DEPENDENT : TASK_FLAG_ZERO;
        constexpr task_flag_bitfield_t molflag = has_split_condition ? TASK_FLAG_MOLDABLE  : TASK_FLAG_ZERO;
        constexpr task_flag_bitfield_t   flags = depflag | molflag;
        constexpr size_t task_size = task_compute_size(flags, ac);
        constexpr size_t args_size = sizeof(f);

        task_t * task = tls->allocate_task(task_size + args_size);
        new (task) task_t(this->formats.host_capture, flags);

        std::function<void(runtime_t *, device_t *, task_t *)> * fcpy = (std::function<void(runtime_t *, device_t *, task_t *)> *) TASK_ARGS(task, task_size);
        new (fcpy) std::function<void(runtime_t *, device_t *, task_t *)>(f);

        if (depflag)
        {
            task_dep_info_t * dep = TASK_DEP_INFO(task);
            new (dep) task_dep_info_t(ac);

            access_t * accesses = TASK_ACCESSES(task, flags);
            set_accesses(task, accesses);
            tls->resolve(accesses, ac);
        }

        if (molflag)
        {
            task_mol_info_t * mol = TASK_MOL_INFO(task);
            new (mol) task_mol_info_t(split_condition, args_size);
        }

        # if XKRT_SUPPORT_DEBUG
        snprintf(task->label, sizeof(task->label), "capture");
        # endif

        return task;
    }

    /* spawn a task in the currently executing thread team */
    template <task_access_counter_t ac, bool has_set_accesses, bool has_split_condition>
    inline void
    task_spawn(
        const std::function<void(task_t *, access_t *)> & set_accesses,
        const std::function<bool(task_t *, access_t *)> & split_condition,
        const std::function<void(runtime_t *, device_t *, task_t *)> & f
    ) {
        // create the task
        task_t * task = this->task_instanciate<ac, has_set_accesses, has_split_condition>(set_accesses, split_condition, f);
        assert(task);

        // commit the task
        thread_t * tls = thread_t::get_tls();
        assert(tls);

        tls->commit(task, task_enqueue, this);
    }

    template <task_access_counter_t ac>
    inline void
    task_spawn(
        const std::function<void(task_t *, access_t *)> & set_accesses,
        const std::function<bool(task_t *, access_t *)> & split_condition,
        const std::function<void(runtime_t *, device_t *, task_t *)> & f
    ) {
        return this->task_spawn<ac, true, true>(set_accesses, split_condition, f);
    }

    template <task_access_counter_t ac>
    inline void
    task_spawn(
        const std::function<void(task_t *, access_t *)> & set_accesses,
        const std::function<void(runtime_t *, device_t *, task_t *)> & f
    ) {
        this->task_spawn<ac, true, false>(set_accesses, nullptr, f);
    }

    inline void
    task_spawn(
        const std::function<void(runtime_t *, device_t *, task_t *)> & f
    ) {
        this->task_spawn<0, false, false>(nullptr, nullptr, f);
    }

    /////////////////////////
    // THREADING - THREADS //
    /////////////////////////

    /* Retrieve the cpuset of the calling thread */
    static void thread_getaffinity(cpu_set_t & cpuset);

    /* Bind the calling thread to the given cpu set */
    static void thread_setaffinity(cpu_set_t & cpuset);

    ///////////////////////
    // THREADING - TEAMS //
    ///////////////////////

    /* create a new thread team */
    void team_create(team_t * team);

    /* wait until all threads called the barrier */
    template<bool worksteal = false>
    void team_barrier(team_t * team, thread_t * thread = NULL);

    /* wait until all threads finished and destroy the team */
    void team_join(team_t * team);

    /* start a critical section */
    void team_critical_begin(team_t * team);

    /* end a critical section */
    void team_critical_end(team_t * team);

    /* blocking parallel_for region */
    typedef std::function<void(team_t * team, thread_t * thread)> team_parallel_for_func_t;
    void team_parallel_for(team_t * team, team_parallel_for_func_t func);

    /////////////////////////
    // THREADING - TASKING //
    /////////////////////////

    inline void
    team_task_spawn(
        team_t * team,
        const std::function<void(task_t *, access_t *)> & set_accesses,
        const std::function<void(runtime_t *, device_t *, task_t *)> & f,
        const int naccesses
    ) {
        assert(naccesses > 0);

        // create the task
        task_t * task = task_instanciate(set_accesses, f, naccesses);
        assert(task);

        // commit the task
        thread_t * tls = thread_t::get_tls();
        tls->commit(task, task_team_enqueue, this, team);
    }

    template <task_access_counter_t ac, bool has_set_accesses, bool has_split_condition>
    inline void
    team_task_spawn(
        team_t * team,
        const std::function<void(task_t *, access_t *)> & set_accesses,
        const std::function<bool(task_t *, access_t *)> & split_condition,
        const std::function<void(runtime_t *, device_t *, task_t *)> & f
    ) {
        // create the task
        task_t * task = task_instanciate<ac, has_set_accesses, has_split_condition>(set_accesses, split_condition, f);
        assert(task);

        // commit the task
        thread_t * tls = thread_t::get_tls();
        tls->commit(task, task_team_enqueue, this, team);
    }

    template <task_access_counter_t ac>
    inline void
    team_task_spawn(
        team_t * team,
        const std::function<void(task_t *, access_t *)> & set_accesses,
        const std::function<bool(task_t *, access_t *)> & split_condition,
        const std::function<void(runtime_t *, device_t *, task_t *)> & f
    ) {
        return this->team_task_spawn<ac, true, true>(team, set_accesses, split_condition, f);
    }

    template <task_access_counter_t ac>
    inline void
    team_task_spawn(
        team_t * team,
        const std::function<void(task_t *, access_t *)> & set_accesses,
        const std::function<void(runtime_t *, device_t *, task_t *)> & f
    ) {
        this->team_task_spawn<ac, true, false>(team, set_accesses, nullptr, f);
    }

    inline void
    team_task_spawn(
        team_t * team,
        const std::function<void(runtime_t *, device_t *, task_t *)> & f
    ) {
        this->team_task_spawn<0, false, false>(team, nullptr, nullptr, f);
    }

    /**
     *  Schedule a task on the executing thread
     */
    task_t * worksteal(void);

    //////////////////
    // TEAM - UTILS //
    //////////////////

    /* retrieve the team of thread for the device of a specific driver */
    team_t * team_get(const driver_type_t type, const device_driver_id_t device_driver_id);

    /* retrieve the team of thread of the specific driver */
    team_t * team_get(const driver_type_t type);

    /* retrieve the first non-null driver' team from the passed bitfield */
    team_t * team_get_any(const driver_type_bitfield_t types);

    ////////////
    // ENERGY //
    ////////////

    /* start recording energy usage */
    void power_start(const device_global_id_t device_global_id, power_t * pwr);

    /* stop recording and return energy usage */
    void power_stop(const device_global_id_t device_global_id, power_t * pwr);

    ///////////////
    // UTILITIES //
    ///////////////

    /* get driver */
    driver_t * driver_get(const driver_type_t type);

    /* get device */
    device_t * device_get(const device_global_id_t device_global_id);

    /* get bitfield of devices for the given driver type */
    device_global_id_bitfield_t devices_get(const driver_type_t type);

    /* get number of commited devices */
    unsigned int get_ndevices(void);

    /* get maximum number of devices available */
    unsigned int get_ndevices_max(void);

    # if XKRT_SUPPORT_STATS

    ///////////
    // STATS //
    ///////////

    struct {
        struct {
            stats_int_t submitted;
            stats_int_t commited;
            stats_int_t completed;
        } tasks[TASK_FORMAT_MAX];

        struct {
            stats_int_t registered;
            stats_int_t unregistered;
            struct {
                struct {
                    stats_int_t device;
                    stats_int_t host;
                } advised;
                struct {
                    stats_int_t device;
                    stats_int_t host;
                } prefetched;
            } unified;
        } memory;
    } stats;

    /* report some stats about the runtime */
    void stats_report(void);

    # endif /* XKRT_SUPPORT_STATS */

}; /* runtime_t */

///////////////
// Utilities //
///////////////

/* submit a ready task */
void runtime_submit_task(runtime_t * runtime, task_t * task);

/* host capture task format */
void task_host_capture_register_format(runtime_t * runtime);

/* copy async format */
void memory_copy_async_register_format(runtime_t * runtime);

/* register v2 format */
void memory_register_async_register_format(runtime_t * runtime);

/* file async format */
void file_async_register_format(runtime_t * runtime);

/* Routine for threads in a device team */
void * device_thread_main(runtime_t * runtime, team_t * team, thread_t * thread);

/* initialize drivers */
void drivers_init(runtime_t * runtime);

/* deinitialize drivers */
void drivers_deinit(runtime_t * runtime);

/* must be call once task accesses were all fetched */
void task_execute(
    runtime_t * runtime,
    device_t * device,
    task_t * task
);

/* arguments passed to the device team */
typedef struct  device_team_args_t
{
    driver_t * driver;
    device_global_id_t device_global_id;
    device_driver_id_t device_driver_id;
    pthread_barrier_t barrier;
}               device_team_args_t;

MemoryCoherencyController * task_get_memory_controller(
    runtime_t * runtime,
    task_t * task,
    const access_t * access
);

void * team_parallel_for_main(runtime_t * runtime, team_t * team, thread_t * thread);
# define XKRT_TEAM_ROUTINE_PARALLEL_FOR ((team_routine_t) team_parallel_for_main)

XKRT_NAMESPACE_END

#endif /* __XKRT_RUNTIME_H__ */
