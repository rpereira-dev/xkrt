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
    int (*f_stream_instructions_progress)(xkrt_stream_t * stream, xkrt_stream_instruction_t * instr, xkrt_stream_instruction_counter_t idx),
    int (*f_stream_instructions_wait)(xkrt_stream_t * stream)
) {
    stream->type = type;
    stream->spinlock = SPINLOCK_INITIALIZER;

    stream->f_instruction_launch    = f_stream_instruction_launch;
    stream->f_instructions_progress = f_stream_instructions_progress;
    stream->f_instructions_wait     = f_stream_instructions_wait;

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

    const xkrt_stream_instruction_counter_t w = this->ready.pos.w % this->ready.capacity;
    xkrt_stream_instruction_t * instr = this->ready.instr + w;
    instr->type = itype;
    instr->callback = callback;

    return instr;
}

int
xkrt_stream_t::commit(xkrt_stream_instruction_t * instr)
{
    assert(instr);

    ++this->ready.pos.w;
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
    assert(this->ready.pos.r <= this->ready.pos.w);

    /* launch every ready instructions */
    int err = 0;
    while (!this->ready.is_empty())
    {
        /* retrieve the next instruction to launch at index 'p' */
        xkrt_stream_instruction_counter_t p = this->ready.pos.r % this->ready.capacity;

        xkrt_stream_instruction_t * instr = this->ready.instr + p;
        assert(instr);

        LOGGER_DEBUG(
            "Decoding instruction `%s` on stream %p of type `%s`",
            xkrt_stream_instruction_type_to_str(instr->type),
            this,
            xkrt_stream_type_to_str(this->type)
        );

        switch (instr->type)
        {
            case (XKRT_STREAM_INSTR_TYPE_KERN):
            {
                err = EINPROGRESS;
                instr->kern.launch(this, instr, p);
                break ;
            }

            case (XKRT_STREAM_INSTR_TYPE_COPY_H2H_1D):
            case (XKRT_STREAM_INSTR_TYPE_COPY_H2H_2D):
            {
                LOGGER_FATAL("Not implemented");
                break ;
            }

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

        switch (err)
        {
            case (0):
            {
                LOGGER_FATAL("Instructions completing early not supported");
                break ;
            }

            case (EINPROGRESS):
            {
                /* recopy op in pending op if still progressing */

                /* the pending queue must not be full */
                assert(!this->pending.is_full());
                const xkrt_stream_instruction_counter_t wp = this->pending.pos.w % this->pending.capacity;
                ++this->pending.pos.w;
                writemem_barrier();

                memcpy(
                    this->pending.instr + wp,
                    this->ready.instr + p,
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

        ++this->ready.pos.r;

    } /* while (!this->ready.is_empty()) */

    return err;
}

// complete all instructions to 'ok_p'
void
xkrt_stream_t::complete_instructions(const xkrt_stream_instruction_counter_t ok_p)
{
    assert(this->pending.pos.r < ok_p);
    assert(ok_p <= this->pending.pos.w);
    for (xkrt_stream_instruction_counter_t p = this->pending.pos.r ; p < ok_p ; ++p)
    {
        xkrt_stream_instruction_t * instr = this->pending.instr + (p % this->pending.capacity);
        assert(instr);

        if (instr->callback.func)
            instr->callback.func(instr->callback.args);

        XKRT_STATS_INCR(this->stats.instructions[instr->type].completed, 1);

        LOGGER_DEBUG(
            "Completed instruction `%s` on stream %p of type `%s`",
            xkrt_stream_instruction_type_to_str(instr->type),
            this,
            xkrt_stream_type_to_str(this->type)
        );
    }
    this->pending.pos.r = ok_p;
}

void
xkrt_stream_t::wait_pending_instructions(void)
{
    assert(this->pending.pos.r <= this->pending.pos.w);

    if (this->pending.pos.r < this->pending.pos.w)
    {
        this->f_instructions_wait(this);
        this->complete_instructions(this->pending.pos.w);
    }
}

int
xkrt_stream_t::progress_pending_instructions(void)
{
    // LOGGER_DEBUG("Progressing pending instructions of stream %p of type `%s` (%d pending)",
    //         this, xkrt_stream_type_to_str(this->type), this->pending.size());

    assert(this->pending.pos.r <= this->pending.pos.w);
    assert(this->f_instructions_progress);

    // empty stream
    if (this->pending.pos.r == this->pending.pos.w)
        return 0;

    int err;
    xkrt_stream_instruction_counter_t p    = this->pending.pos.r;
    xkrt_stream_instruction_counter_t okp  = this->pending.pos.r - 1;

    while (p < this->pending.pos.w)
    {
        const xkrt_stream_instruction_counter_t idx = p % this->pending.capacity;
        xkrt_stream_instruction_t * instr = this->pending.instr + idx;

        err = this->f_instructions_progress(this, instr, idx);
        assert((err == 0) || (err == EINPROGRESS));

        if (err == 0)
            if (okp + 1 == p)
                ++okp;

        ++p;
    }

    /* all events have been tested, test if ok_p has been incremented meaning
     * that some instructions completed */
    if (okp != this->pending.pos.r - 1)
        this->complete_instructions((xkrt_stream_instruction_counter_t) (okp + 1));

    return err;
}

/* return true if the stream is empty, false otherwise */
int
xkrt_stream_t::is_empty(void) const
{
    return (this->ready.is_empty() && this->pending.is_empty());
}
