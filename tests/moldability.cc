/*
** Copyright 2024,2025 INRIA
**
** Contributors :
** Thierry Gautier, thierry.gautier@inrialpes.fr
** Romain PEREIRA, romain.pereira@inria.fr + rpereira@anl.gov
**
** This software is a computer program whose purpose is to execute
** blas subroutines on multi-GPUs system.
**
** This software is governed by the CeCILL-C license under French law and
** abiding by the rules of distribution of free software.  You can  use,
** modify and/ or redistribute the software under the terms of the CeCILL-C
** license as circulated by CEA, CNRS and INRIA at the following URL
** "http://www.cecill.info".

** As a counterpart to the access to the source code and  rights to copy,
** modify and redistribute granted by the license, users are provided only
** with a limited warranty  and the software's author,  the holder of the
** economic rights,  and the successive licensors  have only  limited
** liability.

** In this respect, the user's attention is drawn to the risks associated
** with loading,  using,  modifying and/or developing or reproducing the
** software by the user in light of its specific status of free software,
** that may mean  that it is complicated to manipulate,  and  that  also
** therefore means  that it is reserved for developers  and  experienced
** professionals having in-depth computer knowledge. Users are therefore
** encouraged to load and test the software's suitability as regards their
** requirements in conditions enabling the security of their systems and/or
** data to be ensured and,  more generally, to use and operate it in the
** same conditions as regards security.

** The fact that you are presently reading this means that you have had
** knowledge of the CeCILL-C license and that you accept its terms.
**/

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
            return access->region.interval.segment[0].length() == size;
        },

        // routine
        [] (runtime_t * runtime, device_t * device, task_t * task) {
            access_t * accesses = TASK_ACCESSES(task);
            const access_t * access = accesses + 0;
            const uintptr_t a = access->region.interval.segment[0].a;
            const uintptr_t b = access->region.interval.segment[0].b;
            LOGGER_INFO("Running [%lu, %lu]", a, b);
        }
    );

    /* wait for all tasks completion */
    runtime.task_wait();

    assert(runtime.deinit() == 0);

    return 0;
}
