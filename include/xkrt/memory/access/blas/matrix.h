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

#ifndef __MATRIX_TILE_H__
# define __MATRIX_TILE_H__

# define NUM_OF_TILES(N, TILE_SIZE) (((N)+(TILE_SIZE)-1)/(TILE_SIZE))

# include <assert.h>
# include <stddef.h>
# include <stdint.h>
# include <sys/types.h>
# include <stdlib.h>

typedef enum    matrix_storage_t
{
    /****************
     *  0   1   2   *
     *  3   4   5   *
     *  6   7   8   *
     ****************/
    MATRIX_ROWMAJOR, /* C */

    /****************
     *  0   3   6   *
     *  1   4   7   *
     *  2   5   8   *
     ****************/
    MATRIX_COLMAJOR, /* Fortran */

}               matrix_storage_t;

typedef struct  matrix_tile_t
{
    /* matrix_storage_t */
    matrix_storage_t storage;

    /* matrix address (passed to the BLAS kernel) */
    uintptr_t addr;

    /* matrix ld */
    size_t ld;

    /* tile size (number of element per row/col) */
    size_t m;   // size row
    size_t n;   // size col

    /* size of type in bytes (eg float == 4, double == 8) */
    size_t sizeof_type;

    /* constructors */
    matrix_tile_t() : matrix_tile_t(MATRIX_COLMAJOR, static_cast<uintptr_t>(0), 0, 0, 0, 0, 0, 0) {}

    matrix_tile_t(
        const matrix_storage_t & storage,
        const void * & addr,
        const size_t & ld,
        const size_t & offset_m,
        const size_t & offset_n,
        const size_t & m,
        const size_t & n,
        const size_t & sizeof_type
    ) :
        matrix_tile_t(storage, (uintptr_t)addr, ld, offset_m, offset_n, m, n, sizeof_type)
    {}

    matrix_tile_t(
        const matrix_storage_t & storage,
        const uintptr_t & addr,
        const size_t & ld,
        const size_t & offset_m,
        const size_t & offset_n,
        const size_t & m,
        const size_t & n,
        const size_t & sizeof_type
    ) :
        storage(storage),
        addr(addr),
        ld(ld),
        m(m),
        n(n),
        sizeof_type(sizeof_type)
    {
        assert(this->storage == MATRIX_ROWMAJOR || this->storage == MATRIX_COLMAJOR);
        this->addr = this->offset_addr(offset_m, offset_n);
    }

    matrix_tile_t(const matrix_tile_t & src) :
        storage(src.storage),
        addr(src.addr),
        ld(src.ld),
        m(src.m),
        n(src.n),
        sizeof_type(src.sizeof_type)
    {
    }

    ~matrix_tile_t() {}

    /* size of the memory represented */
    inline size_t
    size(void) const
    {
        return (this->m * this->n * this->sizeof_type);
    }

    /* return begin address */
    inline uintptr_t
    begin_addr(void) const
    {
        return this->addr;
    }

    static inline uintptr_t
    offset_addr(
        const matrix_storage_t storage,
        const uintptr_t addr,
        const size_t ld,
        const size_t sizeof_type,
        const size_t offset_m,
        const size_t offset_n
    ) {
        switch (storage)
        {
            case (MATRIX_ROWMAJOR):
                return addr + ((size_t)offset_n * sizeof_type) +
                              ((size_t)offset_m * sizeof_type * ld);

            case (MATRIX_COLMAJOR):
                return addr + ((size_t)offset_n * sizeof_type * ld) +
                              ((size_t)offset_m * sizeof_type);
            default:
                abort();
        }
    }

    inline uintptr_t
    offset_addr(const size_t offset_m, const size_t offset_n) const
    {
        assert(this->storage == MATRIX_ROWMAJOR || this->storage == MATRIX_COLMAJOR);
        return offset_addr(this->storage, this->addr, this->ld, this->sizeof_type, offset_m, offset_n);
    }

    /* return end address */
    inline uintptr_t
    end_addr(void) const
    {
        assert(this->storage == MATRIX_ROWMAJOR || this->storage == MATRIX_COLMAJOR);
        switch (this->storage)
        {
            case (MATRIX_ROWMAJOR):
                return this->addr +
                    (this->n * this->sizeof_type) +
                    (this->m * this->sizeof_type * this->ld);

            case (MATRIX_COLMAJOR):
                return this->addr +
                    (this->n * this->sizeof_type * this->ld) +
                    (this->m * this->sizeof_type);

            default:
                abort();
        }
    }

    /* return true if this includes the other tile */
    inline bool
    equals(const matrix_tile_t & x)
    {
        return this->addr == x.addr && this->ld == x.ld && this->sizeof_type == x.sizeof_type && this->m == x.m && this->n == x.n;
    }

    inline bool
    includes(const matrix_tile_t & x)
    {
        (void) x;
        abort();
    }

}               matrix_tile_t;

#endif /* __MATRIX_TILE_H__ */
