/* ************************************************************************** */
/*                                                                            */
/*   moldability.cc                                               .-*-.       */
/*                                                              .'* *.'       */
/*   Created: 2025/02/11 14:59:33 by Romain PEREIRA          __/_*_*(_        */
/*   Updated: 2025/08/23 00:11:34 by Romain PEREIRA         / _______ \       */
/*                                                          \_)     (_/       */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/*   Author: Romain PEREIRA <rpereira@anl.gov>                                */
/*                                                                            */
/*   Copyright: see AUTHORS                                                   */
/*                                                                            */
/* ************************************************************************** */

# include <new>

# include <xkrt/runtime.h>
# include <xkrt/logger/logger.h>
# include <xkrt/logger/metric.h>

XKRT_NAMESPACE_USE;

int
main(void)
{
    runtime_t runtime;

    assert(runtime.init() == 0);

    // buffer
    # if 0
    const size_t size = 1LU * 1024 * 1024 * 1024;
    unsigned char * buffer = (unsigned char *) malloc(size);
    assert(buffer);
    # else
    const size_t size = 64;
    unsigned char * buffer = NULL;
    # endif

    // spawn a new task
    runtime.team_task_spawn<1>(

        // use default host team
        runtime.team_get(XKRT_DRIVER_TYPE_HOST),

        // set accesses
        [&buffer] (task_t * task, access_t * accesses) {
            access_t * access = accesses + 0;
            const uintptr_t a = (const uintptr_t) buffer;
            const uintptr_t b = a + size;
            new (access) access_t(task, a, b, ACCESS_MODE_R);
        },

        // split condition
        [&size] (task_t * task, access_t * accesses) {
            const access_t * access = accesses + 0;
            // return access->segment[0].length() >= size / 16;
            return access->segment[0].length() == size;
        },

        // routine
        [] (task_t * task) {
            access_t * accesses = TASK_ACCESSES(task);
            const access_t * access = accesses + 0;
            const uintptr_t a = access->segment[0].a;
            const uintptr_t b = access->segment[0].b;
            LOGGER_INFO("Running [%lu, %lu]", a, b);
        }
    );

    /* wait for all tasks completion */
    runtime.task_wait();

    assert(runtime.deinit() == 0);

    return 0;
}
