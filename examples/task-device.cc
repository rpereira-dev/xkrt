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

int
main(int argc, char ** argv)
{
    runtime_t runtime;

    runtime.init();

    // create a vector 'x' on the host memory
    using TYPE = double;
    constexpr int N = 64;
    TYPE x[N];
    memset(x, 0, sizeof(x));

    // spawn a task that reads 'x' on device 1
    constexpr device_global_id_t gpu_device_global_id = 1;
    runtime.task_spawn<1>(

        // target device
        gpu_device_global_id,

        // set task accesses
        [&x] (task_t * task, access_t * accesses) {
            new (accesses + 0) access_t(task, x, N, sizeof(TYPE), ACCESS_MODE_R);
        },

        // task routine, executes once all accesses are coherent on the scheduled device
        [] (runtime_t * runtime, device_t * device, task_t * task) {

            access_t * accesses = TASK_ACCESSES(task);
            uintptr_t x_dev = (accesses + 0)->device_view.addr;

            // Push a kernel launch command to a queue of the device
            // This call is non-blocking
            runtime->task_detachable_kernel_launch(
                device,
                task,
                 [] (
                    runtime_t * runtime,
                    device_t * device,
                    task_t * task,
                    queue_t * queue,
                    command_t * command,
                    queue_command_list_counter_t event
                ) {
                    driver_t * driver = runtime->driver_get(device->driver_type);
                    const driver_module_fn_t * fn = nullptr; // TODO: the kernel function (e.g., a CUfunction)
                    const unsigned int gx = 1, gy = 1, gz = 1;
                    const unsigned int bx = 1, by = 1, bz = 1;
                    const unsigned int shared_memory_bytes = 0;
                    void * args      = NULL;
                    size_t args_size = 0;
                    driver->f_kernel_launch(queue, event, fn, gx, gy, gz, bx, by, bz, shared_memory_bytes, args, args_size);
                }
            ); /* kernel launcher */

            // The task returns, but only complete once the events associated
            // to the command is fulfilled - that is, on kernel completion

        } /* task routine */
    );

    runtime.deinit();

    return 0;
}
