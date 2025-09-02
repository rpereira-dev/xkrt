/* ************************************************************************** */
/*                                                                            */
/*   sync.cc                                                      .-*-.       */
/*                                                              .'* *.'       */
/*   Created: 2025/01/30 00:16:18 by Romain Pereira          __/_*_*(_        */
/*   Updated: 2025/08/23 00:12:34 by Romain PEREIRA         / _______ \       */
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

int
main(void)
{
    runtime_t runtime;
    assert(runtime.init() == 0);
    runtime.task_wait();
    assert(runtime.deinit() == 0);

    return 0;
}
