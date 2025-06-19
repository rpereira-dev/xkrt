/*
** Copyright 2024,2025 INRIA
**
** Contributors :
** Thierry Gautier, thierry.gautier@inrialpes.fr
** Joao Lima joao.lima@inf.ufsm.br
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

#ifndef __STREAM_INSTRUCTION_H__
# define __STREAM_INSTRUCTION_H__

# include <xkrt/callback.h>
# include <xkrt/consts.h>
# include <xkrt/memory/view.hpp>
# include <xkrt/driver/stream-instruction-type.h>
# include <xkrt/logger/todo.h>
# include <xkrt/memory/cache-line-size.hpp>
# include <xkrt/sync/mutex.h>

# include <functional>

/* counter for the stream queues */
typedef uint32_t xkrt_stream_instruction_counter_t;

/* move data between devices */
typedef struct  xkrt_stream_instruction_copy_1D_t
{
    size_t size;
    uintptr_t dst_device_addr;
    uintptr_t src_device_addr;

}               xkrt_stream_instruction_copy_1D_t;

typedef struct  xkrt_stream_instruction_copy_2D_t
{
    size_t m;
    size_t n;
    size_t sizeof_type;
    memory_replicate_view_t dst_device_view;
    memory_replicate_view_t src_device_view;

}               xkrt_stream_instruction_copy_2D_t;

/* kernel : to launch kernel on the device */
typedef struct  xkrt_stream_instruction_kernel_t
{
    void (*launch)(void * istream, void * instr, xkrt_stream_instruction_counter_t idx);
    void * vargs;
}               xkrt_stream_instruction_kernel_t;

/* read/write files */
typedef struct  xkrt_stream_instruction_fd_t
{
    int fd;
    void * buffer;
    size_t n;
}               xkrt_stream_instruction_fd_t;

/* instructions */
typedef struct  xkrt_stream_instruction_t
{
    xkrt_stream_instruction_type_t type;
    xkrt_callback_t callback;
    union
    {
        xkrt_stream_instruction_copy_1D_t   copy_1D;
        xkrt_stream_instruction_copy_2D_t   copy_2D;
        xkrt_stream_instruction_kernel_t    kern;
        xkrt_stream_instruction_fd_t        fd;
    };

}               xkrt_stream_instruction_t;

#endif /* __STREAM_INSTRUCTION_H__ */
