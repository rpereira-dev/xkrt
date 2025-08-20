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
# include <xkrt/driver/stream.h>
# include <xkrt/logger/todo.h>

const char *
xkrt_stream_type_to_str(xkrt_stream_type_t type)
{
    switch (type)
    {
        case (XKRT_STREAM_TYPE_H2D):        return "H2D";
        case (XKRT_STREAM_TYPE_D2H):        return "D2H";
        case (XKRT_STREAM_TYPE_D2D):        return "D2D";
        case (XKRT_STREAM_TYPE_KERN):       return "KERN";
        case (XKRT_STREAM_TYPE_FD_READ):    return "FD_READ";
        case (XKRT_STREAM_TYPE_FD_WRITE):   return "FD_WRITE";
        case (XKRT_STREAM_TYPE_ALL):        return "ALL";
        default:                            return  NULL;
    }
}

const char *
xkrt_stream_instruction_type_to_str(xkrt_stream_instruction_type_t type)
{
     switch (type)
     {
         case (XKRT_STREAM_INSTR_TYPE_KERN):        return "KERN";
         case (XKRT_STREAM_INSTR_TYPE_COPY_H2H_1D): return "COPY_H2H_1D";
         case (XKRT_STREAM_INSTR_TYPE_COPY_H2D_1D): return "COPY_H2D_1D";
         case (XKRT_STREAM_INSTR_TYPE_COPY_D2H_1D): return "COPY_D2H_1D";
         case (XKRT_STREAM_INSTR_TYPE_COPY_D2D_1D): return "COPY_D2D_1D";
         case (XKRT_STREAM_INSTR_TYPE_COPY_H2H_2D): return "COPY_H2H_2D";
         case (XKRT_STREAM_INSTR_TYPE_COPY_H2D_2D): return "COPY_H2D_2D";
         case (XKRT_STREAM_INSTR_TYPE_COPY_D2H_2D): return "COPY_D2H_2D";
         case (XKRT_STREAM_INSTR_TYPE_COPY_D2D_2D): return "COPY_D2D_2D";
         case (XKRT_STREAM_INSTR_TYPE_FD_READ):     return "FD_READ";
         case (XKRT_STREAM_INSTR_TYPE_FD_WRITE):    return "FD_WRITE";
         default:                                   return NULL;
    }
}

static inline void
xkrt_stream_instruction_queue_init(
    xkrt_stream_instruction_queue_t * queue,
    uint8_t * buffer,
    xkrt_stream_instruction_counter_t capacity
) {
    queue->instr = (xkrt_stream_instruction_t *) buffer;
    queue->capacity = capacity;
    queue->pos.r = 0;
    queue->pos.w = 0;
}

void
xkrt_stream_init(
    xkrt_stream_t * stream,
    xkrt_stream_type_t type,
    xkrt_stream_instruction_counter_t capacity,
    int (*f_stream_instruction_launch)(xkrt_stream_t * stream, xkrt_stream_instruction_t * instr, xkrt_stream_instruction_counter_t idx),
    int (*f_stream_instructions_progress)(xkrt_stream_t * stream),
    int (*f_stream_instructions_wait)(xkrt_stream_t * stream),
    int (*f_stream_instruction_wait)(xkrt_stream_t * stream, xkrt_stream_instruction_t * instr, xkrt_stream_instruction_counter_t idx)
) {
    stream->type = type;
    stream->spinlock = SPINLOCK_INITIALIZER;

    stream->f_instruction_launch    = f_stream_instruction_launch;
    stream->f_instructions_progress = f_stream_instructions_progress;
    stream->f_instructions_wait     = f_stream_instructions_wait;
    stream->f_instruction_wait      = f_stream_instruction_wait;

    uint8_t * mem = (uint8_t *) malloc(sizeof(xkrt_stream_instruction_t) * capacity * 2);
    assert(mem);

    xkrt_stream_instruction_queue_init(
        &stream->ready,
        mem,
        capacity
    );

    xkrt_stream_instruction_queue_init(
       &stream->pending,
        mem + sizeof(xkrt_stream_instruction_t) * capacity,
        capacity
    );

    # if XKRT_SUPPORT_STATS
    memset(&(stream->stats), 0, sizeof(stream->stats));
    # endif /* XKRT_SUPPORT_STATS */
}

void
xkrt_stream_deinit(xkrt_stream_t * stream)
{
    assert(stream);
    assert(stream->ready.instr);
    assert(stream->pending.instr);

    free(stream->ready.instr);
}

xkrt_stream_instruction_t *
xkrt_stream_t::instruction_new(
    const xkrt_stream_instruction_type_t itype,
    const xkrt_callback_t & callback
) {
    if (this->ready.is_full())
        return NULL;

    assert(this->ready.pos.w >= 0 && this->ready.pos.w < this->ready.capacity);
    xkrt_stream_instruction_t * instr = this->ready.instr + this->ready.pos.w;
    instr->type = itype;
    instr->callback = callback;
    instr->completed = false;

    return instr;
}

int
xkrt_stream_t::commit(xkrt_stream_instruction_t * instr)
{
    assert(instr);
    assert(!this->ready.is_full());

    this->ready.pos.w = (this->ready.pos.w + 1) % this->ready.capacity;
    XKRT_STATS_INCR(this->stats.instructions[instr->type].commited, 1);

    LOGGER_DEBUG(
        "Commited an instruction of type `%s` (%d ready, %d pending)`",
        xkrt_stream_instruction_type_to_str(instr->type),
        this->ready.size(),
        this->pending.size()
    );

    this->unlock();

    return 0;
}

int
xkrt_stream_t::launch_ready_instructions(void)
{
    assert(this->f_instruction_launch);
    if (this->ready.is_empty())
        return 0;

    int err = 0;

    /* for each ready instruction */
    const xkrt_stream_instruction_counter_t p = this->ready.iterate([this, &err] (xkrt_stream_instruction_counter_t p) {

        /* if the pending queue is full, we cannot start more instructions */
        if (this->pending.is_full())
            return false;

        /* retrieve it */
        xkrt_stream_instruction_t * instr = this->ready.instr + p;
        assert(instr);

        LOGGER_DEBUG(
            "Decoding instruction `%s` on stream %p of type `%s` - p=%u, r=%u, w=%u",
            xkrt_stream_instruction_type_to_str(instr->type),
            this,
            xkrt_stream_type_to_str(this->type),
            p,
            this->ready.pos.r,
            this->ready.pos.w
        );

        switch (instr->type)
        {
            /* kernel instructions are launched by the instruction itself, not the driver */
            case (XKRT_STREAM_INSTR_TYPE_KERN):
            {
                err = EINPROGRESS;
                ((xkrt_kernel_launcher_t) instr->kern.launch)(this, instr, p);
                break ;
            }

            case (XKRT_STREAM_INSTR_TYPE_COPY_H2H_1D):
            case (XKRT_STREAM_INSTR_TYPE_COPY_H2H_2D):
            {
                LOGGER_FATAL("Not implemented");
                break ;
            }

            /* launch instruction */
            case (XKRT_STREAM_INSTR_TYPE_COPY_H2D_1D):
            case (XKRT_STREAM_INSTR_TYPE_COPY_D2H_1D):
            case (XKRT_STREAM_INSTR_TYPE_COPY_D2D_1D):
            case (XKRT_STREAM_INSTR_TYPE_COPY_H2D_2D):
            case (XKRT_STREAM_INSTR_TYPE_COPY_D2H_2D):
            case (XKRT_STREAM_INSTR_TYPE_COPY_D2D_2D):
            case (XKRT_STREAM_INSTR_TYPE_FD_READ):
            case (XKRT_STREAM_INSTR_TYPE_FD_WRITE):
            default:
            {
                err = this->f_instruction_launch(this, instr, p);
                break ;
            }
        }

        /* retrieve error code */
        switch (err)
        {
            case (0):
            {
                LOGGER_FATAL("Instructions completing early not supported");
                break ;
            }

            /* if in progress, move from the ready to the pending queue */
            case (EINPROGRESS):
            {
                /* the pending queue must not be full at that point */
                assert(!this->pending.is_full());
                const xkrt_stream_instruction_counter_t wp = this->pending.pos.w;
                this->pending.pos.w = (this->pending.pos.w + 1) % this->pending.capacity;

                memcpy(
                    this->pending.instr + wp,
                    this->ready.instr   + p,
                    sizeof(xkrt_stream_instruction_t)
                );

                break ;
            }

            case (ENOSYS):
            {
                LOGGER_FATAL("Instruction `%s` not implemented",
                        xkrt_stream_instruction_type_to_str(instr->type));
                break ;
            }

            default:
            {
                LOGGER_FATAL("Unknown error after decoding instruction");
            }
        }

        /* continue */
        LOGGER_DEBUG("(loop) ready.is_empty() = %d, pending.is_empty() = %d", this->ready.is_empty(), this->pending.is_empty());
        return true;

    }); /* RING_ITERATE */

    // this barrier ensures that the threads that owns the stream correctly
    // sees the 'ready' stream empty but not the 'pending' - else it would go
    // to sleep even though there is pending instructions
    writemem_barrier();
    this->ready.pos.r = p;

    LOGGER_DEBUG("ready.is_empty() = %d, pending.is_empty() = %d", this->ready.is_empty(), this->pending.is_empty());

    return err;
}

// TODO : allow out of order completion

template <bool set_completed_flag>
static inline void
__complete_instruction_internal(
    xkrt_stream_t * stream,
    xkrt_stream_instruction_t * instr
) {
    assert(instr >= stream->pending.instr);
    assert(instr <  stream->pending.instr + stream->pending.capacity);

    LOGGER_DEBUG(
        "Completed instruction `%s` on stream %p of type `%s`",
        xkrt_stream_instruction_type_to_str(instr->type),
        stream,
        xkrt_stream_type_to_str(stream->type)
    );

    if (set_completed_flag)
        instr->completed = true;

    if (instr->callback.func)
        instr->callback.func(instr->callback.args);

    XKRT_STATS_INCR(stream->stats.instructions[instr->type].completed, 1);
}

template <bool set_completed_flag>
static inline void
__complete_instruction_internal(xkrt_stream_t * stream, const xkrt_stream_instruction_counter_t p)
{
    xkrt_stream_instruction_t * instr = stream->pending.instr + p;
    __complete_instruction_internal<set_completed_flag>(stream, instr);
}

// complete the given instruction
void
xkrt_stream_t::complete_instruction(const xkrt_stream_instruction_counter_t p)
{
    assert(p >= 0);
    assert(p <  this->pending.capacity);
    __complete_instruction_internal<true>(this, p);
}

void
xkrt_stream_t::complete_instruction(xkrt_stream_instruction_t * instr)
{
    __complete_instruction_internal<true>(this, instr);
}

void
xkrt_stream_t::complete_instructions(const xkrt_stream_instruction_counter_t p)
{
    this->pending.iterate([this] (xkrt_stream_instruction_counter_t p) {
        __complete_instruction_internal<false>(this, p);
        return true;
    });
    this->pending.pos.r = p;
}

void
xkrt_stream_t::wait_pending_instructions(void)
{
    if (!this->pending.is_empty())
    {
        this->f_instructions_wait(this);
        this->complete_instructions(this->pending.pos.w);
    }
}

int
xkrt_stream_t::progress_pending_instructions(void)
{
    assert(this->f_instructions_progress);

    if (this->pending.is_empty())
        return 0;

    LOGGER_DEBUG("Progressing pending instructions of stream %p of type `%s` (%d pending) - ptr at r=%u, w=%u",
            this, xkrt_stream_type_to_str(this->type), this->pending.size(), this->pending.pos.r, this->pending.pos.w);

    // ask for progression of the given instructions
    const int r = this->f_instructions_progress(this);

    // move reading position to first uncompleted instr
    const xkrt_stream_instruction_counter_t p = this->pending.iterate([this] (xkrt_stream_instruction_counter_t p) {
        return (this->pending.instr[p].completed) ? true : false;
    });
    this->pending.pos.r = p;

    LOGGER_DEBUG("Progressed pending instructions of stream %p of type `%s` (%d pending)",
            this, xkrt_stream_type_to_str(this->type), this->pending.size());

    // return err code
    return r;
}
