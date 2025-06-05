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

# ifndef __LOGGER_ZE_H__
#  define __LOGGER_ZE_H__

#  include <xkrt/logger/logger.h>
#  include <ze_api.h>

static const char *
ze_error_to_str(const ze_result_t & r)
{
    switch (r)
    {
        case ZE_RESULT_SUCCESS:                                 return "ZE_RESULT_SUCCESS";
        case ZE_RESULT_NOT_READY:                               return "ZE_RESULT_NOT_READY";
        case ZE_RESULT_ERROR_DEVICE_LOST:                       return "ZE_RESULT_ERROR_DEVICE_LOST";
        case ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY:                return "ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY";
        case ZE_RESULT_ERROR_OUT_OF_DEVICE_MEMORY:              return "ZE_RESULT_ERROR_OUT_OF_DEVICE_MEMORY";
        case ZE_RESULT_ERROR_MODULE_BUILD_FAILURE:              return "ZE_RESULT_ERROR_MODULE_BUILD_FAILURE";
        case ZE_RESULT_ERROR_MODULE_LINK_FAILURE:               return "ZE_RESULT_ERROR_MODULE_LINK_FAILURE";
        case ZE_RESULT_ERROR_DEVICE_REQUIRES_RESET:             return "ZE_RESULT_ERROR_DEVICE_REQUIRES_RESET";
        case ZE_RESULT_ERROR_DEVICE_IN_LOW_POWER_STATE:         return "ZE_RESULT_ERROR_DEVICE_IN_LOW_POWER_STATE";
        case ZE_RESULT_EXP_ERROR_DEVICE_IS_NOT_VERTEX:          return "ZE_RESULT_EXP_ERROR_DEVICE_IS_NOT_VERTEX";
        case ZE_RESULT_EXP_ERROR_VERTEX_IS_NOT_DEVICE:          return "ZE_RESULT_EXP_ERROR_VERTEX_IS_NOT_DEVICE";
        case ZE_RESULT_EXP_ERROR_REMOTE_DEVICE:                 return "ZE_RESULT_EXP_ERROR_REMOTE_DEVICE";
        case ZE_RESULT_EXP_ERROR_OPERANDS_INCOMPATIBLE:         return "ZE_RESULT_EXP_ERROR_OPERANDS_INCOMPATIBLE";
        case ZE_RESULT_EXP_RTAS_BUILD_RETRY:                    return "ZE_RESULT_EXP_RTAS_BUILD_RETRY";
        case ZE_RESULT_EXP_RTAS_BUILD_DEFERRED:                 return "ZE_RESULT_EXP_RTAS_BUILD_DEFERRED";
        case ZE_RESULT_ERROR_INSUFFICIENT_PERMISSIONS:          return "ZE_RESULT_ERROR_INSUFFICIENT_PERMISSIONS";
        case ZE_RESULT_ERROR_NOT_AVAILABLE:                     return "ZE_RESULT_ERROR_NOT_AVAILABLE";
        case ZE_RESULT_ERROR_DEPENDENCY_UNAVAILABLE:            return "ZE_RESULT_ERROR_DEPENDENCY_UNAVAILABLE";
        case ZE_RESULT_WARNING_DROPPED_DATA:                    return "ZE_RESULT_WARNING_DROPPED_DATA";
        case ZE_RESULT_ERROR_UNINITIALIZED:                     return "ZE_RESULT_ERROR_UNINITIALIZED";
        case ZE_RESULT_ERROR_UNSUPPORTED_VERSION:               return "ZE_RESULT_ERROR_UNSUPPORTED_VERSION";
        case ZE_RESULT_ERROR_UNSUPPORTED_FEATURE:               return "ZE_RESULT_ERROR_UNSUPPORTED_FEATURE";
        case ZE_RESULT_ERROR_INVALID_ARGUMENT:                  return "ZE_RESULT_ERROR_INVALID_ARGUMENT";
        case ZE_RESULT_ERROR_INVALID_NULL_HANDLE:               return "ZE_RESULT_ERROR_INVALID_NULL_HANDLE";
        case ZE_RESULT_ERROR_HANDLE_OBJECT_IN_USE:              return "ZE_RESULT_ERROR_HANDLE_OBJECT_IN_USE";
        case ZE_RESULT_ERROR_INVALID_NULL_POINTER:              return "ZE_RESULT_ERROR_INVALID_NULL_POINTER";
        case ZE_RESULT_ERROR_INVALID_SIZE:                      return "ZE_RESULT_ERROR_INVALID_SIZE";
        case ZE_RESULT_ERROR_UNSUPPORTED_SIZE:                  return "ZE_RESULT_ERROR_UNSUPPORTED_SIZE";
        case ZE_RESULT_ERROR_UNSUPPORTED_ALIGNMENT:             return "ZE_RESULT_ERROR_UNSUPPORTED_ALIGNMENT";
        case ZE_RESULT_ERROR_INVALID_SYNCHRONIZATION_OBJECT:    return "ZE_RESULT_ERROR_INVALID_SYNCHRONIZATION_OBJECT";
        case ZE_RESULT_ERROR_INVALID_ENUMERATION:               return "ZE_RESULT_ERROR_INVALID_ENUMERATION";
        case ZE_RESULT_ERROR_UNSUPPORTED_ENUMERATION:           return "ZE_RESULT_ERROR_UNSUPPORTED_ENUMERATION";
        case ZE_RESULT_ERROR_UNSUPPORTED_IMAGE_FORMAT:          return "ZE_RESULT_ERROR_UNSUPPORTED_IMAGE_FORMAT";
        case ZE_RESULT_ERROR_INVALID_NATIVE_BINARY:             return "ZE_RESULT_ERROR_INVALID_NATIVE_BINARY";
        case ZE_RESULT_ERROR_INVALID_GLOBAL_NAME:               return "ZE_RESULT_ERROR_INVALID_GLOBAL_NAME";
        case ZE_RESULT_ERROR_INVALID_KERNEL_NAME:               return "ZE_RESULT_ERROR_INVALID_KERNEL_NAME";
        case ZE_RESULT_ERROR_INVALID_FUNCTION_NAME:             return "ZE_RESULT_ERROR_INVALID_FUNCTION_NAME";
        case ZE_RESULT_ERROR_INVALID_GROUP_SIZE_DIMENSION:      return "ZE_RESULT_ERROR_INVALID_GROUP_SIZE_DIMENSION";
        case ZE_RESULT_ERROR_INVALID_GLOBAL_WIDTH_DIMENSION:    return "ZE_RESULT_ERROR_INVALID_GLOBAL_WIDTH_DIMENSION";
        case ZE_RESULT_ERROR_INVALID_KERNEL_ARGUMENT_INDEX:     return "ZE_RESULT_ERROR_INVALID_KERNEL_ARGUMENT_INDEX";
        case ZE_RESULT_ERROR_INVALID_KERNEL_ARGUMENT_SIZE:      return "ZE_RESULT_ERROR_INVALID_KERNEL_ARGUMENT_SIZE";
        case ZE_RESULT_ERROR_INVALID_KERNEL_ATTRIBUTE_VALUE:    return "ZE_RESULT_ERROR_INVALID_KERNEL_ATTRIBUTE_VALUE";
        case ZE_RESULT_ERROR_INVALID_MODULE_UNLINKED:           return "ZE_RESULT_ERROR_INVALID_MODULE_UNLINKED";
        case ZE_RESULT_ERROR_INVALID_COMMAND_LIST_TYPE:         return "ZE_RESULT_ERROR_INVALID_COMMAND_LIST_TYPE";
        case ZE_RESULT_ERROR_OVERLAPPING_REGIONS:               return "ZE_RESULT_ERROR_OVERLAPPING_REGIONS";
        case ZE_RESULT_WARNING_ACTION_REQUIRED:                 return "ZE_RESULT_WARNING_ACTION_REQUIRED";
        case ZE_RESULT_ERROR_UNKNOWN:                           return "ZE_RESULT_ERROR_UNKNOWN";
        default:                                                return "ZE_RESULT_FORCE_UINT32";
    }
}

# define ZE_SAFE_CALL(X)                                                                \
    do {                                                                                \
        ze_result_t r = X;                                                              \
        if (r != ZE_RESULT_SUCCESS)                                                     \
            LOGGER_FATAL("`%s` failed with err=%s (%d)", #X, ze_error_to_str(r), r);    \
    } while (0)



# endif
