/* ************************************************************************** */
/*                                                                            */
/*   task-gpu-empty.cc                                            .-*-.       */
/*                                                              .'* *.'       */
/*   Created: 2024/12/20 15:07:55 by Romain PEREIRA          __/_*_*(_        */
/*   Updated: 2025/06/03 18:13:49 by Romain PEREIRA         / _______ \       */
/*                                                          \_)     (_/       */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/*   Author: Thierry GAUTIER <thierry.gautier@inrialpes.fr>                   */
/*   Author: Romain PEREIRA <romain.pereira@inria.fr>                         */
/*                                                                            */
/*   Copyright: see AUTHORS                                                   */
/*                                                                            */
/* ************************************************************************** */

# include <xkrt/xkrt.h>
# include <xkrt/memory/access/blas/region/dependency-tree.hpp>
# include <xkrt/task/task-format.h>
# include <xkrt/task/task.hpp>

# include <assert.h>
# include <string.h>

static int r = 0;

static void
func(task_t * task)
{
    r = 1;
}

static int *
setup(int m, int n, int ld)
{
    int * mem = (int *) malloc(ld * ld * sizeof(int));
    for (int i = 0 ; i < m ; ++i)
        for (int j = 0 ; j < m ; ++j)
            mem[i*ld+j] = 42;
    return mem;
}

int
main(void)
{
    xkrt_runtime_t runtime;
    assert(xkrt_init(&runtime) == 0);

    // create an empty task format
    task_format_id_t FORMAT;
    {
        task_format_t format;
        memset(&format, 0, sizeof(task_format_t));
        format.f[TASK_FORMAT_TARGET_HOST] = (task_format_func_t) func;
        FORMAT = task_format_create(&(runtime.formats.list), &format);
    }
    assert(FORMAT);

    xkrt_thread_t * thread = xkrt_thread_t::get_tls();
    assert(thread);

    // setup memory
    const int  m = 1024;
    const int  n = 1024;
    const int ld = 4096;
    int * mem = setup(m, n, ld);

    // Create a task
    # define AC 1
    static_assert(AC <= TASK_MAX_ACCESSES);
    constexpr task_flag_bitfield_t flags = TASK_FLAG_DEVICE | TASK_FLAG_DEPENDENT;
    constexpr size_t task_size = task_compute_size(flags, AC);

    task_t * task = thread->allocate_task(task_size);
    new(task) task_t(FORMAT, flags);

    task_dev_info_t * dev = TASK_DEV_INFO(task);
    new (dev) task_dev_info_t(UNSPECIFIED_DEVICE_GLOBAL_ID, UNSPECIFIED_TASK_ACCESS);

    task_dep_info_t * dep = TASK_DEP_INFO(task);
    new (dep) task_dep_info_t(AC);

    // set accesses
    access_t * accesses = TASK_ACCESSES(task);
    new(accesses + 0) access_t(task, MATRIX_COLMAJOR, mem, ld, 0, 0, m, n, sizeof(int), ACCESS_MODE_RW);

    thread->resolve<AC>(task, accesses);

    # undef AC

    // submit it to the runtime
    runtime.task_commit(task);

    // wait
    assert(xkrt_sync(&runtime) == 0);

    // deinit has an implicit taskwait
    assert(xkrt_deinit(&runtime) == 0);
    assert(r == 1);

    return 0;
}
