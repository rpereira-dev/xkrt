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

/**
 * XKRT Tooling interface (XKRT-T)
 * ===============================
 *
 * A callback interface that lets a *tool* observe XKRT runtime events. Unlike
 * OMPT, XKRT-T is deliberately expressed in XKRT's own terms: callbacks receive
 * the actual runtime objects (`runtime_t`, `thread_t`, `team_t`, `task_t`,
 * `access_t`). A tool is therefore an XKRT-aware C++ component (it includes the
 * XKRT headers and uses the `xkrt` namespace). For instance, XKOMP's OMPT
 * support is such a tool: it translates XKRT-T events into OpenMP events.
 *
 * The tooling state is stored per `runtime_t` (`runtime_t::tool`), so several
 * runtimes may live in the same process, each with its own attached tool.
 *
 * A tool is activated in one of two ways:
 *   1. Shared library: `XKRT_TOOL_PATH=/path/to/tool.so`. Each runtime dlopen's
 *      it and calls its `xkrt_tool_start()` entry point during init.
 *   2. In-process: `runtime.tool_connect(result)` before `runtime.init()` (used
 *      e.g. by XKOMP).
 * In both cases the runtime then calls `result->initialize(runtime, ...)`, from
 * which the tool registers callbacks with `runtime->tool_set_callback(...)`.
 *
 * Everything compiles away unless XKRT was built with `-DUSE_TOOLS=ON`
 * (`XKRT_SUPPORT_TOOLS`); when built in but no tool is attached, each event is a
 * single (branch-predicted-not-taken) test.
 */

#ifndef __XKRT_TOOL_H__
# define __XKRT_TOOL_H__

# include <xkrt/consts.h>
# include <xkrt/namespace.h>
# include <xkrt/support.h>

# include <atomic>
# include <cstdint>

/* branch hint: a tool is, in the common case, not attached */
# ifndef XKRT_UNLIKELY
#  define XKRT_UNLIKELY(x) (__builtin_expect(!!(x), 0))
# endif

XKRT_NAMESPACE_BEGIN

/* the XKRT entities a tool observes (defined in their own headers) */
struct runtime_t;
struct thread_t;
struct team_t;
struct task_t;
class  access_t;

/**
 * A 64-bit datum attached to each observable entity (thread/team/task). Owned
 * entirely by the tool: the runtime never inspects it, it only keeps it alive
 * for the entity's lifetime and exposes the entity in callbacks.
 */
typedef union   xkrt_tool_data_t
{
    uint64_t    value;
    void *      ptr;
}               xkrt_tool_data_t;

# define XKRT_TOOL_DATA_NONE { .value = 0 }

/* begin/end marker for scoped events (barrier, taskwait, taskgroup) */
typedef enum    xkrt_scope_t
{
    XKRT_SCOPE_BEGIN = 1,
    XKRT_SCOPE_END   = 2
}               xkrt_scope_t;

/* runtime events a tool may register a callback for */
typedef enum    xkrt_callback_t
{
    XKRT_CALLBACK_THREAD_START  = 1,   /* a thread_t started               */
    XKRT_CALLBACK_THREAD_STOP   = 2,   /* a thread_t is stopping           */
    XKRT_CALLBACK_TEAM_CREATE   = 3,   /* a team_t is created              */
    XKRT_CALLBACK_TEAM_JOIN     = 4,   /* a team_t is joined               */
    XKRT_CALLBACK_TASK_CREATE   = 5,   /* a task_t is allocated            */
    XKRT_CALLBACK_TASK_ACCESSES = 6,   /* a task_t's accesses are resolved */
    XKRT_CALLBACK_TASK_SCHEDULE = 7,   /* a thread switches task           */
    XKRT_CALLBACK_TASK_COMPLETE = 8,   /* a task_t completed               */
    XKRT_CALLBACK_BARRIER       = 9,   /* team barrier begin/end           */
    XKRT_CALLBACK_TASKWAIT      = 10,  /* taskwait begin/end               */
    XKRT_CALLBACK_TASKGROUP     = 11,  /* taskgroup begin/end              */

    XKRT_CALLBACK_MAX           = 12
}               xkrt_callback_t;

/* generic callback pointer, cast to the concrete type at registration */
typedef void (*xkrt_callback_generic_t)(void);

/*
 * Typed callbacks. Every callback receives, as its first argument, the runtime
 * that raised the event (so a single callback can serve several runtimes).
 */
typedef void (*xkrt_callback_thread_start_t) (runtime_t *, thread_t *);
typedef void (*xkrt_callback_thread_stop_t)  (runtime_t *, thread_t *);
typedef void (*xkrt_callback_team_create_t)  (runtime_t *, team_t *);
typedef void (*xkrt_callback_team_join_t)    (runtime_t *, team_t *);
typedef void (*xkrt_callback_task_create_t)  (runtime_t *, task_t *);
typedef void (*xkrt_callback_task_accesses_t)(runtime_t *, task_t *, const access_t *, xkrt_task_access_counter_t);
typedef void (*xkrt_callback_task_schedule_t)(runtime_t *, thread_t *, task_t * prev, task_t * next);
typedef void (*xkrt_callback_task_complete_t)(runtime_t *, task_t *);
typedef void (*xkrt_callback_barrier_t)      (runtime_t *, team_t *, thread_t *, xkrt_scope_t);
typedef void (*xkrt_callback_taskwait_t)     (runtime_t *, thread_t *, task_t *, xkrt_scope_t);
typedef void (*xkrt_callback_taskgroup_t)    (runtime_t *, thread_t *, task_t *, xkrt_scope_t);

/* result of registering a callback (see runtime_t::tool_set_callback) */
typedef enum    xkrt_set_result_t
{
    XKRT_SET_ERROR  = 0,   /* invalid arguments                   */
    XKRT_SET_NEVER  = 1,   /* event is never raised by this build */
    XKRT_SET_ALWAYS = 5    /* callback registered                 */
}               xkrt_set_result_t;

/* tool activation entry points */
typedef int  (*xkrt_tool_initialize_t)(runtime_t * runtime, xkrt_tool_data_t * tool_data);
typedef void (*xkrt_tool_finalize_t)  (xkrt_tool_data_t * tool_data);

/* what a tool returns to describe itself to a runtime */
typedef struct  xkrt_tool_result_t
{
    xkrt_tool_initialize_t  initialize;
    xkrt_tool_finalize_t    finalize;
    xkrt_tool_data_t        tool_data;
}               xkrt_tool_result_t;

/* per-runtime tooling state, embedded in runtime_t (see runtime_t::tool) */
typedef struct  xkrt_tool_state_t
{
    bool                    enabled   = false;
    xkrt_callback_generic_t callbacks[XKRT_CALLBACK_MAX] = {};
    xkrt_tool_result_t *    result    = nullptr;
    void *                  dl_handle = nullptr;             /* set iff loaded from XKRT_TOOL_PATH */
    std::atomic<uint64_t>   next_unique_id{1};
}               xkrt_tool_state_t;

XKRT_NAMESPACE_END

/*****************************************************************************
 * emit macro
 *
 * Single entry point used by the runtime to raise an event. It is a no-op when
 * tooling is compiled out (`!XKRT_SUPPORT_TOOLS`) or disabled at runtime
 * (`runtime->tool.enabled == false`). When compiled out, the variadic
 * arguments are not evaluated, so call sites may freely reference tooling data.
 *
 * The runtime is passed once (RT): it is used both to test/reach the tooling
 * state and forwarded as the callback's first argument.
 *****************************************************************************/

# if XKRT_SUPPORT_TOOLS

#  define XKRT_TOOL_ENABLED(RT) (XKRT_UNLIKELY((RT)->tool.enabled))

#  define XKRT_TOOL_EMIT(RT, EVENT, TYPE, ...)                                  \
    do {                                                                        \
        auto * __xkrt_rt = (RT);                                                \
        if (XKRT_UNLIKELY(__xkrt_rt->tool.enabled))                            \
        {                                                                       \
            TYPE __xkrt_cb = (TYPE) __xkrt_rt->tool.callbacks[EVENT];           \
            if (__xkrt_cb)                                                      \
                __xkrt_cb(__xkrt_rt __VA_OPT__(,) __VA_ARGS__);                 \
        }                                                                       \
    } while (0)

# else /* !XKRT_SUPPORT_TOOLS */

#  define XKRT_TOOL_ENABLED(RT) (false)
#  define XKRT_TOOL_EMIT(RT, EVENT, TYPE, ...) do { } while (0)

# endif /* XKRT_SUPPORT_TOOLS */

/*
 * Entry point a tool shared library (loaded via XKRT_TOOL_PATH) must export.
 * Returns the tool description, or NULL to decline activation.
 */
extern "C" XKRT_NAMESPACE::xkrt_tool_result_t * xkrt_tool_start(void);

#endif /* __XKRT_TOOL_H__ */
