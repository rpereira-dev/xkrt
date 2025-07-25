# define __HIP_PLATFORM_AMD__
#include <hip/hip_runtime.h>

# include <heat/consts.h>

/* A naive kernel to update the grid */
__global__
void
diffusion_hip_kernel(TYPE * src, int ld_src, TYPE * dst, int ld_dst, int tile_x, int tile_y, int tsx, int tsy)
{
    const int li = blockIdx.x * blockDim.x + threadIdx.x;
    const int lj = blockIdx.y * blockDim.y + threadIdx.y;
    const int  i = tile_x * tsx + li;
    const int  j = tile_y * tsy + lj;

    // boundary conditions fixed
    if (i > 0 && i < NX - 1 && j > 0 && j < NY - 1)
    {
        #if 1
        GRID(dst, li, lj, ld_dst) = GRID(src, li, lj, ld_src) + ALPHA * DT / (DX * DY) * (
                (GRID(src, li+1,   lj, ld_src) - 2 * GRID(src, li, lj, ld_src) + GRID(src, li-1,   lj, ld_src)) / (DX * DX) +
                (GRID(src,   li, lj+1, ld_src) - 2 * GRID(src, li, lj, ld_src) + GRID(src,   li, lj-1, ld_src)) / (DY * DY)
            );
        #else
        GRID(dst, li, lj, ld_dst) = (li * lj) % 100;
        #endif
    }
}

extern "C"
void
diffusion_hip(
    hipStream_t stream,
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
    hipLaunchKernelGGL(
        diffusion_hip_kernel,
        G, B, 0, stream,
        src, ld_src, dst, ld_dst, tile_x, tile_y, tsx, tsy
    );
}

