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

#ifndef __LOGGER_CUSOLVER_H__
#define __LOGGER_CUSOLVER_H__

#include <xkrt/logger/logger.h>
/* include the cuSOLVER header you actually use; cusolverDn.h covers dense routines */
#include <cusolverDn.h>

static const char *
cusolver_error_to_str(cusolverStatus_t status)
{
    switch (status) {
        case CUSOLVER_STATUS_SUCCESS:                 return "CUSOLVER_STATUS_SUCCESS";
        case CUSOLVER_STATUS_NOT_INITIALIZED:         return "CUSOLVER_STATUS_NOT_INITIALIZED";
        case CUSOLVER_STATUS_ALLOC_FAILED:            return "CUSOLVER_STATUS_ALLOC_FAILED";
        case CUSOLVER_STATUS_INVALID_VALUE:           return "CUSOLVER_STATUS_INVALID_VALUE";
        case CUSOLVER_STATUS_ARCH_MISMATCH:           return "CUSOLVER_STATUS_ARCH_MISMATCH";
#if defined(CUSOLVER_STATUS_MAPPING_ERROR)
        case CUSOLVER_STATUS_MAPPING_ERROR:           return "CUSOLVER_STATUS_MAPPING_ERROR";
#endif
        case CUSOLVER_STATUS_EXECUTION_FAILED:        return "CUSOLVER_STATUS_EXECUTION_FAILED";
        case CUSOLVER_STATUS_INTERNAL_ERROR:          return "CUSOLVER_STATUS_INTERNAL_ERROR";
#if defined(CUSOLVER_STATUS_MATRIX_TYPE_NOT_SUPPORTED)
        case CUSOLVER_STATUS_MATRIX_TYPE_NOT_SUPPORTED: return "CUSOLVER_STATUS_MATRIX_TYPE_NOT_SUPPORTED";
#endif
#if defined(CUSOLVER_STATUS_NOT_SUPPORTED)
        case CUSOLVER_STATUS_NOT_SUPPORTED:           return "CUSOLVER_STATUS_NOT_SUPPORTED";
#endif
#if defined(CUSOLVER_STATUS_ZERO_PIVOT)
        case CUSOLVER_STATUS_ZERO_PIVOT:              return "CUSOLVER_STATUS_ZERO_PIVOT";
#endif
        default: {
            /* try numeric fallback */
            static char buf[64];
            snprintf(buf, sizeof(buf), "UNKNOWN_CUSOLVER_STATUS (%d)", (int) status);
            return buf;
        }
    }
}

#define CUSOLVER_SAFE_CALL(X)                                                            \
    do {                                                                                 \
        cusolverStatus_t r_ = (X);                                                       \
        if (r_ != CUSOLVER_STATUS_SUCCESS) {                                             \
            LOGGER_FATAL("`%s` failed with err=%s (%d)", #X, cusolver_error_to_str(r_), (int)r_); \
        }                                                                                \
    } while (0)

#endif /* __LOGGER_CUSOLVER_H__ */
