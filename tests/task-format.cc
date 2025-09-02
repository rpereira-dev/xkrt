/* ************************************************************************** */
/*                                                                            */
/*   task-format.cc                                               .-*-.       */
/*                                                              .'* *.'       */
/*   Created: 2024/12/20 15:07:55 by Romain PEREIRA          __/_*_*(_        */
/*   Updated: 2025/08/23 00:15:27 by Romain PEREIRA         / _______ \       */
/*                                                          \_)     (_/       */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/*   Author: Thierry GAUTIER <thierry.gautier@inrialpes.fr>                   */
/*   Author: Romain PEREIRA <romain.pereira@inria.fr>                         */
/*                                                                            */
/*   Copyright: see AUTHORS                                                   */
/*                                                                            */
/* ************************************************************************** */

# include <xkrt/runtime.h>
# include <xkrt/task/task-format.h>

# include <assert.h>
# include <string.h>

XKRT_NAMESPACE_USE;

int
main(void)
{
    runtime_t runtime;
    assert(runtime.init() == 0);

    // create an empty task format
    task_format_id_t EMPTY;
    {
        task_format_t format;
        memset(&format, 0, sizeof(task_format_t));
        EMPTY = task_format_create(&(runtime.formats.list), &format);
    }
    assert(EMPTY);

    assert(runtime.deinit() == 0);

    return 0;
}
