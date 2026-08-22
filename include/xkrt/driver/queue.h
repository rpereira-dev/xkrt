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

#ifndef __QUEUE_HPP__
# define __QUEUE_HPP__

# include <xkrt/command/command.hpp>
# include <xkrt/driver/queue-type.h>
# include <xkrt/memory/alignas.h>
# include <xkrt/stats/stats.h>
# include <xkrt/support.h>
# include <xkrt/sync/lockable.hpp>
# include <xkrt/sync/spinlock.h>
# include <xkrt/thread/thread.h>

# include <atomic>
# include <cassert>
# include <cstdint>
# include <functional>
# include <new>
# include <vector>

XKRT_NAMESPACE_BEGIN

const char * command_queue_type_to_str(xkrt_command_queue_type_t type);

/*
 * A single ring slot. The ring stores command POINTERS; the command_t storage
 * lives either in the per-queue pool (runtime-owned commands) or externally
 * (e.g. command-graph node commands committed for replay).
 *
 * 'seq' is the producer<->consumer handshake (Vyukov bounded-MPMC): it encodes
 * whether the slot is free, published, or awaiting reuse, and is the only
 * cross-thread synchronisation the producer side needs.
 *
 * 'completed' is written and read by the owning (consumer) thread only, so it
 * needs no atomicity; it is co-located here (AoS) so completion state travels
 * with the command.
 */
struct command_queue_entry_t
{
    command_t *           cmd;
    std::atomic<uint64_t> seq;
    bool                  completed;
};

/*
 * Per-queue pool backing runtime-owned commands (copies, file I/O, task-emitted
 * programs); externally-owned commands (e.g. a command-graph node pushed for
 * replay) are held by pointer and not pooled. Append-only storage with stable
 * addresses; completed commands are recycled via the freelist. Its own spinlock
 * guards alloc (any producer) vs free (owning thread).
 */
struct command_pool_t
{
    memory_pool_t<command_t> storage;   /* backing storage (stable addresses) */
    std::vector<command_t *> freelist;  /* recycled commands, ready for reuse   */
    spinlock_t               lock;      /* guards storage + freelist            */

    command_pool_t(void) : storage(), freelist(), lock(SPINLOCK_INITIALIZER) {}

    /* allocate + (re)construct a command_t owned by this pool */
    command_t *
    alloc(const cgir::command_type_t ctype, const command_flag_t flags)
    {
        command_t * c;
        SPINLOCK_LOCK(this->lock);
        if (!this->freelist.empty())
        {
            c = this->freelist.back();
            this->freelist.pop_back();
        }
        else
            c = this->storage.put();    /* raw storage, not yet constructed */
        SPINLOCK_UNLOCK(this->lock);

        /* (re)construct: resets flags/callbacks/replay_team. command_t is
         * trivially destructible and owns no heap the pool must release, so no
         * destructor is run before reuse. */
        return new (c) command_t(ctype, flags);
    }

    /* recycle a command previously returned by 'alloc' */
    void
    free(command_t * c)
    {
        SPINLOCK_LOCK(this->lock);
        this->freelist.push_back(c);
        SPINLOCK_UNLOCK(this->lock);
    }
};

/*
 * A single lock-free MPSC ring of commands. Any thread may submit commands
 * (command_new + commit); only the thread that owns the queue may consume them
 * (launch the ready ones, then progress and complete the pending ones).
 *
 * The ring is partitioned by three cursors (monotonic 64-bit indices; the ring
 * slot is 'index % capacity'), with r <= s <= w:
 *
 *      [r, s) : pending  - launched on the device, awaiting completion
 *      [s, w) : ready    - committed by producers, awaiting launch
 *      [w, r) : free
 *
 *  - 'w' is shared: producers reserve a slot with a CAS (multi-producer).
 *  - 's' and 'r' belong to the owning thread only (single consumer), so they
 *    need no atomicity.
 *
 * The single 'idx' handed to the driver at launch (f_command_queue_launch) is
 * the same 'idx' seen at completion (complete_command / events[idx]); with one
 * ring this is a structural invariant.
 */
struct command_queue_t
{
    /* the type of that queue */
    command_queue_type_t type;

    /* the ring of command slots (size 'capacity') */
    command_queue_entry_t * entries;
    xkrt_command_queue_list_counter_t capacity;

    /* Cursors. 'w' is hammered by producers (CAS); 's'/'r' are consumer-only.
     * Plain byte padding keeps them on distinct cache lines to avoid false sharing
     * WITHOUT over-aligning the queue: drivers allocate the enclosing queue with
     * malloc, which would not honour an extended (alignas) alignment. */
    uint8_t _pad0[hardware_destructive_interference_size];

    /* producer reservation cursor (shared, multi-producer) */
    std::atomic<uint64_t> w;

    uint8_t _pad1[hardware_destructive_interference_size];

    /* consumer cursors (owning thread only): launch boundary 's', reclaim 'r' */
    uint64_t s;
    uint64_t r;

    uint8_t _pad2[hardware_destructive_interference_size];

    /* storage pool for runtime-owned commands pushed through 'command_new'
     * (externally-owned commands committed directly do not use it) */
    command_pool_t pool;

    # if XKRT_SUPPORT_STATS
    struct {
        struct {
            stats_int_t commited;
            stats_int_t completed;
        } commands[cgir::COMMAND_TYPE_MAX];
        stats_int_t transfered;
    } stats;
    # endif /* XKRT_SUPPORT_STATS */

    //////////////////////////////////////////////
    //  PRODUCTION - called by any thread        //
    //////////////////////////////////////////////

    /**
     *  Allocate a new pool-owned command. It carries COMMAND_FLAG_POOLED and is
     *  recycled to the pool on completion. It does NOT touch the ring yet: fill it,
     *  then publish it with 'commit' (or, if you abort, return it with pool.free).
     *  Threading: any thread.
     */
    command_t * command_new(const cgir::command_type_t ctype, const command_flag_t flags);

    /**
     *  Publish a command (pool-owned from 'command_new', or externally-owned) into
     *  the ring: reserve a slot, store the pointer, mark it not-completed. Fatal if
     *  the ring is full.
     *  Threading: any thread (lock-free, multi-producer).
     */
    int commit(command_t * command);

    //////////////////////////////////////////////
    //  CONSUMPTION - the owning thread only     //
    //////////////////////////////////////////////

    /**
     *  Peek the next ready (committed, not-yet-launched) command. Returns NULL if
     *  none is launchable yet (ring empty, or the next slot is still being
     *  committed). On success *slot is its ring index (the driver launch 'idx').
     */
    command_t * ready_peek(xkrt_command_queue_list_counter_t * slot);

    /**
     *  Advance past the ready command just launched, keeping it in the pending
     *  region [r, s) to await completion.
     */
    void ready_commit_pending(void);

    /**
     *  Advance past a ready command that completed inline (synchronous, never
     *  entered the device): mark it completed so 'reclaim' can free the slot.
     */
    void ready_commit_synchronous(void);

    /**
     *  Iterate on each not-yet-completed pending command at ring index p.
     *  Stop early if process(cmd, p) returns false.
     */
    xkrt_command_queue_list_counter_t progress(const std::function<bool(command_t * cmd, xkrt_command_queue_list_counter_t p)> & process);

    /**
     *  Complete the command at ring index 'p' (idempotent): raise its callbacks
     *  and recycle it if pool-owned. Does not reclaim the slot (see 'reclaim').
     */
    void complete_command(const xkrt_command_queue_list_counter_t p);

    /**
     *  Complete all pending commands [r, s) and reclaim their slots.
     */
    void complete_commands(void);

    /**
     *  Reclaim the completed prefix of the pending region, returning the freed
     *  slots to producers. Call after 'progress'.
     */
    void reclaim(void);

    //////////////////////////////////////////////
    //  QUERIES - the owning thread only         //
    //////////////////////////////////////////////

    /* the command pointer stored at ring index 'p' */
    inline command_t *
    command_at(const xkrt_command_queue_list_counter_t p) const
    {
        assert(p < this->capacity);
        return this->entries[p].cmd;
    }

    /* true if a ready command is available to launch (seq-aware: false while a
     * producer is mid-commit, which is safe because 'commit' wakes the owner) */
    inline bool
    has_ready(void) const
    {
        const uint64_t s = this->s;
        if (s == this->w.load(std::memory_order_acquire))
            return false;
        return this->entries[s % this->capacity].seq.load(std::memory_order_acquire) == s + 1;
    }

    /* true if there is no pending (launched, not-yet-reclaimed) command */
    inline bool
    pending_empty(void) const
    {
        return this->r == this->s;
    }

    /* number of pending (launched, not-yet-reclaimed) commands */
    inline xkrt_command_queue_list_counter_t
    pending_size(void) const
    {
        return (xkrt_command_queue_list_counter_t) (this->s - this->r);
    }

    /* ring index of the oldest pending command (valid iff !pending_empty()) */
    inline xkrt_command_queue_list_counter_t
    pending_first(void) const
    {
        return (xkrt_command_queue_list_counter_t) (this->r % this->capacity);
    }

};  /* command_queue_t */

void command_queue_init(
    command_queue_t * queue,
    command_queue_type_t qtype,
    xkrt_command_queue_list_counter_t capacity
);

void command_queue_deinit(command_queue_t * queue);

XKRT_NAMESPACE_END

#endif /* __QUEUE_HPP__ */
