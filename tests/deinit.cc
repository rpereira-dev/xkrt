/* ************************************************************************** */
/*                                                                            */
/*   deinit.cc                                                    .-*-.       */
/*                                                              .'* *.'       */
/*   Created: 2025/01/30 00:16:18 by Romain Pereira          __/_*_*(_        */
/*   Updated: 2025/08/22 23:36:57 by Romain PEREIRA         / _______ \       */
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

XKRT_NAMESPACE_USE;

int
main(void)
{
    runtime_t runtime;

    assert(runtime.init() == 0);
    assert(runtime.deinit() == 0);

    return 0;
}
