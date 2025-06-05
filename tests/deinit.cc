/* ************************************************************************** */
/*                                                                            */
/*   deinit.cc                                                    .-*-.       */
/*                                                              .'* *.'       */
/*   Created: 2025/01/30 00:16:18 by Romain Pereira          __/_*_*(_        */
/*   Updated: 2025/06/03 18:13:13 by Romain PEREIRA         / _______ \       */
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
# include <assert.h>

int
main(void)
{
    xkrt_runtime_t runtime;

    assert(xkrt_init(&runtime) == 0);
    assert(xkrt_deinit(&runtime) == 0);

    return 0;
}
