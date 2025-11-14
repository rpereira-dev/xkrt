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

# include <xkrt/support.h>
# include <xkrt/driver/command.h>
# include <xkrt/driver/command-type.h>
# include <xkrt/driver/queue-type.h>
# include <xkrt/stats/stats.h>
# include <xkrt/sync/lockable.hpp>
# include <xkrt/thread/thread.h>

# include <atomic>

XKRT_NAMESPACE_BEGIN

const char * queue_type_to_str(xkrt_queue_type_t type);

class queue_command_list_t
{
    public:

        command_t * cmd;                /* commands buffer */
        queue_command_list_counter_t capacity;       /* buffer capacity */
        struct {
            volatile queue_command_list_counter_t r; /* first command to process */
            volatile queue_command_list_counter_t w; /* next position for inserting commands */
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

        queue_command_list_counter_t
        size(void) const
        {
            if (this->pos.r <= this->pos.w)
                return (this->pos.w - this->pos.r);
            else
                return this->capacity - this->pos.r + this->pos.w;
        }

        template<typename Func>
        queue_command_list_counter_t
        iterate(Func && process)
        {
            const queue_command_list_counter_t a = this->pos.r;
            const queue_command_list_counter_t b = this->pos.w;

            assert(a < this->capacity);
            assert(b < this->capacity);

            if (a <= b) {
                for (queue_command_list_counter_t i = a; i < b; ++i)
                    if (!process(i)) return i;
            } else {
                for (queue_command_list_counter_t i = a; i < capacity; ++i)
                    if (!process(i)) return i;
                for (queue_command_list_counter_t i = 0; i < b; ++i)
                    if (!process(i)) return i;
            }
            return b;
        }
};

# pragma message(TODO "make this a C++ class and use inheritance/pure virtual - currently hybrid of C struct C++ class :(")

/* this is a 'io_queue' equivalent */
class queue_t : public Lockable
{
    public:

        /* the type of that queue */
        queue_type_t type;

        /* queue for ready command */
        queue_command_list_t ready;

        /* queue for pending commands to progress */
        queue_command_list_t pending;

        # if XKRT_SUPPORT_STATS
        struct {
            struct {
                stats_int_t commited;
                stats_int_t completed;
            } commands[COMMAND_TYPE_MAX];
            stats_int_t transfered;
        } stats;
        # endif /* XKRT_SUPPORT_STATS */

        /* launch a queue command */
        int (*f_command_launch)(queue_t * queue, command_t * cmd, queue_command_list_counter_t idx);

        /* progrtream command */
        int (*f_commands_progress)(queue_t * queue);

        /* wait commands completion on a queue */
        int (*f_commands_wait)(queue_t * queue);

        /* wait commands completion on a queue */
        int (*f_command_wait)(queue_t * queue, command_t * cmd, queue_command_list_counter_t idx);

    public:

        /* allocate a new command to the queue (must then be commited via 'commit') */
        command_t * command_new(const command_type_t itype);

        /* complete the command at the i-th position in the pending queue (invoke callbacks) */
        void complete_command(const queue_command_list_counter_t p);

        /* complete the command that must be in the pending queue */
        void complete_command(command_t * cmd);

        /* commit the command to the queue (must be allocated via 'command_new') */
        int commit(command_t * command);

        /* launch commands, and may generate pending commands */
        int launch_ready_commands(void);

        /* progress pending commands */
        int progress_pending_commands(void);

        /* (internal) complete all commands to 'ok_p' */
        void complete_commands(const queue_command_list_counter_t ok_p);

        /* wait for completion of all pending commands */
        void wait_pending_commands(void);

        /* return true if the queue is full of commands, false otherwise */
        int is_full(void) const;

        /* return true if the queue is empty, false otherwise */
        int is_empty(void) const;


};  /* queue_t */

void queue_init(
    queue_t * queue,
    queue_type_t qtype,
    queue_command_list_counter_t capacity,
    int (*f_command_launch)(queue_t * queue, command_t * cmd, queue_command_list_counter_t idx),
    int (*f_commands_progress)(queue_t * queue),
    int (*f_commands_wait)(queue_t * queue),
    int (*f_command_wait)(queue_t * queue, command_t * cmd, queue_command_list_counter_t idx)
);

void queue_deinit(queue_t * queue);

XKRT_NAMESPACE_END

#endif /* __QUEUE_HPP__ */
