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

# include <xkrt/thread/thread.h>
# include <xkrt/runtime.h>
# include <xkrt/logger/logger-hwloc.h>

# include <cassert>
# include <cstring>
# include <cerrno>

# include <stdatomic.h>
# include <hwloc.h>
# include <sched.h>
# include <hwloc/glibc-sched.h>

XKRT_NAMESPACE_BEGIN

# pragma message(TODO "Threading layer had been implemented in half a day with naive algorithm. If perf is an issue, reimplement with well-known hierarchical algorithm")

thread_local thread_t * __TLS = NULL;

void
thread_t::push_tls(thread_t * thread)
{
    thread->prev = __TLS;
    __TLS = thread;
}

void
thread_t::pop_tls(void)
{
    assert(__TLS);
    __TLS = __TLS->prev;
}

thread_t *
thread_t::get_tls(void)
{
    if (__TLS == NULL)
    {
        team_t * team = NULL;
        int tid = 0;
        device_global_id_t device_global_id = HOST_DEVICE_GLOBAL_ID;
        thread_place_t place;
        runtime_t::thread_getaffinity(place);
        thread_t * thread = new thread_t(team, tid, pthread_self(), device_global_id, place);
        assert(thread);
        thread_t::push_tls(thread);
    }
    assert(__TLS);
    return __TLS;
}

void
thread_t::warmup(void)
{
    // touches every pages to avoid minor page faults later during the execution
    size_t pagesize = (size_t) getpagesize();
    for (uint8_t * ptr = this->memory_stack_ptr ;
            ptr < this->memory_stack_bottom + THREAD_MAX_MEMORY ;
            ptr += pagesize)
        *ptr = 0;
}

task_t *
thread_t::allocate_task(const size_t size)
{
    assert(thread_t::get_tls() == this);

    # if 1
    if (this->memory_stack_ptr >= this->memory_stack_bottom + THREAD_MAX_MEMORY)
        LOGGER_FATAL("Stack overflow ! Increase `THREAD_MAX_MEMORY` and recompile");
    task_t * task = (task_t *) this->memory_stack_ptr;
    this->memory_stack_ptr += size;

    # if XKRT_SUPPORT_DEBUG
    this->tasks.push_back(task);
    # endif /* XKRT_SUPPORT_DEBUG */

    return task;
    # else
    return (uint8_t *) malloc(size);
    # endif
}

void
thread_t::deallocate_all_tasks(void)
{
    this->memory_stack_ptr = this->memory_stack_bottom;
}

/////////////////////////////////////////////////////

void
runtime_t::thread_setaffinity(cpu_set_t & cpuset)
{
    pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
    for (int ii = 0; ii < 10; ++ii) sched_yield();
}

void
runtime_t::thread_getaffinity(cpu_set_t & cpuset)
{
    pthread_getaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
}

static inline int
team_barrier_fetch(team_t * team, int delta)
{
    int n = team->priv.barrier.n.fetch_sub(delta, std::memory_order_relaxed) - delta;
    assert(n >= 0);
    if (n == 0)
    {
        team->priv.barrier.n.store(team->priv.nthreads, std::memory_order_seq_cst);
        team->priv.barrier.version += 1;
        pthread_mutex_lock(&team->priv.barrier.mtx);
        {
            pthread_cond_broadcast(&team->priv.barrier.cond);
        }
        pthread_mutex_unlock(&team->priv.barrier.mtx);
    }
    return n;
}

typedef struct  team_recursive_args_t
{
    runtime_t * runtime;
    team_t * team;
    pthread_t pthread;
    device_global_id_t device_global_id;
    thread_place_t place;
    int from;
    int to;
}               team_recursive_args_t;

/* get the number of threads by default for that team description */
static int
team_create_get_nthreads_auto(
    runtime_t * runtime,
    team_t * team
) {
    (void) team;
    int depth = hwloc_get_type_depth(runtime->topology, HWLOC_OBJ_CORE);
    int r = hwloc_get_nbobjs_by_depth(runtime->topology, depth);
    return r;
}

/** Set the cpuset for the given thread */
static void inline
team_create_get_place(
    runtime_t * runtime,
    team_t * team,
    int tid,
    device_global_id_t * device_global_id,
    thread_place_t * place
) {
    switch (team->desc.binding.mode)
    {
        case (XKRT_TEAM_BINDING_MODE_COMPACT):
        {
            switch (team->desc.binding.places)
            {
                case (XKRT_TEAM_BINDING_PLACES_DEVICE):
                {
                    *device_global_id = (team->desc.binding.flags == XKRT_TEAM_BINDING_FLAG_EXCLUDE_HOST) ? (device_global_id_t) (tid + 1) : (device_global_id_t) tid;
                    const device_t * device = runtime->device_get(*device_global_id);
                    assert(device);
                    *place = device->threads[0]->place;
                    return ;
                }

                case (XKRT_TEAM_BINDING_PLACES_HYPERTHREAD):
                case (XKRT_TEAM_BINDING_PLACES_CORE):
                case (XKRT_TEAM_BINDING_PLACES_L1):
                case (XKRT_TEAM_BINDING_PLACES_L2):
                case (XKRT_TEAM_BINDING_PLACES_L3):
                case (XKRT_TEAM_BINDING_PLACES_NUMA):
                case (XKRT_TEAM_BINDING_PLACES_SOCKET):
                case (XKRT_TEAM_BINDING_PLACES_MACHINE):
                {
                    hwloc_obj_type_t type =
                        (team->desc.binding.places == XKRT_TEAM_BINDING_PLACES_HYPERTHREAD) ? HWLOC_OBJ_PU          :
                        (team->desc.binding.places == XKRT_TEAM_BINDING_PLACES_CORE)        ? HWLOC_OBJ_CORE        :
                        (team->desc.binding.places == XKRT_TEAM_BINDING_PLACES_L1)          ? HWLOC_OBJ_L1CACHE     :
                        (team->desc.binding.places == XKRT_TEAM_BINDING_PLACES_L2)          ? HWLOC_OBJ_L2CACHE     :
                        (team->desc.binding.places == XKRT_TEAM_BINDING_PLACES_L3)          ? HWLOC_OBJ_L3CACHE     :
                        (team->desc.binding.places == XKRT_TEAM_BINDING_PLACES_NUMA)        ? HWLOC_OBJ_NUMANODE    :
                        (team->desc.binding.places == XKRT_TEAM_BINDING_PLACES_MACHINE)     ? HWLOC_OBJ_MACHINE     :
                        HWLOC_OBJ_CORE;
                    ;

                    // this is a host thread
                    *device_global_id = HOST_DEVICE_GLOBAL_ID;

                    // get linux cpuset
                    int logical_index = tid;
                    hwloc_obj_t obj = hwloc_get_obj_by_type(runtime->topology, type, logical_index);
                    HWLOC_SAFE_CALL(hwloc_cpuset_to_glibc_sched_affinity(runtime->topology, obj->cpuset, place, sizeof(cpu_set_t)));

                    return ;
                }

                case (XKRT_TEAM_BINDING_PLACES_EXPLICIT):
                {
                    *place = team->desc.binding.places_list[tid % team->desc.binding.nplaces];
                    return ;
                }

                default:
                    LOGGER_FATAL("Team config. not supported");
            }
        }

        default:
            LOGGER_FATAL("Team config. not supported");
    }
}

void team_create_recursive_fork(runtime_t * runtime, team_t * team, int from, int to);

static void *
team_create_recursive(void * vargs)
{
    team_recursive_args_t * args = (team_recursive_args_t *) vargs;

    // recursion end
    if (args->from == args->to)
    {
        // init tls
        team_t * team = args->team;
        int tid = args->from;
        thread_t * thread = team->priv.threads + tid;
        new (thread) thread_t(team, tid, args->pthread, args->device_global_id, args->place);

        // save tls
        thread_t::push_tls(thread);

        // launch routine
        thread->state = XKRT_THREAD_INITIALIZED;

        // warmup thread if conf says so
        if (args->runtime->conf.warmup)
            thread->warmup();

        // starts
        void * r = args->team->desc.routine(team, thread);

        // if master thread
        if (team->desc.master_is_member && tid == 0)
        {
            // args are allocated on the stack, no need to free
            // but we have to reset previous TLS
            thread_t::pop_tls();

            // reset cpu set
        }
        else
        {
            // free heap-allocated args
            free(args);
        }

        return r;
    }

    const int from1 = args->from;
    const int to1   = args->from + (args->to - args->from) / 2;
    const int from2 = to1 + 1;
    const int to2   = args->to;

    if (from2 <= to2)
        team_create_recursive_fork(args->runtime, args->team, from2, to2);

    args->from = from1;
    args->to   = to1;
    team_create_recursive(args);

    return NULL;
}

void
team_create_recursive_fork(
    runtime_t * runtime,
    team_t * team,
    int from,
    int to
) {
    assert(to >= from);
    const int tid = from;

    // save calling thread cpu set
    cpu_set_t save_set;
    runtime_t::thread_getaffinity(save_set);

    // retrieve cpuset and the global device id
    device_global_id_t device_global_id;
    thread_place_t place;
    team_create_get_place(runtime, team, tid, &device_global_id, &place);

    // move thread before allocating future thread-private memory
    runtime_t::thread_setaffinity(place);

    team_recursive_args_t * args = (team_recursive_args_t *) malloc(sizeof(team_recursive_args_t));
    args->runtime = runtime;
    args->team = team;
    args->from = from;
    args->device_global_id = device_global_id;
    args->to = to;
    args->place = place;

    // fork
    int r = pthread_create(&args->pthread, NULL, team_create_recursive, args);
    assert(r == 0);

    // restore calling thread cpu set
    runtime_t::thread_setaffinity(save_set);
}

void
runtime_t::team_create(team_t * team)
{
    // only supported modes currently
    assert(
        (team->desc.binding.mode == XKRT_TEAM_BINDING_MODE_COMPACT && team->desc.binding.places == XKRT_TEAM_BINDING_PLACES_DEVICE   && team->desc.binding.flags == XKRT_TEAM_BINDING_FLAG_NONE)                                                                    ||
        (team->desc.binding.mode == XKRT_TEAM_BINDING_MODE_COMPACT && team->desc.binding.places == XKRT_TEAM_BINDING_PLACES_DEVICE   && team->desc.binding.flags == XKRT_TEAM_BINDING_FLAG_EXCLUDE_HOST)                                                            ||
        (team->desc.binding.mode == XKRT_TEAM_BINDING_MODE_COMPACT && team->desc.binding.flags == XKRT_TEAM_BINDING_FLAG_NONE)                                                                    ||
        (team->desc.binding.mode == XKRT_TEAM_BINDING_MODE_COMPACT && team->desc.binding.places == XKRT_TEAM_BINDING_PLACES_EXPLICIT && team->desc.binding.flags == XKRT_TEAM_BINDING_FLAG_NONE && team->desc.binding.places_list && team->desc.binding.nplaces)
    );

    // set all to zero
    memset((void *) (&team->priv), 0, sizeof(team->priv));

    // init parallel for
    team->priv.parallel_for.index = 0;

    // allocate thread array
    const int nthreads = (team->desc.nthreads == 0) ? team_create_get_nthreads_auto(this, team) : team->desc.nthreads;
    assert(nthreads >= 0);

    // init priv data
    team->priv.nthreads = nthreads;
    team->priv.threads = (thread_t *) calloc(team->priv.nthreads, sizeof(thread_t));
    assert(team->priv.threads);

    // init barrier
    pthread_mutex_init(&team->priv.barrier.mtx, NULL);
    pthread_cond_init(&team->priv.barrier.cond, NULL);
    if (!team->desc.master_is_member)
    {
        // if master thread is not member of the team,
        // init with nthreads + 1 to avoid early barrier release
        team->priv.barrier.n.store(team->priv.nthreads + 1, std::memory_order_seq_cst);
    }
    else
    {
        team->priv.barrier.n.store(team->priv.nthreads + 0, std::memory_order_seq_cst);
    }

    // fork threads
    if (team->priv.nthreads)
    {
        if (!team->desc.master_is_member)
        {
            team_create_recursive_fork(this, team, 0, team->priv.nthreads - 1);
        }
        // if master is member, start recursive fork
        else
        {
            // retrieve cpuset and the global device id
            constexpr int tid = 0;
            device_global_id_t device_global_id;
            thread_place_t place;
            team_create_get_place(this, team, tid, &device_global_id, &place);
            team_recursive_args_t args = {
                .runtime = this,
                .team = team,
                .pthread = pthread_self(),
                .device_global_id = device_global_id,
                .place = place,
                .from = 0,
                .to = team->priv.nthreads - 1
            };

            # pragma message(TODO "What is the expected behavior of openmp parallel regions ? should the master thread reset its affinity after each parallel region ? Currently, xkaapi does not")

            // move thread before running
            runtime_t::thread_setaffinity(place);

            // recursively spawn other threads and run the routine
            team_create_recursive(&args);
        }
    }

    // if master thread is not member of the team, the barrier may now be released
    if (!team->desc.master_is_member)
        team_barrier_fetch(team, 1);
}

/* Return the 'i-th' victim to steal for the thread 'tid' when there is 'n' threads in the tree */
static inline int
get_ith_victim(int tid, int i, int n)
{
    // assume threads are bound 1:1 compactly onto physical cores
    // for instance, if we have n = 4 threads, we have
    //      F : (tid, i, n) -> victim
    // defined as
    //      F(0, 0, 4) = 0  - but whatever, threads dont steal themselves
    //      F(0, 1, 4) = 1
    //      F(0, 2, 4) = 2
    //      F(0, 3, 4) = 3
    // and
    //      F(1, 0, 4) = 1  - but whatever, threads dont steal themselves
    //      F(1, 1, 4) = 0
    //      F(1, 2, 4) = 3
    //      F(1, 3, 4) = 2
    // and
    //      F(2, 0, 4) = 2
    //      F(2, 1, 4) = 3
    //      F(2, 2, 4) = 0
    //      F(2, 3, 4) = 1

    return (i + tid) % n;
}

static inline int
worksteal(
    runtime_t * runtime,
    team_t * team,
    thread_t * thread
) {
    task_t * task = NULL;

    // if the thread is executing within a team, do hierarchical workstealing
    if (team)
    {
        const int n = team->priv.nthreads;
        const int tid = thread->tid;

        for (int i = 0 ; i < n ; ++i)
        {
            const int victim_tid = get_ith_victim(tid, i, n);
            thread_t * victim = team->priv.threads + victim_tid;
            if (victim->state != XKRT_THREAD_INITIALIZED)
                continue ;

            task = (victim_tid == tid) ? victim->deque.pop() : victim->deque.steal();
            if (task)
                break ;
        }
    }
    // else, schedule that thread tasks only
    else
    {
        task = thread->deque.pop();
    }

    if (task)
    {
        task_execute(runtime, NULL, task);
        return 1;
    }

    return 0;
}

void
runtime_t::task_wait(void)
{
    thread_t * thread = thread_t::get_tls();
    assert(thread);

    # define WAIT do { if (thread->current_task->cc.load(std::memory_order_relaxed) == 0) return ; } while (0)

    /* active polling */
    # if 0

    while (1)
    {
        if (tls->team && schedule(this, tls->team, tls->thread))
            continue ;
        WAIT ;
    }

    # else

    /* exponential backoff sleep */
    # define WAIT2   do { WAIT   ; WAIT   ; } while (0)
    # define WAIT4   do { WAIT2  ; WAIT2  ; } while (0)
    # define WAIT8   do { WAIT4  ; WAIT4  ; } while (0)
    # define WAIT16  do { WAIT8  ; WAIT8  ; } while (0)
    # define WAIT32  do { WAIT16 ; WAIT16 ; } while (0)
    # define WAIT64  do { WAIT32 ; WAIT32 ; } while (0)

    // Poll first for fast way out
    mem_barrier();
    WAIT32 ;

    // Else, work steal and sleep with backoff
    constexpr int initial_backoff = 1024;;  // Initial backoff time in nanoseconds
    constexpr int max_backoff = 64 * 1024;  // Maximum backoff time nanoseconds
    int backoff = initial_backoff;          // Initial backoff time in nanoseconds
    assert(max_backoff < 1000000);          // nanosleep condition

    struct timespec ts = { .tv_sec = 0, .tv_nsec = 0 };
    while (1)
    {
        // work steal
        if (worksteal(this, thread->team, thread))
        {
            backoff = initial_backoff;
            continue ;
        }

        // sleep with backoff
        ts.tv_nsec = backoff;
        nanosleep(&ts, NULL);
        if (backoff < max_backoff)
            backoff = (backoff << 1);
        WAIT64 ;

        // TODO : block in a pthread_cond after some threshold
    }

    # undef WAIT

    # endif
}

// TODO : reimplement this using team's topology
template<bool ws>
void
runtime_t::team_barrier(
    team_t * team,
    thread_t * thread
) {
    this->task_wait();  // TODO: i think we should remove this

    if (team->priv.nthreads == 1)
        return ;

    assert((ws && thread) || (!ws && !thread));

    int old_version = team->priv.barrier.version;
    if (team_barrier_fetch(team, 1))
    {
        while (old_version == team->priv.barrier.version)
        {
            if (ws && worksteal(this, team, thread))
                continue ;

            pthread_mutex_lock(&team->priv.barrier.mtx);
            {
                if (old_version == team->priv.barrier.version)
                {
                    pthread_cond_wait(&team->priv.barrier.cond, &team->priv.barrier.mtx);
                }
            }
            pthread_mutex_unlock(&team->priv.barrier.mtx);
        }
    }
}

template void runtime_t::team_barrier<true>(team_t * team, thread_t * thread);
template void runtime_t::team_barrier<false>(team_t * team, thread_t * thread);

static inline void
team_parallel_for_run_f(
    team_t * team,
    thread_t * thread,
    team_parallel_for_func_t f
) {
    ++thread->parallel_for.index;
    f(team, thread);

    // last thread to complete wakes up the master
    if (team->priv.parallel_for.pending.fetch_sub(1, std::memory_order_seq_cst) - 1 == 0)
    {
        // wakeup the master thread
        team->priv.parallel_for.completed = 1;
        syscall(
                SYS_futex,
                &team->priv.parallel_for.completed, // uint32_t *uaddr
                FUTEX_WAKE,                         // int futex_op
                INT_MAX,                            // uint32_t val
                NULL,                               // const struct timespec *timeout | uint32_t val2
                NULL,                               // uint32_t *uaddr2
                NULL                                // uint32_t val3
        );
    }
}

void
runtime_t::team_parallel_for(
    team_t * team,
    team_parallel_for_func_t f
) {
    // register the function to run on each thread
    uint32_t index = team->priv.parallel_for.index;
    team->priv.parallel_for.f[index % XKRT_TEAM_PARALLEL_FOR_MAX_FUNC] = f;

    // the master thread must wait until all threads completed
    team->priv.parallel_for.completed = 0;
    team->priv.parallel_for.pending.store(team->priv.nthreads, std::memory_order_seq_cst);

    // wake up threads
    mem_barrier();
    ++team->priv.parallel_for.index;

    // TODO : within the kernel, this lead to a O(n) with 'n' the number of thread sleeping.
    // This may be an issue if people wanna to large team of threads.
    // Instead, use something hierarchical, with several futexes on small
    // groups of thread to reduce syscall overhead
    syscall(
        SYS_futex,
        &team->priv.parallel_for.index,     // uint32_t *uaddr
        FUTEX_WAKE,                         // int futex_op
        INT_MAX,                            // uint32_t val
        NULL,                               // const struct timespec *timeout | uint32_t val2
        NULL,                               // uint32_t *uaddr2
        NULL                                // uint32_t val3
    );

    if (f)
    {
        // if master is member, run the routine
        if (team->desc.master_is_member)
        {
            // retrieve team's tls of the master
            constexpr int tid = 0;
            thread_t * tls = team->priv.threads + tid;
            assert(tls);

            // set the TLS
            thread_t::push_tls(tls);

            // run
            team_parallel_for_run_f(team, tls, f);

            // pop the TLS
            thread_t::pop_tls();
        }
        // else busy wait a bit before sleeping
        else
        {
            for (int i = 0 ; i < 16 ; ++i)
            {
                if ((volatile uint32_t) team->priv.parallel_for.completed == 1)
                    return ;
                mem_pause();
            }
        }

        // wait until all executed
        while ((volatile uint32_t) team->priv.parallel_for.completed == 0)
        {
            syscall(
                SYS_futex,
                &team->priv.parallel_for.completed, // uint32_t *uaddr
                FUTEX_WAIT,                         // int futex_op
                0,                                  // uint32_t val
                NULL,                               // const struct timespec *timeout | uint32_t val2
                NULL,                               // uint32_t *uaddr2
                NULL                                // uint32_t val3
            );
        }
    }
}

void *
team_parallel_for_main(
    team_t * team,
    thread_t * thread
) {
    if (team->desc.master_is_member && thread->tid == 0)
        return NULL;

    while (1)
    {
        // keep executing functions until all got executed
        while (thread->parallel_for.index < (volatile uint32_t) team->priv.parallel_for.index)
        {
parallel_for_run:
            team_parallel_for_func_t f = team->priv.parallel_for.f[thread->parallel_for.index % XKRT_TEAM_PARALLEL_FOR_MAX_FUNC];
            if (f == nullptr)
                return NULL;
            team_parallel_for_run_f(team, thread, f);
        }

        // some polling before sleeping
        for (int i = 0 ; i < 16 ; ++i)
        {
            if (thread->parallel_for.index < (volatile uint32_t) team->priv.parallel_for.index)
                goto parallel_for_run;
            mem_pause();
        }

        // keep sleeping until there is functions to execute, or until the master is joining
        while (thread->parallel_for.index >= (volatile uint32_t) team->priv.parallel_for.index)
        {
            // sleep that thread
            syscall(
                SYS_futex,
                &team->priv.parallel_for.index,     // uint32_t *uaddr
                FUTEX_WAIT,                         // int futex_op
                thread->parallel_for.index,         // uint32_t val
                NULL,                               // const struct timespec *timeout | uint32_t val2
                NULL,                               // uint32_t *uaddr2
                NULL                                // uint32_t val3
            );
        }
    }

    assert(0);
    return NULL;
}

void
runtime_t::team_join(team_t * team)
{
    if (team->desc.routine == XKRT_TEAM_ROUTINE_PARALLEL_FOR)
        this->team_parallel_for(team, nullptr);

    // TODO : reimpl this using team's topology
    const int begin = team->desc.master_is_member ? 1 : 0;
    for (int i = begin ; i < team->priv.nthreads ; ++i)
    {
        // waiting for the thread to spawn before joining
        while ((volatile thread_state_t) team->priv.threads[i].state != XKRT_THREAD_INITIALIZED)
            ;
        assert(team->priv.threads[i].state == XKRT_THREAD_INITIALIZED);
        int r = pthread_join(team->priv.threads[i].pthread, NULL);
        assert(r == 0);
        team->priv.threads[i].state = XKRT_THREAD_UNINITIALIZED;
    }
    free(team->priv.threads);
}

void
runtime_t::team_critical_begin(team_t * team)
{
    pthread_mutex_lock(&team->priv.critical.mtx);
}

void
runtime_t::team_critical_end(team_t * team)
{
    pthread_mutex_unlock(&team->priv.critical.mtx);
}

team_t *
runtime_t::team_get(const driver_type_t type)
{
    driver_t * driver = this->driver_get(type);
    assert(driver);

    return &driver->team;
}

team_t *
runtime_t::team_get_any(const driver_type_bitfield_t types)
{
    for (uint8_t i = 0 ; i < XKRT_DRIVER_TYPE_MAX ; ++i)
    {
        if (types & (1 << i))
        {
            driver_type_t type = (driver_type_t) i;
            driver_t * driver = this->driver_get(type);
            if (driver && driver->team.priv.nthreads)
                return &driver->team;
        }
    }
    return NULL;
}

XKRT_NAMESPACE_END
