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

# include <xkrt/runtime.h>
# include <xkrt/logger/logger.h>
# include <xkrt/logger/metric.h>

XKRT_NAMESPACE_USE;

int
main(void)
{
    runtime_t runtime;

    assert(runtime.init() == 0);

    # include "memory-register-async.conf.cc"

    runtime.memory_register_async(team, ptr, size, nchunks);
    runtime.task_wait();

    runtime.memory_unregister_async(team, ptr, size, nchunks);
    runtime.task_wait();

    assert(runtime.deinit() == 0);

    return 0;
}
