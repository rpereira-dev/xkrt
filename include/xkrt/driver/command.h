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

#ifndef __QUEUE_COMMAND_H__
# define __QUEUE_COMMAND_H__

# include <xkrt/callback.h>
# include <xkrt/consts.h>
# include <xkrt/types.h>
# include <xkrt/memory/view.hpp>
# include <xkrt/driver/command-type.h>
# include <xkrt/logger/todo.h>
# include <xkrt/memory/cache-line-size.hpp>
# include <xkrt/sync/mutex.h>

# include <functional>

XKRT_NAMESPACE_BEGIN

    /* counter for the queue queues */
    typedef uint32_t queue_command_list_counter_t;

    /* move data between devices */
    typedef struct  queue_command_copy_1D_t
    {
        size_t size;
        uintptr_t dst_device_addr;
        uintptr_t src_device_addr;

    }               queue_command_copy_1D_t;

    typedef struct  queue_command_copy_2D_t
    {
        size_t m;
        size_t n;
        size_t sizeof_type;
        memory_replica_view_t dst_device_view;
        memory_replica_view_t src_device_view;

    }               queue_command_copy_2D_t;

    /* kernel : to launch kernel on the device */
    typedef struct  queue_command_kernel_t
    {
        // arguments are:
        //   queue_t * iqueue, queue_command * cmd, queue_command_list_counter_t idx)
        void (*launch)();
        void * vargs;
    }               queue_command_kernel_t;

    /* read/write files */
    typedef struct  queue_command_file_t
    {
        int fd;
        void * buffer;
        size_t size;
        size_t offset;
    }               queue_command_file_t;

    /* commands */
    typedef struct  command_t
    {
        command_type_t type;
        union
        {
            queue_command_copy_1D_t   copy_1D;
            queue_command_copy_2D_t   copy_2D;
            queue_command_kernel_t    kern;
            queue_command_file_t      file;
        };
        bool completed;
        struct {
            callback_t list[XKRT_COMMAND_CALLBACKS_MAX];
            command_callback_index_t n;
        } callbacks;

        inline void
        push_callback(const callback_t & callback)
        {
            assert(this->callbacks.n >= 0);
            assert(this->callbacks.n < XKRT_COMMAND_CALLBACKS_MAX);
            this->callbacks.list[this->callbacks.n++] = callback;
        }

    }               command_t;

XKRT_NAMESPACE_END

#endif /* __QUEUE_COMMAND_H__ */
