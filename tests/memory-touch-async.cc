/* ************************************************************************** */
/*                                                                            */
/*   memory-touch-async.cc                                        .-*-.       */
/*                                                              .'* *.'       */
/*   Created: 2025/03/05 05:19:56 by Romain PEREIRA          __/_*_*(_        */
/*   Updated: 2025/08/23 00:09:24 by Romain PEREIRA         / _______ \       */
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
# include <xkrt/logger/logger.h>
# include <xkrt/logger/metric.h>

XKRT_NAMESPACE_USE;

int
main(void)
{
    runtime_t runtime;

    assert(runtime.init() == 0);

    # include "memory-register-async.conf.cc"

    uint64_t t0 = get_nanotime();
    runtime.memory_touch_async(team, ptr, size, nchunks);
    runtime.task_wait();
    uint64_t tf = get_nanotime();
    double dt = (tf-t0)/1e9;
    LOGGER_INFO("Touched with %.2lf GB/s", size/1e9/dt);

    assert(runtime.deinit() == 0);

    return 0;
}
