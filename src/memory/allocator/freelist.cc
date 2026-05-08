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

# include <xkrt/memory/allocator/freelist.hpp>
# include <xkrt/logger/logger.h>
# include <xkrt/logger/todo.h>

# include <cassert>
# include <cstring>
# include <cstdlib>

XKRT_NAMESPACE_USE;

/* static */ size_t
freelist_allocator_t::_compute_size(const memory_size_t & ms, size_t capacity)
{
    if (ms.unit == XKRT_MEMORY_SIZE_UNIT_RELATIVE)
        return (size_t) ((double)capacity * (double)ms.amount / (double)MEMORY_SIZE_TYPE_MAX);
    else
        return ms.amount;
}

freelist_allocator_t::freelist_allocator_t(
    memory_size_t                    memory_size_initial,
    memory_size_t                    memory_size_resize,
    f_memory_device_allocate_t       f_alloc,
    f_memory_device_deallocate_t     f_dealloc,
    device_driver_id_t               device_driver_id,
    int                              nmemories,
    const size_t                   * capacities
)
    : allocator_t(
        memory_size_initial,
        memory_size_resize,
        f_alloc,
        f_dealloc,
        device_driver_id,
        nmemories,
        capacities
    )
{
    memset(this->_areas, 0, sizeof(this->_areas));
    memset(this->_initialized, 0, sizeof(this->_initialized));
    for (int i = 0 ; i < XKRT_DEVICE_MEMORIES_MAX ; ++i)
        XKRT_MUTEX_INIT(this->_areas[i].lock);
}

freelist_allocator_t::~freelist_allocator_t()
{
}

void
freelist_allocator_t::_set_chunk0(
    uintptr_t ptr,
    size_t size,
    int area_idx
) {
    area_t * area = &(this->_areas[area_idx]);

    area->chunk0.ptr         = ptr;
    area->chunk0.size        = size;
    area->chunk0.state       = XKRT_ALLOC_CHUNK_STATE_FREE;
    area->chunk0.prev        = NULL;
    area->chunk0.next        = NULL;
    area->chunk0.freelink    = NULL;
    area->chunk0.use_counter = 0;

    /* reset the free list to chunk0 */
    # pragma message(TODO "This is leaking")
    area_chunk_t * chunk0 = (area_chunk_t *) malloc(sizeof(area_chunk_t));
    assert(chunk0);
    memcpy(chunk0, &(area->chunk0), sizeof(area_chunk_t));
    area->free_chunk_list = chunk0;
}

void
freelist_allocator_t::_lazy_init(int area_idx)
{
    assert(area_idx >= 0);
    assert(area_idx < this->_nmemories);

    if ((volatile bool) this->_initialized[area_idx])
        return ;

    area_t * area = &(this->_areas[area_idx]);

    XKRT_MUTEX_LOCK(area->lock);
    {
        if ((volatile bool) this->_initialized[area_idx] == false)
        {
            const size_t size = _compute_size(this->_memory_size_initial, this->_capacities[area_idx]);
            assert(this->_f_alloc);
            const void * device_ptr = this->_f_alloc(this->_device_driver_id, size, area_idx);
            if (device_ptr == NULL)
                LOGGER_FATAL("Out of GPU memory");
            assert(device_ptr);
            this->_set_chunk0((uintptr_t) device_ptr, size, area_idx);
            this->_initialized[area_idx] = true;
        }
    }
    XKRT_MUTEX_UNLOCK(area->lock);
}

void
freelist_allocator_t::reset_on(int area_idx)
{
    assert(area_idx >= 0);
    assert(area_idx < this->_nmemories);

    if (!this->_initialized[area_idx])
        return ;

    area_t * area = &(this->_areas[area_idx]);

    /* deallocate the backing device memory */
    assert(this->_f_dealloc);
    this->_f_dealloc(this->_device_driver_id, (void *) area->chunk0.ptr, area->chunk0.size, area_idx);

    /* mark as uninitialized — next allocate_on will re-allocate */
    this->_initialized[area_idx] = false;

    /* clear the area */
    XKRT_MUTEX_LOCK(area->lock);
    {
        area->chunk0.ptr         = 0;
        area->chunk0.size        = 0;
        area->chunk0.state       = XKRT_ALLOC_CHUNK_STATE_FREE;
        area->chunk0.prev        = NULL;
        area->chunk0.next        = NULL;
        area->chunk0.freelink    = NULL;
        area->chunk0.use_counter = 0;
        area->free_chunk_list    = NULL;
    }
    XKRT_MUTEX_UNLOCK(area->lock);
}

void
freelist_allocator_t::reset(void)
{
    for (int i = 0 ; i < this->_nmemories ; ++i)
        this->reset_on(i);
}

void
freelist_allocator_t::finalize(void)
{
    for (int i = 0 ; i < this->_nmemories ; ++i)
    {
        if (!this->_initialized[i])
            continue ;

        area_t * area = &(this->_areas[i]);

        /* deallocate the backing device memory */
        assert(this->_f_dealloc);
        this->_f_dealloc(this->_device_driver_id, (void *) area->chunk0.ptr, area->chunk0.size, i);

        this->_initialized[i] = false;
    }
}

void
freelist_allocator_t::deallocate(area_chunk_t * chunk)
{
    return this->deallocate_on(chunk, chunk->area_idx);
}

void
freelist_allocator_t::deallocate_on(area_chunk_t * chunk, int area_idx)
{
    assert(chunk->area_idx >= 0);
    area_t * area = &(this->_areas[area_idx]);

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

    if (delete_chunk)
        free(chunk);
}

area_chunk_t *
freelist_allocator_t::allocate_on(const size_t user_size, int area_idx)
{
    /* ensure backing device memory is allocated for this area */
    this->_lazy_init(area_idx);

    area_t * area = &(this->_areas[area_idx]);

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
    }

    return curr;
}

area_chunk_t *
freelist_allocator_t::allocate(const size_t user_size)
{
    return this->allocate_on(user_size, 0);
}
