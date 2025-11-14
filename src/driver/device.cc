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

# include <xkrt/driver/device.hpp>
# include <xkrt/thread/team.h>

XKRT_NAMESPACE_USE;

//////////////////////
// MEMORY MANAGMENT //
//////////////////////

void
device_t::memory_reset_on(int area_idx)
{
    area_t * area = &(this->memories[area_idx].area);

    # pragma message(TODO "This is leaking")
    area_chunk_t * chunk0 = (area_chunk_t *) malloc(sizeof(area_chunk_t));
    assert(chunk0);
    memcpy(chunk0, &(area->chunk0), sizeof(area_chunk_t));
    area->free_chunk_list = chunk0;

    XKRT_STATS_INCR(this->stats.memory.freed, this->stats.memory.allocated.currently);
    XKRT_STATS_SET (this->stats.memory.allocated.currently, 0);
}

void
device_t::memory_reset(void)
{
    for (int i = 0 ; i < this->nmemories ; ++i)
        this->memory_reset_on(i);
}

void
device_t::memory_set_chunk0(
    uintptr_t ptr,
    size_t size,
    int area_idx
) {
    area_t * area = &(this->memories[area_idx].area);

    area->chunk0.ptr            = ptr;
    area->chunk0.size           = size;
    area->chunk0.state          = XKRT_ALLOC_CHUNK_STATE_FREE;
    area->chunk0.prev           = NULL;
    area->chunk0.next           = NULL;
    area->chunk0.freelink       = NULL;
    area->chunk0.use_counter    = 0;

    this->memory_reset_on(area_idx);
}

void
device_t::memory_deallocate(area_chunk_t * chunk)
{
    return this->memory_deallocate_on(chunk, chunk->area_idx);
}

void
device_t::memory_deallocate_on(area_chunk_t * chunk, int area_idx)
{
    assert(chunk->area_idx >= 0);
    assert(chunk->area_idx < this->nmemories);
    area_t * area = &(this->memories[area_idx].area);

    bool delete_chunk = false;
    XKRT_MUTEX_LOCK(area->lock);
    {
        chunk->state = XKRT_ALLOC_CHUNK_STATE_FREE;
        chunk->use_counter = 0;

        /* can we merge chunk into next_chunk ? */
        area_chunk_t * next_chunk = chunk->next;
        if (next_chunk && next_chunk->state == XKRT_ALLOC_CHUNK_STATE_FREE)
        {
            next_chunk->prev = chunk->prev;
            if (chunk->prev)
                chunk->prev->next = next_chunk;
            next_chunk->size += chunk->size;
            assert(next_chunk->ptr > chunk->ptr);
            next_chunk->ptr = chunk->ptr;
            delete_chunk = true;
        }

        area_chunk_t * prev_chunk = chunk->prev;
        if (prev_chunk)
        {
            /*  if prev_chunk is a free chunk and 'delete_chunk' is true,
             *  then we have to merge prev and next */
            if (prev_chunk->state == XKRT_ALLOC_CHUNK_STATE_FREE)
            {
                if (delete_chunk)
                {
                    assert(prev_chunk->ptr < chunk->ptr);
                    assert(prev_chunk->ptr < next_chunk->ptr);

                    prev_chunk->size += next_chunk->size;
                    prev_chunk->next = next_chunk->next;
                    if (next_chunk->next)
                        next_chunk->next->prev = prev_chunk;
                    prev_chunk->freelink = next_chunk->freelink;
                    free(next_chunk);
                }
                else
                {
                    /* merge chunk into prev_chunk */
                    assert(prev_chunk->ptr < chunk->ptr);
                    prev_chunk->next = chunk->next;
                    if (chunk->next)
                        chunk->next->prev = prev_chunk;
                    prev_chunk->size += chunk->size;
                    delete_chunk = true;
                }
            }
            else if (!delete_chunk)
            {
                /* free_chunk_list is ordered by increasing adress: search form prev the previous bloc */
                while (prev_chunk && prev_chunk->state != XKRT_ALLOC_CHUNK_STATE_FREE)
                    prev_chunk = prev_chunk->prev;

                if (!prev_chunk)
                {
                    chunk->freelink = area->free_chunk_list;
                    area->free_chunk_list = chunk;
                }
                else
                {
                    chunk->freelink = prev_chunk->freelink;
                    prev_chunk->freelink = chunk;
                }
            }
        }
        else if (!delete_chunk)
        {
            chunk->freelink = area->free_chunk_list;
            area->free_chunk_list = chunk;
        }
    }
    XKRT_MUTEX_UNLOCK(area->lock);

    XKRT_STATS_INCR(this->stats.memory.freed, chunk->size);
    XKRT_STATS_DECR(this->stats.memory.allocated.currently, chunk->size);

    if (delete_chunk)
        free(chunk);
}

area_chunk_t *
device_t::memory_allocate_on(const size_t user_size, int area_idx)
{
    /* retrieve area */
    assert(area_idx >= 0);
    assert(area_idx < this->nmemories);
    area_t * area = &(this->memories[area_idx].area);

    /* align data */
    const size_t size = (user_size + 7UL) & ~7UL;
    area_chunk_t * curr;

    XKRT_MUTEX_LOCK(area->lock);
    {
        /* best fit strategy */
        curr = area->free_chunk_list;

        area_chunk_t * prevfree = NULL;
        size_t min_size = 0;
        area_chunk_t * min_size_curr = NULL;
        area_chunk_t * min_size_prevfree = NULL;

        while (curr)
        {
            size_t curr_size = curr->size;
            if (curr_size >= size)
            {
                if ((min_size_curr == 0) || (min_size > curr_size))
                {
                    min_size = curr_size;
                    min_size_curr = curr;
                    min_size_prevfree = prevfree;
                }
            }
            prevfree = curr;
            curr = curr->freelink;
        }

        /* and the winner is min_size_curr ! */
        curr = min_size_curr;
        prevfree = min_size_prevfree;

        /* split chunk */
        if ((curr != NULL) && (min_size - size >= (size_t)(0.5*(double)size)))
        {
            size_t curr_size = curr->size;
            area_chunk_t * remainder = (area_chunk_t *) malloc(sizeof(area_chunk_t));
            remainder->ptr         = size + curr->ptr;
            remainder->size        = (curr_size - size);
            remainder->state       = XKRT_ALLOC_CHUNK_STATE_FREE;
            remainder->use_counter = 0;
            remainder->prev        = curr;
            remainder->next        = curr->next;
            remainder->freelink    = curr->freelink;

            /* link remainder segment after curr */
            if (curr->next)
                curr->next->prev = remainder;
            curr->next = remainder;
            curr->size = size;
            curr->freelink = remainder;
        }

        if (curr != NULL)
        {
            if (prevfree)
                prevfree->freelink = curr->freelink;
            else
                area->free_chunk_list = curr->freelink;
            curr->state = XKRT_ALLOC_CHUNK_STATE_ALLOCATED;
            curr->freelink = NULL;
        }
    }

    XKRT_MUTEX_UNLOCK(area->lock);

    if (curr)
    {
        curr->area_idx = area_idx;
        XKRT_STATS_INCR(this->stats.memory.allocated.total,       size);
        XKRT_STATS_INCR(this->stats.memory.allocated.currently,   size);
    }

    return curr;

}

area_chunk_t *
device_t::memory_allocate(const size_t user_size)
{
    return this->memory_allocate_on(user_size, 0);
}

///////////////////////
// QUEUE MANAGEMENT //
///////////////////////

void
device_t::offloader_init(
    int (*f_queue_suggest)(device_driver_id_t device_driver_id, queue_type_t type)
) {
    /* next queue to use (round robin) */
    this->next_thread = 0;
    memset(this->next_queue, 0, sizeof(this->next_queue));

    /* count total number of queue */
    this->nqueues_per_thread = 0;

    for (int qtype = 0 ; qtype < XKRT_QUEUE_TYPE_ALL ; ++qtype)
    {
        this->count[qtype] = (this->conf->offloader.queues[qtype].n >= 0) ? this->conf->offloader.queues[qtype].n : f_queue_suggest ? f_queue_suggest(this->driver_id, (queue_type_t) qtype) : 4;
        this->nqueues_per_thread += this->count[qtype];
    }
}

void
device_t::offloader_init_thread(
    int tid,
    queue_t * (*f_queue_create)(device_t * device, queue_type_t type, queue_command_list_counter_t capacity)
) {
    if (this->nqueues_per_thread == 0)
        return ;

    /* allocate queues array */
    assert(this->nqueues_per_thread);
    queue_t ** all_queues = (queue_t **) malloc(sizeof(queue_t *) * this->nqueues_per_thread);
    assert(all_queues);

    /* retrieve queue offset per type */
    uint16_t i = 0;
    for (int qtype = 0 ; qtype < XKRT_QUEUE_TYPE_ALL ; ++qtype)
    {
        this->queues[tid][qtype] = all_queues + i;
        for (int j = 0 ; j < this->count[qtype] ; ++j, ++i)
        {
            // create a new queue
            all_queues[i] = f_queue_create(this, static_cast<queue_type_t>(qtype), this->conf->offloader.capacity);
            if (all_queues[i] == NULL)
            {
                this->count[qtype] = j;
                break ;
            }
        }
    }
    assert(i <= this->nqueues_per_thread);
}

void
device_t::offloader_queues_are_empty(
    int tid,
    const queue_type_t qtype,
    bool * ready,
    bool * pending
) const {

    *ready   = false;
    *pending = false;

    unsigned int bgn = (qtype == XKRT_QUEUE_TYPE_ALL) ?                    0 : qtype;
    unsigned int end = (qtype == XKRT_QUEUE_TYPE_ALL) ? XKRT_QUEUE_TYPE_ALL : qtype + 1;

    for (unsigned int s = bgn ; s < end ; ++s)
    {
        for (int i = 0 ; i < this->count[s] ; ++i)
        {
            const queue_t * queue = this->queues[tid][s][i];
            if (*ready == false && !queue->ready.is_empty())
                *ready = true;
            if (*pending == false && !queue->pending.is_empty())
                *pending = true;
            if (*ready && *pending)
                return ;
        }
    }
}

int
device_t::offloader_queue_commands_launch(
    int tid,
    const queue_type_t qtype
) {
    int err = 0;

    unsigned int bgn = (qtype == XKRT_QUEUE_TYPE_ALL) ?                    0 : qtype;
    unsigned int end = (qtype == XKRT_QUEUE_TYPE_ALL) ? XKRT_QUEUE_TYPE_ALL : qtype + 1;
    for (unsigned int s = bgn ; s < end ; ++s)
    {
        for (int i = 0 ; i < this->count[s] ; ++i)
        {
            queue_t * queue = this->queues[tid][s][i];
            assert(queue);

            queue->lock();
            int r = queue->launch_ready_commands();
            queue->unlock();

            switch (r)
            {
                case (0):
                    break ;

                case (EINPROGRESS):
                {
                    err = EINPROGRESS;
                    break ;
                }

                case (ENOSYS):
                {
                    LOGGER_FATAL("Not implemented");
                    break ;
                }

                default:
                {
                    LOGGER_FATAL("Driver implementation of `queue_command_launch` returned an unknown error code");
                    break ;
                }
            }
        }
    }

    return err;
}

void
device_t::offloader_queue_next(
    queue_type_t qtype,
    thread_t ** pthread,    /* OUT */
    queue_t ** pqueue       /* OUT */
) {
    // round robin on the thread for this queue type
    int next_thread = this->next_thread.fetch_add(1, std::memory_order_relaxed) % this->team->get_nthreads();

    // round robin on queues for the queues of the given type on the choosen thread
    int count = this->count[qtype];
    assert(count);
    int snext = this->next_queue[next_thread][qtype].fetch_add(1, std::memory_order_relaxed) % count;

    // save thread/queue
    *pthread = this->team->get_thread(next_thread);
    *pqueue  = this->queues[next_thread][qtype][snext];
}

int
device_t::offloader_launch(int tid)
{
    int err = this->offloader_queue_commands_launch(tid, XKRT_QUEUE_TYPE_ALL);
    assert((err == 0) || (err == EINPROGRESS));

    return err;
}

int
device_t::offloader_progress(int tid)
{
    int err = this->offloader_queue_commands_progress<false>(tid, XKRT_QUEUE_TYPE_ALL);
    assert((err == 0) || (err == EINPROGRESS));

    return err;
}

int
device_t::offloader_wait_random_command(int tid)
{
    static unsigned int seed = 0x42;

    // randomly pick a type and a queue
    static_assert(XKRT_QUEUE_TYPE_ALL > 0);
    unsigned int rtype   = rand_r(&seed);
    unsigned int rqueue = rand_r(&seed);
    for (unsigned int itype = 0 ; itype < XKRT_QUEUE_TYPE_ALL ; ++itype)
    {
        unsigned int s = (rtype + itype) % XKRT_QUEUE_TYPE_ALL;

        queue_t * queue = NULL;
        for (int iqueue = 0 ; iqueue < this->count[s] ; ++iqueue)
        {
            unsigned int i = (rqueue + iqueue) % this->count[s];

            queue = this->queues[tid][s][i];
            assert(queue);

            // if the queue has pending commands
            if (!queue->pending.is_empty())
            {
                const queue_command_list_counter_t i = queue->pending.pos.r;
                assert(i >= 0);
                assert(i < queue->pending.capacity);

                command_t * cmd = queue->pending.cmd + i;
                assert(cmd);
                assert(!cmd->completed);

                assert(queue->f_command_wait);

                // waiting on the first event of the randomly elected queue
                int err = queue->f_command_wait(queue, cmd, i);

                // calling this to complete events and move queues pointers
                // but also detect out-of-order completions
                this->offloader_progress(tid);

                return err;
            }
        }
    }
    return 0;
}

////////////////////////////
// COMMAND SUBMISSION //
////////////////////////////

/* commit a queue command and wakeup thread */
void
device_t::offloader_queue_command_commit(
    thread_t * thread,
    queue_t * queue,
    command_t * cmd
) {
    /* commit command to the queue */
    queue->commit(cmd);

    /* wakeup device worker thread */
    thread->wakeup();
}

void
device_t::offloader_queue_command_new(
    const queue_type_t qtype,       /* IN  */
          thread_t ** pthread,      /* OUT */
          queue_t ** pqueue,        /* OUT */
    const command_type_t itype,     /* IN  */
          command_t ** pcmd         /* OUT */
) {
    assert(pqueue);
    assert(pcmd);

    /* retrieve native queue */
    this->offloader_queue_next(qtype, pthread, pqueue);
    assert(*pthread);
    assert(*pqueue);
    assert((*pqueue)->type == qtype);

    /* allocate the command */
    do {
        (*pqueue)->lock();
        (*pcmd) = (*pqueue)->command_new(itype);
        if (*pcmd)
            break ;
        (*pqueue)->unlock();

        LOGGER_FATAL("Stream is full, increase 'XKRT_OFFLOADER_CAPACITY' or implement support for full-queue management yourself :-) (sorry)");

    } while (1);

    /* queue is locked, will be unlocked in the commit */
}

command_t *
device_t::offloader_queue_command_submit_kernel(
    void * runtime,
    void * device,
    task_t * task,
    kernel_launcher_t launch,
    const callback_t & callback
) {
    /* create a new command and retrieve its offload queue */
    thread_t * thread;
    queue_t * queue;
    command_t * cmd;
    this->offloader_queue_command_new(
        XKRT_QUEUE_TYPE_KERN,   /* IN */
        &thread,                /* OUT */
        &queue,                 /* OUT */
        COMMAND_TYPE_KERN,      /* IN */
        &cmd                    /* OUT */
    );
    assert(thread);
    assert(queue);
    assert(cmd);
    assert(queue->is_locked());

    /* create a new kernel command */
    cmd->kern.launch  = (void (*)()) launch;
    cmd->kern.runtime = runtime;
    cmd->kern.device  = device;
    cmd->kern.task    = task;
    cmd->push_callback(callback);

    this->offloader_queue_command_commit(thread, queue, cmd);

    return cmd;
}
