/* ************************************************************************** */
/*                                                                            */
/*   fib-task-format.cc                                           .-*-.       */
/*                                                              .'* *.'       */
/*   Created: 2025/03/04 05:42:49 by Romain PEREIRA          __/_*_*(_        */
/*   Updated: 2025/08/22 23:50:05 by Romain PEREIRA         / _______ \       */
/*                                                          \_)     (_/       */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/*   Author: Thierry GAUTIER <thierry.gautier@inrialpes.fr>                   */
/*   Author: Romain PEREIRA <rpereira@anl.gov>                                */
/*                                                                            */
/*   Copyright: see AUTHORS                                                   */
/*                                                                            */
/* ************************************************************************** */

# include <xkrt/runtime.h>
# include <xkrt/runtime.h>
# include <xkrt/logger/logger.h>
# include <xkrt/logger/metric.h>

XKRT_NAMESPACE_USE;

static int N = 0;
# define CUTOFF_DEPTH 10

static const int fib_values[] = {
    1, 1, 2, 3, 5, 8, 13, 21, 34, 55, 89, 144, 233, 377, 610,
    987, 1597, 2584, 4181, 6765, 10946, 17711, 28657, 46368, 75025,
    121393, 196418, 317811, 514229, 832040, 1346269, 2178309, 3524578,
    5702887, 9227465, 14930352, 24157817, 39088169, 63245986, 102334155,
    165580141, 267914296, 433494437, 701408733, 1134903170, 1836311903
};

static runtime_t runtime;
static task_format_id_t fmtid;

constexpr task_flag_bitfield_t flags = TASK_FLAG_ZERO;
constexpr size_t task_size = task_compute_size(flags, 0);

typedef struct  task_args_t
{
    int * fibn;
    int n;
    int depth;
}               task_args_t;

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
        thread_t * thread = thread_t::get_tls();
        assert(thread);

        // shared(fn1) firstprivate(n, depth)
        {
            task_t * task = thread->allocate_task(task_size + sizeof(task_args_t));
            new(task) task_t(fmtid, flags);

            task_args_t * args = (task_args_t *) TASK_ARGS(task, task_size);
            args->fibn  = &fn1;
            args->n     = n - 1;
            args->depth = depth + 1;

            thread->commit(task, runtime_t::task_thread_enqueue, &runtime, thread);
        }

        // shared(fn2) firstprivate(n, depth)
        {
            task_t * task = thread->allocate_task(task_size + sizeof(task_args_t));
            new(task) task_t(fmtid, flags);

            task_args_t * args = (task_args_t *) TASK_ARGS(task, task_size);
            args->fibn  = &fn2;
            args->n     = n - 2;
            args->depth = depth + 1;

            thread->commit(task, runtime_t::task_thread_enqueue, &runtime, thread);
        }

        runtime.task_wait();
    }
    return fn1 + fn2;
}

static void
body_host(task_t * task)
{
    task_args_t * args = (task_args_t *) TASK_ARGS(task, task_size);
    *(args->fibn) = fib(args->n, args->depth);
}

static void *
main_team(team_t * team, thread_t * thread)
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
        double t0 = get_nanotime();
        int r = fib(N);
        runtime.task_wait();
        double tf = get_nanotime();
        LOGGER_INFO("Fib(%d) = %d - took %.2lf s", N, r, (tf - t0) / (double)1e9);
        assert(r == fib_values[N]);
    }
    runtime.team_barrier<true>(team, thread);

    return NULL;
}

int
main(int argc, char ** argv)
{
    LOGGER_INFO("Task size is %lu", task_compute_size(TASK_FLAG_ZERO, 0));
    LOGGER_INFO("Task size is %lu", task_compute_size(TASK_FLAG_DEPENDENT | TASK_FLAG_MOLDABLE, 1));
    if (argc != 2)
    {
        LOGGER_WARN("usage: %s [n]", argv[0]);
        N = 34;
    }
    else
        N = atoi(argv[1]);

    assert(N < sizeof(fib_values) / sizeof(int));

    assert(runtime.init() == 0);

    // register task format
    task_format_t format;
    memset(format.f, 0, sizeof(format.f));
    format.f[TASK_FORMAT_TARGET_HOST] = (task_format_func_t) body_host;
    snprintf(format.label, sizeof(format.label), "fib");
    fmtid = task_format_create(&(runtime.formats.list), &format);

    team_t team;
    team.desc.routine = main_team;

    runtime.team_create(&team);
    runtime.team_join(&team);

    assert(runtime.deinit() == 0);

    return 0;
}
