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
# include <xkrt/stats/stats.h>
# include <xkrt/support.h>
# include <xkrt/sync/lockable.hpp>
# include <xkrt/sync/spinlock.h>
# include <xkrt/thread/thread.h>
# include <xkrt/thread/reentrant_spinlock.h>

# include <atomic>
# include <new>
# include <vector>

XKRT_NAMESPACE_BEGIN

const char * command_queue_type_to_str(xkrt_command_queue_type_t type);

struct command_queue_list_t
{
    // TODO: memory layout: do we want AoS or SoA here ?
    command_t ** cmd;                                /* ring of command pointers */
    xkrt_command_queue_list_counter_t capacity;      /* buffer capacity */
    struct {
        volatile xkrt_command_queue_list_counter_t r; /* first command to process */
        volatile xkrt_command_queue_list_counter_t w; /* next position for inserting commands */
    } pos;

    /* methods */
    int
    is_full(void) const
    {
        return (this->pos.w  == this->pos.r - 1);
    }

    int
    is_empty(void) const
    {
        return (this->pos.r == this->pos.w);
    }

    xkrt_command_queue_list_counter_t
    size(void) const
    {
        if (this->pos.r <= this->pos.w)
            return (this->pos.w - this->pos.r);
        else
            return this->capacity - this->pos.r + this->pos.w;
    }

    /**
     *  Iterate on each command at index p of the list,
     *  and stop early if process(p) returns false
     */
    inline xkrt_command_queue_list_counter_t
    iterate(const std::function<bool(xkrt_command_queue_list_counter_t p)> & process)
    {
        const xkrt_command_queue_list_counter_t a = this->pos.r;
        const xkrt_command_queue_list_counter_t b = this->pos.w;

        assert(a < this->capacity);
        assert(b < this->capacity);

        if (a <= b) {
            for (xkrt_command_queue_list_counter_t i = a; i < b; ++i)
                if (!process(i)) return i;
        } else {
            for (xkrt_command_queue_list_counter_t i = a; i < capacity; ++i)
                if (!process(i)) return i;
            for (xkrt_command_queue_list_counter_t i = 0; i < b; ++i)
                if (!process(i)) return i;
        }
        return b;
    }
};

/*
 * Per-queue pool of `command_t` storage. The queue rings hold `command_t *`; the
 * pointers are either externally owned (e.g. a command-graph node command pushed
 * for replay -- so the driver's in-place mutations, like a resolved device
 * function handle, persist across replays) or allocated here for producers that
 * need the runtime to own the command (copies, file I/O, task-emitted programs).
 *
 * Storage is append-only with stable addresses (memory_pool_t); completed pooled
 * commands are recycled through the freelist. Bounded by the queue capacity in
 * steady state. Its own spinlock guards it because 'alloc' runs under the queue's
 * reentrant spinlock (producer threads) while 'free' runs on the owning thread at
 * completion (without the queue lock).
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

/* this is a 'io_queue' equivalent */
struct command_queue_t
{
    /* the type of that queue */
    command_queue_type_t type;

    // TODO: currently, ready/pending/completed are SoA, we probably want AoS to:
    //  - perform fast copy from ready to pending
    //  - fast test of completion given a command

    /* queue for ready command */
    command_queue_list_t ready;

    /* queue for pending commands to progress */
    command_queue_list_t pending;

    /* whether command at index 'p' is completed */
    bool * completed;

    /* storage pool for runtime-owned commands pushed through 'command_new'
     * (externally-owned commands pushed via 'emplace' do not use it) */
    command_pool_t pool;

    /* spinlock on the ready queue
     *  - any thread may push to it
     *  - the owning thread may move from it to the pending queue
     * TODO: a heavy reentrant spinlock is not satisfying here; but it is a
     * simple working solution to avoid deadlock when progressing/completing
     * commands triggers resubmitting to the same queue
     */
    reentrant_spinlock_t reentrant_spinlock;

    # if XKRT_SUPPORT_STATS
    struct {
        struct {
            stats_int_t commited;
            stats_int_t completed;
        } commands[cgir::COMMAND_TYPE_MAX];
        stats_int_t transfered;
    } stats;
    # endif /* XKRT_SUPPORT_STATS */

    /**
     *  Return true if the queue is full of commands, false otherwise
     *  Threading: called by any thread
     */
    int is_full(void) const;

    /**
     *  Allocate a new pool-owned command and place its pointer at the next ready
     *  slot (must then be commited via 'commit'). The command carries
     *  COMMAND_FLAG_POOLED and is recycled to the pool on completion.
     *  Threading: called by any thread (under the queue reentrant spinlock)
     */
    command_t * command_new(const cgir::command_type_t ctype, const command_flag_t flags);

    /**
     *  Commit a command previously placed at the next ready slot (via 'command_new'
     *  or 'emplace'): mark it not-completed and advance the ready write cursor.
     *  Threading: called by any thread
     */
    int commit(const command_t * command);

    /**
     *  Store the passed (externally-owned) command POINTER at the next ready slot.
     *  The command must outlive its completion (e.g. a command-graph node command);
     *  it is NOT freed by the queue. Follow with 'commit'.
     *  Threading: called by any thread
     */
    int emplace(command_t * command);

    /**
     *  Iterate on each command at index p of the list, if it is not completed already.
     *  Stop early if process(cmd, p) returned false
     *  Threading: called by the owning thread only
     */
    xkrt_command_queue_list_counter_t progress(const std::function<bool(command_t * cmd, xkrt_command_queue_list_counter_t p)> & process);

    /**
     *  Complete the command at the i-th position in the pending queue (invoke callbacks)
     *  Threading: called by the owning thread only
     */
    void complete_command(const xkrt_command_queue_list_counter_t p);

    /**
     *  Complete all commands until index 'ok_p' (see complete_command)
     *  Threading: called by the owning thread only
     */
    void complete_commands(const xkrt_command_queue_list_counter_t ok_p);

};  /* command_queue_t */

void command_queue_init(
    command_queue_t * queue,
    command_queue_type_t qtype,
    xkrt_command_queue_list_counter_t capacity
);

void command_queue_deinit(command_queue_t * queue);

XKRT_NAMESPACE_END

#endif /* __QUEUE_HPP__ */
