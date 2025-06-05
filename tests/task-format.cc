/* ************************************************************************** */
/*                                                                            */
/*   task-format.cc                                               .-*-.       */
/*                                                              .'* *.'       */
/*   Created: 2024/12/20 15:07:55 by Romain PEREIRA          __/_*_*(_        */
/*   Updated: 2025/06/03 18:13:44 by Romain PEREIRA         / _______ \       */
/*                                                          \_)     (_/       */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/*   Author: Thierry GAUTIER <thierry.gautier@inrialpes.fr>                   */
/*   Author: Romain PEREIRA <romain.pereira@inria.fr>                         */
/*                                                                            */
/*   Copyright: see AUTHORS                                                   */
/*                                                                            */
/* ************************************************************************** */

# include <xkrt/xkrt.h>
# include <xkrt/task/task-format.h>

# include <assert.h>
# include <string.h>

int
main(void)
{
    xkrt_runtime_t runtime;
    assert(xkrt_init(&runtime) == 0);

    // create an empty task format
    task_format_id_t EMPTY;
    {
        task_format_t format;
        memset(&format, 0, sizeof(task_format_t));
        EMPTY = task_format_create(&(runtime.formats.list), &format);
    }
    assert(EMPTY);

    assert(xkrt_deinit(&runtime) == 0);

    return 0;
}
