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

#ifndef __ACCESS_MODE_H__
# define __ACCESS_MODE_H__

// To OpenMP dependencies
//
// omp modifier | (xkrt_access_mode_t, xkrt_access_concurrency_t)
//
//        in      = (ACCESS_MODE_R, _, )
//  out|inout     = (ACCESS_MODE_W, ACCESS_CONCURRENCY_SEQUENTIAL)
//  mutexinoutset = (ACCESS_MODE_W, ACCESS_CONCURRENCY_COMMUTATIVE)
//       inoutset = (ACCESS_MODE_W, ACCESS_CONCURRENCY_CONCURRENT)
//
// scope is sort of always 'ACCESS_SCOPE_UNIFIED' as we cannot specify
// dependency domains in 6.0

/* access mode */
typedef enum    xkrt_access_mode_t
{
    ACCESS_MODE_R       = 0b00000001,   // read
    ACCESS_MODE_W       = 0b00000010,   // write
    ACCESS_MODE_RW      = ACCESS_MODE_R | ACCESS_MODE_W,
    ACCESS_MODE_V       = 0b00000100,   // incoherent access (= 'virtual' = dont really move the memory)
    ACCESS_MODE_VW      = ACCESS_MODE_W | ACCESS_MODE_V,
    ACCESS_MODE_VR      = ACCESS_MODE_R | ACCESS_MODE_V,
    ACCESS_MODE_D       = 0b00001000,   // detached access = do not fulfill
                                        // dependencies on task completion: the
                                        // task is responsible of fulfillment
                                        // the access itself
}               xkrt_access_mode_t;

static inline const char *
xkrt_access_mode_to_str(xkrt_access_mode_t mode)
{
    switch (mode)
    {
        case (ACCESS_MODE_V):     return "ACCESS_MODE_V";
        case (ACCESS_MODE_R):     return "ACCESS_MODE_R";
        case (ACCESS_MODE_W):     return "ACCESS_MODE_W";
        case (ACCESS_MODE_RW):    return "ACCESS_MODE_RW";
        default:                  return "ACCESS_MODE_UNKN";
    }
}

#endif /* __ACCESS_MODE_H__ */
