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

#ifndef __LOGGER_CUBLAS_H__
# define __LOGGER_CUBLAS_H__

# include <xkrt/logger/logger.h>
# include <cublas_v2.h>

static const char *
cublas_error_to_str(cublasStatus_t status)
{
    # if CUBLAS_VER_MAJOR < 12
    static const char * names[] = {
        "SUCCESS",                  /*  0 */
        "NOT_INITIALIZED",          /*  1 */
        NULL,                       /*  2 */
        "ALLOC_FAILED",             /*  3 */
        NULL,                       /*  4 */
        NULL,                       /*  5 */
        NULL,                       /*  6 */
        "INVALID_VALUE",            /*  7 */
        "ARCH_MISMATCH",            /*  8 */
        NULL,                       /*  9 */
        NULL,                       /* 10 */
        "MAPPING_ERROR",            /* 11 */
        NULL,                       /* 12 */
        "EXECUTION_FAILED",         /* 13 */
        "INTERNAL_ERROR",           /* 14 */
        "NOT_SUPPORTED",            /* 15 */
        "LICENSE_ERROR",            /* 16 */
    };
    const char * name  = (status >= 0 && status <= 16) ? names[status] : NULL;
    # else
    const char * name  = cublasGetStatusName(status);
    # endif /* CUBLAS_VER_MAJOR */
    return name;
}

# define CUBLAS_SAFE_CALL(X)                                                                \
    do {                                                                                    \
        cublasStatus_t r = X;                                                               \
        if (r != CUBLAS_STATUS_SUCCESS)                                                     \
            LOGGER_FATAL("`%s` failed with err=%s (%d)", #X, cublas_error_to_str(r), r);    \
    } while (0)

#endif /* __LOGGER_CUBLAS_H__ */
