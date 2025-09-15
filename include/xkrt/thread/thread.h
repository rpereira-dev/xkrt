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
# define XKRT_TEAM_CURRENT (thread_t::get_tls()->team)

XKRT_NAMESPACE_BEGIN

    /////////////
    // THREADS //
    /////////////

    /* thread states */
    typedef enum    thread_state_t
    {
        XKRT_THREAD_UNINITIALIZED   = 0,
        XKRT_THREAD_INITIALIZED     = 1
    }               thread_state_t;

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
    typedef enum    team_node_type_t
    {
        XKRT_TEAM_NODE_TYPE_HYPERTHREAD = 0,    // hyperthread
        XKRT_TEAM_NODE_TYPE_CORE        = 1,    // core
        XKRT_TEAM_NODE_TYPE_CACHE_L1    = 2,    // shared cache, L2 or L3 typically
        XKRT_TEAM_NODE_TYPE_CACHE_L2    = 3,    // shared cache, L2 or L3 typically
        XKRT_TEAM_NODE_TYPE_CACHE_L3    = 4,    // shared cache, L2 or L3 typically
        XKRT_TEAM_NODE_TYPE_NUMA        = 5,    // numa node
        XKRT_TEAM_NODE_TYPE_SOCKET      = 6,    // full dram
        XKRT_TEAM_NODE_TYPE_MACHINE     = 7     // multi socket system
    }               team_node_type_t;

    typedef enum    team_binding_mode_t
    {
        XKRT_TEAM_BINDING_MODE_COMPACT,
        XKRT_TEAM_BINDING_MODE_SPREAD,
    }               team_binding_mode_t;

    typedef enum    team_binding_places_t
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
    }               team_binding_places_t;

    typedef enum    team_binding_flag_t
    {
        XKRT_TEAM_BINDING_FLAG_NONE         = 0,
        XKRT_TEAM_BINDING_FLAG_EXCLUDE_HOST = (1 << 0)
    }               team_binding_flag_t;

    /* a place */
    typedef cpu_set_t thread_place_t;

    struct team_t;

    /* a thread */
    typedef struct  thread_t
    {
        public:

            /* set the current TLS */
            static void push_tls(thread_t * thread);

            /* pop to the previous TLS */
            static void pop_tls(void);

            /* get the TLS */
            static thread_t * get_tls(void);

        public:

            /* the thread team */
            team_t * team;

            /* the place assigned to that thread */
            thread_place_t place;

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
            thread_state_t state;

            /* the pthread */
            pthread_t pthread;

            /* global thread tid */
            int gtid;

            /* the tid in the team */
            int tid;

            /* the device global id attached to that thread */
            device_global_id_t device_global_id;

            /* the thread deque */
            // deque_t<task_t *, 4096> deque;
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
                volatile bool sleeping;
            } sleep;

            struct {
                /* next function index in the team functions */
                uint32_t index;

            } parallel_for;

            /* previous TLS */
            thread_t * prev;

        public:

            // thread_t(int tid) : thread_t(tid, 0, UNSPECIFIED_DEVICE_GLOBAL_ID) {}

            thread_t(
                team_t * team,
                int tid,
                pthread_t pthread,
                device_global_id_t device_global_id,
                thread_place_t place
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
                parallel_for{.index = 0},
                prev(NULL)
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

            ~thread_t()
            {
                free(this->memory_stack_ptr);
            }

        public:


            /* pause the thread until 'test' returns false */
            template<typename Func>
            inline void
            pause(Func && test)
            {
                // poll a few time before actually taking the lock
                for (int i = 0 ; i < 16 ; ++i)
                {
                    if (!test())
                        return ;
                }

                pthread_mutex_lock(&this->sleep.lock);
                {
                    while (test())
                    {
                        this->sleep.sleeping = true;
                        pthread_cond_wait(&this->sleep.cond, &this->sleep.lock);
                    }
                }
                pthread_mutex_unlock(&this->sleep.lock);
            }

            inline void
            wakeup(void)
            {
                pthread_mutex_lock(&this->sleep.lock);
                {
                    if (this->sleep.sleeping)
                    {
                        this->sleep.sleeping = false;
                        // LOGGER_DEBUG("Waking up thread");
                        pthread_cond_signal(&this->sleep.cond);
                    }
                }
                pthread_mutex_unlock(&this->sleep.lock);
            }

            void warmup(void);
            task_t * allocate_task(const size_t size);
            void deallocate_all_tasks(void);

        /////////////////
        // TASK HELPER //
        /////////////////

        public:

            /** Find conflicts and insert accesses in the dependency tree */
            inline void
            resolve(access_t * accesses, task_access_counter_t AC)
            {
                assert(AC > 0);
                assert(accesses);
                assert(this->current_task);
                task_dependency_resolve(this->current_task, accesses, AC);
            }

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
                return __task_commit(task, F, args...);
            }

            # ifndef NDEBUG

            void
            dump_tasks(FILE * f)
            {
                fprintf(f, "digraph G {\n");
                for (task_t * & task : tasks)
                {
                    fprintf(f, "    \"%p\" [label=\"%s\"] ;\n", task, task->label);
                    if (task->flags & TASK_FLAG_DEPENDENT)
                    {
                        task_dep_info_t * dep = TASK_DEP_INFO(task);
                        access_t * accesses = TASK_ACCESSES(task);
                        for (int i = 0 ; i < dep->ac ; ++i)
                        {
                            access_t * pred = accesses + i;
                            for (access_t * succ : pred->successors)
                                fprintf(f, "    \"%p\" -> \"%p\" ;\n", pred->task, succ->task);
                        }
                    }
                }
                fprintf(f, "}\n");
            }

            void
            dump_accesses(FILE * f)
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

            # endif /* NDEBUG */

    }               thread_t;

    //////////
    // TEAM //
    //////////

    /* a node in the topology graph */
    typedef struct  team_node_t
    {
        /* the node type */
        team_node_type_t type;

        /* the thread owning that node */
        thread_t * thread;

    }               team_node_t;

    /**
     *  The supported combinations are:
     *    (mode = COMPACT, places = DEVICE)
     *      -> that will compactly bind 1 thread per device
     *
     *    (mode = SPREAD, places = MACHINE) with any nthreads
     *      -> that will spread threads across all cores of the machine
     */
    struct team_binding_t
    {
        team_binding_t() :
            mode(XKRT_TEAM_BINDING_MODE_COMPACT),
            places(XKRT_TEAM_BINDING_PLACES_CORE),
            places_list(NULL),
            nplaces(0),
            flags(XKRT_TEAM_BINDING_FLAG_NONE)
        {}

        /* how to distribute threads among places */
        team_binding_mode_t mode;

        /* the places, if XKRT_TEAM_BINDING_PLACES_EXPLICIT - then `thread_place_t` must be not null */
        team_binding_places_t places;
        thread_place_t * places_list;
        int nplaces;

        /* additional flags */
        team_binding_flag_t flags;

    };

    /* team description */
    struct  team_desc_t
    {
        team_desc_t() :
            routine(NULL),
            args(NULL),
            nthreads(0),
            binding()
        {}

        // routine that will be executed by each thread
        void * (*routine)(struct team_t * team, struct thread_t * thread);

        // user arguments
        void * args;

        // number of threads to spawn
        int nthreads;

        // type of the team
        team_binding_t binding;

        // whether the master thread should be a member of the team or not
        bool master_is_member;
    };

    /* a team, currently is made of 1 thread max per device, bound onto its closest physical cpu */
    struct team_t
    {
        // default constructor
        team_t() : desc() {}

        // team description, to be filled by the user before forking it
        team_desc_t desc;

        struct {

            ////////////////////////////////////////////////////////

            // threads
            thread_t * threads;
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
                std::function<void(team_t * team, thread_t * thread)> f[XKRT_TEAM_PARALLEL_FOR_MAX_FUNC];
                uint32_t completed;
                std::atomic<uint32_t> pending;
            } parallel_for;


        } priv;

    };

    typedef std::function<void(team_t * team, thread_t * thread)> team_parallel_for_func_t;
    void * team_parallel_for_main(team_t * team, thread_t * thread);
    # define XKRT_TEAM_ROUTINE_PARALLEL_FOR team_parallel_for_main

XKRT_NAMESPACE_END

# endif /* __XKRT_THREAD_H__ */
