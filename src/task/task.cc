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
# include <xkrt/memory/access/blas/region/dependency-tree.hpp>
# include <xkrt/memory/access/blas/region/memory-tree.hpp>
# include <xkrt/memory/access/point/dependency-map.hpp>

/**
 * Retrieve or (insert and return) the memory controller of the passed task for the given access
 */
MemoryCoherencyController *
task_get_memory_controller(
    xkrt_runtime_t * runtime,
    task_t * task,
    const access_t * access
) {
    assert(task);
    assert(task->flags & TASK_FLAG_DOMAIN);

    task_dom_info_t * dom = TASK_DOM_INFO(task);
    assert(dom);

    SPINLOCK_LOCK(dom->mccs_lock);

    /* find previous mcc for that ld */
    for (MemoryCoherencyController * mcc : dom->mccs)
    {
        if (mcc->can_resolve(access))
        {
            SPINLOCK_UNLOCK(dom->mccs_lock);
            return mcc;
        }
    }

    LOGGER_DEBUG("Created a new memory tree with (ld, sizeof(type), merge) = (%lu, %lu, %s)",
            access->host_view.ld, access->host_view.sizeof_type, runtime->conf.merge_transfers ? "true" : "false");

    /* if not found, create a new memory coherency controller dependending on
     * the access type */
    MemoryCoherencyController * mcc;
    switch (access->type)
    {
        case (ACCESS_TYPE_BLAS_MATRIX):
        {
            mcc = new MemoryTree(
                runtime,
                access->host_view.ld,
                access->host_view.sizeof_type,
                runtime->conf.merge_transfers
            );
            break ;
        }

        case (ACCESS_TYPE_POINT):
        {
            mcc = NULL;
            break ;
        }

        default:
        {
            LOGGER_FATAL("Tried to run coherency controller on an unsupported access");
            break ;
        }
    }

    if (mcc)
    {
        assert(mcc->can_resolve(access));
        dom->mccs.push_back(mcc);
    }

    SPINLOCK_UNLOCK(dom->mccs_lock);

    return mcc;
}

/**
 * Retrieve or (insert and return) the dependency domain of the passed task for the given access
 */
DependencyDomain *
task_get_dependency_domain(
    task_t * task,
    const access_t * access
) {
    assert(task);
    assert(task->flags & TASK_FLAG_DOMAIN);

    xkrt_thread_t * thread = xkrt_thread_t::get_tls();
    assert(thread);

    task_dom_info_t * dom = TASK_DOM_INFO(thread->current_task);
    assert(dom);

    /* find previous deptree for that ld */
    for (DependencyDomain * domain : dom->deps)
        if (domain->can_resolve(access))
            return domain;

    /* create a new domain */
    DependencyDomain * domain;
    switch (access->type)
    {
        case (ACCESS_TYPE_BLAS_MATRIX):
        {
            domain = new DependencyTree(access->host_view.ld, access->host_view.sizeof_type);
            break ;
        }

        case (ACCESS_TYPE_POINT):
        {
            domain = new DependencyMap();
            break ;
        }

        default:
        {
            LOGGER_FATAL("Tried to run a dependency domain on an unsupported access");
        }
    }
    dom->deps.push_back(domain);

    return domain;
}
