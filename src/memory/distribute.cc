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
# include <xkrt/xkrt.h>
# include <xkrt/memory/access/blas/dependency-tree.hpp>

# include <math.h>

//////////////////
// DISTRIBUTION //
//////////////////

extern "C"
void
xkrt_distribution_init(
    xkrt_distribution_t * d,
    xkrt_distribution_type_t type,
    size_t count,
    size_t m, size_t n,
    size_t mb, size_t nb
) {
    assert(count);

    d->type  = type;
    d->count = count;
    d->m     = m;
    d->n     = n;
    d->mb    = mb;
    d->nb    = nb;
    d->mt = NUM_OF_TILES(m, mb);
    d->nt = NUM_OF_TILES(n, nb);

    switch (type)
    {
        case (XKRT_DISTRIBUTION_TYPE_CYCLIC2D):
        {
            // nothing to do
            break ;
        }

        case (XKRT_DISTRIBUTION_TYPE_CYCLIC2DBLOCK):
        {

            /* find the most square decomposition of count in d->gm x d->gn */
            d->blkm = 1;
            d->blkn = 1;
            d->gm = (size_t) sqrt((double) count);
            d->gn = d->gm;
            if (d->gm == 0)
            {
                d->gm = 1;
                d->gn = count;
            }
            else
            {
                size_t g;
                for (g = d->gm + 1; g > 0; --g)
                    if (count % g == 0)
                        break;

                # pragma message(TODO "Why this inverts with the previous case ?")
                if (g == 0)
                {
                    d->gm = count; // = 1
                    d->gn = 1;     // = count
                }
                else
                {
                    d->gm = g;
                    d->gn = count / g;
                }
            }

            d->blkm = d->blkm;
            d->blkn = d->blkn;
            d->gm   = d->gm;
            d->gn   = d->gn;

            break ;
        }

        default:
        LOGGER_FATAL("Not implemented");
    }
}

extern "C"
xkrt_device_global_id_t
xkrt_distribution_get(
    xkrt_distribution_t * d,
    size_t tm, size_t tn
) {
    assert(tm < d->mt);
    assert(tn < d->nt);

    switch (d->type)
    {
        /**
         * example on 4 gpus
         *  1 2 3 4
         *  1 2 3 4
         *  1 2 3 4
         *  1 2 3 4
         */
        case (XKRT_DISTRIBUTION_TYPE_CYCLIC2D):
            return 1 + (xkrt_device_global_id_t) ((tm * d->nt + tn) % d->count);

        /**
         * example on 4 gpus
         *  1 2 1 2
         *  3 4 3 4
         *  1 2 1 2
         *  3 4 3 4
         */
        case (XKRT_DISTRIBUTION_TYPE_CYCLIC2DBLOCK):
            return 1 + (xkrt_device_global_id_t) (
                    (
                     ((tm / d->blkm) % d->gm) * d->gn +
                      (tn / d->blkn) % d->gn)
                        % d->count
                    )
                ;
        default:
            LOGGER_FATAL("Not implemented");
    }
}

////////////////
// DISTRIBUTE //
////////////////

static inline void
xkrt_distribute_submit(
    xkrt_runtime_t * runtime,
    matrix_order_t order,
    void * ptr, size_t ld,
    size_t m, size_t n,
    size_t mb, size_t nb,
    size_t sizeof_type,
    size_t hx, size_t hy,
    size_t tm, size_t tn,
    xkrt_device_global_id_t device_global_id
) {
    xkrt_thread_t * thread = xkrt_thread_t::get_tls();
    assert(thread);

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
    {
        const ssize_t  x = tm * mb;
        const ssize_t  y = tn * nb;
        const ssize_t x0 = MAX(x-(ssize_t)hx, 0);
        const ssize_t y0 = MAX(y-(ssize_t)hy, 0);
        const ssize_t x1 = MIN(x+mb+hx, m);
        const ssize_t y1 = MIN(y+nb+hy, n);
        const  size_t sx = x1 - x0;
        const  size_t sy = y1 - y0;
        new(accesses + 0) access_t(task, order, ptr, ld, x0, y0, sx, sy, sizeof_type, ACCESS_MODE_RW);
    }
    thread->resolve<AC>(task, accesses);
    # undef AC

    #ifndef NDEBUG
    snprintf(task->label, sizeof(task->label), "distribute_cyclic_2d_async");
    #endif /* NDEBUG */

    runtime->task_commit(task);
}

extern "C"
void
xkrt_distribute_async(
    xkrt_runtime_t * runtime,
    xkrt_distribution_type_t type,
    matrix_order_t order,
    void * ptr, size_t ld,
    size_t m, size_t n,
    size_t mb, size_t nb,
    size_t sizeof_type,
    size_t hx, size_t hy
) {
    const int ngpus = runtime->drivers.devices.n - 1;
    assert(ngpus);

    xkrt_distribution_t d;
    xkrt_distribution_init(&d, type, ngpus, m, n, mb, nb);

    for (size_t tm = 0; tm < d.mt; ++tm)
        for (size_t tn = 0; tn < d.nt; ++tn)
            xkrt_distribute_submit(runtime, order, ptr, ld, m, n, mb, nb, sizeof_type, hx, hy, tm, tn, xkrt_distribution_get(&d, tm, tn));
}
