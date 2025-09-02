/* ************************************************************************** */
/*                                                                            */
/*   memory-register-protection.cc                                .-*-.       */
/*                                                              .'* *.'       */
/*   Created: 2025/02/11 14:59:33 by Romain PEREIRA          __/_*_*(_        */
/*   Updated: 2025/08/23 00:08:59 by Romain PEREIRA         / _______ \       */
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

int
main(void)
{
    runtime_t runtime;

    assert(runtime.init() == 0);

    const size_t size = 1;
    const unsigned int nchunks = 1;
    const size_t h = 0;

    void * ptr = calloc(1, 2*size);
    assert(ptr);
    uintptr_t p = (uintptr_t) ptr;

    team_t * team = runtime.team_get_any(1 << XKRT_DRIVER_TYPE_HOST);
    assert(team);

    runtime.memory_register(ptr, size);

    runtime.distribute_async(XKRT_DISTRIBUTION_TYPE_CYCLIC1D, ptr, 2*size, 2*size, h);
    // runtime.distribute_async(XKRT_DISTRIBUTION_TYPE_CYCLIC1D, (void *) (p+0)   , size, size, h);
    // runtime.distribute_async(XKRT_DISTRIBUTION_TYPE_CYCLIC1D, (void *) (p+size), size, size, h);
    runtime.task_wait();

    runtime.memory_unregister(ptr, size);

    assert(runtime.deinit() == 0);

    return 0;
}
