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

#ifndef __XKRT_DRIVER_TYPE_H__
# define __XKRT_DRIVER_TYPE_H__

# include <stdint.h>
# include <string.h>

# include <xkrt/support.h>

typedef enum    xkrt_driver_type_t
{
    XKRT_DRIVER_TYPE_HOST   = 0,  // cpu driver
    XKRT_DRIVER_TYPE_CUDA   = 1,  // cuda devices driver
    XKRT_DRIVER_TYPE_ZE     = 2,  // level zero devices driver
    XKRT_DRIVER_TYPE_CL     = 3,  // opencl driver
    XKRT_DRIVER_TYPE_HIP    = 4,  // hip driver
    XKRT_DRIVER_TYPE_SYCL   = 5,  // sycl driver
    XKRT_DRIVER_TYPE_MAX    = 6
}               xkrt_driver_type_t;

typedef uint8_t xkrt_driver_type_bitfield_t;
_Static_assert(XKRT_DRIVER_TYPE_MAX <= sizeof(xkrt_driver_type_bitfield_t)*8, "");

/* get driver name by type */
inline const char *
xkrt_driver_name(xkrt_driver_type_t driver_type)
{
    switch (driver_type)
    {
        case (XKRT_DRIVER_TYPE_HOST):   return "host";
        case (XKRT_DRIVER_TYPE_CUDA):   return "cuda";
        case (XKRT_DRIVER_TYPE_HIP):    return "hip";
        case (XKRT_DRIVER_TYPE_ZE):     return "ze";
        case (XKRT_DRIVER_TYPE_CL):     return "cl";
        case (XKRT_DRIVER_TYPE_SYCL):   return "sycl";
        default:                        return "(null)";
    }
}

/* get driver type by name */
inline xkrt_driver_type_t
xkrt_driver_type_from_name(const char * name)
{
    for (int i = 0 ; i < XKRT_DRIVER_TYPE_MAX ; ++i)
        if (strcmp(name, xkrt_driver_name((xkrt_driver_type_t) i)) == 0)
            return (xkrt_driver_type_t) i;
    return XKRT_DRIVER_TYPE_MAX;
}

/* return true if the driver is supported by the XKRT build */
inline int
xkrt_driver_supported(xkrt_driver_type_t driver_type)
{
    switch (driver_type)
    {
        case (XKRT_DRIVER_TYPE_HOST):   return 1;
        case (XKRT_DRIVER_TYPE_CUDA):   return XKRT_SUPPORT_CUDA;
        case (XKRT_DRIVER_TYPE_HIP):    return XKRT_SUPPORT_HIP;
        case (XKRT_DRIVER_TYPE_ZE):     return XKRT_SUPPORT_ZE;
        case (XKRT_DRIVER_TYPE_CL):     return XKRT_SUPPORT_CL;
        case (XKRT_DRIVER_TYPE_SYCL):   return XKRT_SUPPORT_SYCL;
        default:                        return 0;
    }
}

#endif /* __DRIVER_TYPE_H__ */
