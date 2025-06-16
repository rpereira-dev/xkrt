/* ************************************************************************** */
/*                                                                            */
/*   memory-touch-async.cc                                        .-*-.       */
/*                                                              .'* *.'       */
/*   Created: 2025/03/05 05:19:56 by Romain PEREIRA          __/_*_*(_        */
/*   Updated: 2025/06/16 00:48:32 by Romain PEREIRA         / _______ \       */
/*                                                          \_)     (_/       */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/*   Author: Thierry GAUTIER <thierry.gautier@inrialpes.fr>                   */
/*   Author: Romain PEREIRA <romain.pereira@outlook.com>                      */
/*                                                                            */
/*   Copyright: see AUTHORS                                                   */
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

    uint64_t t0 = xkrt_get_nanotime();
    runtime.memory_touch_async(team, ptr, size, nchunks);
    runtime.task_wait();
    uint64_t tf = xkrt_get_nanotime();
    double dt = (tf-t0)/1e9;
    LOGGER_INFO("Touched with %.2lf GB/s", size/1e9/dt);

    assert(xkrt_deinit(&runtime) == 0);

    return 0;
}
