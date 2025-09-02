/* ************************************************************************** */
/*                                                                            */
/*   register-async.cc                                                        */
/*                                                                   .-*-.    */
/*   Author: Romain PEREIRA <rpereira@anl.gov>                     .'* *.'    */
/*                                                              __/_*_*(_     */
/*   Created: 2025/03/03 01:28:08 by Romain PEREIRA            / _______ \    */
/*   Updated: 2025/05/23 18:09:41 by Romain PEREIRA            \_)     (_/    */
/*                                                                            */
/*   License: ???                                                             */
/*                                                                            */
/* ************************************************************************** */

# include <xkrt/runtime.h>
# include <xkrt/logger/logger.h>
# include <xkrt/logger/metric.h>

XKRT_NAMESPACE_USE;

static void *       ptr  = NULL;
static const size_t size = 4096 * 64 + 123;

int
main(void)
{
    runtime_t runtime;

    assert(runtime.init() == 0);
    ptr = malloc(size);
    runtime.memory_register_async(ptr, size);
    runtime.task_wait();
    assert(runtime.deinit() == 0);

    return 0;
}
