/* ************************************************************************** */
/*                                                                            */
/*   task-dependency-interval.cc                                  .-*-.       */
/*                                                              .'* *.'       */
/*   Created: 2025/05/19 00:09:44 by Romain PEREIRA          __/_*_*(_        */
/*   Updated: 2025/09/15 18:45:48 by Romain PEREIRA         / _______ \       */
/*                                                          \_)     (_/       */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/*   Author: Thierry GAUTIER <thierry.gautier@inrialpes.fr>                   */
/*   Author: Romain PEREIRA <romain.pereira@outlook.com>                      */
/*                                                                            */
/*   Copyright: see AUTHORS                                                   */
/*                                                                            */
/* ************************************************************************** */

# include <xkrt/runtime.h>
# include <xkrt/memory/access/blas/dependency-tree.hpp>
# include <xkrt/task/task-format.h>
# include <xkrt/task/task.hpp>

# include <assert.h>
# include <string.h>

XKRT_NAMESPACE_USE;

static int x = 0;

# define AC 1
constexpr task_flag_bitfield_t flags = TASK_FLAG_DEPENDENT;
constexpr size_t task_size = task_compute_size(flags, AC);
constexpr size_t args_size = sizeof(int);

static void
func(task_t * task)
{
    int * args = (int *) TASK_ARGS(task, task_size);
    assert(*args == x);
    ++x;
}

int
main(void)
{
    runtime_t runtime;
    assert(runtime.init() == 0);

    // create an empty task format
    task_format_id_t FORMAT;
    {
        task_format_t format;
        memset(&format, 0, sizeof(task_format_t));
        format.f[TASK_FORMAT_TARGET_HOST] = (task_format_func_t) func;
        FORMAT = task_format_create(&(runtime.formats.list), &format);
    }
    assert(FORMAT);

    thread_t * thread = thread_t::get_tls();
    assert(thread);

    ////////////////////////////////
    // Create the following graph //
    //  T1 -> T2 -> T3            //
    ////////////////////////////////

    access_mode_t modes[] = {
        ACCESS_MODE_W,
        ACCESS_MODE_R,
        ACCESS_MODE_RW
    };

    const Interval interval[] = {
        Interval((INTERVAL_TYPE_T) (&x - 16), (INTERVAL_TYPE_T) (&x + 16)),
        Interval((INTERVAL_TYPE_T) (&x -  0), (INTERVAL_TYPE_T) (&x + 19)),
        Interval((INTERVAL_TYPE_T) (&x -  3), (INTERVAL_TYPE_T) (&x + 12))
    };

    constexpr int ntasks = sizeof(modes) / sizeof(access_mode_t);
    assert(ntasks == sizeof(interval) / sizeof(Interval));

    for (int t = 0 ; t < ntasks ; ++t)
    {
        // Create a task
        task_t * task = thread->allocate_task(task_size + args_size);
        new(task) task_t(FORMAT, flags);

        task_dep_info_t * dep = TASK_DEP_INFO(task);
        new (dep) task_dep_info_t(AC);

        int * args = (int *) TASK_ARGS(task, task_size);
        *args = t;

        # if XKRT_SUPPORT_DEBUG
        snprintf(task->label, sizeof(task->label), "dependent-task-test-%d", t);
        # endif

        // set accesses
        access_t * accesses = TASK_ACCESSES(task);
        static_assert(AC <= TASK_MAX_ACCESSES);
        new(accesses + 0) access_t(task, interval[t].a, interval[t].b, modes[t]);
        thread->resolve(accesses, AC);

        // submit it to the runtime
        runtime.task_commit(task);
    }

    // wait
    runtime.task_wait();

    // deinit has an implicit taskwait
    assert(runtime.deinit() == 0);
    assert(x == ntasks);

    return 0;
}
