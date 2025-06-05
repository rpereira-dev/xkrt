/*
** Copyright 2024,2025 INRIA
**
** Contributors :
** Romain PEREIRA, romain.pereira@inria.fr + rpereira@anl.gov
** Thierry Gautier, thierry.gautier@inrialpes.fr
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

#ifndef __ACCESS_HPP__
# define __ACCESS_HPP__

# include <xkrt/memory/access/mode.h>
# include <xkrt/memory/access/common/hyperrect.hpp>
# include <xkrt/memory/view.hpp>

# include <vector>

/**
 *  We assume col major, dim 0 is for rows; dim 1 is for cols.
 *  These variables controls how a matrix (A, m, n, ld) is converted to an
 *  hyperrect for the xktree.
 *
 *  e.g the matrix (0, 4, 8, 8) can whether be represented as the hyperrect
 *      (0:4, 0:8) - if ACCESS_BLAS_ROW_DIM == 0
 *   or (0:8, 0:4) - if ACCESS_BLAS_ROW_DIM == 1
 */
# define ACCESS_BLAS_ROW_DIM 0
# define ACCESS_BLAS_COL_DIM (1 - ACCESS_BLAS_ROW_DIM)

// task and accesses depends to one another, breaking chicken/egg problem here
struct task_t;

template<int K>
static inline void
memory_view_from_hyperrect(
    memory_view_t & view,
    const KHyperrect<K> & h,
    const size_t ld,
    const size_t sizeof_type
) {
    static_assert(K == 2);

    const INTERVAL_TYPE_T       x = h[ACCESS_BLAS_ROW_DIM].a;
    const INTERVAL_DIFF_TYPE_T dx = h[ACCESS_BLAS_ROW_DIM].length();
    const INTERVAL_TYPE_T       y = h[ACCESS_BLAS_COL_DIM].a;
    const INTERVAL_DIFF_TYPE_T dy = h[ACCESS_BLAS_COL_DIM].length();
    assert(dx > 0);
    assert(dy > 0);

    view.order         = MATRIX_COLMAJOR;
    view.addr          = x + y * ld * sizeof_type;
    view.ld            = ld;
    view.m             = dx / sizeof_type;
    view.n             = dy;
    view.sizeof_type   = sizeof_type;

    // accesses must be aligned on sizeof(type)
    assert((INTERVAL_DIFF_TYPE_T) (view.m * sizeof_type) == dx);
}

template<int K>
static inline void
memory_view_from_rects(
    memory_view_t & view,
    const KHyperrect<K> & h0,
    const KHyperrect<K> & h1,
    const size_t ld,
    const size_t sizeof_type
) {
    const INTERVAL_DIFF_TYPE_T x0 = (INTERVAL_DIFF_TYPE_T) h0[ACCESS_BLAS_ROW_DIM].a;
    const INTERVAL_DIFF_TYPE_T xf = (INTERVAL_DIFF_TYPE_T) h1[ACCESS_BLAS_ROW_DIM].b;
    const INTERVAL_DIFF_TYPE_T y0 = (INTERVAL_DIFF_TYPE_T) h0[ACCESS_BLAS_COL_DIM].a;
    const INTERVAL_DIFF_TYPE_T yf = (INTERVAL_DIFF_TYPE_T) h1[ACCESS_BLAS_COL_DIM].b;
    assert(0 <= x0 && x0 <= (INTERVAL_DIFF_TYPE_T) (ld * sizeof_type));
    assert(0 <= xf && xf <= (INTERVAL_DIFF_TYPE_T) (ld * sizeof_type));
    assert(y0 < yf);

    INTERVAL_DIFF_TYPE_T n = yf - y0;
    INTERVAL_DIFF_TYPE_T m = xf - x0;
    if (m < 0)
    {
        m += ld * sizeof_type;
        n -= 1;
    }
    m = m / sizeof_type;

    view.order        = MATRIX_COLMAJOR;
    view.addr         = x0 + y0 * ld * sizeof_type;
    view.ld           = ld;
    view.m            = (size_t) m;
    view.n            = (size_t) n;
    view.sizeof_type  = sizeof_type;
}

template<int K>
static inline void
memory_view_to_rects(
    const memory_view_t & view,
    KHyperrect<K> (& rects) [2]
) {
    static_assert(K == 2);

    const size_t  A = view.begin_addr();
    const size_t ld = view.ld;
    const size_t  m = view.m;
    const size_t  n = view.n;
    const size_t  s = view.sizeof_type;

    # if ACCESS_FORCE_ALIGNMENT
    assert((A % (ld * s)) + (m * s) <= ld * s);
    # endif /* ACCESS_FORCE_ALIGNMENT */

    // only 1 rect is needed
    if ((A % (ld * s)) + m * s <= ld * s)
    {
        /**
         *        ^               y0       y1
         *        |         |  .   .   .   .   .
         *        |      x0 |  .   x   x   x   .
         *   ld.s |         |  .   x   x   x   .
         *        |         |  .   x   x   x   .
         *        |      x1 |  .   x   x   x   .
         *        v         v  .   .   .   .   .
         */
        const uintptr_t x0 = A % (ld * s);
        const uintptr_t x1 = x0 + m * s;
        const uintptr_t y0 = A / (ld * s);
        const uintptr_t y1 = y0 + n;

        {
            Interval list[2];
            list[ACCESS_BLAS_ROW_DIM] = Interval(x0, x1);
            list[ACCESS_BLAS_COL_DIM] = Interval(y0, y1);
            rects[0].set_list(list);
            assert(!rects[0].is_empty());
        }

        assert(rects[1].is_empty());
    }
    // 2 rects are needed
    else
    {
        /**
         *                     y2          y3
         *      x2   |  .   .   x   x   x   x   .
         *      x3   |  .   .   x   x   x   x   .
         *           |  .   .   .   .   .   .   .
         *           |  .   .   .   .   .   .   .
         *      x0   |  .   x   x   x   x   .   .
         *      x1   v  .   x   x   x   x   .   .
         *                 y0          y1
         */
        const uintptr_t x0 = A % (ld * s);
        const uintptr_t x1 = ld * s;
        const uintptr_t x2 = 0;
        const uintptr_t x3 = m*s - (x1 - x0);

        const uintptr_t y0 = A / (ld * s);
        const uintptr_t y1 = y0 + n;
        const uintptr_t y2 = y0 + 1;
        const uintptr_t y3 = y1 + 1;

        {
            Interval list0[2];
            list0[ACCESS_BLAS_ROW_DIM] = Interval(x0, x1);
            list0[ACCESS_BLAS_COL_DIM] = Interval(y0, y1);

            rects[0].set_list(list0);
            assert(!rects[0].is_empty());
        }

        {
            Interval list1[2];
            list1[ACCESS_BLAS_ROW_DIM] = Interval(x2, x3);
            list1[ACCESS_BLAS_COL_DIM] = Interval(y2, y3);
            rects[1].set_list(list1);
            assert(!rects[1].is_empty());
        }
    }
}

/* access types */
typedef enum    access_type_t : uint8_t
{
    ACCESS_TYPE_POINT       = 0,
    ACCESS_TYPE_INTERVAL    = 1,
    ACCESS_TYPE_BLAS_MATRIX = 2,
    ACCESS_TYPE_NULL        = 3,
    ACCESS_TYPE_MAX         = 4,
}               access_type_t;

class access_t
{
    public:
        using Rect    = KHyperrect<2>;
        using Segment = KHyperrect<1>;

    public:

        //////////////
        // the mode //
        //////////////

        /* the mode (READ, WRITE) */
        access_mode_t mode;

        /* the concurrency (SEQUENTIAL, COMMUTATIVE, CONCURRENT) */
        access_concurrency_t concurrency;

        /* the scope (UNIFIED or NONUNIFIED) */
        access_scope_t scope;

        /* access type */
        access_type_t type;

        /////////////////////////////////////////////////
        // region -         depends on the access type //
        /////////////////////////////////////////////////

        union {

            ///////////////////
            // BLAS MATRICES //
            ///////////////////

            struct {
                /* Currently always 2 rects that represents a matrix in an
                 * XKTree (1 rect if access is aligned on ld x sizeof_type,
                 * else 2 rects) */
                Rect rects[2];
            };

            //////////////
            // INTERVAL //
            //////////////

            struct {
                Segment segment;
            };

            ///////////
            // POINT //
            ///////////

            struct {
                const void * point;
            };

            // none
        };

        //////////
        // data //
        //////////

        /* As opposed to kaapi/v1, we have no data handle to attach a sync access onto.
         * How to remove that vector and have a similar 'sync access' logic instead ?
         * For now, this implementation will be good enough pre-reserving 8 successors */
        std::vector<access_t *> successors;

        /* The owning task.
         * Instead, we could use a smaller type (uint8_t) with the number of
         * accesses  + the index of that access in the task struct accessses
         * array, allowing to retrieve the original task */
        # define ACCESS_GET_TASK(A) (A->task)
        task_t * task;

        /* host view of the access = mapped memory from the region */
        memory_view_t host_view;

        /* device view of the access - set after fetching the data */
        memory_replicate_view_t device_view;

    public:

        //////////////////////////////////////////////////////////////////////
        // BLAS MATRIX ACCESSES CONSTRUCTORS                                //
        //////////////////////////////////////////////////////////////////////

        access_t(
            task_t * task,
            const matrix_order_t & order,
            const void * addr,
            const size_t ld,
            const size_t offset_m,
            const size_t offset_n,
            const size_t m,
            const size_t n,
            const size_t s, // sizeof_type,
            access_mode_t mode,
            access_concurrency_t concurrency = ACCESS_CONCURRENCY_SEQUENTIAL,
            access_scope_t scope = ACCESS_SCOPE_NONUNIFIED
        ) :
            mode(mode),
            concurrency(concurrency),
            scope(scope),
            type(ACCESS_TYPE_BLAS_MATRIX),
            rects(),
            successors(8),
            task(task),
            host_view(order, addr, ld, offset_m, offset_n, m, n, s),
            device_view()
        {
            /* clear preallocated empty successors */
            successors.clear();

            /* Only ACCESS_CONCURRENCY_SEQUENTIAL is supported yet */
            assert(concurrency == ACCESS_CONCURRENCY_SEQUENTIAL ||
                    concurrency == ACCESS_CONCURRENCY_COMMUTATIVE);

            /* Only ACCESS_MODE_R|ACCESS_MODE_W supported yet */
            assert(mode == ACCESS_MODE_V || mode == ACCESS_MODE_R ||
                    mode == ACCESS_MODE_W || mode == ACCESS_MODE_RW ||
                    mode == ACCESS_MODE_PIN || mode == ACCESS_MODE_UNPIN);

            // not sure about what to do if other ordering
            assert(host_view.order == MATRIX_COLMAJOR);

            // creates the two rects of that memory view
            memory_view_to_rects(host_view, rects);
        }

         access_t(
            task_t * task,
            const matrix_order_t & order,
            const void * addr,
            const size_t ld,
            const size_t m,
            const size_t n,
            const size_t s, // sizeof_type,
            access_mode_t mode,
            access_concurrency_t concurrency = ACCESS_CONCURRENCY_SEQUENTIAL,
            access_scope_t scope = ACCESS_SCOPE_NONUNIFIED
        ) : access_t(task, order, addr, ld, 0, 0, m, n, s, mode, concurrency, scope) {}

        access_t(
            task_t * task,
            const matrix_order_t & order,
            const Rect & h,
            const size_t ld,
            const size_t s,
            access_mode_t mode,
            access_concurrency_t concurrency = ACCESS_CONCURRENCY_SEQUENTIAL,
            access_scope_t scope = ACCESS_SCOPE_NONUNIFIED
        ) :
            mode(mode),
            concurrency(concurrency),
            scope(scope),
            type(ACCESS_TYPE_BLAS_MATRIX),
            rects(),
            successors(8),
            task(task),
            host_view(order, 0, ld, 0, 0, 0, 0, s),
            device_view()
        {
            /* clear preallocated empty successors */
            successors.clear();

            assert(order == MATRIX_COLMAJOR);
            assert(mode == ACCESS_MODE_R); // not a big deal, but right now only called from `coherent_async`
            assert(!h.is_empty());

            memory_view_from_hyperrect(this->host_view, h, ld, s);
            new (this->rects + 0) Rect(h);
        }

        access_t(
            task_t * task,
            const access_t * other,
            access_mode_t mode,
            access_concurrency_t concurrency = ACCESS_CONCURRENCY_SEQUENTIAL,
            access_scope_t scope = ACCESS_SCOPE_NONUNIFIED
        ) :
            access_t(
                task,
                other->host_view.order,
                (void *) other->host_view.addr,
                other->host_view.ld,
                other->host_view.m,
                other->host_view.n,
                other->host_view.sizeof_type,
                mode,
                concurrency,
                scope
            )
        {}

        //////////////////////////////////////////////////////////////////////
        // POINT ACCESSES CONSTRUCTORS                                      //
        //////////////////////////////////////////////////////////////////////

        access_t(
            task_t * task,
            const void * addr,
            access_mode_t mode,
            access_concurrency_t concurrency = ACCESS_CONCURRENCY_SEQUENTIAL,
            access_scope_t scope = ACCESS_SCOPE_NONUNIFIED
        ) :
            mode(mode),
            concurrency(concurrency),
            scope(scope),
            type(ACCESS_TYPE_POINT),
            point(addr),
            successors(8),
            task(task),
            host_view(MATRIX_COLMAJOR, addr, 1, 0, 0, 1, 1, 1),
            device_view()
        {
            /* clear preallocated empty successors */
            successors.clear();

            /* Only ACCESS_CONCURRENCY_SEQUENTIAL is supported yet */
            assert(concurrency == ACCESS_CONCURRENCY_SEQUENTIAL);

            /* Only ACCESS_MODE_R|ACCESS_MODE_W supported yet */
            assert(mode == ACCESS_MODE_V || mode == ACCESS_MODE_R || mode == ACCESS_MODE_W || mode == ACCESS_MODE_RW);
        }

        //////////////////////////////////////////////////////////////////////
        // INTERVAL ACCESSES CONSTRUCTORS                                   //
        //////////////////////////////////////////////////////////////////////

        // TODO : convert it to a BLAS matrix for now, as the memory coherency
        // tree is quite hard/heavy to implement and would require significant
        // code refractoring to mutualize code with a 1D implementation
        access_t(
            task_t * task,
            const uintptr_t a,
            const uintptr_t b,
            access_mode_t mode,
            access_concurrency_t concurrency = ACCESS_CONCURRENCY_SEQUENTIAL,
            access_scope_t scope = ACCESS_SCOPE_NONUNIFIED
        ) :
            access_t(
                task,
                MATRIX_COLMAJOR,    // order
                (const void *) a,   // addr
                SIZE_MAX,           // ld
                0,                  // offset_m
                0,                  // offset_n
                (size_t) (b - a),   // m
                1,                  // n
                1,                  // s
                mode,
                concurrency,
                scope
        ) {
            assert(a < b);
        }

        //////////////////////////////////////////////////////////////////////
        // NULL ACCESS                                                      //
        //////////////////////////////////////////////////////////////////////

        access_t(
            task_t * task,
            access_mode_t mode,
            access_concurrency_t concurrency = ACCESS_CONCURRENCY_SEQUENTIAL,
            access_scope_t scope = ACCESS_SCOPE_NONUNIFIED
        ) :
            mode(mode),
            concurrency(concurrency),
            scope(scope),
            type(ACCESS_TYPE_NULL),
            successors(8),
            task(task),
            host_view(MATRIX_COLMAJOR, 0, 0, 0, 0, 0, 0, 0),
            device_view()
        {
            /* clear preallocated empty successors */
            successors.clear();

            /* Only ACCESS_CONCURRENCY_SEQUENTIAL is supported yet */
            assert(concurrency == ACCESS_CONCURRENCY_SEQUENTIAL);

            /* Only ACCESS_MODE_R|ACCESS_MODE_W supported yet */
            assert(mode == ACCESS_MODE_V || mode == ACCESS_MODE_R || mode == ACCESS_MODE_W || mode == ACCESS_MODE_RW);
        }

        ~access_t() {}

};

#endif /* __ACCESS_HPP__ */
