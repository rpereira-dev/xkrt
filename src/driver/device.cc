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
            remainder->ptr          = size + curr->ptr;
            remainder->size         = (curr_size - size);
            remainder->state        = XKRT_ALLOC_CHUNK_STATE_FREE;
            remainder->use_counter  = 0;
            remainder->prev         = curr;
            remainder->next         = curr->next;
            remainder->freelink     = curr->freelink;

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
// STREAM MANAGEMENT //
///////////////////////

void
device_t::offloader_init(
    int (*f_stream_suggest)(int device_driver_id, stream_type_t type)
) {
    /* next stream to use (round robin) */
    this->next_thread = 0;
    memset(this->next_stream, 0, sizeof(this->next_stream));

    /* count total number of stream */
    this->nstreams_per_thread = 0;

    for (int stype = 0 ; stype < STREAM_TYPE_ALL ; ++stype)
    {
        this->count[stype] = (this->conf->offloader.streams[stype].n >= 0) ? this->conf->offloader.streams[stype].n : f_stream_suggest ? f_stream_suggest(this->driver_id, (stream_type_t) stype) : 4;
        this->nstreams_per_thread += this->count[stype];
    }
}

void
device_t::offloader_init_thread(
    uint8_t device_tid,
    stream_t * (*f_stream_create)(device_t * device, stream_type_t type, stream_instruction_counter_t capacity)
) {
    if (this->nstreams_per_thread == 0)
        return ;

    /* retrieve the thread that is initializing its streams */
    thread_t * thread = this->threads[device_tid];
    assert(thread);

    /* allocate streams array */
    assert(this->nstreams_per_thread);
    stream_t ** all_streams = (stream_t **) malloc(sizeof(stream_t *) * this->nstreams_per_thread);
    assert(all_streams);

    /* retrieve stream offset per type */
    uint16_t i = 0;
    for (int stype = 0 ; stype < STREAM_TYPE_ALL ; ++stype)
    {
        this->streams[device_tid][stype] = all_streams + i;
        for (int j = 0 ; j < this->count[stype] ; ++j, ++i)
        {
            // create a new stream
            all_streams[i] = f_stream_create(this, static_cast<stream_type_t>(stype), this->conf->offloader.capacity);
            if (all_streams[i] == NULL)
            {
                this->count[stype] = j;
                break ;
            }
        }
    }
    assert(i <= this->nstreams_per_thread);
}

void
device_t::offloader_streams_are_empty(
    uint8_t device_tid,
    const stream_type_t stype,
    bool * ready,
    bool * pending
) const {

    *ready   = false;
    *pending = false;

    unsigned int bgn = (stype == STREAM_TYPE_ALL) ?                    0 : stype;
    unsigned int end = (stype == STREAM_TYPE_ALL) ? STREAM_TYPE_ALL : stype + 1;

    for (unsigned int s = bgn ; s < end ; ++s)
    {
        for (int i = 0 ; i < this->count[s] ; ++i)
        {
            const stream_t * stream = this->streams[device_tid][s][i];
            if (*ready == false && !stream->ready.is_empty())
                *ready = true;
            if (*pending == false && !stream->pending.is_empty())
                *pending = true;
            if (*ready && *pending)
                return ;
        }
    }
}

int
device_t::offloader_stream_instructions_launch(
    uint8_t device_tid,
    const stream_type_t stype
) {
    int err = 0;

    unsigned int bgn = (stype == STREAM_TYPE_ALL) ?                    0 : stype;
    unsigned int end = (stype == STREAM_TYPE_ALL) ? STREAM_TYPE_ALL : stype + 1;
    for (unsigned int s = bgn ; s < end ; ++s)
    {
        for (int i = 0 ; i < this->count[s] ; ++i)
        {
            stream_t * stream = this->streams[device_tid][s][i];
            assert(stream);

            stream->lock();
            int r = stream->launch_ready_instructions();
            stream->unlock();

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
                    LOGGER_FATAL("Driver implementation of `stream_instruction_launch` returned an unknown error code");
                    break ;
                }
            }
        }
    }

    return err;
}

void
device_t::offloader_stream_next(
    stream_type_t stype,
    thread_t ** pthread,       /* OUT */
    stream_t ** pstream        /* OUT */
) {
    // round robin on the thread for this stream type
    uint8_t next_thread = this->next_thread.fetch_add(1, std::memory_order_relaxed) % this->nthreads;

    // round robin on streams for the streams of the given type on the choosen thread
    int count = this->count[stype];
    assert(count);
    int snext = this->next_stream[next_thread][stype].fetch_add(1, std::memory_order_relaxed) % count;

    // save thread/stream
    *pthread = this->threads[next_thread];
    *pstream  = this->streams[next_thread][stype][snext];
}

int
device_t::offloader_launch(uint8_t device_tid)
{
    int err = this->offloader_stream_instructions_launch(device_tid, STREAM_TYPE_ALL);
    assert((err == 0) || (err == EINPROGRESS));

    return err;
}

int
device_t::offloader_progress(uint8_t device_tid)
{
    int err = this->offloader_stream_instructions_progress<false>(device_tid, STREAM_TYPE_ALL);
    assert((err == 0) || (err == EINPROGRESS));

    return err;
}

int
device_t::offloader_wait_random_instruction(uint8_t device_tid)
{
    static unsigned int seed = 0x42;

    // randomly pick a type and a stream
    static_assert(STREAM_TYPE_ALL > 0);
    unsigned int rtype   = rand_r(&seed);
    unsigned int rstream = rand_r(&seed);
    for (unsigned int itype = 0 ; itype < STREAM_TYPE_ALL ; ++itype)
    {
        unsigned int s = (rtype + itype) % STREAM_TYPE_ALL;

        stream_t * stream = NULL;
        for (unsigned int istream = 0 ; istream < this->count[s] ; ++istream)
        {
            unsigned int i = (rstream + istream) % this->count[s];

            stream = this->streams[device_tid][s][i];
            assert(stream);

            if (!stream->pending.is_empty())
            {
                const stream_instruction_counter_t i = stream->pending.pos.r;
                assert(i >= 0);
                assert(i < stream->pending.capacity);

                stream_instruction_t * instr = stream->pending.instr + i;
                assert(instr);
                assert(!instr->completed);

                assert(stream->f_instruction_wait);

                // waiting on the first event of the randomly elected stream
                int err = stream->f_instruction_wait(stream, instr, i);

                // calling this to complete events and move queues pointers
                // but also detect out-of-order completions
                this->offloader_progress(device_tid);

                return err;
            }
        }
    }
    return 0;
}

////////////////////////////
// INSTRUCTION SUBMISSION //
////////////////////////////

/* commit a stream instruction and wakeup thread */
void
device_t::offloader_stream_instruction_commit(
    thread_t * thread,
    stream_t * stream,
    stream_instruction_t * instr
) {
    /* commit instruction to the stream */
    stream->commit(instr);

    /* wakeup device worker thread */
    thread->wakeup();
}

void
device_t::offloader_stream_instruction_new(
    const stream_type_t stype,             /* IN  */
          thread_t ** pthread,             /* OUT */
          stream_t ** pstream,             /* OUT */
    const stream_instruction_type_t itype, /* IN  */
          stream_instruction_t ** pinstr,  /* OUT */
    const callback_t & callback            /* IN */
) {
    assert(pstream);
    assert(pinstr);

    /* retrieve native stream */
    this->offloader_stream_next(stype, pthread, pstream);
    assert(*pthread);
    assert(*pstream);
    assert((*pstream)->type == stype);

    /* allocate the instruction */
    do {
        (*pstream)->lock();
        (*pinstr) = (*pstream)->instruction_new(itype, callback);
        if (*pinstr)
            break ;
        (*pstream)->unlock();

        LOGGER_FATAL("Stream is full, increase 'XKRT_OFFLOADER_CAPACITY' or implement support for full-queue management yourself :-) (sorry)");

    } while (1);

    /* stream is locked, will be unlocked in the commit */
}

void
device_t::offloader_stream_instruction_submit_kernel(
    kernel_launcher_t launch,
    void * vargs,
    const callback_t & callback
) {
    /* create a new instruction and retrieve its offload stream */
    thread_t * thread;
    stream_t * stream;
    stream_instruction_t * instr;
    this->offloader_stream_instruction_new(
        STREAM_TYPE_KERN,          /* IN */
       &thread,                         /* OUT */
       &stream,                         /* OUT */
        XKRT_STREAM_INSTR_TYPE_KERN,    /* IN */
       &instr,                          /* OUT */
        callback
    );
    assert(thread);
    assert(stream);
    assert(instr);
    assert(stream->is_locked());

    /* create a new kernel instruction */
    instr->kern.launch  = (void (*)()) launch;
    instr->kern.vargs   = vargs;

    this->offloader_stream_instruction_commit(thread, stream, instr);
}
