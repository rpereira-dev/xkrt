/* ************************************************************************** */
/*                                                                            */
/*   memory-register-async.cc                                     .-*-.       */
/*                                                              .'* *.'       */
/*   Created: 2025/02/11 14:59:33 by Romain PEREIRA          __/_*_*(_        */
/*   Updated: 2025/06/03 21:20:22 by Romain PEREIRA         / _______ \       */
/*                                                          \_)     (_/       */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/*   Author: Thierry GAUTIER <thierry.gautier@inrialpes.fr>                   */
/*   Author: Romain PEREIRA <rpereira@anl.gov>                                */
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

    runtime.memory_register_async(team, ptr, chunk_size, nchunks);
    runtime.task_wait();

    assert(xkrt_deinit(&runtime) == 0);

    return 0;
}
