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

#ifndef __XKRT_DEVICE_HPP__
# define __XKRT_DEVICE_HPP__

# include <stdint.h>    /* uint64_t */

# include <xkrt/support.h>
# include <xkrt/conf/conf.h>
# include <xkrt/driver/driver-type.h>
# include <xkrt/logger/todo.h>
# include <xkrt/memory/area.h>
# include <xkrt/memory/cache-line-size.hpp>
# include <xkrt/stats/stats.h>
# include <xkrt/sync/mutex.h>
# include <xkrt/task/task.hpp>

typedef enum    xkrt_device_state_t : uint8_t
{
    XKRT_DEVICE_STATE_DEALLOCATED = 0,
    XKRT_DEVICE_STATE_CREATE      = 1,
    XKRT_DEVICE_STATE_INIT        = 2,
    XKRT_DEVICE_STATE_COMMIT      = 3,
    XKRT_DEVICE_STATE_STOP        = 4,
    XKRT_DEVICE_STATE_STOPPED     = 5,
    XKRT_DEVICE_STATE_DESTROYED   = 6

}               xkrt_device_state_t;

/* Memory info of a device */
typedef struct  xkrt_device_memory_info_t
{
    ///////////////////////////////////////
    //  TO BE FILL BY THE DRIVER ON INIT //
    ///////////////////////////////////////

    /* memory capacity */
    size_t capacity;

    /* memory used */
    size_t used;

    /* memory name */
    char name[32];

    ////////////////////////////////
    //  TO BE FILL BY THE RUNTIME //
    ////////////////////////////////

    /* whether this area was already allocated+mapped to the device */
    int allocated;

    /* the area of that memory */
    xkrt_area_t area;

}               xkrt_device_memory_info_t;

/* A device virtualize a ressource with its one address space and
   a communication stream between host and the ressource */
typedef struct  xkrt_device_t
{
    /////////////////
    //  ATTRIBUTES //
    /////////////////

    /* the conf */
    xkrt_conf_device_t * conf;

    /* the driver type in [0..XKRT_DRIVER_TYPE_MAX[ */
    xkrt_driver_type_t driver_type;

    /* driver device id in [0..ndevices_for_device] */
    uint8_t driver_id;

    /* global device id in [0, XKRT_DEVICES_MAX[ - host is a virtual device of id 'XKRT_DEVICES_MAX' */
    xkrt_device_global_id_t global_id;

    /* the device state */
    std::atomic<xkrt_device_state_t> state;

    /* affinity[i] - j-th bit is set to '1' if this device has an affinity 'i'
     * with 'j' (the lowest affinity, the better perf) */
    xkrt_device_global_id_bitfield_t * affinity;

    ///////////
    // STATS //
    ///////////

    # if XKRT_SUPPORT_STATS
    struct {
        struct {
            stats_int_t freed;
            struct {
                stats_int_t total;
                stats_int_t currently;
            } allocated;
        } memory;
    } stats;
    # endif /* XKRT_SUPPORT_STATS */

    //////////////////////
    // MEMORY MANAGMENT //
    //////////////////////

    /* memory areas of that device - sorted by performance */
    xkrt_device_memory_info_t memories[XKRT_DEVICE_MEMORIES_MAX];
    int nmemories;

    /* allocate memory on a specific area */
    xkrt_area_chunk_t * memory_allocate_on(const size_t size, int area_idx);

    /* allocate memory */
    xkrt_area_chunk_t * memory_allocate(const size_t size);

    /* deallocate the given chunk */
    void memory_deallocate_on(xkrt_area_chunk_t * chunk, int area_idx);

    /* deallocate the given chunk */
    void memory_deallocate(xkrt_area_chunk_t * chunk);

    /* free all memory of every area of that device, resetting their state to chunk0 */
    void memory_reset(void);

    /* free all memory of the given area of that device, resetting their state to chunk0 */
    void memory_reset_on(int area_idx);

    /* set chunk0 of an area */
    void memory_set_chunk0(uintptr_t device_ptr, size_t size, int area_idx);

    ///////////////////////
    // STREAM MANAGEMENT //
    ///////////////////////

    /* total number of stream (sum of count[:]) */
    int nstreams_per_thread;

    /* number of stream per type */
    int count[XKRT_STREAM_TYPE_ALL];

    /* next thread to use for offloading an instruction */
    std::atomic<uint8_t> next_thread;

    /* next stream to use for the given thread and type */
    std::atomic<int> next_stream[XKRT_MAX_THREADS_PER_DEVICE][XKRT_STREAM_TYPE_ALL];

    /* basic stream */
    xkrt_stream_t ** streams[XKRT_MAX_THREADS_PER_DEVICE][XKRT_STREAM_TYPE_ALL];

    /* initialize the offloader (must be called once before any thread called the 'thread' version) */
    void offloader_init(
        int (*f_stream_suggest)(int device_driver_id, xkrt_stream_type_t type)
    );

    /* initialize a thread of the offloader */
    void offloader_init_thread(
        uint8_t device_tid,
        xkrt_stream_t * (*f_stream_create)(xkrt_device_t * device, xkrt_stream_type_t type, xkrt_stream_instruction_counter_t capacity)
    );

    /* poll the device for launching and progressing pending instructions in every streams */
    int offloader_poll(uint8_t device_tid);

    /* return true if the the streams for the given type are all empty */
    bool offloader_streams_are_empty(uint8_t device_id, const xkrt_stream_type_t stype) const;

    /* get next stream to use for submitting an instruction for the given type */
    void offloader_stream_next(
        const xkrt_stream_type_t type,
        xkrt_thread_t ** pthread,       /* OUT */
        xkrt_stream_t ** pstream        /* OUT */
    );

    /* launch ready instructions dispatching them in streams of the given type */
    int offloader_stream_instructions_launch(uint8_t device_id, const xkrt_stream_type_t stype);

    /* progress pending instructions in streams of the given type of the given thread.
     * If blocking is true, also waits for the completion of pending instructions */
    template <bool blocking>
    int
    offloader_stream_instructions_progress(
        uint8_t device_tid,
        const xkrt_stream_type_t stype
    ) {
        int err = 0;
        unsigned int bgn = (stype == XKRT_STREAM_TYPE_ALL) ?                    0 : stype;
        unsigned int end = (stype == XKRT_STREAM_TYPE_ALL) ? XKRT_STREAM_TYPE_ALL : stype + 1;
        for (unsigned int s = bgn ; s < end ; ++s)
        {
            for (int i = 0 ; i < this->count[s] ; ++i)
            {
                xkrt_stream_t * stream = this->streams[device_tid][s][i];
                assert(stream);

                if (stream->pending.is_empty())
                    continue ;

                xkrt_stream_instruction_counter_t n;
                do {
                    stream->lock();
                    if (blocking)
                    {
                        stream->wait_pending_instructions();
                        err = 0;
                    }
                    else
                        err = stream->progress_pending_instructions();
                    stream->unlock();
                    n = stream->pending.size();
                } while (n > this->conf->offloader.streams[s].concurrency);
                assert(err == 0 || err == EINPROGRESS);
            }
        }
        return 0;
    }

    /* create a new instruction and lock the stream */
    void offloader_stream_instruction_new(
        const xkrt_stream_type_t stype,             /* IN  */
              xkrt_thread_t ** pthread,             /* OUT */
              xkrt_stream_t ** pstream,             /* OUT */
        const xkrt_stream_instruction_type_t itype, /* IN  */
              xkrt_stream_instruction_t ** pinstr,  /* OUT */
        const xkrt_callback_t & callback            /* IN */
    );

    /* commit an instruction previously returned with
     * "offloader_stream_instruction_new" and unlock the stream */
    void offloader_stream_instruction_commit(
        xkrt_thread_t * thread,
        xkrt_stream_t * stream,
        xkrt_stream_instruction_t * instr
    );

    /* submit a kernel execution instruction */
    void offloader_stream_instruction_submit_kernel(
        void (*launch)(void * istream, void * instr, xkrt_stream_instruction_counter_t idx),
        void * vargs,
        const xkrt_callback_t & callback
    );

    /* copy */
    template <typename HOST_VIEW_T, typename DEVICE_VIEW_T>
    void offloader_stream_instruction_submit_copy(
        const HOST_VIEW_T             & host_view,
        const xkrt_device_global_id_t   dst_device_global_id,
        const DEVICE_VIEW_T           & dst_device_view,
        const xkrt_device_global_id_t   src_device_global_id,
        const DEVICE_VIEW_T           & src_device_view,
        const xkrt_callback_t         & callback
    ) {
        assert(this->global_id == dst_device_global_id || this->global_id == src_device_global_id);

        /* find the instruction type */
        xkrt_stream_instruction_type_t itype;
        const int src_is_host = (src_device_global_id == HOST_DEVICE_GLOBAL_ID) ? 1 : 0;
        const int dst_is_host = (dst_device_global_id == HOST_DEVICE_GLOBAL_ID) ? 1 : 0;

        /* assertions */
        # define IS_1D (std::is_same<HOST_VIEW_T, size_t>()        && std::is_same<DEVICE_VIEW_T, uintptr_t>())
        # define IS_2D (std::is_same<HOST_VIEW_T, memory_view_t>() && std::is_same<DEVICE_VIEW_T, memory_replicate_view_t>())
        static_assert(IS_1D || IS_2D);
        if constexpr(IS_1D) {
            assert(host_view);
            assert(dst_device_view);
            assert(src_device_view);
            itype = ( src_is_host &&  dst_is_host) ? XKRT_STREAM_INSTR_TYPE_COPY_H2H_1D :
                    ( src_is_host && !dst_is_host) ? XKRT_STREAM_INSTR_TYPE_COPY_H2D_1D :
                    (!src_is_host &&  dst_is_host) ? XKRT_STREAM_INSTR_TYPE_COPY_D2H_1D :
                    (!src_is_host && !dst_is_host) ? XKRT_STREAM_INSTR_TYPE_COPY_D2D_1D :
                    XKRT_STREAM_INSTR_TYPE_MAX;
        } else if constexpr(IS_2D) {
            assert(host_view.m);
            assert(host_view.n);
            assert(host_view.sizeof_type);

            assert(dst_device_view.addr);
            assert(dst_device_view.ld);

            assert(src_device_view.addr);
            assert(src_device_view.ld);

            itype = ( src_is_host &&  dst_is_host) ? XKRT_STREAM_INSTR_TYPE_COPY_H2H_2D :
                    ( src_is_host && !dst_is_host) ? XKRT_STREAM_INSTR_TYPE_COPY_H2D_2D :
                    (!src_is_host &&  dst_is_host) ? XKRT_STREAM_INSTR_TYPE_COPY_D2H_2D :
                    (!src_is_host && !dst_is_host) ? XKRT_STREAM_INSTR_TYPE_COPY_D2D_2D :
                    XKRT_STREAM_INSTR_TYPE_MAX;
        } else {
            LOGGER_FATAL("Wrong parameters");
        }

        /* find the type of stream to use */
        xkrt_stream_type_t stype;
        switch(itype)
        {
            # pragma message(TODO "No H2H streams, do we want one ? Currently using H2D stream for H2H copies")
            case (XKRT_STREAM_INSTR_TYPE_COPY_H2H_1D):
            case (XKRT_STREAM_INSTR_TYPE_COPY_H2D_1D):
            case (XKRT_STREAM_INSTR_TYPE_COPY_H2H_2D):
            case (XKRT_STREAM_INSTR_TYPE_COPY_H2D_2D):
            {
               stype = XKRT_STREAM_TYPE_H2D;
               break ;
            }

            case (XKRT_STREAM_INSTR_TYPE_COPY_D2H_1D):
            case (XKRT_STREAM_INSTR_TYPE_COPY_D2H_2D):
            {
                stype = XKRT_STREAM_TYPE_D2H;
                break ;
            }

            case (XKRT_STREAM_INSTR_TYPE_COPY_D2D_1D):
            case (XKRT_STREAM_INSTR_TYPE_COPY_D2D_2D):
            {
                stype = XKRT_STREAM_TYPE_D2D;
                break ;
            }

            default:
            {
                LOGGER_FATAL("Impossible occured");
                break ;
            }
        }

        /* create a new instruction and retrieve its offload stream */
        xkrt_thread_t * thread;
        xkrt_stream_t * stream;
        xkrt_stream_instruction_t * instr;
        this->offloader_stream_instruction_new(stype, &thread, &stream, itype, &instr, callback);
        assert(thread);
        assert(stream);
        assert(instr);

        /* create a new copy instruction */
        if constexpr (IS_1D) {
            instr->copy_1D.size = host_view;
            instr->copy_1D.dst_device_addr  = dst_device_view;
            instr->copy_1D.src_device_addr  = src_device_view;
            XKRT_STATS_INCR(stream->stats.transfered, instr->copy_1D.size);
        } else if constexpr (IS_2D) {
            instr->copy_2D.m                = host_view.m;
            instr->copy_2D.n                = host_view.n;
            instr->copy_2D.sizeof_type      = host_view.sizeof_type;
            instr->copy_2D.dst_device_view  = dst_device_view;
            instr->copy_2D.src_device_view  = src_device_view;
            XKRT_STATS_INCR(stream->stats.transfered, host_view.m * host_view.n * host_view.sizeof_type);
        }

        this->offloader_stream_instruction_commit(thread, stream, instr);

        # undef IS_1D
        # undef IS_2D
    }

    //////////////////////
    // TASKS SUBMISSION //
    //////////////////////

    /* worker threads for that device */
    xkrt_thread_t * threads[XKRT_MAX_THREADS_PER_DEVICE];

    /* total number of threads */
    std::atomic<uint8_t> nthreads;

    /* the next thread to receive a task */
    std::atomic<uint8_t> thread_next;

    /* push a task to a thread of the device */
    void push(task_t * const & task)
    {
        assert(this->nthreads <= XKRT_MAX_THREADS_PER_DEVICE);
        uint8_t tid = this->thread_next.fetch_add(1, std::memory_order_relaxed) % this->nthreads;
        xkrt_thread_t * thread = this->threads[tid];
        thread->deque.push(task);
        thread->wakeup();
    }

}               xkrt_device_t;

#endif /* __XKRT_DEVICE_HPP__ */
