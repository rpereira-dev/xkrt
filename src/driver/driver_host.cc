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

# define XKRT_DRIVER_ENTRYPOINT(N) XKRT_DRIVER_TYPE_HOST_ ## N

# include <xkrt/runtime.h>
# include <xkrt/conf/conf.h>
# include <xkrt/driver/device.hpp>
# include <xkrt/driver/driver.h>
# include <xkrt/driver/driver-host.h>
# include <xkrt/driver/stream.h>
# include <xkrt/sync/bits.h>
# include <xkrt/sync/mutex.h>

# include <hwloc.h>
# include <hwloc/glibc-sched.h>
# include <sys/sysinfo.h>

# include <cassert>
# include <cstdio>
# include <cstdint>
# include <cerrno>
# include <functional>

static int
XKRT_DRIVER_ENTRYPOINT(init)(
    unsigned int ndevices,
    bool use_p2p
) {
    (void) ndevices;
    return 0;
}

void
XKRT_DRIVER_ENTRYPOINT(device_info)(
    int device_driver_id,
    char * buffer,
    size_t size
) {
    (void) device_driver_id;

    // Initialize and load topology
    hwloc_topology_t topology;
    hwloc_topology_init(&topology);
    hwloc_topology_load(topology);

    // Get the first PU (Processing Unit) and move up to the package (CPU)
    hwloc_obj_t obj = hwloc_get_obj_by_type(topology, HWLOC_OBJ_PACKAGE, 0);
    if (obj && obj->name)
        snprintf(buffer, size, "%s", obj->name);
    else
        snprintf(buffer, size, "Unknown CPU");

    // Destroy topology
    hwloc_topology_destroy(topology);
}

static void
XKRT_DRIVER_ENTRYPOINT(finalize)(void)
{
}

static const char *
XKRT_DRIVER_ENTRYPOINT(get_name)(void)
{
    return "HOST";
}

static unsigned int
XKRT_DRIVER_ENTRYPOINT(get_ndevices_max)(void)
{
    return 1;
}

static int
XKRT_DRIVER_ENTRYPOINT(device_cpuset)(hwloc_topology_t topology, cpu_set_t * schedset, int device_driver_id)
{
    (void) topology;
    assert(device_driver_id == 0);
    pthread_getaffinity_np(pthread_self(), sizeof(cpu_set_t), schedset);
    return 0;
}

static xkrt_device_t *
XKRT_DRIVER_ENTRYPOINT(device_create)(xkrt_driver_t * driver, int device_driver_id)
{
    (void) driver;
    assert(device_driver_id == 0);
    static xkrt_device_t device;
    return &device;
}

static void
XKRT_DRIVER_ENTRYPOINT(device_init)(int device_driver_id)
{
    (void) device_driver_id;
}

static int
XKRT_DRIVER_ENTRYPOINT(device_destroy)(int device_driver_id)
{
    (void) device_driver_id;
    return 0;
}

/* Called for each device of the driver once they all have been initialized */
static int
XKRT_DRIVER_ENTRYPOINT(device_commit)(int device_driver_id, xkrt_device_global_id_bitfield_t * affinity)
{
    (void) device_driver_id;
    (void) affinity;
    return 0;
}

////////////
// STREAM //
////////////

static int
XKRT_DRIVER_ENTRYPOINT(stream_instruction_launch)(
    xkrt_stream_t * istream,
    xkrt_stream_instruction_t * instr,
    xkrt_stream_instruction_counter_t idx
) {
    (void) istream;
    (void) idx;

    assert(instr->type == XKRT_STREAM_INSTR_TYPE_FD_READ ||
            instr->type == XKRT_STREAM_INSTR_TYPE_FD_WRITE);

    switch (instr->type)
    {
        case (XKRT_STREAM_INSTR_TYPE_FD_READ):
        case (XKRT_STREAM_INSTR_TYPE_FD_WRITE):
        {
            LOGGER_FATAL("IMPL ME");
            return EINPROGRESS;
        }

        default:
            break ;
    }

    return 0;
}

static int
XKRT_DRIVER_ENTRYPOINT(stream_suggest)(
    int device_driver_id,
    xkrt_stream_type_t stype
) {
    assert(device_driver_id == 0);
    switch (stype)
    {
        case (XKRT_STREAM_TYPE_FD_READ):
        case (XKRT_STREAM_TYPE_FD_WRITE):
            return 1;

        default:
            return 0;
    }
    return 0;
}

static inline int
XKRT_DRIVER_ENTRYPOINT(stream_instructions_wait)(
    xkrt_stream_t * istream
) {
    LOGGER_FATAL("Not supported");
    return 0;
}

static int
XKRT_DRIVER_ENTRYPOINT(stream_instructions_progress)(
    xkrt_stream_t * istream,
    xkrt_stream_instruction_t * instr,
    xkrt_stream_instruction_counter_t idx
) {
    (void)istream;
    (void)idx;

    assert(instr->type == XKRT_STREAM_INSTR_TYPE_FD_READ ||
            instr->type == XKRT_STREAM_INSTR_TYPE_FD_WRITE);

    switch (instr->type)
    {
        case (XKRT_STREAM_INSTR_TYPE_FD_READ):
        case (XKRT_STREAM_INSTR_TYPE_FD_WRITE):
        {
            LOGGER_FATAL("IMPL ME");
            return EINPROGRESS;
        }

        default:
            break ;
    }

    return 0;
}

static xkrt_stream_t *
XKRT_DRIVER_ENTRYPOINT(stream_create)(
    xkrt_device_t * idevice,
    xkrt_stream_type_t type,
    xkrt_stream_instruction_counter_t capacity
) {
    (void)idevice;
    (void)type;

    assert(type == XKRT_STREAM_TYPE_FD_READ || type == XKRT_STREAM_TYPE_FD_WRITE);

    uint8_t * mem = (uint8_t *) malloc(sizeof(xkrt_stream_host_t) + capacity * sizeof(xkrt_stream_host_event_t));
    assert(mem);

    xkrt_stream_host_t * stream = (xkrt_stream_host_t *) mem;

    /*************************/
    /* init xkrt stream */
    /*************************/
    xkrt_stream_init(
        (xkrt_stream_t *) stream,
        type,
        capacity,
        XKRT_DRIVER_ENTRYPOINT(stream_instruction_launch),
        XKRT_DRIVER_ENTRYPOINT(stream_instructions_progress),
        XKRT_DRIVER_ENTRYPOINT(stream_instructions_wait)
    );

    /*************************/
    /* do host specific init */
    /*************************/

    /* events */
    stream->host.events.buffer = (xkrt_stream_host_event_t *) (stream + 1);
    stream->host.events.capacity = capacity;
    memset(stream->host.events.buffer, 0, capacity * sizeof(xkrt_stream_host_event_t));

    return (xkrt_stream_t *) stream;
}

static void
XKRT_DRIVER_ENTRYPOINT(stream_delete)(
    xkrt_stream_t * istream
) {
    free(istream);
}

////////////
// MEMORY //
////////////

# if 0
static void *
XKRT_DRIVER_ENTRYPOINT(memory_device_allocate)(int device_driver_id, const size_t size, int area_idx)
{
    (void)device_driver_id;
    (void)size;
    (void)area_idx;
    return NULL;
}

static void
XKRT_DRIVER_ENTRYPOINT(memory_device_deallocate)(int device_driver_id, void * ptr, const size_t size, int area_idx)
{
    (void)device_driver_id;
    (void)ptr;
    (void)size;
    (void)area_idx;
}
# endif

static void
XKRT_DRIVER_ENTRYPOINT(memory_device_info)(
    int device_driver_id,
    xkrt_device_memory_info_t info[XKRT_DEVICE_MEMORIES_MAX],
    int * nmemories
) {
    (void)device_driver_id;
    assert(device_driver_id == 0);

    struct sysinfo sinfo;

    if (sysinfo(&sinfo) == 0)
    {
        const int i = 0;
        strncpy(info[i].name, "RAM", sizeof(info[i].name));
        info[i].used     = sinfo.totalram - sinfo.freeram;
        info[i].capacity = sinfo.totalram;
        *nmemories = 1;
    }
    else
    {
        *nmemories = 0;
    }
}

# if 0
static void *
XKRT_DRIVER_ENTRYPOINT(memory_host_allocate)(
    int device_driver_id,
    uint64_t size
) {
    (void)device_driver_id;
    (void)size;
    return NULL;
}

static void
XKRT_DRIVER_ENTRYPOINT(memory_host_deallocate)(
    int device_driver_id,
    void * mem,
    uint64_t size
) {
    (void)device_driver_id;
    (void)mem;
    (void)size;
}

xkrt_driver_module_t
XKRT_DRIVER_ENTRYPOINT(module_load)(
    int device_driver_id,
    uint8_t * bin,
    size_t binsize,
    xkrt_driver_module_format_t format
) {
    (void)device_driver_id;
    (void)bin;
    (void)binsize;
    (void)format;
    return NULL;
}

void
XKRT_DRIVER_ENTRYPOINT(module_unload)(
    xkrt_driver_module_t module
) {
    (void)module;
}

xkrt_driver_module_fn_t
XKRT_DRIVER_ENTRYPOINT(module_get_fn)(
    xkrt_driver_module_t module,
    const char * name
) {
    (void)module;
    (void)name;
    return NULL;
}
# endif

//////////////////////////
// Routine registration //
//////////////////////////
xkrt_driver_t *
XKRT_DRIVER_ENTRYPOINT(create_driver)(void)
{
    xkrt_driver_t * driver = (xkrt_driver_t *) calloc(1, sizeof(xkrt_driver_t));
    assert(driver);

    # define REGISTER(func) driver->f_##func = XKRT_DRIVER_ENTRYPOINT(func)

    REGISTER(init);
    REGISTER(finalize);

    REGISTER(get_name);
    REGISTER(get_ndevices_max);

    REGISTER(device_create);
    REGISTER(device_init);
    REGISTER(device_commit);
    REGISTER(device_destroy);

    REGISTER(device_info);

    REGISTER(memory_device_info);
 // REGISTER(memory_device_allocate);
 // REGISTER(memory_device_deallocate);
 // REGISTER(memory_host_allocate);
 // REGISTER(memory_host_deallocate);
 // REGISTER(memory_host_register);
 // REGISTER(memory_host_unregister);
 // REGISTER(memory_unified_allocate);
 // REGISTER(memory_unified_deallocate);

    REGISTER(device_cpuset);

    REGISTER(stream_suggest);
    REGISTER(stream_create);
    REGISTER(stream_delete);

 // REGISTER(module_load);
 // REGISTER(module_unload);
 // REGISTER(module_get_fn);

    # undef REGISTER

    return (xkrt_driver_t *) driver;
}
