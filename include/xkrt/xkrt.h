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

#ifndef __XKRT_H__
# define __XKRT_H__

// TODO : xkrt.h should become the C interface of XKaapi

// TODO : this include the whole world, including C++ this is bad
# include <xkrt/runtime.h>

// xkrt public API
extern "C" {

    ////////////////////////
    // Runtime management //
    ////////////////////////
    /* Initialize a runtime instance */
    int xkrt_init(xkrt_runtime_t * runtime);

    /* Deinitialize the runtime interface */
    int xkrt_deinit(xkrt_runtime_t * runtime);

    /* return the maximum number of devices available */
    int xkrt_get_ndevices_max(xkrt_runtime_t * runtime, int * count);

    //////////////////////
    // SYNCHRONIZATIONS //
    //////////////////////

    /* Wait for all tasks and devices instructions to complete */
    int xkrt_sync(xkrt_runtime_t * runtime);

    ///////////////
    // COHERENCY //
    ///////////////

    /* Make all device memory non-coherent */
    void xkrt_coherency_reset(xkrt_runtime_t * runtime);

    /* Submit 1 task per device to create a coherent replicate */
    void xkrt_coherency_replicate_2D_async(
        xkrt_runtime_t * runtime,
        matrix_storage_t storage,
        void * ptr, size_t ld,
        size_t m, size_t n,
        size_t sizeof_type
    );

    /* Submit tasks that reads Region(storage, m, n, addr, ld, sizeof_type) onto the host */
    void xkrt_coherent1D_async(
        xkrt_runtime_t * runtime,
        void * addr, size_t size
    );

    void xkrt_coherent2D_async(
        xkrt_runtime_t * runtime,
        matrix_storage_t storage,
        void * addr, size_t ld,
        size_t m, size_t n,
        size_t sizeof_type
    );

    /* Allocate incoherent memory replicates onto the passed device */
    void xkrt_coherent_allocate_2D(
        xkrt_runtime_t * runtime,
        xkrt_device_global_id_t device_global_id,
        matrix_storage_t storage,
        void * ptr, size_t ld,
        size_t m, size_t n,
        size_t sizeof_type
    );

    ////////////////
    // DISTRIBUTE //
    ////////////////

    // DISTRIBUTE //

    void xkrt_distribute2D_async(
        xkrt_runtime_t * runtime,
        xkrt_distribution_type_t type,
        matrix_storage_t storage,
        void * ptr, size_t ld,
        size_t m, size_t n,
        size_t mb, size_t nb,
        size_t sizeof_type,
        size_t hx, size_t hy
    );

    //////////////////////////////////////////////////
    // EXPLICIT MEMORY MANAGMENT (bypass coherency) //
    //////////////////////////////////////////////////

    /* Create a task that launches a memory copy instruction from 'src' to 'dst' into 'device' streams */
    void xkrt_memory_copy_async(
        xkrt_runtime_t              * runtime,
        const xkrt_device_global_id_t device_global_id,
        const xkrt_device_global_id_t dst_device_global_id,
        const uintptr_t               dst_device_mem,
        const xkrt_device_global_id_t src_device_global_id,
        const uintptr_t               src_device_mem,
        const size_t                  size
    );

    /* see cudaHostRegister */
    int xkrt_memory_register(xkrt_runtime_t * runtime, void * ptr, uint64_t size);

    /* see cudaHostUnregister */
    int xkrt_memory_unregister(xkrt_runtime_t * runtime, void * ptr, uint64_t size);

    /* see cudaHostRegister */
    int xkrt_memory_register_async(xkrt_runtime_t * runtime, void * ptr, uint64_t size);

    /* see cudaHostUnregister */
    int xkrt_memory_unregister_async(xkrt_runtime_t * runtime, void * ptr, uint64_t size);

    /* see cudaHostUnregister */
    int xkrt_memory_register_waitall(xkrt_runtime_t * runtime);

};

#endif /* __XKRT_H__ */
