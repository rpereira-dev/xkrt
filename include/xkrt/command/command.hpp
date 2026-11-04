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

// This file nd CGIR data structures for XKRT needs

#ifndef __XKRT_COMMAND_H__
# define __XKRT_COMMAND_H__

# include <cgir/cgir.hpp>

# include <atomic>

# include <xkrt/callback.h>
# include <xkrt/namespace.h>
# include <xkrt/data-structures/memory-pool.h>
# include <xkrt/sync/mutex.h>
# include <xkrt/sync/spinlock.h>
# include <xkrt/types.h>

XKRT_NAMESPACE_BEGIN;

/* Flags of the command */
typedef enum    command_flag_t
{
    COMMAND_FLAG_NONE           = (0),

    /* if the command should be executed synchronously (i.e., blocking the
     * calling thread until completion) */
    COMMAND_FLAG_SYNCHRONOUS    = (1 << 0),

    /* if the command should be serialized (i.e., so that the thread that
     * emitted it must launch it) */
    COMMAND_FLAG_SERIALIZED     = (1 << 1),

    /* if the command is a host program launching additional commands */
    COMMAND_FLAG_PROG_LAUNCHER  = (1 << 2),

    /* if the command_t storage was allocated from a queue's command pool (and
     * must be recycled to it on completion). Commands NOT carrying this flag are
     * externally owned (e.g. a command-graph node command pushed for replay) and
     * are never freed by the queue. Set by 'command_new'; consumed on completion. */
    COMMAND_FLAG_POOLED         = (1 << 3),

    /* if the driver should time the command's execution and report it (see
     * src/driver/tuning.hpp): its graph wants durations for the
     * `critical-path` pass. Set and cleared by the replay. */
    COMMAND_FLAG_MEASURE        = (1 << 4),

}               command_flag_t;

inline constexpr command_flag_t
operator|(command_flag_t a, command_flag_t b)
{
    return static_cast<command_flag_t>(
        static_cast<int>(a) | static_cast<int>(b)
    );
}

/* commands */
struct command_t : cgir::command_t
{
    /* flags of the command */
    command_flag_t flags;

    /* callback on completion */
    struct {
        callback_t list[XKRT_COMMAND_CALLBACKS_MAX];
        command_callback_index_t n;
    } callbacks;

    /* team_t that must replay the command */
    void * replay_team;

    /* Device-side ordering, during a command-graph replay on a driver that
     * supports it. A command whose predecessors are all commands of devices
     * of the same driver is launched as soon as they are LAUNCHED, not
     * completed: its stream first waits on their `event` (events synchronize
     * across devices of a driver), so the devices order them themselves and
     * the host round trip between the two disappears. */
    struct {
        /* the commands whose `event` the launch must wait on */
        command_t ** waits;
        uint32_t nwaits;

        /* The queue entry the command was last launched from, with bit 0 set
         * once the host saw that launch complete: successors launched after
         * that do not wait on `event`. Tagged with the entry, so that the
         * completion of a previous replay's launch, seen by the host after the
         * command was launched again, is not taken for the new one's.
         * Accessed through std::atomic_ref (see order_launched/_completed/
         * _done), so that command_t stays trivially copyable. */
        uintptr_t launch;

        /* the event of the queue slot the command was last launched from,
         * recorded right after the launch (set by the driver) */
        void * event;

        /* raised by the launching thread once the command is launched (its
         * event recorded): releases the successors that only wait for that */
        callback_t launched;
    } order;

    /* constructor */
    command_t(
        cgir::command_type_t type,
        command_flag_t flags
    ) :
        cgir::command_t(type),
        flags(flags),
        callbacks{},
        replay_team(NULL),
        order{
            .waits = NULL,
            .nwaits = 0,
            .launch = 0,
            .event = NULL,
            .launched = { .func = NULL }
        }
    {
    }

    inline void
    completion_callback_push(const callback_t & callback)
    {
        assert(this->callbacks.n >= 0);
        assert(this->callbacks.n < XKRT_COMMAND_CALLBACKS_MAX);
        this->callbacks.list[this->callbacks.n++] = callback;
    }

    inline void
    completion_callback_clear(void)
    {
        this->callbacks.n = 0;
    }

    inline void
    completion_callback_raise(void)
    {
        for (command_callback_index_t i = 0 ; i < this->callbacks.n ; ++i)
        {
            assert(this->callbacks.list[i].func);
            this->callbacks.list[i].raise();
        }
    }

    /* device-side ordering, see `order.launch` */

    /* the command is being launched from queue entry `entry` */
    inline void
    order_launched(const void * entry)
    {
        assert(((uintptr_t) entry & 1) == 0);
        std::atomic_ref<uintptr_t>(this->order.launch).store((uintptr_t) entry, std::memory_order_relaxed);
    }

    /* the host saw the launch from `entry` complete: no effect if the command
     * was launched again since */
    inline void
    order_completed(const void * entry)
    {
        uintptr_t expected = (uintptr_t) entry;
        std::atomic_ref<uintptr_t>(this->order.launch).compare_exchange_strong(
            expected, expected | 1, std::memory_order_release, std::memory_order_relaxed);
    }

    /* whether the host saw the command's last launch complete */
    inline bool
    order_done(void)
    {
        return std::atomic_ref<uintptr_t>(this->order.launch).load(std::memory_order_acquire) & 1;
    }
};

/* Replay counter */
typedef int command_graph_rc_type_t;
typedef std::atomic<command_graph_rc_type_t> command_graph_rc_t;

/* Wait counter of a command node */
typedef int command_graph_node_wc_type_t;
typedef std::atomic<command_graph_node_wc_type_t> command_graph_node_wc_t;

/* node states */
enum command_graph_node_state_t
{
    COMMAND_GRAPH_NODE_STATE_INIT,
    COMMAND_GRAPH_NODE_STATE_COMPLETE
};

/* a node */
struct command_graph_node_t : cgir::command_graph_node_t
{
    /* replay counter */
    command_graph_rc_t rc;

    /* wait counter to know whether this node can be replayed */
    command_graph_node_wc_t wc;

    /* node state */
    command_graph_node_state_t state;

    /* spinlock to reinitialize the node on replay */
    spinlock_t spinlock;


    /* constructor/destructor */
    command_graph_node_t(
        const cgir::device_unique_id_t device_unique_id,
        const cgir::command_graph_node_type_t type
    ) :
        cgir::command_graph_node_t(device_unique_id, type),
        rc(0),
        wc(0),
        state(COMMAND_GRAPH_NODE_STATE_INIT),
        spinlock(0)
    {}

    command_graph_node_t(
        const cgir::device_unique_id_t device_unique_id,
        cgir::command_t * command
    ) :
        cgir::command_graph_node_t(device_unique_id, command),
        rc(0),
        wc(0),
        state(COMMAND_GRAPH_NODE_STATE_INIT),
        spinlock(0)
    {}
};

/* Additional storage for the `command_graph_t` type */
struct command_graph_t : cgir::command_graph_t
{
    /* command allocator */
    memory_pool_t<command_t> commands;

    /* nodes allocator */
    memory_pool_t<command_graph_node_t> nodes;

    /* replay counter */
    command_graph_rc_type_t rc;

    /* mutex/cond to notify threads waiting on the replay completion */
    std::atomic<uint32_t> completed;

    /* team_t that must replay the graph */
    void * replay_team;

    /* Handle for driver (runtime_t for host, cuGraph for CUDA device, etc.) */
    void * driver_handle;

    /* whether device-side ordering was set up on the nodes (first replay) */
    bool ordering_ready;

    /* storage of every command's `order.waits` array: one allocation per
     * graph, sized by the number of edges ordered on the devices */
    command_t ** order_waits;

    command_graph_t(void) :
        commands(),
        nodes(),
        rc(0),
        completed(0),
        replay_team(NULL),
        driver_handle(NULL),
        ordering_ready(false),
        order_waits(NULL)
    {}

    /* return number of nodes */
    inline size_t
    get_number_of_allocated_nodes(void) const
    {
        return this->nodes.size();
    }
};

XKRT_NAMESPACE_END;

#endif /* __XKRT_COMMAND_H__ */
