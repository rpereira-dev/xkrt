/*
** Copyright 2024,2025 INRIA
**
** Contributors :
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

#define XKRT_DRIVER_ENTRYPOINT(N) XKRT_DRIVER_TYPE_HOST_##N

#include <xkrt/conf/conf.h>
#include <xkrt/driver/device.hpp>
#include <xkrt/driver/driver-host.h>
#include <xkrt/driver/driver.h>
#include <xkrt/driver/queue.h>
#include <xkrt/internals.h>
#include <xkrt/runtime.h>
#include <xkrt/sync/atomic.h>
#include <xkrt/sync/bits.h>
#include <xkrt/sync/mutex.h>

#include <hwloc.h>
#include <hwloc/glibc-sched.h>
#include <sys/sysinfo.h>

#include <cassert>
#include <cstdio>
#include <cstdint>
#include <cerrno>
#include <functional>

#include <stdlib.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/syscall.h>
#include <sys/mman.h>
#include <sys/uio.h>
#include <linux/fs.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdatomic.h>

#include <linux/io_uring.h>

XKRT_NAMESPACE_BEGIN

/* ─── Cached hwloc topology (initialized once) ─── */
static hwloc_topology_t g_topology = nullptr;
static char g_cpu_name[256] = {0};

static void
ensure_topology(void)
{
    if (__builtin_expect(g_topology != nullptr, 1))
        return;

    hwloc_topology_init(&g_topology);
    hwloc_topology_load(g_topology);

    hwloc_obj_t obj = hwloc_get_obj_by_type(g_topology, HWLOC_OBJ_PACKAGE, 0);
    if (obj && obj->name)
        snprintf(g_cpu_name, sizeof(g_cpu_name), "%s", obj->name);
    else
        snprintf(g_cpu_name, sizeof(g_cpu_name), "Unknown CPU");
}

/* ─── io_uring barrier macros ─── */
#define io_uring_smp_store_release(p, v)    \
    (reinterpret_cast<std::atomic<std::remove_reference_t<decltype(*(p))>>*>(p) \
        ->store((v), std::memory_order_release))

#define io_uring_smp_load_acquire(p)        \
    (reinterpret_cast<std::atomic<std::remove_reference_t<decltype(*(p))>>*>(p) \
        ->load(std::memory_order_acquire))

/* ─── Driver entrypoints ─── */

static int
XKRT_DRIVER_ENTRYPOINT(init)(
    unsigned int ndevices,
    bool use_p2p
) {
    (void)ndevices;
    (void)use_p2p;
    ensure_topology();
    return 0;
}

static void
XKRT_DRIVER_ENTRYPOINT(device_info)(
    device_driver_id_t device_driver_id,
    char *buffer,
    size_t size
) {
    (void)device_driver_id;
    ensure_topology();
    snprintf(buffer, size, "%s", g_cpu_name);
}

static void
XKRT_DRIVER_ENTRYPOINT(finalize)(void)
{
    if (g_topology)
    {
        hwloc_topology_destroy(g_topology);
        g_topology = nullptr;
    }
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
XKRT_DRIVER_ENTRYPOINT(device_cpuset)(
    hwloc_topology_t topology,
    cpu_set_t *schedset,
    device_driver_id_t device_driver_id
) {
    (void)topology;
    assert(device_driver_id == 0);
    pthread_getaffinity_np(pthread_self(), sizeof(cpu_set_t), schedset);
    return 0;
}

static device_t *
XKRT_DRIVER_ENTRYPOINT(device_create)(
    driver_t *driver,
    device_driver_id_t device_driver_id
) {
    (void)driver;
    assert(device_driver_id == 0);
    static device_t device;
    return &device;
}

static void
XKRT_DRIVER_ENTRYPOINT(device_init)(device_driver_id_t device_driver_id)
{
    (void)device_driver_id;
}

static int
XKRT_DRIVER_ENTRYPOINT(device_destroy)(device_driver_id_t device_driver_id)
{
    (void)device_driver_id;
    return 0;
}

static int
XKRT_DRIVER_ENTRYPOINT(device_commit)(
    device_driver_id_t device_driver_id,
    device_unique_id_bitfield_t *affinity
) {
    (void)device_driver_id;
    (void)affinity;
    return 0;
}

//////////////////////////////////////
// QUEUE - io_uring based async I/O //
//////////////////////////////////////

static inline void
XKRT_DRIVER_ENTRYPOINT(io_uring_init)(queue_host_t * queue)
{
    struct io_uring_params p;
    memset(&p, 0, sizeof(p));
    queue->io_uring.fd = (int) syscall(__NR_io_uring_setup, XKRT_IO_URING_DEPTH, &p);
    if (queue->io_uring.fd < 0)
        LOGGER_FATAL("io_uring_setup failed: %s", strerror(errno));

    /* Compute ring buffer sizes */
    size_t sring_sz = p.sq_off.array + p.sq_entries * sizeof(unsigned);
    size_t cring_sz = p.cq_off.cqes  + p.cq_entries * sizeof(struct io_uring_cqe);

    const bool single_mmap = (p.features & IORING_FEAT_SINGLE_MMAP);
    if (single_mmap)
    {
        if (cring_sz > sring_sz)
            sring_sz = cring_sz;
        cring_sz = sring_sz;
    }

    /* Map submission ring */
    queue->io_uring.sq_ptr = mmap(0, sring_sz,
        PROT_READ | PROT_WRITE, MAP_SHARED | MAP_POPULATE, queue->io_uring.fd, IORING_OFF_SQ_RING);
    if (queue->io_uring.sq_ptr == MAP_FAILED)
        LOGGER_FATAL("Failed to mmap io_uring SQ ring");

    /* Map completion ring */
    if (single_mmap)
    {
        queue->io_uring.cq_ptr = queue->io_uring.sq_ptr;
    } else {
        queue->io_uring.cq_ptr = mmap(0, cring_sz,
            PROT_READ | PROT_WRITE, MAP_SHARED | MAP_POPULATE, queue->io_uring.fd, IORING_OFF_CQ_RING);
        if (queue->io_uring.cq_ptr == MAP_FAILED)
            LOGGER_FATAL("Failed to mmap io_uring CQ ring");
    }

    /* Cache ring pointers — computed once, used on every submit/complete */
    char *sq = (char *)queue->io_uring.sq_ptr;
    queue->io_uring.sq_tail  = (unsigned *)(sq + p.sq_off.tail);
    queue->io_uring.sq_mask  = (unsigned *)(sq + p.sq_off.ring_mask);
    queue->io_uring.sq_array = (unsigned *)(sq + p.sq_off.array);
    queue->io_uring.sq_flags = (unsigned *)(sq + p.sq_off.flags);

    /* Map SQE array */
    queue->io_uring.sqes = (struct io_uring_sqe *)mmap(0,
        p.sq_entries * sizeof(struct io_uring_sqe),
        PROT_READ | PROT_WRITE, MAP_SHARED | MAP_POPULATE, queue->io_uring.fd, IORING_OFF_SQES);
    if (queue->io_uring.sqes == MAP_FAILED)
        LOGGER_FATAL("Failed to mmap io_uring SQEs");

    char * cq = (char *)queue->io_uring.cq_ptr;
    queue->io_uring.cq_head = (unsigned *)(cq + p.cq_off.head);
    queue->io_uring.cq_tail = (unsigned *)(cq + p.cq_off.tail);
    queue->io_uring.cq_mask = (unsigned *)(cq + p.cq_off.ring_mask);
    queue->io_uring.cqes    = (struct io_uring_cqe *)(cq + p.cq_off.cqes);

    /* Pre-fill the sq_array with identity mapping (index == slot).
     * This never changes for our usage pattern, so do it once. */
    for (unsigned i = 0; i < p.sq_entries; ++i)
        queue->io_uring.sq_array[i] = i;

    queue->io_uring.pending_submits = 0;
}

/*
 * Submit any SQEs that have been queued but not yet submitted to the kernel.
 */
static inline void
io_uring_flush_submits(queue_host_t * queue)
{
    unsigned pending = queue->io_uring.pending_submits;
    if (pending == 0)
        return;

    unsigned enter_flags = 0;
    int r = (int)syscall(__NR_io_uring_enter, queue->io_uring.fd, pending, 0, enter_flags, NULL, 0);
    if (r < 0)
        LOGGER_FATAL("io_uring_enter (submit) failed: %s", strerror(errno));

    queue->io_uring.pending_submits = 0;
}

void XKRT_DRIVER_ENTRYPOINT(command_graph_replay_node_complete)(
    runtime_t * runtime,
    command_graph_t * cg,
    command_graph_node_t * node
);

static void
XKRT_DRIVER_ENTRYPOINT(command_graph_replay_node_completion_callback)(
    void * args[XKRT_CALLBACK_ARGS_MAX]
) {
    runtime_t            * runtime = (runtime_t *)            args[0];
    command_graph_t      * cg      = (command_graph_t *)      args[1];
    command_graph_node_t * node    = (command_graph_node_t *) args[2];
    return XKRT_DRIVER_ENTRYPOINT(command_graph_replay_node_complete)(runtime, cg, node);
}

/* Whether the edge pred -> succ is ordered on the device: succ waits on the
 * device for pred, and is released by pred's launch rather than its
 * completion (see command_graph_ordering_setup) */
static inline bool
__command_graph_edge_on_device(
    cgir::command_graph_node_t * pred,
    cgir::command_graph_node_t * succ
) {
    if (succ->type != cgir::COMMAND_GRAPH_NODE_TYPE_COMMAND || pred->type != cgir::COMMAND_GRAPH_NODE_TYPE_COMMAND)
        return false;
    const command_t * s = (const command_t *) succ->command;
    for (uint32_t i = 0 ; i < s->order.nwaits ; ++i)
        if (s->order.waits[i] == pred->command)
            return true;
    return false;
}

/* Whether some node waits for the completion of `node`: the exit (the replay
 * waits for it), or a successor not ordered after it on the device */
static inline bool
__command_graph_node_completion_awaited(
    command_graph_t * cg,
    command_graph_node_t * node
) {
    if (node == cg->node_get_exit())
        return true;
    for (cgir::command_graph_node_t * succ : node->successors)
        if (!__command_graph_edge_on_device(node, succ))
            return true;
    return false;
}

template <command_graph_node_wc_type_t initial_dwc>
static inline void
XKRT_DRIVER_ENTRYPOINT(command_graph_replay_process_node)(
    runtime_t * runtime,
    command_graph_t * cg,
    command_graph_node_t * node
) {
    // delta wait counter
    command_graph_node_wc_type_t dwc = initial_dwc;

    SPINLOCK_LOCK(node->spinlock);
    {
        // first time processing this node for that replay: reinitialize the node
        if (xkrt_compare_and_swap(node->rc, cg->rc))
        {
            if (node->type == cgir::COMMAND_GRAPH_NODE_TYPE_COMMAND)
            {
                assert(node->command);

                // clear all previous callbacks
                ((command_t *) node->command)->completion_callback_clear();

                // add a callback to notify successor commands of that
                // predecessor completion -- unless every successor was
                // released at its launch: then nothing awaits its completion
                // (see command_graph_ordering_setup)
                if (__command_graph_node_completion_awaited(cg, node))
                {
                    static_assert(XKRT_CALLBACK_ARGS_MAX >= 3);
                    callback_t callback;
                    callback.func = XKRT_DRIVER_ENTRYPOINT(command_graph_replay_node_completion_callback);
                    callback.args[0] = runtime;
                    callback.args[1] = cg;
                    callback.args[2] = node;
                    ((command_t *) node->command)->completion_callback_push(callback);
                }

                // forward the replay team so a host task emitted by this command
                // is spawned on the replay-initiator's team, not the (device)
                // team of the thread that happens to run its completion callback
                ((command_t *) node->command)->replay_team = cg->replay_team;

                // if a host command graph, forward the runtime as a driver_handle
                // and propagate the replay team to the nested sub-graph
                if (node->command->type == cgir::COMMAND_TYPE_PACK && node->device_unique_id == XKRT_HOST_DEVICE_UNIQUE_ID)
                {
                    ((command_graph_t *) node->command->pack.cg)->replay_team   = cg->replay_team;
                    ((command_graph_t *) node->command->pack.cg)->driver_handle = runtime;
                }
            }

            // set node in INIT state
            node->state = COMMAND_GRAPH_NODE_STATE_INIT;

            // increment wc to avoid early release
            static_assert(std::is_signed<command_graph_node_wc_type_t>::value,
                    "command_graph_node_wc_t must be able to represent negative numbers");
            dwc -= (command_graph_node_wc_type_t) node->predecessors.size();
        }
    }
    SPINLOCK_UNLOCK(node->spinlock);

    // if we reached 0, command is now ready
    if (node->wc.fetch_sub(dwc, std::memory_order_relaxed) == dwc)
    {
        // if the node holds a command
        switch (node->type)
        {
            case (cgir::COMMAND_GRAPH_NODE_TYPE_EMPTY):
            {
                // completes it without submitting any command
                XKRT_DRIVER_ENTRYPOINT(command_graph_replay_node_complete)(runtime, cg, node);
                break ;
            }

            case (cgir::COMMAND_GRAPH_NODE_TYPE_COMMAND):
            {
                // replay the command
                assert(node->command);
                assert(node->device_unique_id != XKRT_UNSPECIFIED_DEVICE_UNIQUE_ID);
                assert(node->state == COMMAND_GRAPH_NODE_STATE_INIT);
                runtime->command_submit(node->device_unique_id, (command_t *) node->command);
                break ;
            }

            case (cgir::COMMAND_GRAPH_NODE_TYPE_COMMAND_GRAPH):
            case (cgir::COMMAND_GRAPH_NODE_TYPE_CONDITION):
            default:
            {
                LOGGER_FATAL("Not supported");
                break ;
            }
        }
    }
}

void
XKRT_DRIVER_ENTRYPOINT(command_graph_replay_node_complete)(
    runtime_t * runtime,
    command_graph_t * cg,
    command_graph_node_t * node
) {
    // submit successor nodes -- except those the command already released
    // when it was launched (device-side ordering, see command_graph_ordering_setup)
    node->foreach_successor([&] (cgir::command_graph_node_t * succ) {
        if (__command_graph_edge_on_device(node, succ))
            return ;
        XKRT_DRIVER_ENTRYPOINT(command_graph_replay_process_node)<1>(runtime, cg, (command_graph_node_t *) succ);
    });

    assert(node->state == COMMAND_GRAPH_NODE_STATE_INIT);
    node->state = COMMAND_GRAPH_NODE_STATE_COMPLETE;

    // we completed the last node, notify waiting threads
    if (node == cg->node_get_exit())
    {
        cg->completed.store(0, std::memory_order_seq_cst);
    }
    else
    {
        // nothing to do
    }
}

/* A command was launched (its event recorded): release the successors that
 * wait for its launch rather than its completion. */
static void
XKRT_DRIVER_ENTRYPOINT(command_graph_replay_node_launched)(
    void * args[XKRT_CALLBACK_ARGS_MAX]
) {
    runtime_t            * runtime = (runtime_t *)            args[0];
    command_graph_t      * cg      = (command_graph_t *)      args[1];
    command_graph_node_t * node    = (command_graph_node_t *) args[2];
    node->foreach_successor([&] (cgir::command_graph_node_t * succ) {
        if (__command_graph_edge_on_device(node, succ))
            XKRT_DRIVER_ENTRYPOINT(command_graph_replay_process_node)<1>(runtime, cg, (command_graph_node_t *) succ);
    });
}

/* Device-side ordering (XKRT_REPLAY_DEVICE_ORDERING, command_t::order).
 *
 * A replayed command is normally released when the host has seen all its
 * predecessors complete: between two dependent device commands, the device
 * idles while the host polls the first one's event, runs the completion
 * callbacks, then submits and launches the second -- some 10us per edge on a
 * CUDA device. Instead, an edge between two commands of devices of the same
 * driver is ordered by the devices themselves: the successor waits, on the
 * device, for the predecessor's event (events synchronize across devices of a
 * driver), and that edge counts for its release as soon as the predecessor is
 * LAUNCHED: the predecessor's `launched` callback releases it, its completion
 * no longer does. Every other edge (from the host, from a device of another
 * driver) still counts at the predecessor's completion. A command is released
 * once all its edges counted, so it may wait both for some predecessors'
 * launch and for others' completion.
 *
 * The graph's exit node is an empty command of a device, when some
 * predecessor is a command a driver orders: it is launched like any other
 * command, and the replay completes when its event does. The entry too, when
 * all its successors are commands of one driver. Every command has a path to
 * the exit, whose edges each either order the commands on the device or wait
 * for completion, so the exit completes after every command of the replay
 * did.
 *
 * A command whose successors are all ordered after it on the device has
 * nothing to do on completion: it gets no completion callback. The host may then see it
 * complete only after the next replay launched it again -- see
 * command_t::order.launch.
 *
 * Set up once per graph, on its first replay. */

/* Return the driver if the node is a command its device can order, else NULL */
static driver_t *
__command_graph_node_orderable(
    runtime_t * runtime,
    command_graph_node_t * node
) {
    if (node->type != cgir::COMMAND_GRAPH_NODE_TYPE_COMMAND || node->command == NULL)
        return NULL;
    if (node->device_unique_id == XKRT_HOST_DEVICE_UNIQUE_ID ||
        node->device_unique_id == XKRT_UNSPECIFIED_DEVICE_UNIQUE_ID)
        return NULL;
    if (((command_t *) node->command)->flags & COMMAND_FLAG_PROG_LAUNCHER)
        return NULL;

    /* programs, copies and empty commands: commands the driver launches on
     * one of its streams */
    switch (node->command->type)
    {
        case (cgir::COMMAND_TYPE_EMPTY):
        case (cgir::COMMAND_TYPE_PROG):
        case (cgir::COMMAND_TYPE_COPY_H2D_1D):
        case (cgir::COMMAND_TYPE_COPY_D2H_1D):
        case (cgir::COMMAND_TYPE_COPY_D2D_1D):
        case (cgir::COMMAND_TYPE_COPY_H2D_2D):
        case (cgir::COMMAND_TYPE_COPY_D2H_2D):
        case (cgir::COMMAND_TYPE_COPY_D2D_2D):
            break ;
        default:
            return NULL;
    }

    device_t * device = runtime->device_get(node->device_unique_id);
    if (device == NULL)
        return NULL;

    driver_t * driver = runtime->driver_get(device->driver_type);
    if (driver == NULL || !driver->device_ordering)
        return NULL;

    return driver;
}

/* Whether the node waits on the device for its predecessor `p`: both are
 * orderable commands of one driver */
static inline bool
__command_graph_edge_orderable(
    runtime_t * runtime,
    driver_t * driver,
    cgir::command_graph_node_t * p
) {
    return driver && __command_graph_node_orderable(runtime, (command_graph_node_t *) p) == driver;
}

/* Return the number of predecessors the node waits for on the device */
static uint32_t
__command_graph_node_nwaits(
    runtime_t * runtime,
    command_graph_node_t * node
) {
    driver_t * driver = __command_graph_node_orderable(runtime, node);
    uint32_t nwaits = 0;
    for (cgir::command_graph_node_t * p : node->predecessors)
        if (__command_graph_edge_orderable(runtime, driver, p))
            ++nwaits;
    return nwaits;
}

/* Make the EMPTY `node` an empty command of the device of one of
 * `neighbors`, orderable commands: the first one, if `all` of them are
 * orderable commands of one driver, else if any is */
static void
__command_graph_node_to_empty_command(
    runtime_t * runtime,
    command_graph_t * cg,
    command_graph_node_t * node,
    const std::list<cgir::command_graph_node_t *> & neighbors,
    const bool all
) {
    if (node->type != cgir::COMMAND_GRAPH_NODE_TYPE_EMPTY)
        return ;

    driver_t * driver = NULL;
    cgir::command_graph_node_t * first = NULL;
    for (cgir::command_graph_node_t * n : neighbors)
    {
        driver_t * d = __command_graph_node_orderable(runtime, (command_graph_node_t *) n);
        if (all && (d == NULL || (driver && d != driver)))
            return ;
        if (d && first == NULL)
        {
            driver = d;
            first  = n;
        }
    }
    if (first == NULL)
        return ;

    node->device_unique_id = first->device_unique_id;
    node->command          = cg->commands.put(cgir::COMMAND_TYPE_EMPTY, COMMAND_FLAG_NONE);
    node->type             = cgir::COMMAND_GRAPH_NODE_TYPE_COMMAND;
}

static void
command_graph_ordering_setup(
    runtime_t * runtime,
    command_graph_t * cg
) {
    cg->ordering_ready = true;
    if (!runtime->conf.replay_device_ordering)
        return ;

    command_graph_node_t * entry = (command_graph_node_t *) cg->node_get_entry();
    command_graph_node_t * exit  = (command_graph_node_t *) cg->node_get_exit();

    // The exit becomes an empty command of a device if some predecessor is
    // an orderable command: it waits on the device for those of its driver,
    // for the completion of the others. The entry, only if all its successors
    // are orderable commands of one driver: its other successors would
    // otherwise wait for its completion, where they need not wait at all.
    __command_graph_node_to_empty_command(runtime, cg, exit,  exit->predecessors, false);
    __command_graph_node_to_empty_command(runtime, cg, entry, entry->successors,  true);

    // count edges ordered on the devices, to allocate all `waits` arrays at once
    size_t nedges = 0;
    cg->walk([&] (cgir::command_graph_node_t * node) {
        nedges += __command_graph_node_nwaits(runtime, (command_graph_node_t *) node);
    });
    if (nedges == 0)
        return ;

    // one buffer for all the `waits` arrays (freed with the graph)
    cg->order_waits = (command_t **) malloc(nedges * sizeof(command_t *));
    assert(cg->order_waits);

    // set up the device edges: the `waits` of their successor, the `launched`
    // callback of their predecessor
    command_t ** next = cg->order_waits;
    int nlaunch = 0;
    int ncommands = 0;
    cg->walk([&] (cgir::command_graph_node_t * n) {

        command_graph_node_t * node = (command_graph_node_t *) n;
        if (node->type == cgir::COMMAND_GRAPH_NODE_TYPE_COMMAND)
            ++ncommands;

        const uint32_t nwaits = __command_graph_node_nwaits(runtime, node);
        if (nwaits == 0)
            return ;

        driver_t * driver = __command_graph_node_orderable(runtime, node);
        command_t * command = (command_t *) node->command;
        command->order.waits  = next;
        command->order.nwaits = 0;
        next += nwaits;

        for (cgir::command_graph_node_t * p : node->predecessors)
        {
            if (!__command_graph_edge_orderable(runtime, driver, p))
                continue ;

            command_graph_node_t * pred = (command_graph_node_t *) p;
            command_t * pcommand = (command_t *) pred->command;
            command->order.waits[command->order.nwaits++] = pcommand;

            // first time this predecessor releases a successor at launch
            if (pcommand->order.launched.func == NULL)
            {
                pcommand->order.launched.func    = XKRT_DRIVER_ENTRYPOINT(command_graph_replay_node_launched);
                pcommand->order.launched.args[0] = runtime;
                pcommand->order.launched.args[1] = cg;
                pcommand->order.launched.args[2] = pred;
            }
        }
        assert(command->order.nwaits == nwaits);
        ++nlaunch;
    });
    assert(next == cg->order_waits + nedges);

    LOGGER_INFO("replay ordering: %d of %d commands wait on the device for %zu predecessors",
                nlaunch, ncommands, nedges);
}

static int
XKRT_DRIVER_ENTRYPOINT(command_graph_wait)(
    runtime_t * runtime,
    command_graph_t * cg
) {
    runtime->task_wait(&cg->completed);
    return 0;
}

static int
XKRT_DRIVER_ENTRYPOINT(command_graph_replay_sequence)(
    runtime_t * runtime,
    command_graph_t * cg
);

static int
XKRT_DRIVER_ENTRYPOINT(command_graph_launch)(
    runtime_t * runtime,
    command_graph_t * cg
) {
    assert(runtime);
    assert(cg);

    // a linear sequence of OpenMP tasks replays as a single super-task
    if (cg->is_serial)
        return XKRT_DRIVER_ENTRYPOINT(command_graph_replay_sequence)(runtime, cg);

    // decide, once, which nodes the device can order itself
    if (!cg->ordering_ready)
        command_graph_ordering_setup(runtime, cg);

     // increase replay counter
    ++cg->rc;

    // set completed flag
    cg->completed.store(1, std::memory_order_seq_cst);

    // get entry/exit nodes to launch and wait completion
    command_graph_node_t * entry = (command_graph_node_t *) cg->node_get_entry();
    command_graph_node_t * exit  = (command_graph_node_t *) cg->node_get_exit();

    // submit and reinitialized entry and exit nodes.
    // Exit must be initialized first, to avoid early completion
    XKRT_DRIVER_ENTRYPOINT(command_graph_replay_process_node)<0>(runtime, cg, exit);
    XKRT_DRIVER_ENTRYPOINT(command_graph_replay_process_node)<0>(runtime, cg, entry);

    return 0;
}

/* Replay a pack whose sub-graph is a linear sequence of OpenMP-task PROG
 * commands (marked `is_serial` by CGIR's sequence pass).
 *
 * Instead of the wavefront (which would spawn one task per command), we spawn a
 * single "super" task: the worker that schedules it serially runs the recorded
 * routine of every kmp task of the chain on that one thread. This collapses the
 * n task-spawns and n scheduling decisions of a task chain into a single one. */
static int
XKRT_DRIVER_ENTRYPOINT(command_graph_replay_sequence)(
    runtime_t * runtime,
    command_graph_t * cg
) {
    assert(runtime);
    assert(cg);

    command_graph_node_t * exit  = (command_graph_node_t *) cg->node_get_exit();

    // Completion flags: `exit->state` is polled by the KERN command_queue_progress
    // (async pack path), while `cg->completed` is waited on by command_graph_wait
    // (synchronous command_execute path). Reset both before spawning the task.
    exit->state = COMMAND_GRAPH_NODE_STATE_INIT;
    cg->completed.store(1, std::memory_order_seq_cst);

    // IMPORTANT: a task routine is byte-copied (memcpy) into the task, not
    // copy-constructed (see task_new/body_host_capture). This only works while
    // the closure stays inside std::function's small-buffer (<= 16B on libstdc++,
    // i.e. ~2 pointers): a larger closure is heap-allocated and the shallow copy
    // ends up with a dangling pointer once the temporary std::function dies.
    // Capture the single `cg` pointer only, and derive entry/exit inside.
    const runtime_t::task_routine_t routine =
        [cg] (runtime_t *, device_t *, task_t *) {

            command_graph_node_t * entry = (command_graph_node_t *) cg->node_get_entry();
            command_graph_node_t * exit  = (command_graph_node_t *) cg->node_get_exit();

            // walk the linear chain entry -> ... -> exit (inclusive), running
            // each body. entry/exit may themselves be PROG commands: the
            // sequence pass reuses the chain head/tail as the sub-graph
            // entry/exit rather than adding empty control nodes.
            command_graph_node_t * node = entry;
            while (true)
            {
                if (node->type == cgir::COMMAND_GRAPH_NODE_TYPE_COMMAND)
                {
                    assert(node->command);
                    assert(node->command->type == cgir::COMMAND_TYPE_PROG);
                    command_prog_run_host((command_t *) node->command);
                }

                if (node == exit)
                    break ;

                // a non-exit sequence node has exactly one successor
                assert(node->successors.size() == 1);
                node = (command_graph_node_t *) node->successors.front();
            }

            // notify both completion mechanisms
            exit->state = COMMAND_GRAPH_NODE_STATE_COMPLETE;
            cg->completed.store(0, std::memory_order_seq_cst);
        };

    // Spawn the super-task onto the replay-initiator's team (any of its threads
    // runs the whole chain on a host device == NULL thread). Fall back to the
    // current team if the replay team is unknown (e.g. not driven by a replay).
    if (cg->replay_team != nullptr)
        runtime->team_task_spawn((team_t *) cg->replay_team, routine);
    else
        runtime->task_spawn(routine);

    return 0;
}

static int
XKRT_DRIVER_ENTRYPOINT(command_execute)(
    device_driver_id_t device_driver_id,
    command_t * command
) {
    assert(command->type == cgir::COMMAND_TYPE_PACK);

    command_graph_t * cg = (command_graph_t *) command->pack.cg;
    assert(cg);

    runtime_t * runtime = (runtime_t *) cg->driver_handle;
    assert(runtime);

    // command_graph_launch dispatches on cg->is_serial (super-task vs wavefront)
    XKRT_DRIVER_ENTRYPOINT(command_graph_launch)(runtime, cg);
    XKRT_DRIVER_ENTRYPOINT(command_graph_wait)(runtime, cg);

    return 0;
}

static int
XKRT_DRIVER_ENTRYPOINT(command_queue_launch)(
    device_driver_id_t device_driver_id,
    command_queue_t * iqueue,
    command_t * command,
    xkrt_command_queue_list_counter_t idx
) {
    (void)device_driver_id;

    assert(
        command->type == cgir::COMMAND_TYPE_FD_READ  ||
        command->type == cgir::COMMAND_TYPE_FD_WRITE ||
        command->type == cgir::COMMAND_TYPE_PACK
    );

    queue_host_t * queue = (queue_host_t *)iqueue;

    // enqueue iouring events
    if (command->type == cgir::COMMAND_TYPE_FD_READ || command->type == cgir::COMMAND_TYPE_FD_WRITE)
    {
        if (__builtin_expect(queue->io_uring.sq_ptr == NULL, 0))
            XKRT_DRIVER_ENTRYPOINT(io_uring_init)(queue);

        /* Add SQE to the tail of the ring */
        unsigned tail  = * queue->io_uring.sq_tail;
        unsigned index = tail & * queue->io_uring.sq_mask;
        struct io_uring_sqe *sqe = &queue->io_uring.sqes[index];

        /* Zero-fill and populate — clearing avoids stale flags from prior use */
        memset(sqe, 0, sizeof(*sqe));
        sqe->opcode    = (command->type == cgir::COMMAND_TYPE_FD_READ)
                             ? IORING_OP_READ
                             : IORING_OP_WRITE;
        sqe->fd        = command->file.fd;
        sqe->addr      = (unsigned long)command->file.buffer;
        sqe->len       = command->file.size;
        sqe->off       = command->file.offset;
        sqe->user_data = (__u64)idx;

        /* sq_array is pre-filled with identity mapping in init */

        io_uring_smp_store_release(queue->io_uring.sq_tail, tail + 1);
        ++queue->io_uring.pending_submits;

        /*
         * Batch heuristic: flush when we've accumulated enough SQEs or when
         * the ring is getting full.  This amortises the syscall cost across
         * multiple commands.
         */
        constexpr unsigned PACK_THRESHOLD = 16;
        if (queue->io_uring.pending_submits >= PACK_THRESHOLD)
            io_uring_flush_submits(queue);

        return EINPROGRESS;
    }
    // pack commands = emit all sub-cg commands
    else
    {
        assert(command->type == cgir::COMMAND_TYPE_PACK);

        command_graph_t * cg = (command_graph_t *) command->pack.cg;
        assert(cg);

        runtime_t * runtime  = (runtime_t *) cg->driver_handle;
        assert(runtime);

        // command_graph_launch dispatches on cg->is_serial (super-task vs wavefront)
        XKRT_DRIVER_ENTRYPOINT(command_graph_launch)(runtime, cg);
    }

    return 0;
}

static int
XKRT_DRIVER_ENTRYPOINT(command_queue_suggest)(
    device_driver_id_t device_driver_id,
    command_queue_type_t qtype
) {
    (void)device_driver_id;
    switch (qtype)
    {
        case (XKRT_QUEUE_TYPE_FD_READ):
        case (XKRT_QUEUE_TYPE_FD_WRITE):
            return 1;

        // KERN is used for packes
        case (XKRT_QUEUE_TYPE_KERN):
            return 1;

        default:
            return 0;
    }
}

static inline int
XKRT_DRIVER_ENTRYPOINT(command_queue_wait_all)(
    command_queue_t * iqueue
) {
    assert(iqueue);
    queue_host_t * queue = (queue_host_t *)iqueue;


    switch (queue->super.type)
    {
        case (XKRT_QUEUE_TYPE_FD_READ):
        case (XKRT_QUEUE_TYPE_FD_WRITE):
        {
            assert(queue->super.type == XKRT_QUEUE_TYPE_FD_READ ||
                   queue->super.type == XKRT_QUEUE_TYPE_FD_WRITE);

            /* Ensure everything has been submitted before waiting */
            io_uring_flush_submits(queue);

            int min_completion = queue->super.pending_size();
            if (min_completion)
            {
                LOGGER_DEBUG("Waiting for %d i/o commands to complete", min_completion);
                int r = (int)syscall(__NR_io_uring_enter,
                                     queue->io_uring.fd, 0, min_completion,
                                     IORING_ENTER_GETEVENTS, NULL, 0);
                if (r < 0)
                    LOGGER_FATAL("io_uring_enter (wait) failed: %s", strerror(errno));
            }
            break ;
        }

        case (XKRT_QUEUE_TYPE_KERN):
        {
            LOGGER_FATAL("Not supported");
            break ;
        }

        default:
        {
            LOGGER_FATAL("Not supported");
            break ;
        }
    }

    return 0;
}

static inline int
XKRT_DRIVER_ENTRYPOINT(command_queue_wait)(
    command_queue_t * iqueue,
    command_t * command,
    xkrt_command_queue_list_counter_t idx
) {
    assert(command);
    assert(iqueue);
    queue_host_t * queue = (queue_host_t *)iqueue;

    switch (queue->super.type)
    {
        case (XKRT_QUEUE_TYPE_FD_READ):
        case (XKRT_QUEUE_TYPE_FD_WRITE):
        {
            // TODO: wait for specific CQE via user_data matching
            return XKRT_DRIVER_ENTRYPOINT(command_queue_wait_all)(iqueue);
        }

        case (XKRT_QUEUE_TYPE_KERN):
        {
            assert(command->type == cgir::COMMAND_TYPE_PACK);

            command_graph_t * cg = (command_graph_t *) command->pack.cg;
            assert(cg);

            runtime_t * runtime = (runtime_t *) cg->driver_handle;
            assert(runtime);

            XKRT_DRIVER_ENTRYPOINT(command_graph_wait)(runtime, cg);

            return 0;
        }

        default:
        {
            LOGGER_FATAL("Not supported");
            return 0;
        }
    }

    return 0;
}

static int
XKRT_DRIVER_ENTRYPOINT(command_queue_progress)(
    command_queue_t * iqueue
) {
    assert(iqueue);
    queue_host_t * queue = (queue_host_t *)iqueue;

    int r = 0;

    switch (queue->super.type)
    {
        case (XKRT_QUEUE_TYPE_FD_READ):
        case (XKRT_QUEUE_TYPE_FD_WRITE):
        {
            /* Flush any pending submissions so completions can arrive */
            io_uring_flush_submits(queue);

            /*
             * Drain the CQ ring.  Read tail once, then process all available
             * entries — avoids re-reading the tail on every iteration.
             */
            unsigned head = io_uring_smp_load_acquire(queue->io_uring.cq_head);
            unsigned tail = io_uring_smp_load_acquire(queue->io_uring.cq_tail);
            const unsigned mask = * queue->io_uring.cq_mask;

            if (head == tail)
                return 0;

            unsigned completed = 0;

            while (head != tail)
            {
                struct io_uring_cqe * cqe = &queue->io_uring.cqes[head & mask];

                if (__builtin_expect(cqe->res < 0, 0))
                    LOGGER_FATAL("io_uring CQE error: %s", strerror(abs(cqe->res)));

                const xkrt_command_queue_list_counter_t p =
                    (const xkrt_command_queue_list_counter_t)cqe->user_data;
                assert(cqe->res == (int)iqueue->command_at(p)->file.size);

                ++head;
                ++completed;

                /* Complete the command (callback may enqueue more work) */
                iqueue->complete_command(p);
            }

            /* Single store-release to advance head past all processed CQEs */
            io_uring_smp_store_release(queue->io_uring.cq_head, head);
            break ;
        }

        case (XKRT_QUEUE_TYPE_KERN):
        {
            iqueue->progress([&] (command_t * command, xkrt_command_queue_list_counter_t p) {

                assert(command->type == cgir::COMMAND_TYPE_PACK);
                assert(command->pack.cg);

                command_graph_node_t * exit  = (command_graph_node_t *) command->pack.cg->node_get_exit();
                if ((volatile command_graph_node_state_t) exit->state == COMMAND_GRAPH_NODE_STATE_COMPLETE)
                {
                    iqueue->complete_command(p);
                }
                else
                {
                    r = EINPROGRESS;
                }

                return true;
            });

            break ;
        }

        default:
        {
            LOGGER_FATAL("Not supported");
            break ;
        }
    }

   return r;
}

static command_queue_t *
XKRT_DRIVER_ENTRYPOINT(command_queue_create)(
    device_t * idevice,
    command_queue_type_t type,
    xkrt_command_queue_list_counter_t capacity
) {
    (void)idevice;

    assert(
        type == XKRT_QUEUE_TYPE_FD_READ  ||
        type == XKRT_QUEUE_TYPE_FD_WRITE ||
        type == XKRT_QUEUE_TYPE_KERN
    );

    queue_host_t * queue;
    if (type == XKRT_QUEUE_TYPE_FD_READ || type == XKRT_QUEUE_TYPE_FD_WRITE)
        queue = (queue_host_t *) calloc(1, sizeof(queue_host_t));
    else if (type == XKRT_QUEUE_TYPE_KERN)
    {
        queue = (queue_host_t *) malloc(sizeof(queue_host_t) + capacity * sizeof(queue_host_event_t));
        queue->events.buffer = (queue_host_event_t *) (queue + 1);
        queue->events.capacity = capacity;
    }
    else
        return NULL;

    if (!queue)
        LOGGER_FATAL("Failed to allocate queue_host_t");

    command_queue_init((command_queue_t *)queue, type, capacity);

    return (command_queue_t *)queue;
}

static void
XKRT_DRIVER_ENTRYPOINT(command_queue_delete)(
    device_t * device,
    command_queue_t * iqueue
) {
    /* TODO: munmap the io_uring ring buffers and close the fd */
    queue_host_t * queue = (queue_host_t *)iqueue;
    switch (iqueue->type)
    {
        case (XKRT_QUEUE_TYPE_FD_READ):
        case (XKRT_QUEUE_TYPE_FD_WRITE):
        {
            if (queue->io_uring.fd > 0)
                close(queue->io_uring.fd);
            break ;
        }

        // KERN is used for packes
        case (XKRT_QUEUE_TYPE_KERN):
        {
            break ;
        }

        default:
            break ;
    }
    free(iqueue);
}

////////////
// MEMORY //
////////////

static void *
XKRT_DRIVER_ENTRYPOINT(memory_device_allocate)(
    device_driver_id_t device_driver_id,
    const size_t size,
    int area_idx
) {
    assert(device_driver_id == 0 && area_idx == 0);
    return malloc(size);
}

static void
XKRT_DRIVER_ENTRYPOINT(memory_device_deallocate)(
    device_driver_id_t device_driver_id,
    void *ptr,
    const size_t size,
    int area_idx
) {
    assert(device_driver_id == 0 && area_idx == 0);
    (void)size;
    free(ptr);
}

static void
XKRT_DRIVER_ENTRYPOINT(memory_device_info)(
    device_driver_id_t device_driver_id,
    device_memory_info_t info[XKRT_DEVICE_MEMORIES_MAX],
    int * nmemories
) {
    (void)device_driver_id;
    assert(device_driver_id == 0);

    struct sysinfo sinfo;
    if (sysinfo(&sinfo) == 0)
    {
        strncpy(info[0].name, "RAM", sizeof(info[0].name));
        info[0].used     = sinfo.totalram - sinfo.freeram;
        info[0].capacity = sinfo.totalram;
        *nmemories = 1;
    } else {
        *nmemories = 0;
    }
}

/////////////////////////
// Driver registration //
/////////////////////////

driver_t *
XKRT_DRIVER_ENTRYPOINT(create_driver)(void)
{
    driver_t * driver = (driver_t *) calloc(1, sizeof(driver_t));
    assert(driver);

    # define REGISTER(func) driver->f_##func = XKRT_DRIVER_ENTRYPOINT(func)

    REGISTER(command_queue_create);
    REGISTER(command_queue_delete);
    REGISTER(command_queue_launch);
    REGISTER(command_queue_progress);
    REGISTER(command_queue_suggest);
    REGISTER(command_queue_wait);
    REGISTER(command_queue_wait_all);
    REGISTER(command_execute);

    REGISTER(device_commit);
    REGISTER(device_cpuset);
    REGISTER(device_create);
    REGISTER(device_destroy);
    REGISTER(device_info);
    REGISTER(device_init);

    REGISTER(finalize);

    REGISTER(get_name);
    REGISTER(get_ndevices_max);

    REGISTER(init);

    REGISTER(memory_device_allocate);
    REGISTER(memory_device_deallocate);
    REGISTER(memory_device_info);

    # undef REGISTER

    return driver;
}

XKRT_NAMESPACE_END
