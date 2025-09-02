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

XKRT_NAMESPACE_BEGIN

typedef enum    device_state_t : uint8_t
{
    XKRT_DEVICE_STATE_DEALLOCATED = 0,
    XKRT_DEVICE_STATE_CREATE      = 1,
    XKRT_DEVICE_STATE_INIT        = 2,
    XKRT_DEVICE_STATE_COMMIT      = 3,
    XKRT_DEVICE_STATE_STOP        = 4,
    XKRT_DEVICE_STATE_STOPPED     = 5,
    XKRT_DEVICE_STATE_DESTROYED   = 6

}               device_state_t;

/* Memory info of a device */
typedef struct  device_memory_info_t
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
    area_t area;

}               device_memory_info_t;

/* A device virtualize a ressource with its one address space and
   a communication stream between host and the ressource */
typedef struct  device_t
{
    /////////////////
    //  ATTRIBUTES //
    /////////////////

    /* the conf */
    conf_device_t * conf;

    /* the driver type in [0..XKRT_DRIVER_TYPE_MAX[ */
    driver_type_t driver_type;

    /* driver device id in [0..ndevices_for_device] */
    uint8_t driver_id;

    /* global device id in [0, XKRT_DEVICES_MAX[ - host is a virtual device of id 'XKRT_DEVICES_MAX' */
    device_global_id_t global_id;

    /* the device state */
    std::atomic<device_state_t> state;

    /* affinity[i] - j-th bit is set to '1' if this device has an affinity 'i'
     * with 'j' (the lowest affinity, the better perf) */
    device_global_id_bitfield_t * affinity;

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
    device_memory_info_t memories[XKRT_DEVICE_MEMORIES_MAX];
    int nmemories;

    /* allocate memory on a specific area */
    area_chunk_t * memory_allocate_on(const size_t size, int area_idx);

    /* allocate memory */
    area_chunk_t * memory_allocate(const size_t size);

    /* deallocate the given chunk */
    void memory_deallocate_on(area_chunk_t * chunk, int area_idx);

    /* deallocate the given chunk */
    void memory_deallocate(area_chunk_t * chunk);

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
    int count[STREAM_TYPE_ALL];

    /* next thread to use for offloading an instruction */
    std::atomic<uint8_t> next_thread;

    /* next stream to use for the given thread and type */
    std::atomic<int> next_stream[XKRT_MAX_THREADS_PER_DEVICE][STREAM_TYPE_ALL];

    /* basic stream */
    stream_t ** streams[XKRT_MAX_THREADS_PER_DEVICE][STREAM_TYPE_ALL];

    /* initialize the offloader (must be called once before any thread called the 'thread' version) */
    void offloader_init(
        int (*f_stream_suggest)(int device_driver_id, stream_type_t type)
    );

    /* initialize a thread of the offloader */
    void offloader_init_thread(
        uint8_t device_tid,
        stream_t * (*f_stream_create)(device_t * device, stream_type_t type, stream_instruction_counter_t capacity)
    );

    /* launch ready instructions in every streams */
    int offloader_launch(uint8_t device_tid);

    /* progress pending instructions in every streams */
    int offloader_progress(uint8_t device_tid);

    /* progress pending instructions in every streams */
    int offloader_wait_random_instruction(uint8_t device_tid);

    /* set 'ready' and 'pending' to false whether there is ready/pending
     * instructions in the streams of the given type */
    void offloader_streams_are_empty(uint8_t device_id, const stream_type_t stype, bool * ready, bool * pending) const;

    /* get next stream to use for submitting an instruction for the given type */
    void offloader_stream_next(
        const stream_type_t type,
        thread_t ** pthread,       /* OUT */
        stream_t ** pstream        /* OUT */
    );

    /* launch ready instructions dispatching them in streams of the given type */
    int offloader_stream_instructions_launch(uint8_t device_id, const stream_type_t stype);

    /* progress pending instructions in streams of the given type of the given thread.
     * If blocking is true, also waits for the completion of pending instructions */
    template <bool blocking>
    int
    offloader_stream_instructions_progress(
        uint8_t device_tid,
        const stream_type_t stype
    ) {
        int err = 0;
        unsigned int bgn = (stype == STREAM_TYPE_ALL) ?               0 : stype;
        unsigned int end = (stype == STREAM_TYPE_ALL) ? STREAM_TYPE_ALL : stype + 1;
        for (unsigned int s = bgn ; s < end ; ++s)
        {
            for (int i = 0 ; i < this->count[s] ; ++i)
            {
                stream_t * stream = this->streams[device_tid][s][i];
                assert(stream);

                if (stream->pending.is_empty())
                    continue ;

                stream_instruction_counter_t n;
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
        const stream_type_t stype,             /* IN  */
              thread_t ** pthread,             /* OUT */
              stream_t ** pstream,             /* OUT */
        const stream_instruction_type_t itype, /* IN  */
              stream_instruction_t ** pinstr,  /* OUT */
        const callback_t & callback            /* IN */
    );

    /* commit an instruction previously returned with
     * "offloader_stream_instruction_new" and unlock the stream */
    void offloader_stream_instruction_commit(
        thread_t * thread,
        stream_t * stream,
        stream_instruction_t * instr
    );

    /* submit a file I/O instruction */
    template <stream_instruction_type_t T>
    void offloader_stream_instruction_submit_file(
        int fd,
        void * buffer,
        size_t n,
        size_t offset,
        const callback_t & callback
    ) {
        static_assert(
            T == XKRT_STREAM_INSTR_TYPE_FD_READ ||
            T == XKRT_STREAM_INSTR_TYPE_FD_WRITE
        );

        /* create a new instruction and retrieve its offload stream */
        constexpr stream_type_t stype = (T == XKRT_STREAM_INSTR_TYPE_FD_READ) ? STREAM_TYPE_FD_READ : STREAM_TYPE_FD_WRITE;
        constexpr stream_instruction_type_t itype = T;

        thread_t * thread;
        stream_t * stream;
        stream_instruction_t * instr;
        this->offloader_stream_instruction_new(stype, &thread, &stream, itype, &instr, callback);
        assert(thread);
        assert(stream);
        assert(instr);

        /* create a new file i/o instruction */
        instr->file.fd = fd;
        instr->file.buffer = buffer;
        instr->file.n = n;
        instr->file.offset = offset;

        /* submit instr */
        this->offloader_stream_instruction_commit(thread, stream, instr);

    }

    /* submit a kernel execution instruction */
    void offloader_stream_instruction_submit_kernel(
        kernel_launcher_t launcher,
        void * vargs,
        const callback_t & callback
    );

    /* copy */
    template <typename HOST_VIEW_T, typename DEVICE_VIEW_T>
    stream_t *
    offloader_stream_instruction_submit_copy(
        const HOST_VIEW_T             & host_view,
        const device_global_id_t   dst_device_global_id,
        const DEVICE_VIEW_T           & dst_device_view,
        const device_global_id_t   src_device_global_id,
        const DEVICE_VIEW_T           & src_device_view,
        const callback_t         & callback
    ) {
        assert(this->global_id == dst_device_global_id || this->global_id == src_device_global_id);

        /* find the instruction type */
        stream_instruction_type_t itype;
        const int src_is_host = (src_device_global_id == HOST_DEVICE_GLOBAL_ID) ? 1 : 0;
        const int dst_is_host = (dst_device_global_id == HOST_DEVICE_GLOBAL_ID) ? 1 : 0;

        /* assertions */
        # define IS_1D (std::is_same<HOST_VIEW_T, size_t>()        && std::is_same<DEVICE_VIEW_T, uintptr_t>())
        # define IS_2D (std::is_same<HOST_VIEW_T, memory_view_t>() && std::is_same<DEVICE_VIEW_T, memory_replica_view_t>())
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
        stream_type_t stype;
        switch(itype)
        {
            case (XKRT_STREAM_INSTR_TYPE_COPY_H2H_1D):
            case (XKRT_STREAM_INSTR_TYPE_COPY_H2D_1D):
            case (XKRT_STREAM_INSTR_TYPE_COPY_H2H_2D):
            case (XKRT_STREAM_INSTR_TYPE_COPY_H2D_2D):
            {
               stype = STREAM_TYPE_H2D;
               break ;
            }

            case (XKRT_STREAM_INSTR_TYPE_COPY_D2H_1D):
            case (XKRT_STREAM_INSTR_TYPE_COPY_D2H_2D):
            {
                stype = STREAM_TYPE_D2H;
                break ;
            }

            case (XKRT_STREAM_INSTR_TYPE_COPY_D2D_1D):
            case (XKRT_STREAM_INSTR_TYPE_COPY_D2D_2D):
            {
                stype = STREAM_TYPE_D2D;
                break ;
            }

            default:
            {
                LOGGER_FATAL("Impossible occured");
                break ;
            }
        }

        /* create a new instruction and retrieve its offload stream */
        thread_t * thread;
        stream_t * stream;
        stream_instruction_t * instr;
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

        return stream;
    }

    //////////////////////
    // TASKS SUBMISSION //
    //////////////////////

    /* worker threads for that device */
    thread_t * threads[XKRT_MAX_THREADS_PER_DEVICE];

    /* total number of threads */
    std::atomic<uint8_t> nthreads;

    /* the next thread to receive a task */
    std::atomic<uint8_t> thread_next;

}               device_t;

XKRT_NAMESPACE_END

#endif /* __XKRT_DEVICE_HPP__ */
