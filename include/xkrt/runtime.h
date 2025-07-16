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
# include <xkrt/distribution/distribution.h>
# include <xkrt/driver/driver.h>
# include <xkrt/thread/thread.h>
# include <xkrt/memory/access/coherency-controller.hpp>
# include <xkrt/memory/register.h>
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
        task_format_id_t memory_register_async;
        task_format_id_t memory_unregister_async;
        task_format_id_t file_read_async;
        task_format_id_t file_write_async;
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
    // PUBLIC INTERFACES //
    //////////////////////////////////////////////////////////////////////////////////////////////

    /* deallocate all memory replicates and tasks */
    void reset(void);

    ////////////////////
    // DATA MOVEMENTS //
    ////////////////////

    /* Submit a copy instruction to a stream of the device */
    xkrt_stream_t * copy(
        const xkrt_device_global_id_t   device_global_id,
        const size_t                    size,
        const xkrt_device_global_id_t   dst_device_global_id,
        const uintptr_t                 dst_device_addr,
        const xkrt_device_global_id_t   src_device_global_id,
        const uintptr_t                 src_device_addr,
        const xkrt_callback_t         & callback
    );

    /* Submit a copy instruction to a stream of the device */
    xkrt_stream_t * copy(
        const xkrt_device_global_id_t   device_global_id,
        const memory_view_t           & host_view,
        const xkrt_device_global_id_t   dst_device_global_id,
        const memory_replicate_view_t & dst_device_view,
        const xkrt_device_global_id_t   src_device_global_id,
        const memory_replicate_view_t & src_device_view,
        const xkrt_callback_t         & callback
    );

    /* synchronous allocation of a device incoherent replicate */
    void incoherent_allocate(xkrt_device_global_id_t device_global_id, void * ptr, size_t size);
    void incoherent_allocate(xkrt_device_global_id_t device_global_id, matrix_storage_t storage, void * ptr, size_t ld, size_t m, size_t n, size_t sizeof_type);

    /* spawn tasks to make the host replicate coherent */
    void coherent_async(void * ptr, size_t size);
    void coherent_async(matrix_storage_t storage, void * ptr, size_t ld, size_t m, size_t n, size_t sizeof_type);

    /* distribute memory segment across all devices */
    void distribute_async(
        xkrt_distribution_type_t type,
        void * ptr, size_t size,
        size_t nb,
        size_t h
    );

    /* distribute array of segment across all devices */
    void distribute_async(
        xkrt_distribution_type_t type,
        void ** ptr, size_t chunk_size,
        unsigned int n
    );

    /* distribute matrix across all devices */
    void distribute_async(
        xkrt_distribution_type_t type,
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
     *  Create a task that reads 'n' bytes from the file descriptor 'fd' ,
     *  and write to the 'buffer' memory.
     *
     *  Task' access is `write: Interval(buffer, n)`
     *
     *  Dependencies may be released early if nchunks > 1
     *  Example:
     *      if nchunks == 1, then dependencies are released once 'n' bytes got read
     *      if nchunks == 2, then dependencies are released twice, on
     *          - Interval(buffer      , n/2) - once it has been read
     *          - Interval(buffer + n/2, n/2) - once it has been read
     */
    int file_read_async(int fd, void * buffer, size_t n, unsigned int nchunks);
    int file_write_async(int fd, void * buffer, size_t n, unsigned int nchunks);

    inline void file_foreach_chunk(
        void * buffer,
        const size_t total_size,
        size_t nchunks,
        const std::function<void(uintptr_t, uintptr_t)> & func)
    {
        // compute number of instructions to spawn
        if (total_size < nchunks)
            nchunks = (unsigned int) total_size;

        // compute chunk size
        const size_t chunksize = total_size / nchunks;
        assert(chunksize > 0);

        for (std::size_t i = 0; i < nchunks; ++i) {
            const uintptr_t a = reinterpret_cast<uintptr_t>(static_cast<char*>(buffer) + i * chunksize);
            const uintptr_t b = reinterpret_cast<uintptr_t>(
                (i == nchunks - 1)
                    ? static_cast<char*>(buffer) + total_size
                    : static_cast<char*>(buffer) + (i + 1) * chunksize);
            func(a, b);
        }
    }

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
     *  Create 'n' tasks so that each task i in [0..n-1]
     *      - access - commutative write on ptr + i*size/n
     *      - routine - register/unregister/touch [ptr + i*size/n,  MIN(ptr + (i+1)*size/n, ptr+size)]
     *
     *  Note: each task may run several 'cuMemRegister' several time on a single chunk
     */
    int memory_register_async(xkrt_team_t * team, void * ptr, const size_t size, int n);
    int memory_unregister_async(xkrt_team_t * team, void * ptr, const size_t size, int n);
    int memory_touch_async(xkrt_team_t * team, void * ptr, const size_t size, int n);

    /////////////////////
    // SYNCHRONIZATION //
    /////////////////////

    /////////////
    // TASKING //
    /////////////

    /* Commit a task - so it may be schedule from now once its dependences
     * completed. The task will be pushed to a device team */
    void task_commit(task_t * task);

    /* Notify once a detachable task */
    void task_detachable_post(task_t * task);

    /* Complete a task */
    void task_complete(task_t * task);

    /* wait for children tasks of the current task to complete */
    void task_wait(void);

    /* schedule a ready task, and return 1 if one task was found, 0 otherwise */
    int task_schedule(void);

    /* enqueue a task to the given thread */
    void task_thread_enqueue(xkrt_thread_t * thread, task_t * task);
    static inline void
    task_thread_enqueue(xkrt_runtime_t * runtime, xkrt_thread_t * thread, task_t * task)
    {
        runtime->task_thread_enqueue(thread, task);
    }

    /* enqueue a task to the given team */
    void task_team_enqueue(xkrt_team_t * team, task_t * task);
    static inline void
    task_team_enqueue(xkrt_runtime_t * runtime, xkrt_team_t * team, task_t * task)
    {
        return runtime->task_team_enqueue(team, task);
    }

    /* duplicate a moldable task (do not use unless you know what you're doing) */
    task_t * task_dup(const task_t * task);

    /* instanciate a new task (do not use unless you know what you're doing,
     * you may want to use `task_spawn` instead) */
    template <task_access_counter_t ac, bool has_set_accesses, bool has_split_condition>
    inline task_t *
    task_instanciate(
        const std::function<void(task_t *, access_t *)> & set_accesses,
        const std::function<bool(task_t *, access_t *)> & split_condition,
        const std::function<void(task_t *)> & f
    ) {
        static_assert(ac == 0 || has_set_accesses);     // must have both or none
        static_assert(!has_split_condition || ac > 0);  // cannot split if task has no accesses

        assert(has_set_accesses    == (set_accesses    != nullptr));
        assert(has_split_condition == (split_condition != nullptr));

        // retrieve tls
        xkrt_thread_t * tls = xkrt_thread_t::get_tls();

        // create the task
        constexpr task_flag_bitfield_t flags = (ac == 0) ? TASK_FLAG_ZERO : (has_split_condition) ? (TASK_FLAG_DEPENDENT | TASK_FLAG_MOLDABLE) : TASK_FLAG_DEPENDENT;
        constexpr size_t task_size = task_compute_size(flags, ac);
        constexpr size_t args_size = sizeof(f);

        task_t * task = tls->allocate_task(task_size + args_size);
        new (task) task_t(this->formats.host_capture, flags);

        std::function<void(task_t *)> * fcpy = (std::function<void(task_t *)> *) TASK_ARGS(task, task_size);
        new (fcpy) std::function<void(task_t *)>(f);

        if (ac)
        {
            task_dep_info_t * dep = TASK_DEP_INFO(task);
            new (dep) task_dep_info_t(ac);

            access_t * accesses = TASK_ACCESSES(task, flags);
            set_accesses(task, accesses);
            tls->resolve<ac>(task, accesses);
        }

        if (split_condition)
        {
            task_mol_info_t * mol = TASK_MOL_INFO(task);
            new (mol) task_mol_info_t(split_condition, args_size);
        }

        # ifndef NDEBUG
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
        const std::function<void(task_t *)> & f
    ) {
        // create the task
        task_t * task = this->task_instanciate<ac, has_set_accesses, has_split_condition>(set_accesses, split_condition, f);
        assert(task);

        // commit the task
        xkrt_thread_t * tls = xkrt_thread_t::get_tls();
        tls->commit(task, task_thread_enqueue, this, tls);
    }

    template <task_access_counter_t ac>
    inline void
    task_spawn(
        const std::function<void(task_t *, access_t *)> & set_accesses,
        const std::function<bool(task_t *, access_t *)> & split_condition,
        const std::function<void(task_t *)> & f
    ) {
        return this->task_spawn<ac, true, true>(set_accesses, split_condition, f);
    }

    template <task_access_counter_t ac>
    inline void
    task_spawn(
        const std::function<void(task_t *, access_t *)> & set_accesses,
        const std::function<void(task_t *)> & f
    ) {
        this->task_spawn<ac, true, false>(set_accesses, nullptr, f);
    }

    inline void
    task_spawn(
        const std::function<void(task_t *)> & f
    ) {
        this->task_spawn<0, false, false>(nullptr, nullptr, f);
    }

    /* run a task on the given team, using its host routine
     * (do not use unless you know what you are doing, you may want
     * `task_spawn` instead) */
    void task_run(xkrt_team_t * team, xkrt_thread_t * thread, task_t * task);

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

    template <task_access_counter_t ac, bool has_set_accesses, bool has_split_condition>
    inline void
    team_task_spawn(
        xkrt_team_t * team,
        const std::function<void(task_t *, access_t *)> & set_accesses,
        const std::function<bool(task_t *, access_t *)> & split_condition,
        const std::function<void(task_t *)> & f
    ) {
        // create the task
        task_t * task = task_instanciate<ac, has_set_accesses, has_split_condition>(set_accesses, split_condition, f);
        assert(task);

        // commit the task
        xkrt_thread_t * tls = xkrt_thread_t::get_tls();
        tls->commit(task, task_team_enqueue, this, team);
    }

    template <task_access_counter_t ac>
    inline void
    team_task_spawn(
        xkrt_team_t * team,
        const std::function<void(task_t *, access_t *)> & set_accesses,
        const std::function<bool(task_t *, access_t *)> & split_condition,
        const std::function<void(task_t *)> & f
    ) {
        return this->team_task_spawn<ac, true, true>(team, set_accesses, split_condition, f);
    }

    template <task_access_counter_t ac>
    inline void
    team_task_spawn(
        xkrt_team_t * team,
        const std::function<void(task_t *, access_t *)> & set_accesses,
        const std::function<void(task_t *)> & f
    ) {
        this->team_task_spawn<ac, true, false>(team, set_accesses, nullptr, f);
    }

    inline void
    team_task_spawn(
        xkrt_team_t * team,
        const std::function<void(task_t *)> & f
    ) {
        this->team_task_spawn<0, false, false>(team, nullptr, nullptr, f);
    }

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

     # if XKRT_SUPPORT_STATS
     struct {
         struct {
             stats_int_t submitted;
             stats_int_t commited;
             stats_int_t completed;
         } tasks[TASK_FORMAT_MAX];

         struct {
             stats_int_t registered;
             stats_int_t unregistered;
         } memory;
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
void xkrt_memory_async_register_format(xkrt_runtime_t * runtime);

/* file async format */
void xkrt_file_async_register_format(xkrt_runtime_t * runtime);

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
