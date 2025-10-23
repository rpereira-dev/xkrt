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

# include <stdlib.h>
# include <sys/stat.h>
# include <sys/ioctl.h>
# include <sys/syscall.h>
# include <sys/mman.h>
# include <sys/uio.h>
# include <linux/fs.h>
# include <fcntl.h>
# include <unistd.h>
# include <string.h>
# include <stdatomic.h>

# include <linux/io_uring.h>

XKRT_NAMESPACE_BEGIN

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

static device_t *
XKRT_DRIVER_ENTRYPOINT(device_create)(driver_t * driver, int device_driver_id)
{
    (void) driver;
    assert(device_driver_id == 0);
    static device_t device;
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
XKRT_DRIVER_ENTRYPOINT(device_commit)(int device_driver_id, device_global_id_bitfield_t * affinity)
{
    (void) device_driver_id;
    (void) affinity;
    return 0;
}

////////////
// STREAM //
////////////

/* Macros for barriers needed by io_uring */
# define io_uring_smp_store_release(p, v)                                                       \
    (reinterpret_cast<std::atomic<std::remove_reference_t<decltype(*(p))>>*>(p)->store((v), std::memory_order_release))

#define io_uring_smp_load_acquire(p)                                                            \
    (reinterpret_cast<std::atomic<std::remove_reference_t<decltype(*(p))>>*>(p)->load(std::memory_order_acquire))

/** see : https://man7.org/linux/man-pages/man7/io_uring.7.html */
static inline void
XKRT_DRIVER_ENTRYPOINT(io_uring_init)(stream_host_t * stream)
{
    // TODO: what is this ?
    # define QUEUE_DEPTH 1
    struct io_uring_params p;
    memset(&p, 0, sizeof(p));
    stream->io_uring.fd = (int) syscall(__NR_io_uring_setup, QUEUE_DEPTH, &p);
    // stream->io_uring.fd = io_uring_setup(QUEUE_DEPTH, &p);
    # undef QUEUE_DEPTH

    /*
     * io_uring communication happens via 2 shared kernel-user space ring
     * buffers, which can be jointly mapped with a single mmap() call in
     * kernels >= 5.4.
     */

    int sring_sz = p.sq_off.array + p.sq_entries * sizeof(unsigned);
    int cring_sz = p.cq_off.cqes + p.cq_entries * sizeof(struct io_uring_cqe);

    /* Rather than check for kernel version, the recommended way is to
     * check the features field of the io_uring_params structure, which is a
     * bitmask. If IORING_FEAT_SINGLE_MMAP is set, we can do away with the
     * second mmap() call to map in the completion ring separately.
     */
    if (p.features & IORING_FEAT_SINGLE_MMAP)
    {
        if (cring_sz > sring_sz)
            sring_sz = cring_sz;
        cring_sz = sring_sz;
    }

    /* Map in the submission and completion queue ring buffers.
     *  Kernels < 5.4 only map in the submission queue, though. */

    stream->io_uring.sq_ptr = (unsigned char *) mmap(0, sring_sz, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_POPULATE, stream->io_uring.fd, IORING_OFF_SQ_RING);
    if (stream->io_uring.sq_ptr == MAP_FAILED)
        LOGGER_FATAL("Failed to mmap io_uring for asynchronous file i/o (1)");

    if (p.features & IORING_FEAT_SINGLE_MMAP) {
        stream->io_uring.cq_ptr = stream->io_uring.sq_ptr;
    } else {
        /* Map in the completion queue ring buffer in older kernels separately */
        stream->io_uring.cq_ptr = (unsigned char *) mmap(0, cring_sz, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_POPULATE, stream->io_uring.fd, IORING_OFF_CQ_RING);
        if (stream->io_uring.cq_ptr == MAP_FAILED)
            LOGGER_FATAL("Failed to mmap io_uring for asynchronous file i/o (2)");
    }

    /* Save useful fields for later easy reference */
    stream->io_uring.sq_tail  = stream->io_uring.sq_ptr + p.sq_off.tail;
    stream->io_uring.sq_mask  = stream->io_uring.sq_ptr + p.sq_off.ring_mask;
    stream->io_uring.sq_array = stream->io_uring.sq_ptr + p.sq_off.array;

    /* Map in the submission queue entries array */
    stream->io_uring.sqes = (struct io_uring_sqe *) mmap(0, p.sq_entries * sizeof(struct io_uring_sqe), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_POPULATE, stream->io_uring.fd, IORING_OFF_SQES);
    if (stream->io_uring.sqes == MAP_FAILED)
        LOGGER_FATAL("Failed to mmap io_uring for asynchronous file i/o (3)");

    /* Save useful fields for later easy reference */
    stream->io_uring.cq_head = stream->io_uring.cq_ptr + p.cq_off.head;
    stream->io_uring.cq_tail = stream->io_uring.cq_ptr + p.cq_off.tail;
    stream->io_uring.cq_mask = stream->io_uring.cq_ptr + p.cq_off.ring_mask;
    stream->io_uring.cqes    = (struct io_uring_cqe *) (stream->io_uring.cq_ptr + p.cq_off.cqes);
}

static int
XKRT_DRIVER_ENTRYPOINT(stream_instruction_launch)(
    stream_t * istream,
    stream_instruction_t * instr,
    stream_instruction_counter_t idx
) {
    (void) istream;
    (void) idx;

    assert(instr->type == XKRT_STREAM_INSTR_TYPE_FD_READ ||
            instr->type == XKRT_STREAM_INSTR_TYPE_FD_WRITE);

    stream_host_t * stream = (stream_host_t *) istream;

    switch (instr->type)
    {
        case (XKRT_STREAM_INSTR_TYPE_FD_READ):
        case (XKRT_STREAM_INSTR_TYPE_FD_WRITE):
        {
            if (stream->io_uring.sq_ptr == NULL)
                XKRT_DRIVER_ENTRYPOINT(io_uring_init)(stream);
            assert(stream->io_uring.sq_ptr);

            /* Add our submission queue entry to the tail of the SQE ring buffer */
            unsigned tail  = *stream->io_uring.sq_tail;
            unsigned index = tail & *stream->io_uring.sq_mask;
            struct io_uring_sqe * sqe = &stream->io_uring.sqes[index];

            /* Fill in the parameters required for the read or write operation */
            sqe->opcode    = (instr->type == XKRT_STREAM_INSTR_TYPE_FD_READ) ? IORING_OP_READ : IORING_OP_WRITE;
            sqe->fd        = instr->file.fd;
            sqe->addr      = (unsigned long) instr->file.buffer;
            sqe->len       = instr->file.n;
            sqe->off       = instr->file.offset;
            sqe->user_data = (__u64) idx;

            stream->io_uring.sq_array[index] = index;
            ++tail;

            /* Update the tail */
            io_uring_smp_store_release(stream->io_uring.sq_tail, tail);

            /* Tell the kernel we have submitted events with the io_uring_enter() system call. */
            const int to_submit      = 1;
            const int min_complete   = 0;
            const unsigned int flags = 0; // IORING_ENTER_GETEVENTS;
            int r = (int) syscall(__NR_io_uring_enter, stream->io_uring.fd, to_submit, min_complete, flags, NULL, 0);
            if (r < 0)
                LOGGER_FATAL("__NR_io_uring_enter");

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
    stream_type_t stype
) {
    assert(device_driver_id == 0);
    switch (stype)
    {
        case (STREAM_TYPE_FD_READ):
        case (STREAM_TYPE_FD_WRITE):
            return 1;

        default:
            return 0;
    }
    return 0;
}

static inline int
XKRT_DRIVER_ENTRYPOINT(stream_instructions_wait)(
    stream_t * istream
) {
    assert(istream);
    stream_host_t * stream = (stream_host_t *) istream;

    switch (stream->super.type)
    {
        case (STREAM_TYPE_FD_READ):
        case (STREAM_TYPE_FD_WRITE):
        {
            int min_completion = stream->super.pending.size();
            if (min_completion)
            {
                LOGGER_DEBUG("Waiting for %d i/o instructions to complete", min_completion);
                int r = (int) syscall(__NR_io_uring_enter, stream->io_uring.fd, 0, min_completion, IORING_ENTER_GETEVENTS, NULL, 0);
                if (r < 0)
                    LOGGER_FATAL("__NR_io_uring_enter (wait) failed: %s", strerror(errno));
            }
            break ;
        }

        default:
            LOGGER_FATAL("Not supported");
    }
    return 0;
}

static inline int
XKRT_DRIVER_ENTRYPOINT(stream_instruction_wait)(
    stream_t * istream,
    stream_instruction_t * instr,
    stream_instruction_counter_t idx
) {
    assert(instr);

    // TODO: currently naively wait for all events to complete, instead we
    // should wait for the specific i/o instruction
    XKRT_DRIVER_ENTRYPOINT(stream_instructions_wait)(istream);

    # if 0
    switch (instr->type)
    {
        case (STREAM_TYPE_FD_READ):
        case (STREAM_TYPE_FD_WRITE):
        {
        }

        default:
            LOGGER_FATAL("Not supported");
    }
    # endif

    return 0;
}

static int
XKRT_DRIVER_ENTRYPOINT(stream_instructions_progress)(
    stream_t * istream
) {
    assert(istream);

    stream_host_t * stream = (stream_host_t *) istream;
    int r = 0;

    // no need to iterate through instr, its saved in the completion queue for I/O Instructions
    {
        /*
         * Read from completion queue.
         * We read completion events from the completion queue.
         * We dequeue the CQE, update and head and return the result of the operation.
         */

        while (1)
        {
            /* Read barrier */
            unsigned head = io_uring_smp_load_acquire(stream->io_uring.cq_head);

            /* If head == tail, it means that the buffer is empty. */
            if (head == *stream->io_uring.cq_tail)
                break ;

            /* Get the entry */
            struct io_uring_cqe * cqe = &stream->io_uring.cqes[head & (*stream->io_uring.cq_mask)];

            if (cqe->res < 0)
                LOGGER_FATAL("Error: %s", strerror(abs(cqe->res)));
            else
            {
                const stream_instruction_counter_t p = (const stream_instruction_counter_t) cqe->user_data;
                stream_instruction_t * instr = istream->pending.instr + p;
                assert(instr);
                assert(instr->completed == false);
                assert(cqe->res == instr->file.n);

                ++head;

                /* Write barrier so that update to the head are made visible */
                io_uring_smp_store_release(stream->io_uring.cq_head, head);

                /* complete kinstruction */
                istream->complete_instruction(instr);
            }
        }
    }

    return r;
}

static stream_t *
XKRT_DRIVER_ENTRYPOINT(stream_create)(
    device_t * idevice,
    stream_type_t type,
    stream_instruction_counter_t capacity
) {
    (void)idevice;
    (void)type;
    (void)capacity;

    if (type != STREAM_TYPE_FD_READ && type != STREAM_TYPE_FD_WRITE)
        return NULL;
    assert(type == STREAM_TYPE_FD_READ || type == STREAM_TYPE_FD_WRITE);

    uint8_t * mem = (uint8_t *) malloc(sizeof(stream_host_t));
    assert(mem);

    stream_host_t * stream = (stream_host_t *) mem;

    /*************************/
    /* init xkrt stream */
    /*************************/
    stream_init(
        (stream_t *) stream,
        type,
        capacity,
        XKRT_DRIVER_ENTRYPOINT(stream_instruction_launch),
        XKRT_DRIVER_ENTRYPOINT(stream_instructions_progress),
        XKRT_DRIVER_ENTRYPOINT(stream_instructions_wait),
        XKRT_DRIVER_ENTRYPOINT(stream_instruction_wait)
    );

    /*************************/
    /* do host specific init */
    /*************************/

    // nothing to do

    return (stream_t *) stream;
}

static void
XKRT_DRIVER_ENTRYPOINT(stream_delete)(
    stream_t * istream
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
    device_memory_info_t info[XKRT_DEVICE_MEMORIES_MAX],
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

driver_module_t
XKRT_DRIVER_ENTRYPOINT(module_load)(
    int device_driver_id,
    uint8_t * bin,
    size_t binsize,
    driver_module_format_t format
) {
    (void)device_driver_id;
    (void)bin;
    (void)binsize;
    (void)format;
    return NULL;
}

void
XKRT_DRIVER_ENTRYPOINT(module_unload)(
    driver_module_t module
) {
    (void)module;
}

driver_module_fn_t
XKRT_DRIVER_ENTRYPOINT(module_get_fn)(
    driver_module_t module,
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
driver_t *
XKRT_DRIVER_ENTRYPOINT(create_driver)(void)
{
    driver_t * driver = (driver_t *) calloc(1, sizeof(driver_t));
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

    return (driver_t *) driver;
}

XKRT_NAMESPACE_END
