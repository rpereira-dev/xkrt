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

#ifndef __XKRT_TASK_HPP__
# define __XKRT_TASK_HPP__

// https://stackoverflow.com/questions/45342776/how-to-include-c11-headers-when-compiling-c-with-gcc
// cannot use <stdatomic.h> with c++
// # include <stdatomic.h>
# include <atomic>

# include <assert.h>
# include <stdint.h>

# include <xkrt/consts.h>
# include <xkrt/memory/access/dependency-domain.hpp>
# include <xkrt/task/task-format.h>
# include <xkrt/logger/logger.h>
# include <xkrt/logger/todo.h>
# include <xkrt/memory/access/access.hpp>
# include <xkrt/memory/cache-line-size.hpp>
# include <xkrt/memory/access/coherency-controller.hpp>
# include <xkrt/sync/spinlock.h>

/* task states */
typedef enum    task_state_t : uint8_t
{
    TASK_STATE_ALLOCATED        = 0,    // task_t is allocated
    TASK_STATE_READY            = 1,    // task_t data can be fetched
    TASK_STATE_DATA_FETCHING    = 2,    // task_t data is being fetched
    TASK_STATE_DATA_FETCHED     = 3,    // task_t data is fetched, kernel can be executed
    TASK_STATE_COMPLETED        = 4,    // task_t completed, dependences can be resolved (kernel executed)
    TASK_STATE_DEALLOCATED      = 5,    // task_t is deallocated (virtual state, never set)
    TASK_STATE_MAX              = 6,
}               task_state_t;

static inline const char *
task_state_to_str(task_state_t state)
{
    switch (state)
    {
        case (TASK_STATE_ALLOCATED):
            return "allocated";
        case (TASK_STATE_READY):
            return "ready";
        case (TASK_STATE_DATA_FETCHING):
            return "fetching";
        case (TASK_STATE_DATA_FETCHED):
            return "fetched";
        case (TASK_STATE_COMPLETED):
            return "completed";
        case (TASK_STATE_DEALLOCATED):
            return "deallocated";
        default:
            return "unk";
    }
}

# ifndef NDEBUG
#  define LOGGER_DEBUG_TASK_STATE(task)                                                                     \
    do {                                                                                                    \
        LOGGER_DEBUG("task `%s` is now in state `%s`", task->label, task_state_to_str(task->state.value));  \
    } while (0)
# else
#  define LOGGER_DEBUG_TASK_STATE(task)
# endif

/**
 *  Task flags. Constraints:
 *      - cannot have both TASK_FLAG_DOMAIN and TASK_FLAG_DEVICE
 */
typedef enum    task_flags_t
{
    TASK_FLAG_ZERO          = 0,
    TASK_FLAG_DEPENDENT     = (1 << 0), // the task may have dependencies
    TASK_FLAG_DETACHABLE    = (1 << 1), // the task completion is associated with the completion of user-defined external events
    TASK_FLAG_DEVICE        = (1 << 2), // task task may execute on a device
    TASK_FLAG_DOMAIN        = (1 << 3), // if this task may have dependent children tasks - in such case, it will have a dependency and a memory domain
    TASK_FLAG_REQUEUE       = (1 << 4), // if this flag is set, the task will be re-queued after returning from its body

    // support me in the future
  // TASK_FLAG_UNDEFERED     = (1 << X), // suspend the current task execution until that task completed
  // TASK_FLAG_PERSISTENT    = (1 << Y), // persistence

    TASK_FLAG_MAX           = (1 << 5)
}               task_flags_t;

// if test fails increase size of 'task_flag_bitfield_t'
typedef uint8_t task_flag_bitfield_t;
static_assert(TASK_FLAG_MAX <= (1 << 8*sizeof(task_flag_bitfield_t)));

typedef struct  task_t
{
    public:

        /* task format id */
        task_format_id_t fmtid;

        /* task flags */
        task_flag_bitfield_t flags;

        /* task state */
        struct {
            spinlock_t      lock;
            task_state_t    value;
        } state;

        /** Tasks currently do not support 'OpenMP private data' or 'kaapi/cilk stack' */

        /* parent task */
        task_t * parent;

        /* children counter - number of threads with uncompleted tasks scheduled */
        std::atomic<uint32_t> cc;

        # ifndef NDEBUG
        char label[128];
        # endif /* NDEBUG */

    public:

        task_t(task_format_id_t fmtid, task_flag_bitfield_t flags) :
            fmtid(fmtid), flags(flags),
            state { .lock = SPINLOCK_INITIALIZER, .value = TASK_STATE_ALLOCATED },
            parent(NULL), cc(0) {}

}               task_t;

typedef uint8_t task_wait_counter_type_t;
typedef std::atomic<task_wait_counter_type_t> task_wait_counter_t;

typedef uint8_t task_access_counter_t;
# define UNSPECIFIED_TASK_ACCESS ((task_access_counter_t)-1)
# define TASK_MAX_ACCESSES (5)
static_assert(TASK_MAX_ACCESSES < (1 << 8*sizeof(task_access_counter_t)));

/* task dependencies infos */
typedef struct  task_dep_info_t
{
    /*
     * wait counter
     * - if dependent task, it may be scheduled once it reached 0
     * - if detachable task, it is completed when it reached 2
     */
    task_wait_counter_t wc;

    /* access counter (number of accesses) */
    task_access_counter_t ac;

    /* constructor, wc is initially '1' as task must be commited */
    task_dep_info_t(task_access_counter_t ac) : wc(1), ac(ac) {}

}               task_dep_info_t;

/* detachable counter, shared with 'task_dep_info_t' if the task is both DEPENDENT and DETACHABLE */
typedef struct  task_det_info_t
{
    task_wait_counter_t wc;
    task_det_info_t() : wc(0) {}
}               task_det_info_t;

typedef struct  task_dev_info_t
{
    /* worker id on where to schedule once ready (or 'UNSPECIFIED_DEVICE_GLOBAL_ID' if leaving the decision to the scheduler) */
    xkrt_device_global_id_t targeted_device_id;

    // TODO : move the 'ocr' field to the 'dep_info' : it is tight to accesses, not to a device

    /* execute on the device that owns a copy of the access at accesses[ocr_access_index]
     * If 'UNSPECIFIED_TASK_ACCESS', leave the decision to the scheduler */
    uint8_t ocr_access_index;

    /* constructor */
    task_dev_info_t(xkrt_device_global_id_t target, uint8_t ocr)
        : targeted_device_id(target), ocr_access_index(ocr) {}

}               task_dev_info_t;

/* info about domain of dependencies */
typedef struct  task_dom_info_t
{
    /* dependency controller - only the thread currently executing the task may read this list */
    struct {
        std::vector<DependencyDomain *> blas;
        DependencyDomain * interval;
        DependencyDomain * point;
    } deps;

    /* memory controller for coherency - all threads may try to access this list */
    struct {
        std::vector<MemoryCoherencyController *> blas;
        // DependencyDomain * interval; - not implemented
        // DependencyDomain * point; - not implemented
    } mccs;

    task_dom_info_t() : deps{}, mccs{} {}

}               task_dom_info_t;

DependencyDomain * task_get_dependency_domain(
    task_t * task,
    const access_t * access
);

/* fallback if wrong flags parameter - https://stackoverflow.com/questions/20461121/constexpr-error-at-compile-time-but-no-overhead-at-run-time */
static size_t
task_get_base_size_fallback(task_flag_bitfield_t flags)
{
    LOGGER_FATAL("Invalid task flag combination: `%u`", flags);
    return 0;
}

/* compute the base size of a task (without arguments and private data) */
static constexpr size_t
task_get_extra_size(const task_flag_bitfield_t flags)
{
    switch (flags)
    {
        /* no flags (= cilk/kaapi task) */
        case (                   TASK_FLAG_ZERO |               TASK_FLAG_ZERO |               TASK_FLAG_ZERO |              TASK_FLAG_ZERO):
            return                            0 +                            0 +                            0 +                            0;  // 0.0.0.0

        case (                   TASK_FLAG_ZERO |               TASK_FLAG_ZERO |               TASK_FLAG_ZERO |         TASK_FLAG_DEPENDENT):
            return                            0 +                            0 +                            0 +      sizeof(task_dep_info_t);  // 0.0.0.1

        case (                   TASK_FLAG_ZERO |               TASK_FLAG_ZERO |         TASK_FLAG_DETACHABLE |              TASK_FLAG_ZERO):
            return                            0 +                            0 +      sizeof(task_det_info_t) +                            0;  // 0.0.1.0

        case (                   TASK_FLAG_ZERO |               TASK_FLAG_ZERO |         TASK_FLAG_DETACHABLE |         TASK_FLAG_DEPENDENT):
            return                            0 +                            0 +                          0x0 +      sizeof(task_dep_info_t);  // 0.0.1.1 - dep and det shared 'wc'

        case (                   TASK_FLAG_ZERO |             TASK_FLAG_DEVICE |               TASK_FLAG_ZERO |              TASK_FLAG_ZERO):
            return                            0 +      sizeof(task_dev_info_t) +                            0 +                            0;  // 0.1.0.0

        case (                   TASK_FLAG_ZERO |             TASK_FLAG_DEVICE |               TASK_FLAG_ZERO |         TASK_FLAG_DEPENDENT):
            return                            0 +      sizeof(task_dev_info_t) +                            0 +      sizeof(task_dep_info_t);  // 0.1.0.1

        case (                   TASK_FLAG_ZERO |             TASK_FLAG_DEVICE |         TASK_FLAG_DETACHABLE |              TASK_FLAG_ZERO):
            return                            0 +      sizeof(task_dev_info_t) +      sizeof(task_det_info_t) +                            0;  // 0.1.1.0

        case (                   TASK_FLAG_ZERO |             TASK_FLAG_DEVICE |         TASK_FLAG_DETACHABLE |         TASK_FLAG_DEPENDENT):
            return                            0 +      sizeof(task_dev_info_t) +                          0x0 +      sizeof(task_dep_info_t);  // 0.1.1.1 - dep and det shared 'wc'

        case (                 TASK_FLAG_DOMAIN |               TASK_FLAG_ZERO |               TASK_FLAG_ZERO |              TASK_FLAG_ZERO):
            return      sizeof(task_dom_info_t) +                            0 +                            0 +                            0;  // 1.0.0.0

        case (                 TASK_FLAG_DOMAIN |               TASK_FLAG_ZERO |               TASK_FLAG_ZERO |         TASK_FLAG_DEPENDENT):
            return      sizeof(task_dom_info_t) +                            0 +                            0 +      sizeof(task_dep_info_t);  // 1.0.0.1

        case (                 TASK_FLAG_DOMAIN |               TASK_FLAG_ZERO |         TASK_FLAG_DETACHABLE |              TASK_FLAG_ZERO):
            return      sizeof(task_dom_info_t) +                            0 +      sizeof(task_det_info_t) +                            0;  // 1.0.1.0

        case (                 TASK_FLAG_DOMAIN |               TASK_FLAG_ZERO |         TASK_FLAG_DETACHABLE |         TASK_FLAG_DEPENDENT):
            return      sizeof(task_dom_info_t) +                            0 +                          0x0 +      sizeof(task_dep_info_t);  // 1.0.1.1 - dep and det shared 'wc'

        // this is a constexpr, if we reach this default case, then the compiler will fail
        default:
            return task_get_base_size_fallback(flags);

         // return sizeof(task_t) + sizeof(task_dom_info_t) + sizeof(task_dev_info_t) +                            0 +                            0;  // 1.1.0.0 - forbidden combination
         // return sizeof(task_t) + sizeof(task_dom_info_t) +                            0 +                          0x0 + sizeof(task_dep_info_t);  // 1.1.0.1 - forbidden combination
         // return sizeof(task_t) + sizeof(task_dom_info_t) +                            0 +                          0x0 + sizeof(task_dep_info_t);  // 1.1.1.0 - forbidden combination
         // return sizeof(task_t) + sizeof(task_dom_info_t) +                            0 +                          0x0 + sizeof(task_dep_info_t);  // 1.1.1.1 - forbidden combination
    }
}

/* Given flags and the number of accesses, computes (at compile-time) the size
 * in bytes required for the task (without args_t) */
static constexpr inline size_t
task_compute_size(const task_flag_bitfield_t flags, const uint8_t ac)
{
    return sizeof(task_t) + task_get_extra_size(flags) + ac*sizeof(access_t);
}

/**
 *  In case of a task with all flags, its memory is
 *   ________________________________________________________________________________
 *  |                                                                                |
 *  | task_t | task_dep_info_t | task_dev_info_t | task_dom_info_t | accesses | args |
 *  |________________________________________________________________________________|
 *
 * if some flags are removed, builing blocks are removed
 */

// TODO : implement DEP_INFO, DET_INFO, ... with a `flags` argument to
// compute the offset at compile-time

static inline task_dep_info_t *
TASK_DEP_INFO(const task_t * task)
{
    assert(task->flags & TASK_FLAG_DEPENDENT);
    return (task_dep_info_t *) (task + 1);
}

static inline task_det_info_t *
TASK_DET_INFO(const task_t * task)
{
    assert(task->flags & TASK_FLAG_DETACHABLE);
    return (task_det_info_t *) (task + 1);
}

static inline task_dev_info_t *
TASK_DEV_INFO(const task_t * task)
{
    assert(  task->flags & TASK_FLAG_DEVICE);
    assert(!(task->flags & TASK_FLAG_DOMAIN));  // device tasks cannot have dependency domains
    if (task->flags & TASK_FLAG_DEPENDENT)
        return (task_dev_info_t *) (TASK_DEP_INFO(task) + 1);
    else if (task->flags & TASK_FLAG_DETACHABLE)
        return (task_dev_info_t *) (TASK_DET_INFO(task) + 1);
    else
        return (task_dev_info_t *) (task + 1);
}

static inline task_dom_info_t *
TASK_DOM_INFO(const task_t * task)
{
    assert(  task->flags & TASK_FLAG_DOMAIN);
    assert(!(task->flags & TASK_FLAG_DEVICE));  // device tasks cannot have dependency domains
    if (task->flags & TASK_FLAG_DEPENDENT)
        return (task_dom_info_t *) (TASK_DEP_INFO(task) + 1);
    else if (task->flags & TASK_FLAG_DETACHABLE)
        return (task_dom_info_t *) (TASK_DET_INFO(task) + 1);
    else
        return (task_dom_info_t *) (task + 1);
}

// tells the runtime to requeue this task after returning from its main
# define TASK_MUST_REQUEUE(T)           \
    do {                                \
        T->flags |= TASK_FLAG_REQUEUE;  \
    } while (0)

static constexpr size_t
TASK_ACCESSES_OFFSET(const task_flag_bitfield_t flags)
{
    // accesses must be stored right after the task struct
    return sizeof(task_t) + task_get_extra_size(flags);
}

static inline access_t *
TASK_ACCESSES(const task_t * task, const task_flag_bitfield_t flags)
{
    return (access_t *) (((char *) task) + TASK_ACCESSES_OFFSET(flags));
}

static inline access_t *
TASK_ACCESSES(const task_t * task)
{
    return TASK_ACCESSES(task, task->flags);
}

static inline void *
TASK_ARGS(const task_t * task, const size_t task_size)
{
    return (void *) (((char *) task) + task_size);
}

static inline size_t
TASK_SIZE(const task_t * task)
{
    if (task->flags & TASK_FLAG_DEPENDENT)
    {
        task_dep_info_t * dep = TASK_DEP_INFO(task);
        return sizeof(task_t) + task_get_extra_size(task->flags) + dep->ac*sizeof(access_t);
    }
    else
        return sizeof(task_t) + task_get_extra_size(task->flags);
}

static inline void *
TASK_ARGS(const task_t * task)
{
    return TASK_ARGS(task, TASK_SIZE(task));
}

///////////////////////////////////
// Methods to setup dependencies //
///////////////////////////////////

/* pred precedes succ - call 'F(args)' if 'pred' isnt completed yet in a lock region */
template <typename... Args>
static inline void
__task_precedes(
    task_t * pred,
    task_t * succ,
    void (*F)(Args...),
    Args... args
) {
    assert(pred);
    assert(succ);
    assert(pred->state.value >= TASK_STATE_ALLOCATED);
    assert(succ->state.value >= TASK_STATE_ALLOCATED);
    assert(pred->flags & TASK_FLAG_DEPENDENT);
    assert(succ->flags & TASK_FLAG_DEPENDENT);

    if (pred->state.value < TASK_STATE_COMPLETED)
    {
        SPINLOCK_LOCK(pred->state.lock);
        {
            if (pred->state.value < TASK_STATE_COMPLETED)
            {
                task_dep_info_t * sdep = TASK_DEP_INFO(succ);
                sdep->wc.fetch_add(1, std::memory_order_seq_cst);
                F(std::forward<Args>(args)...);
            }
        }
        SPINLOCK_UNLOCK(pred->state.lock);
    }
}

static inline void
__access_link(access_t * pred, access_t * succ)
{
    pred->successors.push_back(succ);
}

inline void
__access_precedes(access_t * pred, access_t * succ)
{
    // succ must be a dependent task
    assert(succ->task->flags & TASK_FLAG_DEPENDENT);

    // succ must have a wc>0 at this point: we are still processing dependencies, it cannot be scheduled yet
    assert(TASK_DEP_INFO(succ->task)->wc > 0);

    // succ has reached the maximum number of dependencies
    assert(TASK_DEP_INFO(succ->task)->wc < ((1 << (8 * sizeof(task_wait_counter_type_t))) - 1));

    // avoid redundant edges
    if (pred->successors.size() && pred->successors.back()->task == succ->task)
        return ;

    // set edge
    __task_precedes(pred->task, succ->task, __access_link, pred, succ);
}

////////////////////////////////////
// Methods to transition the task //
////////////////////////////////////

/* mark the task ready and call F(args) */
template <typename... Args>
static inline void
__task_ready(
    task_t * task,
    void (*F)(Args..., task_t *),
    Args... args
) {
    assert(task->state.value == TASK_STATE_ALLOCATED);
    assert(!(task->flags & TASK_FLAG_DEPENDENT) || (TASK_DEP_INFO(task)->wc.load() == 0));
    task->state.value = TASK_STATE_READY;
    LOGGER_DEBUG_TASK_STATE(task);
    F(std::forward<Args>(args)..., task);
}

/* mark the task as completed, and call F(..., succ) for each of its successors
 * 'succ' that are now ready */
template <typename... Args>
static inline void
__task_complete(
    task_t * task,
    void (*F)(Args..., task_t *),
    Args... args
) {
    task->parent->cc.fetch_sub(1, std::memory_order_relaxed);

    assert(
        task->state.value == TASK_STATE_DATA_FETCHED ||
        task->state.value == TASK_STATE_READY
    );
    if (task->flags & (TASK_FLAG_DETACHABLE | TASK_FLAG_DEPENDENT))
    {
        assert(
            ((task->flags & TASK_FLAG_DEPENDENT)  && (TASK_DEP_INFO(task)->wc.load() == 0)) ||
            ((task->flags & TASK_FLAG_DETACHABLE) && (TASK_DET_INFO(task)->wc.load() == 2))
        );
    }
    SPINLOCK_LOCK(task->state.lock);
    {
        task->state.value = TASK_STATE_COMPLETED;
        LOGGER_DEBUG_TASK_STATE(task);
    }
    SPINLOCK_UNLOCK(task->state.lock);
    assert(task->parent);
    if (task->flags & TASK_FLAG_DEPENDENT)
    {
        task_dep_info_t * dep = TASK_DEP_INFO(task);
        access_t * accesses = TASK_ACCESSES(task);
        for (uint8_t i = 0 ; i < dep->ac ; ++i)
        {
            access_t * access = accesses + i;
            for (access_t * succ_access : access->successors)
            {
                task_t * succ = succ_access->task;
                assert(succ->flags & TASK_FLAG_DEPENDENT);
                task_dep_info_t * sdep = TASK_DEP_INFO(succ);
                if (sdep->wc.fetch_sub(1, std::memory_order_seq_cst) == 1)
                    __task_ready(succ, F, args...);
            }
        }
    }
}


/* commit the task and call F(args) if it is now ready */
template <typename... Args>
static inline void
__task_commit(
    task_t * task,
    void (*F)(Args..., task_t *),
    Args... args
) {
    assert(task->state.value == TASK_STATE_ALLOCATED);
    if (task->flags & TASK_FLAG_DEPENDENT)
    {
        task_dep_info_t * dep = TASK_DEP_INFO(task);
        if (dep->wc.fetch_sub(1, std::memory_order_seq_cst) == 1)
            __task_ready(task, F, args...);
    }
    else
        __task_ready(task, F, args...);
}

static inline void
__task_fetching(
    task_wait_counter_type_t n,
    task_t * task
) {
    assert(task->flags & TASK_FLAG_DEPENDENT);
    task_dep_info_t * dep = TASK_DEP_INFO(task);
    if (dep->wc.fetch_add(n, std::memory_order_seq_cst) == 0)
    {
        assert(task->state.value == TASK_STATE_READY);
        task->state.value = TASK_STATE_DATA_FETCHING;
        LOGGER_DEBUG_TASK_STATE(task);
    }
}

/* notify that 'n' accesses had been fetched. If all accesses were fetched,
 * then mark the task as 'fetched' and call F(...) */
template <typename... Args>
static inline void
__task_fetched(
    task_wait_counter_type_t n,
    task_t * task,
    void (*F)(Args..., task_t *),
    Args... args
) {
    assert(task->state.value == TASK_STATE_DATA_FETCHING);
    assert(task->flags & TASK_FLAG_DEPENDENT);
    task_dep_info_t * dep = TASK_DEP_INFO(task);
    if (dep->wc.fetch_sub(n, std::memory_order_seq_cst) == n)
    {
        task->state.value = TASK_STATE_DATA_FETCHED;
        LOGGER_DEBUG_TASK_STATE(task);
        F(std::forward<Args>(args)..., task);
    }
}

/* decrease detachable ref counter by 1, and call F(..., succ) foreach task
 * 'succ' that became ready */
template <typename... Args>
static inline void
__task_detachable_post(
    task_t * task,
    void (*F)(Args..., task_t *),
    Args... args
) {
    assert(task->flags & TASK_FLAG_DETACHABLE);
    task_det_info_t * det = TASK_DET_INFO(task);
    if (det->wc.fetch_add(1, std::memory_order_relaxed) == 1)
        __task_complete(task, F, args...);
}

/* mark the task as 'executed' and call F(..., succ) for each task 'succ' that
 * became ready */
template <typename... Args>
static inline void
__task_executed(
    task_t * task,
    void (*F)(Args..., task_t *),
    Args... args
) {
    if (task->flags & TASK_FLAG_DETACHABLE)
        __task_detachable_post(task, F, args...);
    else
        __task_complete(task, F, args...);
}

# endif /* __XKRT_TASK_HPP__ */
