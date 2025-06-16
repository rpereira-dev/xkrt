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
# include <xkrt/memory/access/blas/dependency-tree.hpp>
# include <xkrt/memory/access/blas/memory-tree.hpp>
# include <xkrt/memory/access/interval/dependency-tree.hpp>
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
    assert(access->type >= 0 && access->type < ACCESS_TYPE_MAX);

    task_dom_info_t * dom = TASK_DOM_INFO(task);
    assert(dom);

    /* if not found, create a new memory coherency controller dependending on
     * the access type */
    MemoryCoherencyController * mcc;
    switch (access->type)
    {
        case (ACCESS_TYPE_POINT):
        {
            mcc = NULL;
            break ;
        }

        case (ACCESS_TYPE_INTERVAL):
        {
            // TODO: blas memory coherency tree of ;d SIZE_MAX
            assert(access->host_view.ld == SIZE_MAX);
            if (dom->mccs.interval == NULL)
            {
                mcc = new BLASMemoryTree(
                    runtime,
                    SIZE_MAX,
                    1,
                    runtime->conf.merge_transfers
                );
            }
            mcc = dom->mccs.interval;
            break ;
        }

        case (ACCESS_TYPE_BLAS_MATRIX):
        {
            // TODO : have this list dynamically switch to a hashmap ift here
            // is too many differnet matrices LD

            /* find previous mcc for that ld */
            for (MemoryCoherencyController * mcc : dom->mccs.blas)
            {
                BLASMemoryTree * memtree = (BLASMemoryTree *) mcc;
                if (memtree->ld == access->host_view.ld &&
                        memtree->sizeof_type == access->host_view.sizeof_type)
                    return mcc;
            }

            /* else insert a new one */
            mcc = new BLASMemoryTree(
                runtime,
                access->host_view.ld,
                access->host_view.sizeof_type,
                runtime->conf.merge_transfers
            );
            dom->mccs.blas.push_back(mcc);

            # if XKRT_MEMORY_REGISTER_OVERFLOW_PROTECTION
            /* insert regions that represents registered memory segment, to
             * enforce the split in multiple copies */
            if (runtime->conf.protect_registered_memory_overflow)
                for (const auto & [ptr, size] : runtime->registered_memory)
                    ((BLASMemoryTree *) mcc)->registered(ptr, size);
            # endif /* XKRT_MEMORY_REGISTER_OVERFLOW_PROTECTION */

            LOGGER_DEBUG("Created a new memory tree with (ld, sizeof(type), merge) = (%lu, %lu, %s)",
                    access->host_view.ld, access->host_view.sizeof_type, runtime->conf.merge_transfers ? "true" : "false");

            break ;
        }

        default:
        {
            LOGGER_FATAL("Tried to run coherency controller on an unsupported access");
            break ;
        }
    }

    return mcc;
}

/* retrieve the dependency domain of the given blas matrix */
DependencyDomain *
task_get_dependency_domain_blas_matrix(
    task_t * task,
    size_t ld,
    size_t sizeof_type
) {
    assert(task);
    assert(task->flags & TASK_FLAG_DOMAIN);

    task_dom_info_t * dom = TASK_DOM_INFO(task);
    assert(dom);

    /* find previous deptree for that ld */
    for (DependencyDomain * domain : dom->deps.blas)
    {
        BLASDependencyTree * deptree = (BLASDependencyTree *) domain;
        if (deptree->ld == ld && deptree->sizeof_type == sizeof_type)
            return deptree;
    }

    /* if none, create a new one */
    BLASDependencyTree * deptree = new BLASDependencyTree(ld, sizeof_type);
    dom->deps.blas.push_back(deptree);

    /* push each uncompleted tasks from the interval dependency tree,
     * so dependencies between previously spawned interval accesses and
     * future blas matrix accesses are detected */
    IntervalDependencyTree * inttree = (IntervalDependencyTree *) dom->deps.interval;
    if (inttree)
    {
        for (auto it = inttree->accesses.begin() ; it != inttree->accesses.end() ; )
        {
            access_t * access = *it;
            assert(access->type == ACCESS_TYPE_INTERVAL);

            if (access->task && access->task->state.value == TASK_STATE_COMPLETED)
            {
                it = inttree->accesses.erase(it);
            }
            else
            {
                deptree->resolve_interval(access);
                ++it;
            }
        }
    }

    return deptree;
}

/**
 * Retrieve or (insert and return) the dependency domain of the passed task for the given access
 */
void
task_dependency_resolve(
    task_t * task,
    access_t * access
) {
    assert(task);
    assert(task->flags & TASK_FLAG_DOMAIN);
    assert(access->type >= 0 && access->type < ACCESS_TYPE_MAX);

    task_dom_info_t * dom = TASK_DOM_INFO(task);
    assert(dom);

    /* create a new domain */
    switch (access->type)
    {
        case (ACCESS_TYPE_POINT):
        {
            if (dom->deps.point == NULL)
                dom->deps.point = new DependencyMap();
            dom->deps.point->resolve<1>(access);
            break ;
        }

        case (ACCESS_TYPE_INTERVAL):
        {
            if (dom->deps.interval == NULL)
                dom->deps.interval = new IntervalDependencyTree();
            dom->deps.interval->resolve<1>(access);

            for (DependencyDomain * domain : dom->deps.blas)
            {
                BLASDependencyTree * deptree = (BLASDependencyTree *) domain;
                deptree->resolve_interval(access);
            }
            break ;
        }

        case (ACCESS_TYPE_BLAS_MATRIX):
        {
            BLASDependencyTree * deptree = (BLASDependencyTree *) task_get_dependency_domain_blas_matrix(task, access->host_view.ld, access->host_view.sizeof_type);
            assert(deptree);

            deptree->resolve<1>(access);
            break ;
        }

        default:
            LOGGER_FATAL("Tried to run a dependency domain on an unsupported access");
    }
}
