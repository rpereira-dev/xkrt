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

#ifndef __STREAM_HPP__
# define __STREAM_HPP__

# include <xkrt/support.h>
# include <xkrt/driver/stream-instruction.h>
# include <xkrt/driver/stream-type.h>
# include <xkrt/stats/stats.h>
# include <xkrt/sync/lockable.hpp>
# include <xkrt/thread/thread.h>

# include <atomic>

XKRT_NAMESPACE_BEGIN

    class stream_instruction_queue_t
    {
        public:

            stream_instruction_t * instr;                /* instructions buffer */
            stream_instruction_counter_t capacity;       /* buffer capacity */
            struct {
                volatile stream_instruction_counter_t r; /* first instruction to process */
                volatile stream_instruction_counter_t w; /* next position for inserting instructions */
            } pos;

        public:

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

            stream_instruction_counter_t
            size(void) const
            {
                if (this->pos.r <= this->pos.w)
                    return (this->pos.w - this->pos.r);
                else
                    return this->capacity - this->pos.r + this->pos.w;
            }

            template<typename Func>
            stream_instruction_counter_t
            iterate(Func && process)
            {
                const stream_instruction_counter_t a = this->pos.r;
                const stream_instruction_counter_t b = this->pos.w;

                assert(a >= 0);
                assert(b >= 0);
                assert(a < this->capacity);
                assert(b < this->capacity);

                if (a <= b) {
                    for (stream_instruction_counter_t i = a; i < b; ++i)
                        if (!process(i)) return i;
                } else {
                    for (stream_instruction_counter_t i = a; i < capacity; ++i)
                        if (!process(i)) return i;
                    for (stream_instruction_counter_t i = 0; i < b; ++i)
                        if (!process(i)) return i;
                }
                return b;
            }
    };

    # pragma message(TODO "make this a C++ class and use inheritance/pure virtual - currently hybrid of C struct C++ class :(")

    /* this is a 'io_stream' equivalent */
    class stream_t : public Lockable
    {
        public:

            /* the type of that stream */
            stream_type_t type;

            /* queue for ready instruction */
            stream_instruction_queue_t ready;

            /* queue for pending instructions to progress */
            stream_instruction_queue_t pending;

            # if XKRT_SUPPORT_STATS
            struct {
                struct {
                    stats_int_t commited;
                    stats_int_t completed;
                } instructions[XKRT_STREAM_INSTR_TYPE_MAX];
                stats_int_t transfered;
            } stats;
            # endif /* XKRT_SUPPORT_STATS */

            /* launch a stream instruction */
            int (*f_instruction_launch)(stream_t * stream, stream_instruction_t * instr, stream_instruction_counter_t idx);

            /* progrtream instruction */
            int (*f_instructions_progress)(stream_t * stream);

            /* wait instructions completion on a stream */
            int (*f_instructions_wait)(stream_t * stream);

            /* wait instructions completion on a stream */
            int (*f_instruction_wait)(stream_t * stream, stream_instruction_t * instr, stream_instruction_counter_t idx);

        public:

            /* allocate a new instruction to the stream (must then be commited via 'commit') */
            stream_instruction_t * instruction_new(
                const stream_instruction_type_t itype,
                const callback_t & callback
            );

            /* complete the instruction at the i-th position in the pending queue (invoke the callback) */
            void complete_instruction(const stream_instruction_counter_t p);

            /* complete the instruction that must be in the pending queue */
            void complete_instruction(stream_instruction_t * instr);

            /* commit the instruction to the stream (must be allocated via 'instruction_new') */
            int commit(stream_instruction_t * instruction);

            /* launch instructions, and may generate pending instructions */
            int launch_ready_instructions(void);

            /* progress pending instructions */
            int progress_pending_instructions(void);

            /* (internal) complete all instructions to 'ok_p' */
            void complete_instructions(const stream_instruction_counter_t ok_p);

            /* wait for completion of all pending instructions */
            void wait_pending_instructions(void);

            /* return true if the stream is full of instructions, false otherwise */
            int is_full(void) const;

            /* return true if the stream is empty, false otherwise */
            int is_empty(void) const;


    };  /* stream_t */

    void stream_init(
        stream_t * stream,
        stream_type_t type,
        stream_instruction_counter_t capacity,
        int (*f_instruction_launch)(stream_t * stream, stream_instruction_t * instr, stream_instruction_counter_t idx),
        int (*f_instructions_progress)(stream_t * stream),
        int (*f_instructions_wait)(stream_t * stream),
        int (*f_instruction_wait)(stream_t * stream, stream_instruction_t * instr, stream_instruction_counter_t idx)
    );

    void stream_deinit(stream_t * stream);

    /* routine to launch a kernel from a thread */
    typedef void (*kernel_launcher_t)(
        stream_t * istream,
        stream_instruction_t * instr,
        stream_instruction_counter_t idx
    );

XKRT_NAMESPACE_END

#endif /* __STREAM_HPP__ */
