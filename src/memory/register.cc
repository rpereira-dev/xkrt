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

# include <xkrt/xkrt.h>
# include <xkrt/runtime.h>

typedef struct  memory_op_async_args_t
{
    xkrt_runtime_t * runtime;
    uintptr_t start;
    uintptr_t end;

}               memory_op_async_args_t;

typedef enum    memory_op_type_t
{
    REGISTER,
    UNREGISTER,
    TOUCH
}               memory_op_type_t;

constexpr size_t args_size = sizeof(memory_op_async_args_t);
constexpr task_flag_bitfield_t flags = TASK_FLAG_DEPENDENT;

template<memory_op_type_t T>
static void
body_memory_async(task_t * task)
{
    assert(task);

    constexpr int AC = (T == TOUCH) ? 1 : 2;
    constexpr size_t task_size = task_compute_size(flags, AC);

    memory_op_async_args_t * args = (memory_op_async_args_t *) TASK_ARGS(task, task_size);
    assert(args->runtime);
    assert(args->start < args->end);

    if constexpr (T == REGISTER)
        xkrt_memory_register(args->runtime, (void *) args->start, (size_t) (args->end - args->start));
    else if constexpr (T == UNREGISTER)
        xkrt_memory_unregister(args->runtime, (void *) args->start, (size_t) (args->end - args->start));
    else if constexpr (T == TOUCH)
    {
        // volatile to trick the compiler and avoid optimization of *p = *p
        volatile unsigned char * a = (volatile unsigned char *) args->start;
           const unsigned char * b = (   const unsigned char *) args->end;
             const size_t pagesize = (size_t) getpagesize();

        for ( ; a < b ; a += pagesize)
            *a = 0;
    }
}

template<memory_op_type_t T>
static int
memory_op_async(
    xkrt_runtime_t * runtime,
    xkrt_team_t * team,
    void * ptr,
    size_t size,
    int n
) {
    assert(n > 0);

    constexpr int AC = (T == TOUCH) ? 1 : 2;
    constexpr size_t task_size = task_compute_size(flags, AC);

    xkrt_thread_t * tls = xkrt_thread_t::get_tls();
    assert(tls);

    const task_format_id_t fmtid = (T == REGISTER)   ? runtime->formats.memory_register_async   :
                                   (T == UNREGISTER) ? runtime->formats.memory_unregister_async :
                                   (T == TOUCH)      ? runtime->formats.memory_touch_async      :
                                   0;
    assert(fmtid);

    const size_t pagesize = (size_t) getpagesize();

    // compute number of pages to register
    const size_t npages = (size < pagesize) ? 1 : size / pagesize;

    // compute number of tasks
    const size_t ntasks = (npages < (size_t) n) ? npages : (size_t) n;

    // compute number of pages to register per task
    const size_t pages_per_task = (npages < ntasks) ? 1 : npages / ntasks;

    // round to closest pages
    const uintptr_t  p = (const uintptr_t) ptr;
    const uintptr_t pp = p + size;
    const uintptr_t  a = p - (p % pagesize);
    const uintptr_t  b = pp + (pagesize - (pp % pagesize)) % pagesize;
    assert(a < b);
    assert(a % pagesize == 0);
    assert(b % pagesize == 0);

    // spawn tasks
    for (size_t i = 0 ; i < ntasks ; ++i)
    {
        // create a task that will register/pin/unpin the memory
        task_t * task = tls->allocate_task(task_size + args_size);
        new(task) task_t(fmtid, flags);

        task_dep_info_t * dep = TASK_DEP_INFO(task);
        new (dep) task_dep_info_t(AC);

        // setup register args
        memory_op_async_args_t * args = (memory_op_async_args_t *) TASK_ARGS(task);
        args->runtime = runtime;

        // ensure the same page is not registered twice by consecutive tasks
        args->start = a + (i+0) * pagesize * pages_per_task;
        args->end   = a + (i+1) * pagesize * pages_per_task;

        // clamp upper
        if (args->end > p + size)
            args->end = p + size;

        assert(a <= args->start);
        assert(     args->start < b);
        assert(a <= args->end);
        assert(     args->end <= b);
        assert(args->start < args->end);

        // virtual write onto the memory segment
        access_t * accesses = TASK_ACCESSES(task, flags);
        constexpr access_mode_t mode = (access_mode_t) (ACCESS_MODE_W | ACCESS_MODE_V);
        new(accesses + 0) access_t(task, args->start, args->end, mode);

        // if register/unregister, create a virtual write on NULL, to
        // serialize, and avoid blocking thread in cuda driver
        if constexpr(T == REGISTER || T == UNREGISTER)
            new(accesses + 1) access_t(task, (const void*) NULL, mode, ACCESS_CONCURRENCY_COMMUTATIVE);

        # ifndef NDEBUG
        snprintf(task->label, sizeof(task->label),
                T == REGISTER   ? "register"   :
                T == UNREGISTER ? "unregister" :
                T == TOUCH      ? "touch"      :
                "(null)");
        # endif /* NDEBUG */

        // resolve dependencies (point-based)
        tls->resolve<AC>(task, accesses);

        // commit task
        tls->commit(task, xkrt_team_task_enqueue, runtime, team);
    }

    return 0;
}

int
xkrt_runtime_t::memory_unregister_async(
    xkrt_team_t * team,
    void * ptr,
    const size_t size,
    int n
) {
    return memory_op_async<UNREGISTER>(this, team, ptr, size, n);
}

int
xkrt_runtime_t::memory_register_async(
    xkrt_team_t * team,
    void * ptr,
    const size_t size,
    int n
) {
    return memory_op_async<REGISTER>(this, team, ptr, size, n);
}

int
xkrt_runtime_t::memory_touch_async(
    xkrt_team_t * team,
    void * ptr,
    const size_t size,
    int n
) {
    return memory_op_async<TOUCH>(this, team, ptr, size, n);
}

void
xkrt_memory_async_register_format(xkrt_runtime_t * runtime)
{
    {
        task_format_t format;
        memset(format.f, 0, sizeof(format.f));
        format.f[TASK_FORMAT_TARGET_HOST] = (task_format_func_t) body_memory_async<REGISTER>;
        snprintf(format.label, sizeof(format.label), "memory_register_async");
        runtime->formats.memory_register_async = task_format_create(&(runtime->formats.list), &format);
    }

    {
        task_format_t format;
        memset(format.f, 0, sizeof(format.f));
        format.f[TASK_FORMAT_TARGET_HOST] = (task_format_func_t) body_memory_async<UNREGISTER>;
        snprintf(format.label, sizeof(format.label), "memory_unregister_async");
        runtime->formats.memory_unregister_async = task_format_create(&(runtime->formats.list), &format);
    }

    {
        task_format_t format;
        memset(format.f, 0, sizeof(format.f));
        format.f[TASK_FORMAT_TARGET_HOST] = (task_format_func_t) body_memory_async<TOUCH>;
        snprintf(format.label, sizeof(format.label), "memory_touch_async");
        runtime->formats.memory_touch_async = task_format_create(&(runtime->formats.list), &format);
    }
}
