/* ************************************************************************** */
/*                                                                            */
/*   team-cpus-parallel-for.cc                                    .-*-.       */
/*                                                              .'* *.'       */
/*   Created: 2025/03/05 05:19:56 by Romain PEREIRA          __/_*_*(_        */
/*   Updated: 2025/08/23 00:17:42 by Romain PEREIRA         / _______ \       */
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

#ifdef NDEBUG
# define XKRT_ASSERT(...) if (__VA_ARGS__) {}
#else
# define XKRT_ASSERT(...) assert(__VA_ARGS__)
#endif

# include <xkrt/runtime.h>
# include <xkrt/logger/logger.h>
# include <xkrt/logger/metric.h>

XKRT_NAMESPACE_USE;

int
main(void)
{
    runtime_t runtime;
    XKRT_ASSERT(runtime.init() == 0);

    // team on all cpus
    team_t team;
    team.desc.routine = XKRT_TEAM_ROUTINE_PARALLEL_FOR;

    // TEST 1
    // create and destroy the team without working
    {
        std::atomic<int> counter(0);
        runtime.team_create(&team);
        runtime.team_join(&team);
        XKRT_ASSERT(counter == 0);
    }

    // TEST 2
    // create, dispatch 1 function, destroy
    {
        std::atomic<int> counter(0);
        runtime.team_create(&team);
        runtime.team_parallel_for(&team, [&counter] (team_t * team, thread_t * thread) {
                LOGGER_INFO("Thread `%3d` running on `sched_getcpu() -> %3d`", thread->tid, sched_getcpu());
                ++counter;
            }
        );
        runtime.team_join(&team);
        XKRT_ASSERT(counter == team.priv.nthreads);
    }

    // TEST 3
    // create, dispatch 'n' functions, destroy
    {
        constexpr int n = 10000;
        std::atomic<int> counter(0);
        runtime.team_create(&team);

        uint64_t t0 = get_nanotime();
        for (int i = 0 ; i < n ; ++i)
            runtime.team_parallel_for(&team, [] (team_t * team, thread_t * thread) { });
        uint64_t tf = get_nanotime();
        LOGGER_INFO("`%d` empty parallel on `%d` threads for took %lf s - that is %luns/task\n", n, team.priv.nthreads, (tf-t0)/1e9, (tf-t0)/(n*team.priv.nthreads));

        runtime.team_join(&team);
    }
    XKRT_ASSERT(runtime.deinit() == 0);

    return 0;
}
