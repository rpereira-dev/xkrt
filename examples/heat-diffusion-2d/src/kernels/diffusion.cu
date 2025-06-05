/*
** Copyright 2024 INRIA
**
** Contributors :
**
** Romain PEREIRA, romain.pereira@inria.fr
** Romain PEREIRA, rpereira@anl.gov
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

// from GPT4.o

# if 0
// Optimizing a CUDA stencil kernel involves several strategies, including improving memory access patterns, using shared memory, and optimizing thread and block configurations. Here's a revised version of your kernel with some optimizations:

__global__ void diffusion_cuda_kernel(TYPE *src, int ld_src, TYPE *dst, int ld_dst, int tile_x, int tile_y, int tsx, int tsy) {
    // Calculate global indices
    const int li = threadIdx.x;
    const int lj = threadIdx.y;
    const int i = tile_x * tsx + blockIdx.x * blockDim.x + li;
    const int j = tile_y * tsy + blockIdx.y * blockDim.y + lj;

    // Shared memory for the current tile
    extern __shared__ TYPE shared_mem[];

    // Calculate shared memory indices
    const int shared_i = li + 1;
    const int shared_j = lj + 1;

    // Load data into shared memory
    if (i < NX && j < NY) {
        shared_mem[shared_j * (blockDim.x + 2) + shared_i] = GRID(src, i, j, ld_src);

        // Load halo elements
        if (li == 0 && i > 0) {
            shared_mem[shared_j * (blockDim.x + 2)] = GRID(src, i - 1, j, ld_src);
        }
        if (li == blockDim.x - 1 && i < NX - 1) {
            shared_mem[shared_j * (blockDim.x + 2) + shared_i + 1] = GRID(src, i + 1, j, ld_src);
        }
        if (lj == 0 && j > 0) {
            shared_mem[shared_i] = GRID(src, i, j - 1, ld_src);
        }
        if (lj == blockDim.y - 1 && j < NY - 1) {
            shared_mem[(shared_j + 1) * (blockDim.x + 2) + shared_i] = GRID(src, i, j + 1, ld_src);
        }
    }

    __syncthreads();

    // Perform computation if within bounds
    if (i > 0 && i < NX - 1 && j > 0 && j < NY - 1) {
        GRID(dst, i, j, ld_dst) = shared_mem[shared_j * (blockDim.x + 2) + shared_i] + ALPHA * DT / (DX * DY) * (
            (shared_mem[shared_j * (blockDim.x + 2) + shared_i + 1] - 2 * shared_mem[shared_j * (blockDim.x + 2) + shared_i] + shared_mem[shared_j * (blockDim.x + 2) + shared_i - 1]) / (DX * DX) +
            (shared_mem[(shared_j + 1) * (blockDim.x + 2) + shared_i] - 2 * shared_mem[shared_j * (blockDim.x + 2) + shared_i] + shared_mem[(shared_j - 1) * (blockDim.x + 2) + shared_i]) / (DY * DY)
        );
    }
}
Key Optimizations:
Shared Memory Usage: The kernel now uses shared memory to store the tile being processed. This reduces global memory accesses, which are slower compared to shared memory accesses.

Halo Region: The kernel loads a halo region into shared memory to handle boundary conditions within the tile. This ensures that all necessary data for computation is available in shared memory.

Thread Synchronization: A __syncthreads() call is used to ensure all threads have loaded their data into shared memory before any computation begins.

Boundary Checks: The kernel checks if the current thread is within the valid computational domain before performing the stencil operation.

Thread and Block Configuration: Ensure that the block size is chosen to maximize occupancy while considering shared memory usage. A typical block size might be 16x16 or 32x32, but this should be tuned based on your specific hardware and problem size.

Additional Considerations:
Memory Coalescing: Ensure that global memory accesses are coalesced. This is generally achieved by having threads access contiguous memory locations.

Occupancy: Use CUDA occupancy calculator tools to determine the optimal block size and shared memory usage for your specific GPU.

# endif

# include <heat/consts.h>

/* A naive kernel to update the grid */
__global__
void
diffusion_cuda_kernel(TYPE * src, int ld_src, TYPE * dst, int ld_dst, int tile_x, int tile_y, int tsx, int tsy)
{
    const int li = blockIdx.x * blockDim.x + threadIdx.x;
    const int lj = blockIdx.y * blockDim.y + threadIdx.y;
    const int  i = tile_x * tsx + li;
    const int  j = tile_y * tsy + lj;

    // boundary conditions fixed
    if (i > 0 && i < NX - 1 && j > 0 && j < NY - 1)
    {
        GRID(dst, li, lj, ld_dst) = GRID(src, li, lj, ld_src) + ALPHA * DT / (DX * DY) * (
                (GRID(src, li+1,   lj, ld_src) - 2 * GRID(src, li, lj, ld_src) + GRID(src, li-1,   lj, ld_src)) / (DX * DX) +
                (GRID(src,   li, lj+1, ld_src) - 2 * GRID(src, li, lj, ld_src) + GRID(src,   li, lj-1, ld_src)) / (DY * DY)
            );
    }
}

extern "C"
void
diffusion_cuda(
    cudaStream_t stream,
    TYPE * src, int ld_src,
    TYPE * dst, int ld_dst,
    int tile_x, int tile_y,
    int tsx, int tsy
) {
    // Number of threads per block line
    const unsigned int dtsx = (32 < (tsx) ? 32 : (tsx));
    const unsigned int dtsy = (32 < (tsy) ? 32 : (tsy));

    // how many threads we need in total
    dim3 T = {(unsigned int) tsx, (unsigned int) tsy, 1};

    // block dim
    dim3 B(dtsx, dtsy, 1);

    // grid
    dim3 G((T.x + B.x - 1) / B.x,  (T.y + B.y - 1) / B.y, 1);

    // kernel launch
    diffusion_cuda_kernel<<<G, B, 0, stream>>>(src, ld_src, dst, ld_dst, tile_x, tile_y, tsx, tsy);
}
