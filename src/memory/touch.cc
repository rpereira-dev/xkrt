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

# include <xkrt/runtime.h>

typedef struct  memory_touch_async_args_t
{
    xkrt_runtime_t * runtime;
    void * ptr;
    size_t chunk_size;
    int i;
}               memory_touch_async_args_t;

static void
body_memory_touch_async(task_t * task)
{
    assert(task);

    memory_touch_async_args_t * args = (memory_touch_async_args_t *) TASK_ARGS(task);
    size_t pagesize = (size_t) getpagesize();

    // volatile to trick the compiler and avoid optimization of *p = *p
    volatile unsigned char *   p = ((volatile unsigned char *) args->ptr) + (args->i+0) * args->chunk_size;
             unsigned char * end = (         (unsigned char *) args->ptr) + (args->i+1) * args->chunk_size;
    for ( ; p < end ; p += pagesize)
        *p = *p;
}

int
xkrt_runtime_t::memory_touch_async(
    void * ptr,
    const size_t chunk_size,
    int nchunks
) {
    xkrt_thread_t * thread = xkrt_thread_t::get_tls();
    for (int i = 0 ; i < nchunks ; ++i)
    {
        constexpr task_flag_bitfield_t flags = TASK_FLAG_ZERO;
        constexpr size_t task_size = task_compute_size(flags, 0);
        constexpr size_t args_size = sizeof(memory_touch_async_args_t);

        task_t * task = thread->allocate_task(task_size + args_size);
        new(task) task_t(this->formats.memory_touch_async, flags);

        memory_touch_async_args_t * args = (memory_touch_async_args_t *) TASK_ARGS(task, task_size);
        args->runtime    = this;
        args->ptr        = ptr;
        args->chunk_size = chunk_size;
        args->i          = i;

        thread->commit(task, xkrt_team_thread_task_enqueue, this, thread->team, thread);
    }
    return 0;
}

void
xkrt_memory_touch_async_register_format(xkrt_runtime_t * runtime)
{
    task_format_t format;
    memset(format.f, 0, sizeof(format.f));
    format.f[TASK_FORMAT_TARGET_HOST] = (task_format_func_t) body_memory_touch_async;
    snprintf(format.label, sizeof(format.label), "memory_touch_async");
    runtime->formats.memory_touch_async = task_format_create(&(runtime->formats.list), &format);
}
