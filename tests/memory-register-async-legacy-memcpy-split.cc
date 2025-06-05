/* ************************************************************************** */
/*                                                                            */
/*   register-async.cc                                                        */
/*                                                                   .-*-.    */
/*   Author: Romain PEREIRA <rpereira@anl.gov>                     .'* *.'    */
/*                                                              __/_*_*(_     */
/*   Created: 2025/03/03 01:28:08 by Romain PEREIRA            / _______ \    */
/*   Updated: 2025/05/23 18:09:41 by Romain PEREIRA            \_)     (_/    */
/*                                                                            */
/*   License: ???                                                             */
/*                                                                            */
/* ************************************************************************** */

# include <xkrt/xkrt.h>
# include <xkrt/logger/logger.h>
# include <xkrt/logger/metric.h>

#include <algorithm>    // std::shuffle

static xkrt_runtime_t runtime;

# define TYPE unsigned char
# define S    (sizeof(TYPE))
# define N    (512)

# define REGISTER_OFFSET (0)
# define REGISTER_SIZE   (5*pagesize + 123)

int
main(void)
{
    assert(xkrt_init(&runtime) == 0);

    // retrieve page size
    const size_t pagesize = getpagesize();

    // allocate N x N bytes = a matrix of LD = N - forcing alignement on LD.s
    const size_t size = N*N*S;
    assert(REGISTER_OFFSET+REGISTER_SIZE < size);

    # if 1
    const uintptr_t alignon = N * S;
    const uintptr_t memsize = (alignon + alignon/2 + size);
    const uintptr_t mem = (const uintptr_t) malloc(memsize);
    # if 0 /* force memory alignment on ld.s */
    const uintptr_t p = mem + (alignon - (mem % alignon));
    assert(p % alignon == 0);
    # else /* force not to be aligned */
    const uintptr_t p = mem + (alignon - (mem % alignon)) + alignon / 2;
    assert(p % alignon != 0);
    # endif
    # else
    const uintptr_t p = (const uintptr_t) malloc(size);
    # endif

    // Randomly touch pages outside the registered range
    std::vector<size_t> unregistered_pages;
    for (size_t offset = 0; offset < size; offset += pagesize)
        if (offset < REGISTER_OFFSET-pagesize || offset >= REGISTER_OFFSET+REGISTER_SIZE-pagesize)
            unregistered_pages.push_back(offset);

    // Randomly shuffle the access pattern
    std::random_device rd;
    std::mt19937 g(rd());
    std::shuffle(unregistered_pages.begin(), unregistered_pages.end(), g);

    for (size_t offset : unregistered_pages)
        ((unsigned char *)p)[offset] = 42; // Touch to make it dirty / paged-in

    // register a portion of it
    xkrt_memory_register_async(
        &runtime,
        (void *) (p+REGISTER_OFFSET),
        REGISTER_SIZE
    );
    runtime.task_wait();

    // submit data to devices
    xkrt_coherency_replicate_2D_async(&runtime, MATRIX_COLMAJOR, (void *) p, N, N, N, S);
    runtime.task_wait();

    // unregister a portion of it
    xkrt_memory_unregister_async(
        &runtime,
        (void *) (p+REGISTER_OFFSET),
        REGISTER_SIZE
    );
    runtime.task_wait();

    // finalize
    assert(xkrt_deinit(&runtime) == 0);

    return 0;
}
