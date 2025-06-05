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

# ifndef __LOGGER_CL_H__
#  define __LOGGER_CL_H__

#  include <xkrt/logger/logger.h>
#  include <CL/cl.h>

static const char *
cl_error_to_str(const int & r)
{
    switch (r)
    {
        case CL_SUCCESS:                                    return "CL_SUCCESS";
        case CL_DEVICE_NOT_FOUND:                           return "CL_DEVICE_NOT_FOUND";
        case CL_DEVICE_NOT_AVAILABLE:                       return "CL_DEVICE_NOT_AVAILABLE";
        case CL_COMPILER_NOT_AVAILABLE:                     return "CL_COMPILER_NOT_AVAILABLE";
        case CL_MEM_OBJECT_ALLOCATION_FAILURE:              return "CL_MEM_OBJECT_ALLOCATION_FAILURE";
        case CL_OUT_OF_RESOURCES:                           return "CL_OUT_OF_RESOURCES";
        case CL_OUT_OF_HOST_MEMORY:                         return "CL_OUT_OF_HOST_MEMORY";
        case CL_PROFILING_INFO_NOT_AVAILABLE:               return "CL_PROFILING_INFO_NOT_AVAILABLE";
        case CL_MEM_COPY_OVERLAP:                           return "CL_MEM_COPY_OVERLAP";
        case CL_IMAGE_FORMAT_MISMATCH:                      return "CL_IMAGE_FORMAT_MISMATCH";
        case CL_IMAGE_FORMAT_NOT_SUPPORTED:                 return "CL_IMAGE_FORMAT_NOT_SUPPORTED";
        case CL_BUILD_PROGRAM_FAILURE:                      return "CL_BUILD_PROGRAM_FAILURE";
        case CL_MAP_FAILURE:                                return "CL_MAP_FAILURE";
        case CL_MISALIGNED_SUB_BUFFER_OFFSET:               return "CL_MISALIGNED_SUB_BUFFER_OFFSET";
        case CL_EXEC_STATUS_ERROR_FOR_EVENTS_IN_WAIT_LIST:  return "CL_EXEC_STATUS_ERROR_FOR_EVENTS_IN_WAIT_LIST";
        case CL_COMPILE_PROGRAM_FAILURE:                    return "CL_COMPILE_PROGRAM_FAILURE";
        case CL_LINKER_NOT_AVAILABLE:                       return "CL_LINKER_NOT_AVAILABLE";
        case CL_LINK_PROGRAM_FAILURE:                       return "CL_LINK_PROGRAM_FAILURE";
        case CL_DEVICE_PARTITION_FAILED:                    return "CL_DEVICE_PARTITION_FAILED";
        case CL_KERNEL_ARG_INFO_NOT_AVAILABLE:              return "CL_KERNEL_ARG_INFO_NOT_AVAILABLE";
        case CL_INVALID_VALUE:                              return "CL_INVALID_VALUE";
        case CL_INVALID_DEVICE_TYPE:                        return "CL_INVALID_DEVICE_TYPE";
        case CL_INVALID_PLATFORM:                           return "CL_INVALID_PLATFORM";
        case CL_INVALID_DEVICE:                             return "CL_INVALID_DEVICE";
        case CL_INVALID_CONTEXT:                            return "CL_INVALID_CONTEXT";
        case CL_INVALID_QUEUE_PROPERTIES:                   return "CL_INVALID_QUEUE_PROPERTIES";
        case CL_INVALID_COMMAND_QUEUE:                      return "CL_INVALID_COMMAND_QUEUE";
        case CL_INVALID_HOST_PTR:                           return "CL_INVALID_HOST_PTR";
        case CL_INVALID_MEM_OBJECT:                         return "CL_INVALID_MEM_OBJECT";
        case CL_INVALID_IMAGE_FORMAT_DESCRIPTOR:            return "CL_INVALID_IMAGE_FORMAT_DESCRIPTOR";
        case CL_INVALID_IMAGE_SIZE:                         return "CL_INVALID_IMAGE_SIZE";
        case CL_INVALID_SAMPLER:                            return "CL_INVALID_SAMPLER";
        case CL_INVALID_BINARY:                             return "CL_INVALID_BINARY";
        case CL_INVALID_BUILD_OPTIONS:                      return "CL_INVALID_BUILD_OPTIONS";
        case CL_INVALID_PROGRAM:                            return "CL_INVALID_PROGRAM";
        case CL_INVALID_PROGRAM_EXECUTABLE:                 return "CL_INVALID_PROGRAM_EXECUTABLE";
        case CL_INVALID_KERNEL_NAME:                        return "CL_INVALID_KERNEL_NAME";
        case CL_INVALID_KERNEL_DEFINITION:                  return "CL_INVALID_KERNEL_DEFINITION";
        case CL_INVALID_KERNEL:                             return "CL_INVALID_KERNEL";
        case CL_INVALID_ARG_INDEX:                          return "CL_INVALID_ARG_INDEX";
        case CL_INVALID_ARG_VALUE:                          return "CL_INVALID_ARG_VALUE";
        case CL_INVALID_ARG_SIZE:                           return "CL_INVALID_ARG_SIZE";
        case CL_INVALID_KERNEL_ARGS:                        return "CL_INVALID_KERNEL_ARGS";
        case CL_INVALID_WORK_DIMENSION:                     return "CL_INVALID_WORK_DIMENSION";
        case CL_INVALID_WORK_GROUP_SIZE:                    return "CL_INVALID_WORK_GROUP_SIZE";
        case CL_INVALID_WORK_ITEM_SIZE:                     return "CL_INVALID_WORK_ITEM_SIZE";
        case CL_INVALID_GLOBAL_OFFSET:                      return "CL_INVALID_GLOBAL_OFFSET";
        case CL_INVALID_EVENT_WAIT_LIST:                    return "CL_INVALID_EVENT_WAIT_LIST";
        case CL_INVALID_EVENT:                              return "CL_INVALID_EVENT";
        case CL_INVALID_OPERATION:                          return "CL_INVALID_OPERATION";
        case CL_INVALID_GL_OBJECT:                          return "CL_INVALID_GL_OBJECT";
        case CL_INVALID_BUFFER_SIZE:                        return "CL_INVALID_BUFFER_SIZE";
        case CL_INVALID_MIP_LEVEL:                          return "CL_INVALID_MIP_LEVEL";
        case CL_INVALID_GLOBAL_WORK_SIZE:                   return "CL_INVALID_GLOBAL_WORK_SIZE";
        case CL_INVALID_PROPERTY:                           return "CL_INVALID_PROPERTY";
        case CL_INVALID_IMAGE_DESCRIPTOR:                   return "CL_INVALID_IMAGE_DESCRIPTOR";
        case CL_INVALID_COMPILER_OPTIONS:                   return "CL_INVALID_COMPILER_OPTIONS";
        case CL_INVALID_LINKER_OPTIONS:                     return "CL_INVALID_LINKER_OPTIONS";
        case CL_INVALID_DEVICE_PARTITION_COUNT:             return "CL_INVALID_DEVICE_PARTITION_COUNT";
        case CL_INVALID_PIPE_SIZE:                          return "CL_INVALID_PIPE_SIZE";
        case CL_INVALID_DEVICE_QUEUE:                       return "CL_INVALID_DEVICE_QUEUE";
        case CL_INVALID_SPEC_ID:                            return "CL_INVALID_SPEC_ID";
        case CL_MAX_SIZE_RESTRICTION_EXCEEDED:              return "CL_MAX_SIZE_RESTRICTION_EXCEEDED";
        default:                                            return "UNKNOWN_ERROR";
    }
}

# define CL_SAFE_CALL(X)                                                                \
    do {                                                                                \
        int r = X;                                                                      \
        if (r != CL_SUCCESS)                                                            \
            LOGGER_FATAL("`%s` failed with err=%s (%d)", #X, cl_error_to_str(r), r);    \
    } while (0)

# endif
