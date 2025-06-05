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

# include <xkrt/xkrt.h>
# include <xkrt/logger/logger.h>
# include <xkrt/logger/metric.h>

static xkrt_runtime_t runtime;

static void *       ptr         = NULL;
static const size_t size        = 4096 * 64 + 123;

int
main(void)
{
    assert(xkrt_init(&runtime) == 0);
    ptr = malloc(size);
    xkrt_memory_register_async(&runtime, ptr, size);
    assert(xkrt_deinit(&runtime) == 0);

    return 0;
}
