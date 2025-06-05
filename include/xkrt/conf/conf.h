/*
** Copyright 2024 INRIA
**
** Contributors :
**
** Romain PEREIRA, romain.pereira@inria.fr
** Romain PEREIRA, rpereira@anl.gov
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

#ifndef __XKRT_CONF_H__
# define __XKRT_CONF_H__

# include <stdint.h>

# include <xkrt/logger/todo.h>
# include <xkrt/driver/driver-type.h>
# include <xkrt/driver/stream.h>

//////////////////
//  DEVICE CONF //
//////////////////

typedef struct  xkrt_conf_stream_t
{
    /* number of stream per operation (<=> cuda stream) */
    int8_t n;

    /* number of concurrent operations */
    uint32_t concurrency;

}               xkrt_conf_stream_t;

typedef struct  xkrt_conf_offloader_t
{
    xkrt_conf_stream_t streams[XKRT_STREAM_TYPE_ALL];
    uint16_t capacity;

}               xkrt_conf_offloader_t;

typedef struct  xkrt_conf_device_t
{
    float gpu_mem_percent;              /* % of gpu memory to allocate initially */
    int ngpus;                          /* number of GPU for this node */
    bool use_p2p;                       /* enable/disable p2p */
    xkrt_conf_offloader_t offloader;    /* offloader conf */
}               xkrt_conf_device_t;

typedef struct  xkrt_conf_driver_t
{
    int nthreads_per_device;
    int used;
}               xkrt_conf_driver_t;

typedef struct  xkrt_conf_drivers_t
{
    xkrt_conf_driver_t list[XKRT_DRIVER_TYPE_MAX];

}               xkrt_conf_drivers_t;

//////////////////////////////////////////////////////////////////

typedef struct  xkrt_conf_s
{
    xkrt_conf_device_t device;      /* device conf */
    xkrt_conf_drivers_t drivers;    /* driver conf */
    bool merge_transfers;           /* attempt to merge continuous memory to a single transfer */
    bool report_stats_on_deinit;    /* report stats on deinit */

    /* keep track of registered memory, and split transfers for each registered
     * segment to avoid cuda crashing while transfering memory that is
     * partially registered */
    bool protect_registered_memory_overflow;
}               xkrt_conf_t;

void xkrt_init_conf(xkrt_conf_t * conf);

#endif /* __XKRT_CONF_H__ */
