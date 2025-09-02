/* ************************************************************************** */
/*                                                                            */
/*   task-format-host.cc                                          .-*-.       */
/*                                                              .'* *.'       */
/*   Created: 2024/12/20 15:07:55 by Romain PEREIRA          __/_*_*(_        */
/*   Updated: 2025/08/23 00:16:01 by Romain PEREIRA         / _______ \       */
/*                                                          \_)     (_/       */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/*   Author: Thierry GAUTIER <thierry.gautier@inrialpes.fr>                   */
/*   Author: Romain PEREIRA <romain.pereira@inria.fr>                         */
/*                                                                            */
/*   Copyright: see AUTHORS                                                   */
/*                                                                            */
/* ************************************************************************** */

# include <xkrt/runtime.h>
# include <xkrt/task/task-format.h>
# include <xkrt/task/task.hpp>

# include <assert.h>
# include <string.h>

XKRT_NAMESPACE_USE;

static int r = 0;

static void
func(task_t * task)
{
    r = 1;
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

    // create an host task
    thread_t * thread = thread_t::get_tls();
    assert(thread);

    // Submit the task
    constexpr task_flag_bitfield_t flags = TASK_FLAG_ZERO;
    constexpr size_t task_size = task_compute_size(flags, 0);

    task_t * task = thread->allocate_task(task_size);
    new (task) task_t(FORMAT, flags);

    runtime.task_commit(task);

    // wait
    runtime.task_wait();

    // deinit has an implicit taskwait
    assert(runtime.deinit() == 0);
    assert(r == 1);

    return 0;
}
