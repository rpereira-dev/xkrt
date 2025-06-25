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

using Segment   = KHyperrect<1>;
using Rect      = KHyperrect<2>;


static inline void
interval_to_rects(
    INTERVAL_TYPE_T      a,
    INTERVAL_DIFF_TYPE_T size,
    size_t ld,
    size_t sizeof_type,
    Rect (& rects) [3]
) {
     /**
      *  a = start address
      *  b = end address
      *  size = b - a
      *      --------------------------------->
      *      |      x  x  x
      *      |      x  x  x
      *      |      x  x  x
      * LD.s |   a  x  x  x
      *      |   x  x  x  b
      *      |   x  x  x
      *      v
      *
      *  generate 3 rects from it
      *
      *          y0 y1 y2 y3
      *      --------------------------------->
      *      |      1  1  2   x2
      *      |      1  1  2
      *      |      1  1  2
      * x0   |   0  1  1  2
      *      |   0  1  1  2   x3
      * x1   |   0  1  1
      *      v
      */

    assert(size > 0);
    const INTERVAL_TYPE_T LDs = ld * sizeof_type;

    const INTERVAL_TYPE_T x0 = a % LDs;
    const INTERVAL_TYPE_T x1 = MIN(x0 + size, LDs);
    const INTERVAL_TYPE_T dx10 = x1 - x0;

    const INTERVAL_TYPE_T x2 = 0;
    const INTERVAL_TYPE_T x3 = (size - dx10) % LDs;
    const INTERVAL_TYPE_T dx32 = x3 - x2;

    assert((size - dx10 - dx32) % LDs == 0);

    const INTERVAL_TYPE_T y0 = a / LDs;
    const INTERVAL_TYPE_T y1 = y0 + 1;
    const INTERVAL_TYPE_T y3 = y0 + ((size - dx10 - dx32) / LDs) + 2;
    const INTERVAL_TYPE_T y2 = y3 - 1;

    Interval intervals[3][2];
    intervals[0][ACCESS_BLAS_ROW_DIM] = Interval(x0, x1);
    intervals[0][ACCESS_BLAS_COL_DIM] = Interval(y0, y1);
    intervals[1][ACCESS_BLAS_ROW_DIM] = Interval(0, LDs);
    intervals[1][ACCESS_BLAS_COL_DIM] = Interval(y1, y2);
    intervals[2][ACCESS_BLAS_ROW_DIM] = Interval(x2, x3);
    intervals[2][ACCESS_BLAS_COL_DIM] = Interval(y2, y3);

    rects[0].set_list(intervals[0]);
    rects[1].set_list(intervals[1]);
    rects[2].set_list(intervals[2]);
}

static inline void
matrix_from_rect(
    matrix_tile_t & mat,
    const Rect & rect,
    const size_t ld,
    const size_t sizeof_type
) {
    const INTERVAL_TYPE_T       x = rect[ACCESS_BLAS_ROW_DIM].a;
    const INTERVAL_DIFF_TYPE_T dx = rect[ACCESS_BLAS_ROW_DIM].length();
    const INTERVAL_TYPE_T       y = rect[ACCESS_BLAS_COL_DIM].a;
    const INTERVAL_DIFF_TYPE_T dy = rect[ACCESS_BLAS_COL_DIM].length();
    assert(dx > 0);
    assert(dy > 0);

    mat.storage     = MATRIX_COLMAJOR;
    mat.addr        = x + y * ld * sizeof_type;
    mat.ld          = ld;
    mat.m           = dx / sizeof_type;
    mat.n           = dy;
    mat.sizeof_type = sizeof_type;

    // accesses must be aligned on sizeof(type)
    assert((INTERVAL_DIFF_TYPE_T) (mat.m * sizeof_type) == dx);
}

static inline void
matrix_from_rects(
    matrix_tile_t & mat,
    const Rect & r0,
    const Rect & r1,
    const size_t ld,
    const size_t sizeof_type
) {
    const INTERVAL_DIFF_TYPE_T x0 = (INTERVAL_DIFF_TYPE_T) r0[ACCESS_BLAS_ROW_DIM].a;
    const INTERVAL_DIFF_TYPE_T xf = (INTERVAL_DIFF_TYPE_T) r1[ACCESS_BLAS_ROW_DIM].b;
    const INTERVAL_DIFF_TYPE_T y0 = (INTERVAL_DIFF_TYPE_T) r0[ACCESS_BLAS_COL_DIM].a;
    const INTERVAL_DIFF_TYPE_T yf = (INTERVAL_DIFF_TYPE_T) r1[ACCESS_BLAS_COL_DIM].b;
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

    mat.storage        = MATRIX_COLMAJOR;
    mat.addr         = x0 + y0 * ld * sizeof_type;
    mat.ld           = ld;
    mat.m            = (size_t) m;
    mat.n            = (size_t) n;
    mat.sizeof_type  = sizeof_type;
}

/* rects must have at least a capacity of 2x Rect */
static inline void
matrix_to_rects(
    matrix_tile_t & mat,
    Rect (& rects) []
) {
    const size_t  A = mat.begin_addr();
    const size_t ld = mat.ld;
    const size_t  m = mat.m;
    const size_t  n = mat.n;
    const size_t  s = mat.sizeof_type;

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

/* access state */
typedef enum    access_state_t : uint8_t
{
    ACCESS_STATE_INIT,
    ACCESS_STATE_FETCHING,
    ACCESS_STATE_FETCHED
}               access_state_t;

class access_t
{
    public:

        ///////////////
        // the state //
        ///////////////

        /* the access state */
        access_state_t state;

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
                /**
                 *  BLAS matrices have 2 rects in their frame of reference (ld, s)
                 *  Interval accesses have 3 rects different on each frame of reference (ld, s)
                 */
                Rect rects[3];
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
        // POINT ACCESSES CONSTRUCTORS                                      //
        //////////////////////////////////////////////////////////////////////

        access_t(
            task_t * task,
            const void * addr,
            access_mode_t mode,
            access_concurrency_t concurrency = ACCESS_CONCURRENCY_SEQUENTIAL,
            access_scope_t scope = ACCESS_SCOPE_NONUNIFIED
        ) :
            state(ACCESS_STATE_INIT),
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
            assert(concurrency == ACCESS_CONCURRENCY_SEQUENTIAL ||
                    concurrency == ACCESS_CONCURRENCY_COMMUTATIVE);
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
                MATRIX_COLMAJOR,    // storage
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
            )
        {
            assert(a < b);
            this->type = ACCESS_TYPE_INTERVAL;
        }

        //////////////////////////////////////////////////////////////////////
        // BLAS MATRIX ACCESSES CONSTRUCTORS                                //
        //////////////////////////////////////////////////////////////////////

        access_t(
            task_t * task,
            const matrix_storage_t & storage,
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
            state(ACCESS_STATE_INIT),
            mode(mode),
            concurrency(concurrency),
            scope(scope),
            type(ACCESS_TYPE_BLAS_MATRIX),
            rects(),
            successors(8),
            task(task),
            host_view(storage, addr, ld, offset_m, offset_n, m, n, s),
            device_view()
        {
            /* clear preallocated empty successors */
            successors.clear();

            /* Only ACCESS_CONCURRENCY_SEQUENTIAL is supported yet */
            assert(concurrency == ACCESS_CONCURRENCY_SEQUENTIAL ||
                    concurrency == ACCESS_CONCURRENCY_COMMUTATIVE);

            // not sure about what to do if other storageing
            assert(host_view.storage == MATRIX_COLMAJOR);

            // creates the two rects of that memory view
            matrix_to_rects(host_view, rects);
        }

         access_t(
            task_t * task,
            const matrix_storage_t & storage,
            const void * addr,
            const size_t ld,
            const size_t m,
            const size_t n,
            const size_t s, // sizeof_type,
            access_mode_t mode,
            access_concurrency_t concurrency = ACCESS_CONCURRENCY_SEQUENTIAL,
            access_scope_t scope = ACCESS_SCOPE_NONUNIFIED
        ) : access_t(task, storage, addr, ld, 0, 0, m, n, s, mode, concurrency, scope) {}

        access_t(
            task_t * task,
            const matrix_storage_t & storage,
            const Rect & h,
            const size_t ld,
            const size_t s,
            access_mode_t mode,
            access_concurrency_t concurrency = ACCESS_CONCURRENCY_SEQUENTIAL,
            access_scope_t scope = ACCESS_SCOPE_NONUNIFIED
        ) :
            state(ACCESS_STATE_INIT),
            mode(mode),
            concurrency(concurrency),
            scope(scope),
            type(ACCESS_TYPE_BLAS_MATRIX),
            rects(),
            successors(8),
            task(task),
            host_view(storage, 0, ld, 0, 0, 0, 0, s),
            device_view()
        {
            /* clear preallocated empty successors */
            successors.clear();

            assert(storage == MATRIX_COLMAJOR);
            assert(mode == ACCESS_MODE_R); // not a big deal, but right now only called from `coherent_async`
            assert(!h.is_empty());

            matrix_from_rect(this->host_view, h, ld, s);
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
                other->host_view.storage,
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
        // NULL ACCESS                                                      //
        //////////////////////////////////////////////////////////////////////

        access_t(
            task_t * task,
            access_mode_t mode,
            access_concurrency_t concurrency = ACCESS_CONCURRENCY_SEQUENTIAL,
            access_scope_t scope = ACCESS_SCOPE_NONUNIFIED
        ) :
            state(ACCESS_STATE_INIT),
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
        }

        ~access_t() {}

};

#endif /* __ACCESS_HPP__ */
