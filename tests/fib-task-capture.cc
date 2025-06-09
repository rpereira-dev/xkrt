/* ************************************************************************** */
/*                                                                            */
/*   fib-task-capture.cc                                          .-*-.       */
/*                                                              .'* *.'       */
/*   Created: 2025/03/04 05:42:49 by Romain PEREIRA          __/_*_*(_        */
/*   Updated: 2025/06/09 03:46:37 by Romain PEREIRA         / _______ \       */
/*                                                          \_)     (_/       */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/*   Author: Thierry GAUTIER <thierry.gautier@inrialpes.fr>                   */
/*   Author: Romain PEREIRA <rpereira@anl.gov>                                */
/*                                                                            */
/*   Copyright: see AUTHORS                                                   */
/*                                                                            */
/* ************************************************************************** */

# include <xkrt/xkrt.h>
# include <xkrt/runtime.h>
# include <xkrt/logger/logger.h>
# include <xkrt/logger/metric.h>

static int N = 0;
# define CUTOFF_DEPTH 10

static const int fib_values[] = {
    1, 1, 2, 3, 5, 8, 13, 21, 34, 55, 89, 144, 233, 377, 610,
    987, 1597, 2584, 4181, 6765, 10946, 17711, 28657, 46368, 75025,
    121393, 196418, 317811, 514229, 832040, 1346269, 2178309, 3524578,
    5702887, 9227465, 14930352, 24157817, 39088169, 63245986, 102334155,
    165580141, 267914296, 433494437, 701408733, 1134903170, 1836311903
};

static xkrt_runtime_t runtime;

static inline int
fib(int n, int depth = 0)
{
    if (n <= 2)
        return n;

    int fn1, fn2;
    if (depth >= CUTOFF_DEPTH)
    {
        fn1 = fib(n-1, depth+1);
        fn2 = fib(n-2, depth+1);
    }
    else
    {
        runtime.task_spawn(
            [&n, &fn1, depth] (task_t * task) {
                fn1 = fib(n - 1, depth + 1);
            }
        );

        runtime.task_spawn(
            [&n, &fn2, depth] (task_t * task) {
                fn2 = fib(n - 2, depth + 1);
            }
        );

        runtime.task_wait();
    }
    return fn1 + fn2;
}

static void *
main_team(xkrt_team_t * team, xkrt_thread_t * thread)
{
    // warmup
    if (thread->tid == 0)
    {
        fib(5);
        runtime.task_wait();
    }
    runtime.team_barrier<true>(team, thread);

    // run
    if (thread->tid == 0)
    {
        double t0 = xkrt_get_nanotime();
        int r = fib(N);
        runtime.task_wait();
        double tf = xkrt_get_nanotime();
        LOGGER_INFO("Fib(%d) = %d - took %.2lf s", N, r, (tf - t0) / (double)1e9);
        assert(r == fib_values[N]);
    }
    runtime.team_barrier<true>(team, thread);

    return NULL;
}

int
main(int argc, char ** argv)
{
    if (argc != 2)
    {
        LOGGER_WARN("usage: %s [n]", argv[0]);
        N = 34;
    }
    else
        N = atoi(argv[1]);

    assert(N < sizeof(fib_values) / sizeof(int));

    assert(xkrt_init(&runtime) == 0);

    xkrt_team_t team = XKRT_TEAM_STATIC_INITIALIZER;
    team.desc.routine = main_team;

    runtime.team_create(&team);
    runtime.team_join(&team);

    assert(xkrt_deinit(&runtime) == 0);

    return 0;
}
