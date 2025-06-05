/* ************************************************************************** */
/*                                                                            */
/*   register-async.cc                                            .-*-.       */
/*                                                              .'* *.'       */
/*   Created: 2025/02/11 14:59:33 by Romain PEREIRA          __/_*_*(_        */
/*   Updated: 2025/06/03 18:13:31 by Romain PEREIRA         / _______ \       */
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

static void *       ptr         = NULL;
static const size_t chunk_size  = 4096 * 64 + 123;
static const int    nchunks     = 16;

int
main(void)
{
    assert(xkrt_init(&runtime) == 0);
    ptr = malloc(chunk_size * nchunks);
    runtime.memory_register_async(ptr, chunk_size, nchunks);
    assert(xkrt_deinit(&runtime) == 0);

    return 0;
}
