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

# include <xkrt/xkrt.h>
# include <xkrt/runtime.h>

constexpr int                         ac = 1;
constexpr task_flag_bitfield_t     flags = TASK_FLAG_DEPENDENT;
constexpr               size_t task_size = task_compute_size(flags, ac);
constexpr               size_t args_size = 0;

int
xkrt_runtime_t::memory_register_async(
    xkrt_team_t * team,
    void * ptr,
    const size_t chunk_size,
    int n
) {
    LOGGER_FATAL("Not implemented");

    xkrt_thread_t * tls = xkrt_thread_t::get_tls();

    // null format, the registration occurs during the fetching/fetched state
    const task_format_id_t fmtid = TASK_FORMAT_NULL;

    for (int i = 0 ; i < n ; ++i)
    {
        // inserts the interval in the tree to ensure they exist
        const uintptr_t a = ((const uintptr_t) ptr) + (i+0) * chunk_size;
        const uintptr_t b = ((const uintptr_t) ptr) + (i+1) * chunk_size;

        // create a task that will register/pin/unpin the memory
        task_t * task = tls->allocate_task(task_size + args_size);
        new(task) task_t(fmtid, flags);

        #ifndef NDEBUG
        snprintf(task->label, sizeof(task->label), "memory_register_async");
        #endif

        task_dep_info_t * dep = TASK_DEP_INFO(task);
        new (dep) task_dep_info_t(ac);

        access_t * accesses = TASK_ACCESSES(task, flags);
        new(accesses + 0) access_t(task, a, b, ACCESS_MODE_PIN, ACCESS_CONCURRENCY_COMMUTATIVE);

        tls->commit(task, xkrt_team_task_enqueue, this, team);
    }

    return 0;
}
