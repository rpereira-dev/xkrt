/* ************************************************************************** */
/*                                                                            */
/*   memory-register-protection.cc                                .-*-.       */
/*                                                              .'* *.'       */
/*   Created: 2025/02/11 14:59:33 by Romain PEREIRA          __/_*_*(_        */
/*   Updated: 2025/08/11 19:12:28 by Romain PEREIRA         / _______ \       */
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

    const size_t size = 1;
    const unsigned int nchunks = 1;
    const size_t h = 0;

    void * ptr = calloc(1, 2*size);
    assert(ptr);
    uintptr_t p = (uintptr_t) ptr;

    xkrt_team_t * team = runtime.team_get_any(1 << XKRT_DRIVER_TYPE_HOST);
    assert(team);

    # if 0
    runtime.memory_register_async(team, ptr, size, nchunks);
    runtime.task_wait();
    # else
    xkrt_memory_register(&runtime, ptr, size);
    # endif

    runtime.distribute_async(XKRT_DISTRIBUTION_TYPE_CYCLIC1D, ptr, 2*size, 2*size, h);
    // runtime.distribute_async(XKRT_DISTRIBUTION_TYPE_CYCLIC1D, (void *) (p+0)   , size, size, h);
    // runtime.distribute_async(XKRT_DISTRIBUTION_TYPE_CYCLIC1D, (void *) (p+size), size, size, h);
    runtime.task_wait();

    # if 0
    runtime.memory_unregister_async(team, ptr, size, nchunks);
    runtime.task_wait();
    # else
    xkrt_memory_unregister(&runtime, ptr, size);
    # endif

    assert(xkrt_deinit(&runtime) == 0);

    return 0;
}
