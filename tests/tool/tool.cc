/*
** Copyright 2024,2025 INRIA
**
** Contributors :
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

/*
 * A minimal XKRT-T tool used by the `tool-basic` test. It is an XKRT-aware C++
 * shared library, loaded by the runtime through `XKRT_TOOL_PATH`. Each callback
 * receives the XKRT objects directly (runtime/thread/team/task/access) and bumps
 * a per-event counter; at finalize it prints a summary and, when every event has
 * been observed, the "XKRT-T:OK" marker the test checks.
 */

# include <xkrt/runtime.h>
# include <xkrt/tool.h>

# include <atomic>
# include <cstdint>
# include <cstdio>

XKRT_NAMESPACE_USE;

/* per-event counters, indexed by xkrt_callback_t */
static std::atomic<uint64_t> COUNTS[XKRT_CALLBACK_MAX];

static inline void
bump(xkrt_callback_t event)
{
    COUNTS[event].fetch_add(1, std::memory_order_relaxed);
}

/* ------------------------------------------------------------------ */
/* callbacks - note they use the XKRT namespace types directly        */
/* ------------------------------------------------------------------ */

static void
on_thread_start(runtime_t * runtime, thread_t * thread)
{
    /* stamp the thread with a unique id, proving tool data is writable */
    thread->tool_data.value = runtime->tool_unique_id();
    bump(XKRT_CALLBACK_THREAD_START);
}

static void
on_thread_stop(runtime_t * runtime, thread_t * thread)
{
    (void) runtime; (void) thread;
    bump(XKRT_CALLBACK_THREAD_STOP);
}

static void
on_team_create(runtime_t * runtime, team_t * team)
{
    team->tool_data.value = runtime->tool_unique_id();
    bump(XKRT_CALLBACK_TEAM_CREATE);
}

static void
on_team_join(runtime_t * runtime, team_t * team)
{
    (void) runtime; (void) team;
    bump(XKRT_CALLBACK_TEAM_JOIN);
}

static void
on_task_create(runtime_t * runtime, task_t * task)
{
    task->tool_data.value = runtime->tool_unique_id();
    bump(XKRT_CALLBACK_TASK_CREATE);
}

static void
on_task_accesses(runtime_t * runtime, task_t * task, const access_t * accesses, xkrt_task_access_counter_t n)
{
    (void) runtime; (void) task; (void) accesses; (void) n;
    bump(XKRT_CALLBACK_TASK_ACCESSES);
}

static void
on_task_schedule(runtime_t * runtime, thread_t * thread, task_t * prev, task_t * next)
{
    (void) runtime; (void) thread; (void) prev; (void) next;
    bump(XKRT_CALLBACK_TASK_SCHEDULE);
}

static void
on_task_complete(runtime_t * runtime, task_t * task)
{
    (void) runtime; (void) task;
    bump(XKRT_CALLBACK_TASK_COMPLETE);
}

static void
on_barrier(runtime_t * runtime, team_t * team, thread_t * thread, xkrt_scope_t scope)
{
    (void) runtime; (void) team; (void) thread; (void) scope;
    bump(XKRT_CALLBACK_BARRIER);
}

static void
on_taskwait(runtime_t * runtime, thread_t * thread, task_t * task, xkrt_scope_t scope)
{
    (void) runtime; (void) thread; (void) task; (void) scope;
    bump(XKRT_CALLBACK_TASKWAIT);
}

static void
on_taskgroup(runtime_t * runtime, thread_t * thread, task_t * task, xkrt_scope_t scope)
{
    (void) runtime; (void) thread; (void) task; (void) scope;
    bump(XKRT_CALLBACK_TASKGROUP);
}

/* ------------------------------------------------------------------ */
/* activation                                                        */
/* ------------------------------------------------------------------ */

static int
tool_initialize(runtime_t * runtime, xkrt_tool_data_t * tool_data)
{
    (void) tool_data;

    # define REGISTER(EV, FN) \
        runtime->tool_set_callback(EV, (xkrt_callback_generic_t) FN)

    REGISTER(XKRT_CALLBACK_THREAD_START,  on_thread_start);
    REGISTER(XKRT_CALLBACK_THREAD_STOP,   on_thread_stop);
    REGISTER(XKRT_CALLBACK_TEAM_CREATE,   on_team_create);
    REGISTER(XKRT_CALLBACK_TEAM_JOIN,     on_team_join);
    REGISTER(XKRT_CALLBACK_TASK_CREATE,   on_task_create);
    REGISTER(XKRT_CALLBACK_TASK_ACCESSES, on_task_accesses);
    REGISTER(XKRT_CALLBACK_TASK_SCHEDULE, on_task_schedule);
    REGISTER(XKRT_CALLBACK_TASK_COMPLETE, on_task_complete);
    REGISTER(XKRT_CALLBACK_BARRIER,       on_barrier);
    REGISTER(XKRT_CALLBACK_TASKWAIT,      on_taskwait);
    REGISTER(XKRT_CALLBACK_TASKGROUP,     on_taskgroup);

    # undef REGISTER

    fprintf(stdout, "[tool] initialized\n");
    fflush(stdout);
    return 1; /* stay active */
}

static void
tool_finalize(xkrt_tool_data_t * tool_data)
{
    (void) tool_data;

    static const struct { xkrt_callback_t ev; const char * name; } NAMES[] = {
        { XKRT_CALLBACK_THREAD_START,  "thread_start"   },
        { XKRT_CALLBACK_THREAD_STOP,   "thread_stop"    },
        { XKRT_CALLBACK_TEAM_CREATE,   "team_create"    },
        { XKRT_CALLBACK_TEAM_JOIN,     "team_join"      },
        { XKRT_CALLBACK_TASK_CREATE,   "task_create"    },
        { XKRT_CALLBACK_TASK_ACCESSES, "task_accesses"  },
        { XKRT_CALLBACK_TASK_SCHEDULE, "task_schedule"  },
        { XKRT_CALLBACK_TASK_COMPLETE, "task_complete"  },
        { XKRT_CALLBACK_BARRIER,       "barrier"        },
        { XKRT_CALLBACK_TASKWAIT,      "taskwait"       },
        { XKRT_CALLBACK_TASKGROUP,     "taskgroup"      },
    };

    bool all = true;
    for (auto & e : NAMES)
    {
        uint64_t c = COUNTS[e.ev].load(std::memory_order_relaxed);
        fprintf(stdout, "[tool] %-14s = %llu\n", e.name, (unsigned long long) c);
        if (c == 0)
            all = false;
    }

    fprintf(stdout, all ? "XKRT-T:OK\n" : "XKRT-T:MISSING-EVENTS\n");
    fflush(stdout);
}

extern "C" xkrt_tool_result_t *
xkrt_tool_start(void)
{
    static xkrt_tool_result_t result = {
        &tool_initialize,
        &tool_finalize,
        XKRT_TOOL_DATA_NONE
    };
    return &result;
}
