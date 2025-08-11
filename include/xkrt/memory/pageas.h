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

#ifndef __PAGEAS_H__
# define __PAGEAS_H__

# include <assert.h>

/**
 *  Convert the segment [ptr, ptr+size] to [*p_a, *p_a+*p_size] where
 *      - *p_a is aligned on getpagesize()
 *      - *p_size is a multiple of getpagesize()
 *      - [*p_a, *p_a+*p_size] is the minimum segment including [ptr, ptr+size]
 */
static inline void
pageas(void * ptr, size_t size, uintptr_t * p_a, size_t * p_size)
{
    assert(size > 0);

    const size_t pagesize = (size_t) getpagesize();
    const uintptr_t  p = (const uintptr_t) ptr;
    const uintptr_t pp = p + size;
    const uintptr_t  a = p - (p % pagesize);
    const uintptr_t  b = pp + (pagesize - (pp % pagesize)) % pagesize;

    assert(a % pagesize == 0);
    assert(b % pagesize == 0);
    assert(a <= p);
    assert(b >= p + size);
    assert(a < b);

    *p_a    = a;
    *p_size = b - a;
}

#endif /* __PAGEAS_H__ */
