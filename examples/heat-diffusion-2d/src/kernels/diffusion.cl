/* ************************************************************************** */
/*                                                                            */
/*   diffusion.cl                                                 .-*-.       */
/*                                                              .'* *.'       */
/*   Created: 2025/02/21 01:19:50 by Romain PEREIRA          __/_*_*(_        */
/*   Updated: 2025/06/03 18:15:10 by Romain PEREIRA         / _______ \       */
/*                                                          \_)     (_/       */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/*   Author: Romain PEREIRA <rpereira@anl.gov>                                */
/*                                                                            */
/*   Copyright: see AUTHORS                                                   */
/*                                                                            */
/* ************************************************************************** */

# include <heat/consts.h>

__kernel
void
diffusion_cl_kernel(
    __global TYPE * src, int ld_src,
    __global TYPE * dst, int ld_dst,
    int tile_x, int tile_y,
    int tsx, int tsy
) {
    const int li = get_global_id(0);
    const int lj = get_global_id(1);
    const int  i = tile_x * tsx + li;
    const int  j = tile_y * tsy + lj;
    // printf("Running (li, lj) = (%d, %d) and (i, j) = (%d, %d)\n", li, lj, i, j);

    // boundary conditions fixed
    if (i > 0 && i < NX - 1 && j > 0 && j < NY - 1)
    {
        GRID(dst, li, lj, ld_dst) = GRID(src, li, lj, ld_src) + ALPHA * DT / (DX * DY) * (
                (GRID(src, li+1,   lj, ld_src) - 2 * GRID(src, li, lj, ld_src) + GRID(src, li-1,   lj, ld_src)) / (DX * DX) +
                (GRID(src,   li, lj+1, ld_src) - 2 * GRID(src, li, lj, ld_src) + GRID(src,   li, lj-1, ld_src)) / (DY * DY)
            );
    }
}
