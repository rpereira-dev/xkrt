/* ************************************************************************** */
/*                                                                            */
/*   team-cpus.cc                                                 .-*-.       */
/*                                                              .'* *.'       */
/*   Created: 2025/03/05 05:19:56 by Romain PEREIRA          __/_*_*(_        */
/*   Updated: 2025/08/23 00:18:42 by Romain PEREIRA         / _______ \       */
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

# include <xkrt/runtime.h>
# include <xkrt/logger/logger.h>
# include <xkrt/logger/metric.h>

XKRT_NAMESPACE_USE;

static std::atomic<int> counter;

static void *
main_team(team_t * team, thread_t * thread)
{
    int cpu = sched_getcpu();
    LOGGER_INFO("Thread `%3d` running on `sched_getcpu() -> %3d`", thread->tid, cpu);
    ++counter;
    return NULL;
}

int
main(void)
{
    runtime_t runtime;
    assert(runtime.init() == 0);

    // team of 1 thread
    team_t team;
    team.desc.nthreads = 1;
    team.desc.routine = main_team;

    // spawn the team threads
    runtime.team_create(&team);
    runtime.team_join(&team);
    assert(counter == 1);

    // team on all cpus
    team.desc.nthreads = 0;
    runtime.team_create(&team);
    runtime.team_join(&team);
    assert(counter == 1 + team.priv.nthreads);

    assert(runtime.deinit() == 0);

    return 0;
}
