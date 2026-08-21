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

# include <errno.h>
# include <string.h>

# include <xkrt/logger/logger.h>
# include <xkrt/driver/queue.h>
# include <xkrt/logger/todo.h>

XKRT_NAMESPACE_BEGIN;

//////////////
//  HELPERS //
//////////////

const char *
command_queue_type_to_str(xkrt_command_queue_type_t type)
{
    switch (type)
    {
        case (XKRT_QUEUE_TYPE_H2D):        return "H2D";
        case (XKRT_QUEUE_TYPE_D2H):        return "D2H";
        case (XKRT_QUEUE_TYPE_D2D):        return "D2D";
        case (XKRT_QUEUE_TYPE_P2P):        return "P2P";
        case (XKRT_QUEUE_TYPE_KERN):       return "KERN";
        case (XKRT_QUEUE_TYPE_FD_READ):    return "FD_READ";
        case (XKRT_QUEUE_TYPE_FD_WRITE):   return "FD_WRITE";
        case (XKRT_QUEUE_TYPE_ALL):        return "ALL";
        default:                           return  NULL;
    }
}

void
command_queue_init(
    command_queue_t * queue,
    command_queue_type_t type,
    xkrt_command_queue_list_counter_t capacity
) {
    assert(capacity > 0);

    queue->type     = type;
    queue->capacity = capacity;

    /* The ring holds command POINTERS; the command_t storage lives either in the
     * per-queue pool (runtime-owned commands) or externally (e.g. graph nodes).
     * 'new[]' constructs the per-slot 'seq' atomics; seed them with the Vyukov
     * free-slot value seq[k] = k. */
    queue->entries = new command_queue_entry_t[capacity];
    assert(queue->entries);
    for (xkrt_command_queue_list_counter_t k = 0; k < capacity; ++k)
    {
        queue->entries[k].cmd       = NULL;
        queue->entries[k].completed = false;
        queue->entries[k].seq.store((uint64_t) k, std::memory_order_relaxed);
    }

    /* the queue is malloc'd (no constructor ran), so construct in place the
     * atomic reservation cursor and the command pool */
    new (&queue->w) std::atomic<uint64_t>(0);
    queue->s = 0;
    queue->r = 0;

    new (&queue->pool) command_pool_t();

    # if XKRT_SUPPORT_STATS
    memset(&(queue->stats), 0, sizeof(queue->stats));
    # endif /* XKRT_SUPPORT_STATS */
}

void
command_queue_deinit(command_queue_t * queue)
{
    assert(queue);
    assert(queue->entries);

    queue->pool.~command_pool_t();
    delete [] queue->entries;
    queue->entries = NULL;
}

//////////////////////////////////////////
//  MANAGEMENT CALLED BY ANY THREADS    //
//////////////////////////////////////////

command_t *
command_queue_t::command_new(
    const cgir::command_type_t ctype,
    const command_flag_t flags
) {
    /* pool-owned command: tagged so completion recycles it to the pool. This does
     * not touch the ring: the caller fills the command then publishes it with
     * 'commit' (or, on abort, returns it with pool.free). */
    return this->pool.alloc(ctype, flags | COMMAND_FLAG_POOLED);
}

int
command_queue_t::commit(command_t * cmd)
{
    assert(cmd);

    const uint64_t C = (uint64_t) this->capacity;

    /* Vyukov bounded-MPMC enqueue: reserve a slot with a CAS on 'w', then publish
     * the command by bumping the slot 'seq' to pos+1 (release). Producers are
     * lock-free and may run concurrently from any thread. */
    uint64_t pos = this->w.load(std::memory_order_relaxed);
    for (;;)
    {
        command_queue_entry_t * e = &this->entries[pos % C];
        const uint64_t seq  = e->seq.load(std::memory_order_acquire);
        const int64_t  diff = (int64_t) seq - (int64_t) pos;

        if (diff == 0)
        {
            /* slot is free and it is ours if the CAS succeeds */
            if (this->w.compare_exchange_weak(pos, pos + 1, std::memory_order_relaxed))
            {
                e->cmd       = cmd;
                e->completed = false;
                e->seq.store(pos + 1, std::memory_order_release); /* publish */

                XKRT_STATS_INCR(this->stats.commands[cmd->type].commited, 1);
                LOGGER_DEBUG(
                    "Commited a command of type `%s` at slot %u",
                    cgir::command_type_to_str(cmd->type),
                    (unsigned) (pos % C)
                );
                return 0;
            }
            /* CAS failed: 'pos' was reloaded, retry */
        }
        else if (diff < 0)
        {
            /* the slot's previous occupant has not been reclaimed yet: the ring is
             * full. A single ring bounds ready+pending to 'capacity'. */
            LOGGER_FATAL("Command queue is full, increase 'XKRT_OFFLOADER_CAPACITY' or implement support for full-queue management yourself :-) (sorry)");
            return ENOSPC;
        }
        else
        {
            /* another producer is ahead of us: reload and retry */
            pos = this->w.load(std::memory_order_relaxed);
        }
    }
}

//////////////////////////////////////////////////
//  LAUNCH - called by the owning thread only    //
//////////////////////////////////////////////////

command_t *
command_queue_t::ready_peek(xkrt_command_queue_list_counter_t * slot)
{
    const uint64_t s = this->s;

    /* a slot is launchable only once its producer published it (seq == s + 1);
     * this also naturally reports "empty" and preserves in-order launch. */
    command_queue_entry_t * e = &this->entries[s % this->capacity];
    if (e->seq.load(std::memory_order_acquire) != s + 1)
        return NULL;

    *slot = (xkrt_command_queue_list_counter_t) (s % this->capacity);
    return e->cmd;
}

void
command_queue_t::ready_commit_pending(void)
{
    /* keep the just-launched command in the pending region [r, s) */
    ++this->s;
}

void
command_queue_t::ready_commit_synchronous(void)
{
    /* a synchronous command never enters the device: mark it completed so
     * 'reclaim' frees its slot, then advance past it */
    this->entries[this->s % this->capacity].completed = true;
    ++this->s;
}

//////////////////////////////////////////////
//  MANAGEMENT CALLED BY THE OWNING THREAD  //
//////////////////////////////////////////////

static inline void
__complete_command_internal(
    command_queue_t * queue,
    const xkrt_command_queue_list_counter_t p
) {
    assert(p < queue->capacity);

    command_queue_entry_t * e = &queue->entries[p];

    /* idempotent: a slot may already be completed (e.g. an out-of-order
     * non-blocking progress before a blocking pending-wait). Completing twice
     * would double-raise callbacks and double-free the pooled command. */
    if (e->completed)
        return ;

    command_t * cmd = e->cmd;
    assert(cmd);

    LOGGER_DEBUG(
        "Completed command `%s` on queue %p of type `%s`",
        cgir::command_type_to_str(cmd->type),
        queue,
        command_queue_type_to_str(queue->type)
    );

    e->completed = true;
    cmd->completion_callback_raise();
    XKRT_STATS_INCR(queue->stats.commands[cmd->type].completed, 1);

    switch (cmd->type)
    {
        case (cgir::COMMAND_TYPE_COPY_H2H_1D):
        case (cgir::COMMAND_TYPE_COPY_H2D_1D):
        case (cgir::COMMAND_TYPE_COPY_D2H_1D):
        case (cgir::COMMAND_TYPE_COPY_D2D_1D):
        {
            XKRT_STATS_INCR(queue->stats.transfered, cmd->copy_1D.size);
            break ;
        }

        case (cgir::COMMAND_TYPE_COPY_H2H_2D):
        case (cgir::COMMAND_TYPE_COPY_H2D_2D):
        case (cgir::COMMAND_TYPE_COPY_D2H_2D):
        case (cgir::COMMAND_TYPE_COPY_D2D_2D):
        {
            XKRT_STATS_INCR(queue->stats.transfered, cmd->copy_2D.m * cmd->copy_2D.n * cmd->copy_2D.sizeof_type);
            break ;
        }

        default:
        {
            break ;
        }
    }

    /* recycle pool-owned commands; externally-owned commands (e.g. graph node
     * commands pushed for replay) are left untouched. Done last: callbacks and
     * stats above still read 'cmd'. */
    if (cmd->flags & COMMAND_FLAG_POOLED)
        queue->pool.free(cmd);
}

void
command_queue_t::complete_command(const xkrt_command_queue_list_counter_t p)
{
    assert(p < this->capacity);
    __complete_command_internal(this, p);
}

void
command_queue_t::complete_commands(void)
{
    const uint64_t C = (uint64_t) this->capacity;

    /* complete every pending command [r, s) (idempotent) ... */
    for (uint64_t i = this->r; i != this->s; ++i)
        __complete_command_internal(this, (xkrt_command_queue_list_counter_t) (i % C));

    /* ... then reclaim them all (they are now all completed) */
    this->reclaim();
}

void
command_queue_t::reclaim(void)
{
    const uint64_t C = (uint64_t) this->capacity;

    /* advance 'r' over the completed prefix of the pending region, releasing each
     * slot back to producers by bumping its 'seq' to (index + capacity). */
    uint64_t r = this->r;
    while (r != this->s && this->entries[r % C].completed)
    {
        this->entries[r % C].seq.store(r + C, std::memory_order_release);
        ++r;
    }
    this->r = r;
}

xkrt_command_queue_list_counter_t
command_queue_t::progress(
    const std::function<bool(command_t * cmd, xkrt_command_queue_list_counter_t p)> & process
) {
    const uint64_t C = (uint64_t) this->capacity;

    /* iterate the pending region [r, s), skipping already-completed slots */
    for (uint64_t i = this->r; i != this->s; ++i)
    {
        const xkrt_command_queue_list_counter_t p = (xkrt_command_queue_list_counter_t) (i % C);
        command_queue_entry_t * e = &this->entries[p];
        if (e->completed)
            continue ;
        if (!process(e->cmd, p))
            break ;
    }
    return 0;
}

XKRT_NAMESPACE_END;
