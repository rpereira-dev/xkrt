/* ************************************************************************** */
/*                                                                            */
/*   unregister-async.cc                                                      */
/*                                                                   .-*-.    */
/*   Author: Romain PEREIRA <rpereira@anl.gov>                     .'* *.'    */
/*                                                              __/_*_*(_     */
/*   Created: 2025/03/03 01:28:08 by Romain PEREIRA            / _______ \    */
/*   Updated: 2025/06/02 13:21:27 by Romain PEREIRA            \_)     (_/    */
/*                                                                            */
/*   License: ???                                                             */
/*                                                                            */
/* ************************************************************************** */

# include <xkrt/xkrt.h>
# include <xkrt/logger/logger.h>
# include <xkrt/logger/metric.h>

static xkrt_runtime_t runtime;

int
main(void)
{
    assert(xkrt_init(&runtime) == 0);

    # include "memory-register-async.conf.cc"

    runtime.memory_register_async(team, ptr, chunk_size, nchunks);
    runtime.task_wait();

    runtime.memory_unregister_async(team, ptr, chunk_size, nchunks);
    runtime.task_wait();

    assert(xkrt_deinit(&runtime) == 0);

    return 0;
}
