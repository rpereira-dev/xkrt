/* ************************************************************************** */
/*                                                                            */
/*   touch-async.cc                                               .-*-.       */
/*                                                              .'* *.'       */
/*   Created: 2025/03/05 05:19:56 by Romain PEREIRA          __/_*_*(_        */
/*   Updated: 2025/06/03 18:14:04 by Romain PEREIRA         / _______ \       */
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

static void *
main_team(xkrt_team_t * team, xkrt_thread_t * thread)
{
    if (thread->tid == 0)
    {
        ptr = malloc(chunk_size * nchunks);
        runtime.memory_touch_async(ptr, chunk_size, nchunks);
    }
    return NULL;
}

static int
get_ncpus(void)
{
    // Try to get the number of CPU cores from topology
    int depth = hwloc_get_type_depth(runtime.topology, HWLOC_OBJ_CORE);
    int r = hwloc_get_nbobjs_by_depth(runtime.topology, depth);
    return r;
}

int
main(void)
{
    assert(xkrt_init(&runtime) == 0);

    xkrt_team_t team = {
        .desc = {
            .routine = main_team,
            .args = NULL,
            .nthreads = get_ncpus(),
            .binding = {
                .mode = XKRT_TEAM_BINDING_MODE_COMPACT,
                .places = XKRT_TEAM_BINDING_PLACES_CORE,
                .flags = XKRT_TEAM_BINDING_FLAG_NONE
            }
        }
    };

    runtime.team_create(&team);
    runtime.team_join(&team);

    assert(xkrt_deinit(&runtime) == 0);

    return 0;
}
