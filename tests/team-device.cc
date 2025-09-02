/* ************************************************************************** */
/*                                                                            */
/*   team-device.cc                                               .-*-.       */
/*                                                              .'* *.'       */
/*   Created: 2025/03/04 05:42:49 by Romain PEREIRA          __/_*_*(_        */
/*   Updated: 2025/08/23 00:20:51 by Romain PEREIRA         / _______ \       */
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
# include <assert.h>

XKRT_NAMESPACE_USE;

static runtime_t runtime;
static volatile int run_for_device[XKRT_DEVICES_MAX];

static void *
run(team_t * team, thread_t * thread)
{
    assert(thread->tid >= 0);
    assert(thread->tid < runtime.drivers.devices.n);
    run_for_device[thread->tid] = 1;
    return NULL;
}

int
main(void)
{
    assert(runtime.init() == 0);

    team_t team;
    team.desc.routine           = run;
    team.desc.args              = NULL;
    team.desc.nthreads          = runtime.drivers.devices.n;
    team.desc.binding.mode      = XKRT_TEAM_BINDING_MODE_COMPACT;
    team.desc.binding.places    = XKRT_TEAM_BINDING_PLACES_DEVICE;
    team.desc.binding.flags     = XKRT_TEAM_BINDING_FLAG_NONE;

    runtime.team_create(&team);
    runtime.team_join(&team);

    for (int i = 0 ; i < runtime.drivers.devices.n ; ++i)
        assert(run_for_device[i] == 1);

    assert(runtime.deinit() == 0);

    return 0;
}
