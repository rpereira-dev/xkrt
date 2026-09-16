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
 * Driver for the XKRT-T tooling interface. It exercises the instrumented code
 * paths (team lifecycle, tasks, dependences, synchronization). When run with
 * `XKRT_TOOL_PATH` pointing at the sample tool (see tool.cc), the tool observes
 * every event and prints "XKRT-T:OK" from its finalize callback, which the
 * CTest matches.
 */

# include <xkrt/runtime.h>
# include <xkrt/logger/logger.h>

# include <cassert>
# include <cstdlib>
# include <new>

XKRT_NAMESPACE_USE;

static void *
main_team(runtime_t * rt, team_t * team, thread_t * thread)
{
    (void) thread;
    /* implicit barrier: raises a sync_region(barrier) event */
    rt->team_barrier(team);
    return NULL;
}

int
main(void)
{
    runtime_t runtime;
    assert(runtime.init() == 0);

    /* 1) a team: thread begin/end, team create/join, implicit task, barrier */
    {
        team_t team;
        team.desc.nthreads         = 4;
        team.desc.routine          = (team_routine_t) main_team;
        team.desc.master_is_member = true;

        runtime.team_create(&team);
        runtime.team_join(&team);
    }

    /* 2) a plain task + taskwait: task_create, task_schedule, sync_region */
    {
        int v = 0;
        runtime.task_spawn(
            [&v] (runtime_t *, device_t *, task_t *) { v = 1; }
        );
        runtime.task_wait();
        assert(v == 1);
    }

    /* 3) dependent tasks: task_dependences (W then R on the same datum) */
    {
        int * mem = (int *) malloc(sizeof(int));
        assert(mem);
        *mem = 0;

        runtime.task_spawn<1>(
            [mem] (task_t * task, access_t * accesses) {
                new (accesses + 0) access_t(task, (const void *) mem, ACCESS_MODE_W);
            },
            [mem] (runtime_t *, device_t *, task_t *) { *mem = 42; }
        );

        runtime.task_spawn<1>(
            [mem] (task_t * task, access_t * accesses) {
                new (accesses + 0) access_t(task, (const void *) mem, ACCESS_MODE_R);
            },
            [mem] (runtime_t *, device_t *, task_t *) { assert(*mem == 42); }
        );

        runtime.task_wait();
        free(mem);
    }

    /* 4) a taskgroup: sync_region(taskgroup) begin/end */
    {
        runtime.taskgroup_begin();
        int w = 0;
        runtime.task_spawn(
            [&w] (runtime_t *, device_t *, task_t *) { w = 7; }
        );
        runtime.taskgroup_end();
        assert(w == 7);
    }

    assert(runtime.deinit() == 0);

    return 0;
}
