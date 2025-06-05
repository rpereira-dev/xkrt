/*
** Copyright 2024,2025 INRIA
**
** Contributors :
** Thierry Gautier, thierry.gautier@inrialpes.fr
** Joao Lima joao.lima@inf.ufsm.br
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

extern "C"
void
xkrt_coherency_replicate_2D_async(
    xkrt_runtime_t * runtime,
    matrix_order_t order,
    void * ptr, size_t ld,
    size_t m, size_t n,
    size_t sizeof_type
) {
    assert(runtime->drivers.devices.n >= 2);

    xkrt_thread_t * thread = xkrt_thread_t::get_tls();
    assert(thread);

    for (xkrt_device_global_id_t device_global_id = 1 ; device_global_id < runtime->drivers.devices.n ; ++device_global_id)
    {
        # define AC 1
        constexpr task_flag_bitfield_t flags = TASK_FLAG_DEPENDENT | TASK_FLAG_DEVICE;
        constexpr size_t task_size = task_compute_size(flags, AC);

        task_t * task = thread->allocate_task(task_size);
        new(task) task_t(TASK_FORMAT_NULL, flags);

        task_dep_info_t * dep = TASK_DEP_INFO(task);
        new (dep) task_dep_info_t(AC);

        task_dev_info_t * dev = TASK_DEV_INFO(task);
        new (dev) task_dev_info_t(device_global_id, UNSPECIFIED_TASK_ACCESS);

        access_t * accesses = TASK_ACCESSES(task);
        new(accesses + 0) access_t(task, order, ptr, ld, m, n, sizeof_type, ACCESS_MODE_R);

        thread->resolve<AC>(task, accesses);
        # undef AC

        #ifndef NDEBUG
        snprintf(task->label, sizeof(task->label), "replicate_cyclic_2d_async");
        #endif /* NDEBUG */

        runtime->task_commit(task);
    }
}
