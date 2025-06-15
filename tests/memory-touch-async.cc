/* ************************************************************************** */
/*                                                                            */
/*   memory-touch-async.cc                                        .-*-.       */
/*                                                              .'* *.'       */
/*   Created: 2025/03/05 05:19:56 by Romain PEREIRA          __/_*_*(_        */
/*   Updated: 2025/06/14 01:26:42 by Romain PEREIRA         / _______ \       */
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

    runtime.memory_touch_async(team, ptr, size, nchunks);
    runtime.task_wait();

    assert(xkrt_deinit(&runtime) == 0);

    return 0;
}
