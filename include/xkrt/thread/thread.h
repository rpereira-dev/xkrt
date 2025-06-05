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

# ifndef __XKRT_THREAD_H__
#  define __XKRT_THREAD_H__

#  include <xkrt/consts.h>
#  include <xkrt/sync/spinlock.h>
#  include <xkrt/memory/access/blas/dependency-tree.hpp>
#  include <xkrt/task/task.hpp>

#  include <xkrt/memory/alignas.h>
#  include <xkrt/thread/deque.hpp>
#  include <xkrt/thread/naive-queue.hpp>

#  include <pthread.h>
#  include <atomic>
#  include <random>

#  include <linux/futex.h>      /* Definition of FUTEX_* constants */
#  include <sys/syscall.h>      /* Definition of SYS_* constants */
#  include <unistd.h>

/* pointer to the current team */
# define XKRT_TEAM_CURRENT (xkrt_thread_t::get_tls()->team)

/////////////
// THREADS //
/////////////

/* thread states */
typedef enum    xkrt_thread_state_t
{
    XKRT_THREAD_UNINITIALIZED   = 0,
    XKRT_THREAD_INITIALIZED     = 1
}               xkrt_thread_state_t;

//  NOTES
//
//  A binary tree of height 'n' has
//      'm' nodes  with 2^(n-1) + 1 <= m <= 2^n - 1
//  and 'k' leaves with           1 <= k <= 2^(n-1)
//
//  <=>
//
//  Given a binary tree with 'k' leaves, its height 'n' must verify
//     1 <=      k      <= 2^(n-1)
// <=> 0 <= log2(k)     <= n-1
// <=>      log2(k) + 1 <= n
//                                     _            _
//  So we need a tree of height 'n' = |  log2(k) + 1 | to represent the 'k' threads

/* type of nodes in the tree */
typedef enum    xkrt_team_node_type_t
{
    XKRT_TEAM_NODE_TYPE_HYPERTHREAD = 0,    // hyperthread
    XKRT_TEAM_NODE_TYPE_CORE        = 1,    // core
    XKRT_TEAM_NODE_TYPE_CACHE_L1    = 2,    // shared cache, L2 or L3 typically
    XKRT_TEAM_NODE_TYPE_CACHE_L2    = 3,    // shared cache, L2 or L3 typically
    XKRT_TEAM_NODE_TYPE_CACHE_L3    = 4,    // shared cache, L2 or L3 typically
    XKRT_TEAM_NODE_TYPE_NUMA        = 5,    // numa node
    XKRT_TEAM_NODE_TYPE_SOCKET      = 6,    // full dram
    XKRT_TEAM_NODE_TYPE_MACHINE     = 7     // multi socket system
}               xkrt_team_node_type_t;

typedef enum    xkrt_team_binding_mode_t
{
    XKRT_TEAM_BINDING_MODE_COMPACT,
    XKRT_TEAM_BINDING_MODE_SPREAD,
}               xkrt_team_binding_mode_t;

typedef enum    xkrt_team_binding_places_t
{
    XKRT_TEAM_BINDING_PLACES_HYPERTHREAD,
    XKRT_TEAM_BINDING_PLACES_CORE,
    XKRT_TEAM_BINDING_PLACES_L1,
    XKRT_TEAM_BINDING_PLACES_L2,
    XKRT_TEAM_BINDING_PLACES_L3,
    XKRT_TEAM_BINDING_PLACES_NUMA,
    XKRT_TEAM_BINDING_PLACES_DEVICE,
    XKRT_TEAM_BINDING_PLACES_SOCKET,
    XKRT_TEAM_BINDING_PLACES_MACHINE,
    XKRT_TEAM_BINDING_PLACES_EXPLICIT,
}               xkrt_team_binding_places_t;

typedef enum    xkrt_team_binding_flag_t
{
    XKRT_TEAM_BINDING_FLAG_NONE         = 0,
    XKRT_TEAM_BINDING_FLAG_EXCLUDE_HOST = (1 << 0)
}               xkrt_team_binding_flag_t;

/* a place */
typedef cpu_set_t xkrt_thread_place_t;

struct xkrt_team_t;

/* a thread */
typedef struct  xkrt_thread_t
{
    public:

        /* set the TLS */
        static void save_tls(xkrt_thread_t * thread);

        /* get the TLS */
        static xkrt_thread_t * get_tls(void);

    public:

        /* the thread team */
        xkrt_team_t * team;

        /* the place assigned to that thread */
        xkrt_thread_place_t place;

        /* the thread implicit task */
        union {
            task_t implicit_task;
            char _implicit_task_buffer[task_compute_size(TASK_FLAG_DOMAIN, 0)];
        };

        /* the current task */
        task_t * current_task;

        # ifndef NDEBUG
        std::vector<task_t *> tasks;
        # endif /* NDEBUG */

        /* the thread state, use for synchronizing */
        xkrt_thread_state_t state;

        /* the pthread */
        pthread_t pthread;

        /* global thread tid */
        int gtid;

        /* the tid in the team */
        int tid;

        /* the device global id attached to that thread */
        xkrt_device_global_id_t device_global_id;

        /* the thread deque */
        // xkrt_deque_t<task_t *, 4096> deque;
        NaiveQueue<task_t *> deque;

        /* tasks stack */
        uint8_t * memory_stack_bottom;

        /* next free task pointer in the stack */
        uint8_t * memory_stack_ptr;

        /* memory capacity */
        size_t memory_stack_capacity;

        /* random number generator */
        std::minstd_rand rng;

        /* lock and condition to sleep the mutex */
        struct {
            pthread_mutex_t lock;
            pthread_cond_t  cond;
            volatile bool   sleeping;
        } sleep;

        struct {
            /* next function index in the team functions */
            uint32_t index;

        } parallel_for;

    public:

        // xkrt_thread_t(int tid) : xkrt_thread_t(tid, 0, UNSPECIFIED_DEVICE_GLOBAL_ID) {}

        xkrt_thread_t(
            xkrt_team_t * team,
            int tid,
            pthread_t pthread,
            xkrt_device_global_id_t device_global_id,
            xkrt_thread_place_t place
        ) :
            team(team),
            place(place),
            implicit_task(TASK_FORMAT_NULL, TASK_FLAG_DOMAIN),
            state(XKRT_THREAD_INITIALIZED),
            pthread(pthread),
            gtid(gettid()),
            tid(tid),
            device_global_id(device_global_id),
            deque(),
            memory_stack_bottom(NULL),
            memory_stack_capacity(THREAD_MAX_MEMORY),
            rng(),
            parallel_for{.index = 0}
        {
            // set current task
            this->current_task = &this->implicit_task;

            // initialize sync primitives
            pthread_mutex_init(&this->sleep.lock, 0);
            pthread_cond_init (&this->sleep.cond, 0);
            this->sleep.sleeping = false;

            // initialize implicit task dependency domain
            task_dom_info_t * dom = TASK_DOM_INFO(&this->implicit_task);
            new (dom) task_dom_info_t();
            # ifndef NDEBUG
            snprintf(this->implicit_task.label, sizeof(this->implicit_task.label), "implicit");
            # endif

            // initialize memory allocator
            while (1)
            {
                this->memory_stack_bottom = (uint8_t *) malloc(this->memory_stack_capacity);
                if (this->memory_stack_bottom)
                    break ;

                this->memory_stack_capacity = (size_t) (this->memory_stack_capacity * 2 / 3);
                if (this->memory_stack_capacity == 0)
                    this->memory_stack_bottom = NULL;
            }
            this->memory_stack_ptr = this->memory_stack_bottom;
            assert(this->memory_stack_bottom);
        }

        ~xkrt_thread_t()
        {
            free(this->memory_stack_ptr);
        }

    public:
        void pause(void);
        void wakeup(void);
        void warmup(void);
        task_t * allocate_task(const size_t size);
        void deallocate_all_tasks(void);

    /////////////////
    // TASK HELPER //
    /////////////////

    public:

        /**
         * Find conflicts and insert accesses int he dependency tree
         */
        template <int AC>
        inline void
        resolve(task_t * task, access_t * accesses)
        {
            (void) task;
            assert(task->flags & TASK_FLAG_DEPENDENT);
            assert(AC > 0);

            DependencyDomain * dom = task_get_dependency_domain(this->current_task, accesses + 0);
            dom->resolve<AC>(accesses);
        }

        /**
         * Insert a task and its access in the dependency tree, without finding conflicts
         */
        template <int AC>
        inline void
        insert(task_t * task, access_t * accesses)
        {
            (void) task;
            assert(task->flags & TASK_FLAG_DEPENDENT);
            assert(AC > 0);

            DependencyDomain * dom = task_get_dependency_domain(this->current_task, accesses + 0);
            dom->put<AC>(accesses);
        }

        # define __Thread_task_execute(T, t, F, ...)                                                \
        do {                                                                                        \
                assert(T && t);                                                                     \
                task_format_t * format = runtime->formats.list.list + t->fmtid;                     \
                assert(format->f[TASK_FORMAT_TARGET_HOST]);                                         \
                task_t * current = T->current_task;                                                 \
                T->current_task = t;                                                                \
                void (*f)(task_t *) = (void (*)(task_t *)) format->f[TASK_FORMAT_TARGET_HOST];      \
                f(t);                                                                               \
                __task_executed(t, F, __VA_ARGS__);                                                 \
                T->current_task = current;                                                          \
            } while (0)

        template <typename... Args>
        inline void
        commit(
            task_t * task,
            void (*F)(Args..., task_t *),
            Args... args
        ) {
            assert(this->current_task);
            ++this->current_task->cc;
            task->parent = this->current_task;
            __task_commit(task, F, args...);
        }

        # ifndef NDEBUG
        static void
        dump_tasks(FILE * f, std::vector<task_t *> & tasks)
        {
            fprintf(f, "digraph G {\n");
            for (task_t * & task : tasks)
            {
                if (task->flags & TASK_FLAG_DEPENDENT)
                {
                    task_dep_info_t * dep = TASK_DEP_INFO(task);
                    access_t * accesses = TASK_ACCESSES(task);
                    for (int i = 0 ; i < dep->ac ; ++i)
                    {
                        access_t * pred = accesses + i;
                        fprintf(f, "    \"%p\" [label=\"%s - ac %d\"] ;\n", pred, task->label, i);
                        for (access_t * succ : pred->successors)
                            fprintf(f, "    \"%p\" -> \"%p\" ;\n", pred, succ);
                    }
                }
            }
            fprintf(f, "}\n");
        }

        void
        dump_tasks(FILE * f)
        {
            xkrt_thread_t::dump_tasks(f, this->tasks);
        }
        # endif /* NDEBUG */

}               xkrt_thread_t;

//////////
// TEAM //
//////////

/* a node in the topology graph */
typedef struct  xkrt_team_node_t
{
    /* the node type */
    xkrt_team_node_type_t type;

    /* the thread owning that node */
    xkrt_thread_t * thread;

}               xkrt_team_node_t;

/**
 *  The supported combinations are:
 *    (mode = COMPACT, places = DEVICE)
 *      -> that will compactly bind 1 thread per device
 *
 *    (mode = SPREAD, places = MACHINE) with any nthreads
 *      -> that will spread threads across all cores of the machine
 */
typedef struct  xkrt_team_binding_t
{
    /* how to distribute threads among places */
    xkrt_team_binding_mode_t mode;

    /* the places, if XKRT_TEAM_BINDING_PLACES_EXPLICIT - then `xkrt_thread_place_t` must be not null */
    xkrt_team_binding_places_t places;
    xkrt_thread_place_t * places_list;
    int nplaces;

    /* additional flags */
    xkrt_team_binding_flag_t flags;

}               xkrt_team_binding_t;

/* team description */
typedef struct  xkrt_team_desc_t
{
    // routine that will be executed by each thread
    void * (*routine)(struct xkrt_team_t * team, struct xkrt_thread_t * thread);

    // user arguments
    void * args;

    // number of threads to spawn
    int nthreads;

    // type of the team
    xkrt_team_binding_t binding;

    // TODO : add flags with enabled feature ? (barrier, critical, etc...)

}               xkrt_team_desc_t;

/* a team, currently is made of 1 thread max per device, bound onto its closest physical cpu */
typedef struct  xkrt_team_t
{
    // team description, to be filled by the user before forking it
    xkrt_team_desc_t desc;

    struct {

        ////////////////////////////////////////////////////////

        // threads
        xkrt_thread_t * threads;
        int nthreads;

        // custom barrier for workstealing
        struct {
            std::atomic<int> n;     /* for spawned threads to sync */
            volatile int version;
            pthread_cond_t cond;    /* to sleep threads when synchronizing */
            pthread_mutex_t mtx;
        } barrier;

        // critical
        struct {
            pthread_mutex_t mtx;
        } critical;

        ////////////////////////////////////////////////////////////

        // if the routine is parallel for, then this is the stack of lambdas to execute
        # define XKRT_TEAM_PARALLEL_FOR_MAX_FUNC 1
        alignas(hardware_destructive_interference_size)
        struct {
            uint32_t index;
            std::function<void(xkrt_team_t * team, xkrt_thread_t * thread)> f[XKRT_TEAM_PARALLEL_FOR_MAX_FUNC];
            uint32_t completed;
            std::atomic<uint32_t> pending;
        } parallel_for;


    } priv;

}               xkrt_team_t;

typedef std::function<void(xkrt_team_t * team, xkrt_thread_t * thread)> xkrt_team_parallel_for_func_t;
void * xkrt_team_parallel_for_main(xkrt_team_t * team, xkrt_thread_t * thread);
# define XKRT_TEAM_ROUTINE_PARALLEL_FOR xkrt_team_parallel_for_main

# endif /* __XKRT_THREAD_H__ */
