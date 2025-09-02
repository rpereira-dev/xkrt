/* ************************************************************************** */
/*                                                                            */
/*   register-async.cc                                            .-*-.       */
/*                                                              .'* *.'       */
/*   Created: 2025/02/11 14:59:33 by Romain PEREIRA          __/_*_*(_        */
/*   Updated: 2025/08/23 00:11:51 by Romain PEREIRA         / _______ \       */
/*                                                          \_)     (_/       */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/*   Author: Thierry GAUTIER <thierry.gautier@inrialpes.fr>                   */
/*   Author: Romain PEREIRA <rpereira@anl.gov>                                */
/*                                                                            */
/*   Copyright: see AUTHORS                                                   */
/*                                                                            */
/* ************************************************************************** */

# include <xkrt/runtime.h>
# include <xkrt/logger/logger.h>
# include <xkrt/logger/metric.h>

XKRT_NAMESPACE_USE;

static void *       ptr         = NULL;
static const size_t chunk_size  = 4096 * 64 + 123;
static const int    nchunks     = 16;

int
main(void)
{
    runtime_t runtime;

    assert(runtime.init() == 0);
    ptr = malloc(chunk_size * nchunks);
    runtime.memory_register_async(ptr, chunk_size, nchunks);
    assert(runtime.deinit() == 0);

    return 0;
}
