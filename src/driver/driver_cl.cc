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

/* see https://oneapi-src.github.io/level-zero-spec/level-zero/latest/core/api.html */

# define XKRT_DRIVER_ENTRYPOINT(N) XKRT_DRIVER_TYPE_CL_ ## N

# include <xkrt/runtime.h>
# include <xkrt/conf/conf.h>
# include <xkrt/driver/device.hpp>
# include <xkrt/driver/driver.h>
# include <xkrt/driver/stream.h>
# include <xkrt/sync/bits.h>
# include <xkrt/sync/mutex.h>

# include <xkrt/driver/driver-cl.h>
# include <xkrt/logger/logger-cl.h>
# include <xkrt/logger/logger-hwloc.h>

# include <CL/cl.h>
# include <hwloc/opencl.h>
# include <hwloc/glibc-sched.h>

# include <cassert>
# include <cstdio>
# include <cstdint>
# include <cerrno>
# include <functional>

// opencl does not allow pointer arithmetic on device memory, so we hard a
// virtual address space per device.
// The device 0 virtual address space is
//  [VIRT_MEM_ORIGIN, VIRT_MEM_ORIGIN + VIRT_MEM_PER_DEVICE_MAX[
// The device 1 virtual address space is
//  [VIRT_MEM_ORIGIN + VIRT_MEM_PER_DEVICE_MAX, VIRT_MEM_ORIGIN + 2*VIRT_MEM_PER_DEVICE_MAX[
// ...
# include <stdint.h>
# define VIRT_MEM_ORIGIN            (0x161103 + 0x270196 + 0x300194 + 1240019) // = 1 << 23
static_assert(VIRT_MEM_ORIGIN > 0);

# define VIRT_MEM_PER_DEVICE_MAX    ((UINTPTR_MAX - VIRT_MEM_ORIGIN) / XKRT_DEVICES_MAX)
static_assert((uintptr_t)(VIRT_MEM_ORIGIN + VIRT_MEM_PER_DEVICE_MAX * XKRT_DEVICES_MAX) < UINTPTR_MAX);

// platforms
static cl_platform_id cl_platforms[XKRT_DEVICES_MAX];
static cl_device_id cl_devices[XKRT_DEVICES_MAX];

static xkrt_device_cl_t DEVICES[XKRT_DEVICES_MAX];
static cl_uint cl_n_devices = 0;

static xkrt_device_cl_t *
device_cl_get(int device_driver_id)
{
    assert(device_driver_id >= 0);
    assert(device_driver_id < cl_n_devices);
    return DEVICES + device_driver_id;
}

static xkrt_device_cl_t *
device_cl_get_from_addr(uintptr_t addr)
{
    // TODO : this can be accelerated with bitwise op, hopefully the compiler notices
    int device_driver_id = ((addr - VIRT_MEM_ORIGIN) / VIRT_MEM_PER_DEVICE_MAX);
    assert(device_driver_id >= 0);
    assert(device_driver_id < cl_n_devices);
    return device_cl_get(device_driver_id);
}

// retrieve the buffer and the offset in it of the given pointer
static inline xkrt_device_cl_buffer_t *
XKRT_DRIVER_ENTRYPOINT(xkrt_buffer_from_addr)(
    xkrt_device_cl_t * device,
    uintptr_t addr
) {
    // find which 'xkrt_device_cl_buffer_t' holds the virtual address
    for (int i = 0 ; i < device->memory.nbuffers ; ++i)
    {
        xkrt_device_cl_buffer_t * buffer = device->memory.buffers + i;
        if (buffer->addr <= addr && addr < buffer->addr + buffer->size)
            return buffer;
    }
    LOGGER_FATAL("Passed an invalid address");
}

void
xkrt_driver_cl_get_buffer_and_offset_1D(
    xkrt_device_cl_t * device,
    uintptr_t addr,
    cl_mem * mem,
    size_t * offset
) {
    xkrt_device_cl_buffer_t * buffer = XKRT_DRIVER_ENTRYPOINT(xkrt_buffer_from_addr)(device, addr);
    *mem = buffer->cl.mem;
    *offset = addr - buffer->addr;
}

void
xkrt_driver_cl_get_buffer_and_offset_2D(
    xkrt_device_cl_t * device,
    uintptr_t addr,
    size_t pitch,
    cl_mem * mem,
    size_t * offset
) {
    xkrt_device_cl_buffer_t * buffer = XKRT_DRIVER_ENTRYPOINT(xkrt_buffer_from_addr)(device, addr);
    *mem = buffer->cl.mem;

    // with 0 <= offset[0] < pitch - we have
    //    addr                = buffer->addr + offset[0] + offset[1] * pitch;
    // => addr - buffer->addr =                offset[0] + offset[1] * pitch;
    // => offset[0] = (addr - buffer->addr) % pitch
    // => offset[1] = (addr - buffer->addr) / pitch

    offset[0] = (addr - buffer->addr) % pitch;
    offset[1] = (addr - buffer->addr) / pitch;
}

static void xkrt_cl_pfn_notify(
    const char * errinfo,
    const void * private_info,
    size_t cb,
    void * user_data
) {
    LOGGER_ERROR("CL Error occured `%s`", errinfo);
}

static int
XKRT_DRIVER_ENTRYPOINT(init)(
    unsigned int ndevices,
    bool use_p2p
) {
    (void) use_p2p;

    assert(0 < ndevices);
    assert(ndevices <= XKRT_DEVICES_MAX);

    // get all drivers
    cl_uint cl_n_platforms = ndevices; // i believe cl ensure at least 1 device per platform ?
    CL_SAFE_CALL(clGetPlatformIDs(cl_n_platforms, cl_platforms, &cl_n_platforms));
    assert(0 <= cl_n_platforms);

    for (cl_uint i = 0; i < cl_n_platforms; ++i)
    {
        // retrieve cl device ids
        cl_device_id * devices = cl_devices + cl_n_devices;
        cl_uint ndevices = ndevices - cl_n_devices;
        int err = clGetDeviceIDs(cl_platforms[i], CL_DEVICE_TYPE_GPU, ndevices, devices, &ndevices);
        if (err == CL_DEVICE_NOT_FOUND)
            continue ;
        CL_SAFE_CALL(err);
        assert(0 <= ndevices);
        assert(ndevices <= ndevices - cl_n_devices);

        // create a context for all these platform devices
        const cl_context_properties properties[] = {
            CL_CONTEXT_PLATFORM,
            (cl_context_properties)cl_platforms[i],
            0 // end of properties
        };
        cl_context context = clCreateContext(properties, ndevices, devices, xkrt_cl_pfn_notify, NULL, &err);
        CL_SAFE_CALL(err);

        for (cl_uint j = 0; j < ndevices ; ++j)
        {
            xkrt_device_cl_t * device = DEVICES + cl_n_devices;
            device->cl.id = devices[j];
            device->cl.context = context;

            // initialize device virtual memory
            device->memory.nbuffers = 0;
            device->memory.head = VIRT_MEM_ORIGIN + j * VIRT_MEM_PER_DEVICE_MAX;
            if (++cl_n_devices >= ndevices)
                return 0;
        }
    }

    return 0;
}

static void
XKRT_DRIVER_ENTRYPOINT(finalize)(void)
{

}

static const char *
XKRT_DRIVER_ENTRYPOINT(get_name)(void)
{
    return "CL";
}

static unsigned int
XKRT_DRIVER_ENTRYPOINT(get_ndevices_max)(void)
{
    return cl_n_devices;
}

static int
XKRT_DRIVER_ENTRYPOINT(device_cpuset)(hwloc_topology_t topology, cpu_set_t * schedset, int device_driver_id)
{
    xkrt_device_cl_t * device = device_cl_get(device_driver_id);

    hwloc_cpuset_t cpuset = hwloc_bitmap_alloc();
    HWLOC_SAFE_CALL(hwloc_opencl_get_device_cpuset(topology, device->cl.id, cpuset));
    CPU_ZERO(schedset);
    HWLOC_SAFE_CALL(hwloc_cpuset_to_glibc_sched_affinity(topology, cpuset, schedset, sizeof(cpu_set_t)));

    hwloc_bitmap_free(cpuset);
    return 0;
}

static xkrt_device_t *
XKRT_DRIVER_ENTRYPOINT(device_create)(xkrt_driver_t * driver, int device_driver_id)
{
    assert(device_driver_id >= 0 && device_driver_id < XKRT_DEVICES_MAX);

    xkrt_device_cl_t * device = device_cl_get(device_driver_id);
    assert(device->inherited.state == XKRT_DEVICE_STATE_DEALLOCATED);

    // nothing to do

    return (xkrt_device_t *) device;
}

static void
XKRT_DRIVER_ENTRYPOINT(device_init)(int device_driver_id)
{
    // TODO : move some stuff from driver init to here
}

static int
XKRT_DRIVER_ENTRYPOINT(device_destroy)(int device_driver_id)
{
    return 0;
}

/* Called for each device of the driver once they all have been initialized */
static int
XKRT_DRIVER_ENTRYPOINT(device_commit)(int device_driver_id, xkrt_device_global_id_bitfield_t * affinity)
{
    return 0;
}

void
XKRT_DRIVER_ENTRYPOINT(device_info)(
    int device_driver_id,
    char * buffer,
    size_t size
) {
    xkrt_device_cl_t * device = device_cl_get(device_driver_id);

    char name[64];
    CL_SAFE_CALL(clGetDeviceInfo(device->cl.id, CL_DEVICE_NAME, sizeof(name), name, NULL));

    char vendor[64];
    CL_SAFE_CALL(clGetDeviceInfo(device->cl.id, CL_DEVICE_VENDOR, sizeof(vendor), vendor, NULL));

    cl_ulong max_mem_alloc_size;
    CL_SAFE_CALL(clGetDeviceInfo(device->cl.id, CL_DEVICE_MAX_MEM_ALLOC_SIZE, sizeof(cl_ulong), &max_mem_alloc_size, NULL));

    snprintf(
        buffer,
        size,
        "XKRT device %d named %s of vendor %s - max-mem-alloc-size=%.2lfGB",
        device_driver_id,
        name,
        vendor,
        max_mem_alloc_size / 1e9
    );
}

////////////
// STREAM //
////////////

static int
XKRT_DRIVER_ENTRYPOINT(stream_suggest)(
    int device_driver_id,
    xkrt_stream_type_t stype
) {
    switch (stype)
    {
        case (XKRT_STREAM_TYPE_KERN):
            return 8;
        default:
            return 2;
    }
}

static int
XKRT_DRIVER_ENTRYPOINT(stream_instruction_launch)(
    xkrt_stream_t * istream,
    xkrt_stream_instruction_t * instr,
    xkrt_stream_instruction_counter_t idx
) {
    xkrt_stream_cl_t * stream = (xkrt_stream_cl_t *) istream;
    assert(stream);

    cl_event * event = stream->cl.events + idx;

    switch (instr->type)
    {
        case (XKRT_STREAM_INSTR_TYPE_COPY_H2D_1D):
        case (XKRT_STREAM_INSTR_TYPE_COPY_D2H_1D):
        case (XKRT_STREAM_INSTR_TYPE_COPY_D2D_1D):
        {
            const uintptr_t dst = instr->copy_1D.dst_device_addr;
            const uintptr_t src = instr->copy_1D.src_device_addr;
            const size_t size   = instr->copy_1D.size;

            const cl_bool blocking = CL_FALSE;

            cl_uint num_events_in_wait_list = 0;
            const cl_event * event_wait_list = NULL;

            switch (instr->type)
            {
                case (XKRT_STREAM_INSTR_TYPE_COPY_H2D_1D):
                {
                    cl_mem dst_buffer;
                    size_t dst_offset;
                    xkrt_driver_cl_get_buffer_and_offset_1D(stream->device, (uintptr_t) dst, &dst_buffer, &dst_offset);

                    CL_SAFE_CALL(
                        clEnqueueWriteBuffer(
                            stream->cl.queue,
                            dst_buffer,
                            blocking,
                            dst_offset,
                            size,
                            (const void *) src,
                            num_events_in_wait_list,
                            event_wait_list,
                            event
                        )
                    );
                    break ;
                }

                case (XKRT_STREAM_INSTR_TYPE_COPY_D2H_1D):
                {
                    size_t src_offset;
                    cl_mem src_buffer;
                    xkrt_driver_cl_get_buffer_and_offset_1D(stream->device, (uintptr_t) src, &src_buffer, &src_offset);

                    CL_SAFE_CALL(
                        clEnqueueReadBuffer(
                            stream->cl.queue,
                            src_buffer,
                            blocking,
                            src_offset,
                            size,
                            (void *) dst,
                            num_events_in_wait_list,
                            event_wait_list,
                            event
                        )
                    );
                    break ;
                }

                case (XKRT_STREAM_INSTR_TYPE_COPY_D2D_1D):
                {
                    cl_mem src_buffer;
                    size_t src_offset;
                    xkrt_device_cl_t * src_device = device_cl_get_from_addr(src);
                    xkrt_driver_cl_get_buffer_and_offset_1D(src_device, (uintptr_t) src, &src_buffer, &src_offset);

                    cl_mem dst_buffer;
                    size_t dst_offset;
                    xkrt_device_cl_t * dst_device = device_cl_get_from_addr(dst);
                    xkrt_driver_cl_get_buffer_and_offset_1D(dst_device, (uintptr_t) dst, &dst_buffer, &dst_offset);

                    CL_SAFE_CALL(
                        clEnqueueCopyBuffer(
                            stream->cl.queue,
                            src_buffer,
                            dst_buffer,
                            src_offset,
                            dst_offset,
                            size,
                            num_events_in_wait_list,
                            event_wait_list,
                            event
                        )
                    );

                    break ;
                }

                default:
                    LOGGER_FATAL("unreachable");
            }
            break ;
        }

        case (XKRT_STREAM_INSTR_TYPE_COPY_H2D_2D):
        case (XKRT_STREAM_INSTR_TYPE_COPY_D2H_2D):
        case (XKRT_STREAM_INSTR_TYPE_COPY_D2D_2D):
        {
            const uintptr_t dst     = instr->copy_2D.dst_device_view.addr;
            const uintptr_t src     = instr->copy_2D.src_device_view.addr;

            size_t dst_row_pitch    = instr->copy_2D.dst_device_view.ld * instr->copy_2D.sizeof_type;
            size_t src_row_pitch    = instr->copy_2D.src_device_view.ld * instr->copy_2D.sizeof_type;

            // assume col major - if not, need to do some shit here
            const size_t width  = instr->copy_2D.m * instr->copy_2D.sizeof_type;
            const size_t height = instr->copy_2D.n;
            assert(width >= 0);
            assert(height >= 0);

            const cl_bool blocking = CL_FALSE;

            size_t dst_origin[] = {0, 0, 0};
            size_t src_origin[] = {0, 0, 0};
            const size_t region[]     = {width, height, 1};

            const size_t dst_slice_pitch = 0;
            const size_t src_slice_pitch = 0;

            cl_uint num_events_in_wait_list = 0;
            const cl_event * event_wait_list = NULL;

            switch (instr->type)
            {
                case (XKRT_STREAM_INSTR_TYPE_COPY_H2D_2D):
                {
                    cl_mem dst_buffer;
                    xkrt_driver_cl_get_buffer_and_offset_2D(stream->device, (uintptr_t) dst, dst_row_pitch, &dst_buffer, dst_origin);

                    CL_SAFE_CALL(
                        clEnqueueWriteBufferRect(
                            stream->cl.queue,
                            dst_buffer,
                            blocking,
                            dst_origin,
                            src_origin,
                            region,
                            dst_row_pitch,
                            dst_slice_pitch,
                            src_row_pitch,
                            src_slice_pitch,
                            (const void *) src,
                            num_events_in_wait_list,
                            event_wait_list,
                            event
                        )
                    );
                    break ;
                }

                case (XKRT_STREAM_INSTR_TYPE_COPY_D2H_2D):
                {
                    cl_mem src_buffer;
                    xkrt_driver_cl_get_buffer_and_offset_2D(stream->device, (uintptr_t) src, src_row_pitch, &src_buffer, src_origin);

                    CL_SAFE_CALL(
                        clEnqueueReadBufferRect(
                            stream->cl.queue,
                            src_buffer,
                            blocking,
                            src_origin,
                            dst_origin,
                            region,
                            src_row_pitch,
                            src_slice_pitch,
                            dst_row_pitch,
                            dst_slice_pitch,
                            (void *) dst,
                            num_events_in_wait_list,
                            event_wait_list,
                            event
                        )
                    );
                    break ;
                }

                case (XKRT_STREAM_INSTR_TYPE_COPY_D2D_2D):
                {
                    cl_mem src_buffer;
                    xkrt_device_cl_t * src_device = device_cl_get_from_addr(src);
                    xkrt_driver_cl_get_buffer_and_offset_2D(src_device, (uintptr_t) src, src_row_pitch, &src_buffer, src_origin);

                    cl_mem dst_buffer;
                    xkrt_device_cl_t * dst_device = device_cl_get_from_addr(dst);
                    xkrt_driver_cl_get_buffer_and_offset_2D(dst_device, (uintptr_t) dst, dst_row_pitch, &dst_buffer, dst_origin);

                    CL_SAFE_CALL(
                        clEnqueueCopyBufferRect(
                            stream->cl.queue,
                            src_buffer,
                            dst_buffer,
                            src_origin,
                            dst_origin,
                            region,
                            src_row_pitch,
                            src_slice_pitch,
                            dst_row_pitch,
                            dst_slice_pitch,
                            num_events_in_wait_list,
                            event_wait_list,
                            event
                        )
                    );

                    break ;
                }

                default:
                {
                    LOGGER_FATAL("instr->type got modified, something went really wrong");
                    break ;
                }
            }
            break ;
        }

        default:
            return EINVAL;
    }

    // that flush may be unnecessary
    CL_SAFE_CALL(clFlush(stream->cl.queue));
    return EINPROGRESS;
}

static int
XKRT_DRIVER_ENTRYPOINT(stream_instructions_wait)(
    xkrt_stream_t * istream
) {
    xkrt_stream_cl_t * stream = (xkrt_stream_cl_t *) istream;
    assert(stream);

    CL_SAFE_CALL(clFinish(stream->cl.queue));
    return 0;
}

static int
XKRT_DRIVER_ENTRYPOINT(stream_instructions_progress)(
    xkrt_stream_t * istream,
    xkrt_stream_instruction_counter_t a,
    xkrt_stream_instruction_counter_t b
) {
    assert(istream);

    xkrt_stream_cl_t * stream = (xkrt_stream_cl_t *) istream;
    int r = 0;

    for (xkrt_stream_instruction_counter_t idx = a ; idx < b ; ++idx)
    {
        xkrt_stream_instruction_t * instr = istream->pending.instr + idx;
        cl_event event = stream->cl.events[idx];

        switch (instr->type)
        {
            case (XKRT_STREAM_INSTR_TYPE_KERN):
            case (XKRT_STREAM_INSTR_TYPE_COPY_H2D_1D):
            case (XKRT_STREAM_INSTR_TYPE_COPY_H2H_1D):
            case (XKRT_STREAM_INSTR_TYPE_COPY_D2H_1D):
            case (XKRT_STREAM_INSTR_TYPE_COPY_D2D_1D):
            case (XKRT_STREAM_INSTR_TYPE_COPY_H2D_2D):
            case (XKRT_STREAM_INSTR_TYPE_COPY_H2H_2D):
            case (XKRT_STREAM_INSTR_TYPE_COPY_D2H_2D):
            case (XKRT_STREAM_INSTR_TYPE_COPY_D2D_2D):
            {
                /* poll event */
                for (int i = 0 ; i < 4 ; ++i)
                {
                    cl_int event_status;
                    CL_SAFE_CALL(clGetEventInfo(event, CL_EVENT_COMMAND_EXECUTION_STATUS, sizeof(cl_int), &event_status, NULL));
                    if (event_status == CL_COMPLETE)
                    {
                        istream->complete_instruction(idx);
                        goto next_instr;
                    }
                    sched_yield();
                }
                r = EINPROGRESS;
                break ;
            }

            default:
                LOGGER_FATAL("Wrong instruction");
        }

        next_instr:
            continue ;
    }

    return r;
}


static xkrt_stream_t *
XKRT_DRIVER_ENTRYPOINT(stream_create)(
    xkrt_device_t * idevice,
    xkrt_stream_type_t type,
    xkrt_stream_instruction_counter_t capacity
) {
    assert(idevice);

    uint8_t * mem = (uint8_t *) malloc(sizeof(xkrt_stream_cl_t) + sizeof(cl_event) * capacity);
    assert(mem);

    xkrt_stream_init(
        (xkrt_stream_t *) mem,
        type,
        capacity,
        XKRT_DRIVER_ENTRYPOINT(stream_instruction_launch),
        XKRT_DRIVER_ENTRYPOINT(stream_instructions_progress),
        XKRT_DRIVER_ENTRYPOINT(stream_instructions_wait)
    );

    xkrt_device_cl_t * device = (xkrt_device_cl_t *) idevice;
    xkrt_stream_cl_t * stream = (xkrt_stream_cl_t *) mem;

    // TODO : no control over the queue type with OpenCL
    (void) type;

    // create a queue
    const cl_queue_properties properties[] = {
        CL_QUEUE_PROPERTIES,
        CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE | CL_QUEUE_ON_DEVICE,    // not sure why we must specify 'CL_QUEUE_ON_DEVICE' ?
        CL_QUEUE_SIZE,
        CL_DEVICE_QUEUE_ON_DEVICE_PREFERRED_SIZE,                       // default parameter
        0                                                               // end of properties
    };
    int err;
    stream->cl.queue = clCreateCommandQueueWithProperties(device->cl.context, device->cl.id, 0, &err);
    CL_SAFE_CALL(err);

    // create events
    stream->cl.events = (cl_event *) (stream + 1);
    for (xkrt_stream_instruction_counter_t i = 0 ; i < capacity ; ++i)
    {
        int err;
        stream->cl.events[i] = clCreateUserEvent(device->cl.context, &err);
        CL_SAFE_CALL(err);
    }

    // save context for later buffer use
    stream->device = device;

    return (xkrt_stream_t *) stream;
}

static void
XKRT_DRIVER_ENTRYPOINT(stream_delete)(
    xkrt_stream_t * istream
) {
    xkrt_stream_cl_t * stream = (xkrt_stream_cl_t *) istream;

    for (xkrt_stream_instruction_counter_t i = 0 ; i < istream->pending.capacity ; ++i)
        CL_SAFE_CALL(clReleaseEvent(stream->cl.events[i]));

    CL_SAFE_CALL(clReleaseCommandQueue(stream->cl.queue));

    free(stream);
}

////////////
// MEMORY //
////////////

static void *
XKRT_DRIVER_ENTRYPOINT(memory_device_allocate)(int device_driver_id, const size_t size, int area_idx)
{
    assert(area_idx == 0);

    xkrt_device_cl_t * device = device_cl_get(device_driver_id);

    if (device->memory.nbuffers >= XKRT_DRIVER_CL_MAX_BUFFERS)
        LOGGER_FATAL("More than `XKRT_DRIVER_CL_MAX_BUFFERS` = %d memory allocations on CL drivers. Increase it and recompile.",
                device->memory.nbuffers);

    // OpenCL does not allow to directly offset pointers.
    // So, we map buffer to a virtual address space per device

    // create a device-specific virtual memory range
    int err;
    xkrt_device_cl_buffer_t * buffer = device->memory.buffers + device->memory.nbuffers++;
    buffer->addr = device->memory.head;
    buffer->size = size;
    buffer->cl.mem = clCreateBuffer(device->cl.context, CL_MEM_READ_WRITE, size, NULL, &err);
    CL_SAFE_CALL(err);

    // overflow check
    assert(size);
    assert(device->memory.head + size > device->memory.head);
    device->memory.head += size;

    return (void *) buffer->addr;
}

static void
XKRT_DRIVER_ENTRYPOINT(memory_device_info)(int device_driver_id, xkrt_device_memory_info_t info[XKRT_DEVICE_MEMORIES_MAX], int * nmemories)
{
    xkrt_device_cl_t * device = device_cl_get(device_driver_id);

    cl_ulong max_mem_alloc_size;
    CL_SAFE_CALL(clGetDeviceInfo(device->cl.id, CL_DEVICE_MAX_MEM_ALLOC_SIZE, sizeof(cl_ulong), &max_mem_alloc_size, NULL));
    info[0].capacity = (size_t) max_mem_alloc_size;
    info[0].used     = SIZE_MAX;
    strncpy(info[0].name, "(null)", sizeof(info[0].name));
    *nmemories = 1;
}

static int
XKRT_DRIVER_ENTRYPOINT(memory_register)(
    void * ptr,
    uint64_t size
) {
    LOGGER_WARN("OpenCL driver has no support for memory regsiter");
    return 0;
}

static int
XKRT_DRIVER_ENTRYPOINT(memory_unregister)(
    void * ptr,
    uint64_t size
) {
    LOGGER_WARN("OpenCL driver has no support for memory regsiter");
    return 0;
}

//////////////////////////
// Routine registration //
//////////////////////////
xkrt_driver_t *
XKRT_DRIVER_ENTRYPOINT(create_driver)(void)
{
    xkrt_driver_cl_t * driver = (xkrt_driver_cl_t *) calloc(1, sizeof(xkrt_driver_cl_t));
    assert(driver);

    # define REGISTER(func) driver->super.f_##func = XKRT_DRIVER_ENTRYPOINT(func)

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
    REGISTER(memory_device_allocate);
    // REGISTER(memory_device_allocate_on);
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

    # undef REGISTER

    return (xkrt_driver_t *) driver;
}
