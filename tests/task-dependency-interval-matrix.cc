/* ************************************************************************** */
/*                                                                            */
/*   task-dependency-interval-matrix.cc                           .-*-.       */
/*                                                              .'* *.'       */
/*   Created: 2025/05/19 00:09:44 by Romain PEREIRA          __/_*_*(_        */
/*   Updated: 2025/09/15 18:46:33 by Romain PEREIRA         / _______ \       */
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
    usleep(1000);
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

    // Create task on interval [0..100]
    {
        // Create a task
        task_t * task = thread->allocate_task(task_size + args_size);
        new (task) task_t(FORMAT, flags);

        task_dep_info_t * dep = TASK_DEP_INFO(task);
        new (dep) task_dep_info_t(AC);

        int * args = (int *) TASK_ARGS(task, task_size);
        *args = 0;

        #ifndef NDEBUG
        snprintf(task->label, sizeof(task->label), "task-0");
        #endif

        // set accesses
        access_t * accesses = TASK_ACCESSES(task);
        static_assert(AC <= TASK_MAX_ACCESSES);
        new (accesses + 0) access_t(task, 0, 100, ACCESS_MODE_W);
        thread->resolve(accesses, AC);

        // submit it to the runtime
        runtime.task_commit(task);
    }

    // Create task on matrix that conflicts with [0..100]
    {
        // Create a task
        task_t * task = thread->allocate_task(task_size + args_size);
        new (task) task_t(FORMAT, flags);

        task_dep_info_t * dep = TASK_DEP_INFO(task);
        new (dep) task_dep_info_t(AC);

        int * args = (int *) TASK_ARGS(task, task_size);
        *args = 1;

        #ifndef NDEBUG
        snprintf(task->label, sizeof(task->label), "task-1");
        #endif

        // set accesses
        access_t * accesses = TASK_ACCESSES(task);
        static_assert(AC <= TASK_MAX_ACCESSES);
        const void * addr = (void *) 3;
        const size_t ld = 8;
        const size_t offset_m = 0;
        const size_t offset_n = 0;
        const size_t m = 8;
        const size_t n = 8;
        const size_t s = 1;
        new (accesses + 0) access_t(task, MATRIX_COLMAJOR, addr, ld, 0, 0, m, n, s, ACCESS_MODE_W);
        thread->resolve(accesses, AC);

        // submit it to the runtime
        runtime.task_commit(task);
    }

    // Create task on interval [0..100]
    {
        // Create a task
        task_t * task = thread->allocate_task(task_size + args_size);
        new (task) task_t(FORMAT, flags);

        task_dep_info_t * dep = TASK_DEP_INFO(task);
        new (dep) task_dep_info_t(AC);

        int * args = (int *) TASK_ARGS(task, task_size);
        *args = 2;

        #ifndef NDEBUG
        snprintf(task->label, sizeof(task->label), "task-2");
        #endif

        // set accesses
        access_t * accesses = TASK_ACCESSES(task);
        static_assert(AC <= TASK_MAX_ACCESSES);
        new (accesses + 0) access_t(task, 0, 100, ACCESS_MODE_W);
        thread->resolve(accesses, AC);

        // submit it to the runtime
        runtime.task_commit(task);
    }

    // wait
    runtime.task_wait();

    // deinit has an implicit taskwait
    assert(runtime.deinit() == 0);
    assert(x == 3);

    return 0;
}
