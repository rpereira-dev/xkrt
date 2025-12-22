/*
** Copyright 2024,2025 INRIA
**
** Contributors :
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

# include <xkrt/runtime.h>

XKRT_NAMESPACE_USE;

# define BYTE unsigned char

static void
func(task_t * task)
{
    access_t * accesses = TASK_ACCESSES(task);
    assert(accesses);
    BYTE * x = (BYTE *) (accesses + 0)->host_view.addr;
    BYTE * y = (BYTE *) (accesses + 1)->host_view.addr;
    BYTE * z = (BYTE *) (accesses + 2)->host_view.addr;
    *z = *x + *y;
}

static void
check(BYTE * x, BYTE * y, BYTE * z, int i)
{
    assert(z[i] == (BYTE) (x[i] + y[i]));
}

int
main(int argc, char ** argv)
{
    runtime_t runtime;

    runtime.init();

    // create a vector 'x' on the host memory
    int N = 32768;
    uintptr_t x = (uintptr_t) calloc(1, N);
    uintptr_t y = (uintptr_t) calloc(1, N);
    uintptr_t z = (uintptr_t) calloc(1, N);
    srand(time(NULL));
    for (size_t i = 0; i < N ; ++i)
    {
        ((BYTE *) x)[i] = rand() % 256;
        ((BYTE *) y)[i] = rand() % 256;
        ((BYTE *) z)[i] = rand() % 256;
    }

    // spawn a task that reads 'x' on device 0
    uint64_t t0 = get_nanotime();
    for (int i = 0 ; i < N ; ++i)
    {
        runtime.task_spawn<3>(

            // set task accesses
            [&x, &y, &z, &i] (task_t * task, access_t * accesses) {
                new (accesses + 0) access_t(task, x+i, x+(i+1), ACCESS_MODE_R);
                new (accesses + 1) access_t(task, y+i, y+(i+1), ACCESS_MODE_R);
                new (accesses + 2) access_t(task, z+i, z+(i+1), ACCESS_MODE_W);
            },

            // task routine, executes once all accesses are coherent on the scheduled device
            [] (runtime_t * runtime, device_t * device, task_t * task) {
                func(task);
            }
        );
    }
    runtime.task_wait();
    uint64_t tf = get_nanotime();
    printf("Took %lf s\n", (tf - t0) / 1.e9);

    for (int i = 0 ; i < N ; ++i)
        check((BYTE *) x, (BYTE *) y, (BYTE *) z, i);
    printf("Correct\n");

    runtime.deinit();

    return 0;
}
