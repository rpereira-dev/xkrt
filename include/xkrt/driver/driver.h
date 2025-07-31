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

#ifndef __DRIVER_H__
# define __DRIVER_H__

# ifndef _GNU_SOURCE
#  define _GNU_SOURCE
# endif
# include <sched.h>     /* cpu_set_t */
# include <stdint.h>    /* uint64_t */

# include <xkrt/consts.h>
# include <xkrt/driver/device.hpp>
# include <xkrt/driver/driver-type.h>
# include <xkrt/driver/stream.h>
# include <xkrt/logger/metric.h>
# include <xkrt/logger/todo.h>
# include <xkrt/sync/mutex.h>

# include <hwloc.h>

typedef enum    xkrt_driver_module_format_t
{
    XKRT_DRIVER_MODULE_FORMAT_SPIRV,
    XKRT_DRIVER_MODULE_FORMAT_NATIVE,
    XKRT_DRIVER_MODULE_FORMAT_UNKWN
}               xkrt_driver_module_format_t;

typedef void * xkrt_driver_module_t;
typedef void * xkrt_driver_module_fn_t;

typedef struct  xkrt_driver_t
{
    /* type */
    xkrt_driver_type_t type;

    /* driver team */
    xkrt_team_t team;

    /* a barrier to synchronize all threads of the team and the main thread */
    pthread_barrier_t barrier;

    /* devices */
    xkrt_device_t * devices[XKRT_DEVICES_MAX];

    /* number of devices commited */
    std::atomic<int> ndevices_commited;

    /////////////////////////////////////
    //  API TO IMPLEMENT BY THE DRIVER //
    /////////////////////////////////////

    ///////////////////////
    //  DRIVER META DATA //
    ///////////////////////
    const char   *(*f_get_name)(void);          /* name of the driver (human-readable) */
    unsigned int (*f_get_ndevices_max)(void);   /* return the number of devices available to the driver */

    ///////////////////////
    //  DRIVER LIFECYCLE //
    ///////////////////////
    int (*f_init)(unsigned int ndevices, bool use_p2p);
    void (*f_finalize)(void);

    /////////////////////////////////
    //  DEVICES MANAGEMENT         //
    /////////////////////////////////

    /* Create a device for the given driver id */
    xkrt_device_t * (*f_device_create)(xkrt_driver_t *, int);

    /* initialize device */
    void (*f_device_init)(int device_driver_id);

    /* commit device (called once all devices of that driver had been initialized) */
    int (*f_device_commit)(int device_driver_id, xkrt_device_global_id_bitfield_t * affinity);

    /* Release a device */
    int (*f_device_destroy)(int device_driver_id);

    /* get device infos */
    void (*f_device_info)(int device_driver_id, char * buffer, size_t size);

    ////////////////////////////////
    //  MEMORY MANAGEMENT         //
    ////////////////////////////////

    /* retrieve memory infos */
    void   (*f_memory_device_info)(int device_driver_id, xkrt_device_memory_info_t info[XKRT_DEVICE_MEMORIES_MAX], int * nmemories);

    /* allocate device memory */
    void * (*f_memory_device_allocate)(int device_driver_id, const size_t size, int area_idx);
    void   (*f_memory_device_deallocate)(int device_driver_id, void * ptr, const size_t size, int area_idx);

    /* allocate host memory */
    void * (*f_memory_host_allocate)(int device_driver_id, const size_t size);
    void   (*f_memory_host_deallocate)(int device_driver_id, void * mem, const size_t size);

    /* allocate unified memory */
    void * (*f_memory_unified_allocate)(int device_driver_id, const size_t size);
    void   (*f_memory_unified_deallocate)(int device_driver_id, void * mem, const size_t size);

    /* register host memory */
    int    (*f_memory_host_register)(void * mem, uint64_t size);
    int    (*f_memory_host_unregister)(void * mem, uint64_t size);

    //////////////////////
    // MEMORY TRANSFERS //
    //////////////////////

    int (*f_transfer_h2d)(void * dst, void * src, const size_t size);
    int (*f_transfer_d2h)(void * dst, void * src, const size_t size);
    int (*f_transfer_d2d)(void * dst, void * src, const size_t size);
    int (*f_transfer_h2d_async)(xkrt_stream_t * stream, void * dst, void * src, const size_t size);
    int (*f_transfer_d2h_async)(xkrt_stream_t * stream, void * dst, void * src, const size_t size);
    int (*f_transfer_d2d_async)(xkrt_stream_t * stream, void * dst, void * src, const size_t size);

    ///////////////
    // THREADING //
    ///////////////

    /* Get a cpuset of cpus with the best affinity for the given device */
    int (*f_device_cpuset)(hwloc_topology_t, cpu_set_t*, int);

    ////////////////////////////////
    // STREAM MANAGEMENT          //
    ////////////////////////////////

    /* suggest a number of stream to use for the given stream type */
    int (*f_stream_suggest)(int device_driver_id, xkrt_stream_type_t stype);

    /* alllocate and initialize a stream */
    xkrt_stream_t * (*f_stream_create)(xkrt_device_t * device, xkrt_stream_type_t type, xkrt_stream_instruction_counter_t capacity);

    /* deallocate a stream */
    void (*f_stream_delete)(xkrt_stream_t * istream);

    ///////////////////
    //  P2P ROUTING  //
    ///////////////////

    // TODO

    /////////////
    // MODULES //
    /////////////
    xkrt_driver_module_t    (*f_module_load)(int device_driver_id, uint8_t * bin, size_t binsize, xkrt_driver_module_format_t format);
    void                    (*f_module_unload)(xkrt_driver_module_t module);
    xkrt_driver_module_fn_t (*f_module_get_fn)(xkrt_driver_module_t module, const char * name);

    /////////////////////
    // ENERGY COUNTER  //
    /////////////////////

    /* start power recording on the given device */
    void (*f_power_start)(int device_driver_id, xkrt_power_t * pwr);

    /* return time elapsed (in s.) and the power (in Watt) since last 'f_energy_power_start' call */
    void (*f_power_stop)(int device_driver_id, xkrt_power_t * pwr);

}               xkrt_driver_t;

extern "C"
xkrt_device_t * xkrt_driver_device_get(xkrt_driver_t * driver, xkrt_device_global_id_t driver_device_id);

/* one function per task per driver */
static_assert((uint8_t)XKRT_DRIVER_TYPE_MAX <= (uint8_t)TASK_FORMAT_TARGET_MAX);

typedef struct  xkrt_drivers_t
{
    /* list of drivers */
    xkrt_driver_t * list[XKRT_DRIVER_TYPE_MAX];

    struct {

        /* list of devices */
        xkrt_device_t * list[XKRT_DEVICES_MAX];

        /* number of devices */
        std::atomic<uint8_t> n;

        /* next device id to use */
        std::atomic<uint8_t> next_id;

        /* next worker to offload round robin mode */
        std::atomic<uint8_t> round_robin_device_global_id;

    } devices;

}               xkrt_drivers_t;

#endif /* __DRIVER_H__ */
