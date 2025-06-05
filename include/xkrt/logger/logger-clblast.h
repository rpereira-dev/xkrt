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

#ifndef __LOGGER_CLBLAST_H__
# define __LOGGER_CLBLAST_H__

# include <xkrt/logger/logger.h>
# include <clblast_c.h>

static const char *
clblast_error_to_str(CLBlastStatusCode status)
{
    switch (status)
    {
        case CLBlastSuccess                   : return "CLBlastSuccess";
        case CLBlastOpenCLCompilerNotAvailable: return "CLBlastOpenCLCompilerNotAvailable";
        case CLBlastTempBufferAllocFailure    : return "CLBlastTempBufferAllocFailure";
        case CLBlastOpenCLOutOfResources      : return "CLBlastOpenCLOutOfResources";
        case CLBlastOpenCLOutOfHostMemory     : return "CLBlastOpenCLOutOfHostMemory";
        case CLBlastOpenCLBuildProgramFailure : return "CLBlastOpenCLBuildProgramFailure";
        case CLBlastInvalidValue              : return "CLBlastInvalidValue";
        case CLBlastInvalidCommandQueue       : return "CLBlastInvalidCommandQueue";
        case CLBlastInvalidMemObject          : return "CLBlastInvalidMemObject";
        case CLBlastInvalidBinary             : return "CLBlastInvalidBinary";
        case CLBlastInvalidBuildOptions       : return "CLBlastInvalidBuildOptions";
        case CLBlastInvalidProgram            : return "CLBlastInvalidProgram";
        case CLBlastInvalidProgramExecutable  : return "CLBlastInvalidProgramExecutable";
        case CLBlastInvalidKernelName         : return "CLBlastInvalidKernelName";
        case CLBlastInvalidKernelDefinition   : return "CLBlastInvalidKernelDefinition";
        case CLBlastInvalidKernel             : return "CLBlastInvalidKernel";
        case CLBlastInvalidArgIndex           : return "CLBlastInvalidArgIndex";
        case CLBlastInvalidArgValue           : return "CLBlastInvalidArgValue";
        case CLBlastInvalidArgSize            : return "CLBlastInvalidArgSize";
        case CLBlastInvalidKernelArgs         : return "CLBlastInvalidKernelArgs";
        case CLBlastInvalidLocalNumDimensions : return "CLBlastInvalidLocalNumDimensions";
        case CLBlastInvalidLocalThreadsTotal  : return "CLBlastInvalidLocalThreadsTotal";
        case CLBlastInvalidLocalThreadsDim    : return "CLBlastInvalidLocalThreadsDim";
        case CLBlastInvalidGlobalOffset       : return "CLBlastInvalidGlobalOffset";
        case CLBlastInvalidEventWaitList      : return "CLBlastInvalidEventWaitList";
        case CLBlastInvalidEvent              : return "CLBlastInvalidEvent";
        case CLBlastInvalidOperation          : return "CLBlastInvalidOperation";
        case CLBlastInvalidBufferSize         : return "CLBlastInvalidBufferSize";
        case CLBlastInvalidGlobalWorkSize     : return "CLBlastInvalidGlobalWorkSize";
        case CLBlastNotImplemented            : return "CLBlastNotImplemented";
        case CLBlastInvalidMatrixA            : return "CLBlastInvalidMatrixA";
        case CLBlastInvalidMatrixB            : return "CLBlastInvalidMatrixB";
        case CLBlastInvalidMatrixC            : return "CLBlastInvalidMatrixC";
        case CLBlastInvalidVectorX            : return "CLBlastInvalidVectorX";
        case CLBlastInvalidVectorY            : return "CLBlastInvalidVectorY";
        case CLBlastInvalidDimension          : return "CLBlastInvalidDimension";
        case CLBlastInvalidLeadDimA           : return "CLBlastInvalidLeadDimA";
        case CLBlastInvalidLeadDimB           : return "CLBlastInvalidLeadDimB";
        case CLBlastInvalidLeadDimC           : return "CLBlastInvalidLeadDimC";
        case CLBlastInvalidIncrementX         : return "CLBlastInvalidIncrementX";
        case CLBlastInvalidIncrementY         : return "CLBlastInvalidIncrementY";
        case CLBlastInsufficientMemoryA       : return "CLBlastInsufficientMemoryA";
        case CLBlastInsufficientMemoryB       : return "CLBlastInsufficientMemoryB";
        case CLBlastInsufficientMemoryC       : return "CLBlastInsufficientMemoryC";
        case CLBlastInsufficientMemoryX       : return "CLBlastInsufficientMemoryX";
        case CLBlastInsufficientMemoryY       : return "CLBlastInsufficientMemoryY";
        case CLBlastInsufficientMemoryTemp    : return "CLBlastInsufficientMemoryTemp";
        case CLBlastInvalidBatchCount         : return "CLBlastInvalidBatchCount";
        case CLBlastInvalidOverrideKernel     : return "CLBlastInvalidOverrideKernel";
        case CLBlastMissingOverrideParameter  : return "CLBlastMissingOverrideParameter";
        case CLBlastInvalidLocalMemUsage      : return "CLBlastInvalidLocalMemUsage";
        case CLBlastNoHalfPrecision           : return "CLBlastNoHalfPrecision";
        case CLBlastNoDoublePrecision         : return "CLBlastNoDoublePrecision";
        case CLBlastInvalidVectorScalar       : return "CLBlastInvalidVectorScalar";
        case CLBlastInsufficientMemoryScalar  : return "CLBlastInsufficientMemoryScalar";
        case CLBlastDatabaseError             : return "CLBlastDatabaseError";
        case CLBlastUnknownError              : return "CLBlastUnknownError";
        case CLBlastUnexpectedError           : return "CLBlastUnexpectedError";
        default                               : return "Unknown CLBlastStatusCode";
    }
}

# define CLBLAST_SAFE_CALL(X)                                                               \
    do {                                                                                    \
        CLBlastStatusCode r = X;                                                            \
        if (r != CLBlastSuccess)                                                            \
            LOGGER_FATAL("`%s` failed with err=%s (%d)", #X, clblast_error_to_str(r), r);   \
    } while (0)

#endif /* __LOGGER_CLBLAST_H__ */
