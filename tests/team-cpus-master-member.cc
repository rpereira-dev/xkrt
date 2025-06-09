/* ************************************************************************** */
/*                                                                            */
/*   team-cpus-master-member.cc                                   .-*-.       */
/*                                                              .'* *.'       */
/*   Created: 2025/03/05 05:19:56 by Romain PEREIRA          __/_*_*(_        */
/*   Updated: 2025/06/09 04:03:30 by Romain PEREIRA         / _______ \       */
/*                                                          \_)     (_/       */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/*   Author: Thierry GAUTIER <thierry.gautier@inrialpes.fr>                   */
/*   Author: Romain PEREIRA <rpereira@anl.gov>                                */
/*                                                                            */
/*   Copyright: see AUTHORS                                                   */
/*                                                                            */
/* ************************************************************************** */

# ifndef _GNU_SOURCE
#  define _GNU_SOURCE
# endif
# include <sched.h>

# include <xkrt/xkrt.h>
# include <xkrt/logger/logger.h>
# include <xkrt/logger/metric.h>

static xkrt_runtime_t runtime;
static std::atomic<int> counter;
static bool master_run;

static void *
main_team(xkrt_team_t * team, xkrt_thread_t * thread)
{
    int cpu = sched_getcpu();
    LOGGER_INFO("Thread `%3d` running on `sched_getcpu() -> %3d`", thread->tid, cpu);
    ++counter;
    if (thread->tid == 0)
        master_run = true;
    return NULL;
}

int
main(void)
{
    assert(xkrt_init(&runtime) == 0);

    // team of 1 thread
    xkrt_team_t team = XKRT_TEAM_STATIC_INITIALIZER;
    team.desc.nthreads = 1;
    team.desc.routine = main_team;
    team.desc.master_is_member = true;

    // spawn the team threads
    master_run = false;
    runtime.team_create(&team);
    assert(master_run);
    runtime.team_join(&team);
    assert(counter == 1);

    // team on all cpus
    team.desc.nthreads = 0;
    master_run = false;
    runtime.team_create(&team);
    assert(master_run);
    runtime.team_join(&team);
    assert(counter == 1 + team.priv.nthreads);

    assert(xkrt_deinit(&runtime) == 0);

    return 0;
}
