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

# include <xkrt/runtime.h>

XKRT_NAMESPACE_BEGIN

typedef struct  copy_args_t
{
    // the runtime
    runtime_t * runtime;

    // the device responsible to perform the copy
    device_global_id_t device_global_id;

    // pointers
    device_global_id_t dst_device_global_id;
    uintptr_t dst_device_mem;

    device_global_id_t src_device_global_id;
    uintptr_t src_device_mem;

    // size of the copy
    size_t size;

}               copy_args_t;

void
runtime_t::copy(
    const device_global_id_t      device_global_id,
    const memory_view_t         & host_view,
    const device_global_id_t      dst_device_global_id,
    const memory_replica_view_t & dst_device_view,
    const device_global_id_t      src_device_global_id,
    const memory_replica_view_t & src_device_view,
    const callback_t            & callback
) {
    device_t * device = this->device_get(device_global_id);
    device->offloader_stream_instruction_submit_copy<memory_view_t, memory_replica_view_t>(
        host_view,
        dst_device_global_id,
        dst_device_view,
        src_device_global_id,
        src_device_view,
        callback
    );
}

void
runtime_t::copy(
    const device_global_id_t   device_global_id,
    const size_t               size,
    const device_global_id_t   dst_device_global_id,
    const uintptr_t            dst_device_addr,
    const device_global_id_t   src_device_global_id,
    const uintptr_t            src_device_addr,
    const callback_t         & callback
) {
    device_t * device = this->device_get(device_global_id);
    // TODO: create 1x instruction per pinned segment, and callback
    device->offloader_stream_instruction_submit_copy<size_t, uintptr_t>(
        size,
        dst_device_global_id,
        dst_device_addr,
        src_device_global_id,
        src_device_addr,
        callback
    );
}

XKRT_NAMESPACE_END
