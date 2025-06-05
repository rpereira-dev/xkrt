/* ************************************************************************** */
/*                                                                            */
/*   router-cfs.hpp                                               .-*-.       */
/*                                                              .'* *.'       */
/*   Created: 2025/03/31 08:26:12/05:00 by Romain PEREI      __/_*_*(_        */
/*   Updated: 2025/06/03 18:04:37 by Romain PEREIRA         / _______ \       */
/*                                                          \_)     (_/       */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/*   Author: Thierry GAUTIER <thierry.gautier@inrialpes.fr>                   */
/*   Author: Romain PEREIRA <rpereira@anl.gov>                                */
/*                                                                            */
/*   Copyright: see AUTHORS                                                   */
/*                                                                            */
/* ************************************************************************** */

/**
 *  Idea is to use a dijkstra algorithm with weights being BW, but ignoring the link if there is already a pending transfer
 *  Problem is it may add significant latency
 */

# ifndef __ROUTER_CFS_HPP__
#  define __ROUTER_CFS_HPP__

# include <xkrt/consts.h>
# include <xkrt/memory/routing/router.hpp>
# include <xkrt/sync/bits.h>

# include <stdint.h>

/** The higher the rank, the lower the performance */
class RouterCFS : public Router
{
    public:
        typedef struct xkrt_router_cfs_map_t
        {
            struct {
                uint8_t weight;
                bool used;
            } values[XKRT_DEVICES_MAX][XKRT_DEVICES_MAX];

            xkrt_router_cfs_map_t(const uint8_t weights[XKRT_DEVICES_MAX][XKRT_DEVICES_MAX])
            {
                for (xkrt_device_global_id_t i = 0 ; i < XKRT_DEVICES_MAX ; ++i)
                {
                    for (xkrt_device_global_id_t j = 0 ; j < XKRT_DEVICES_MAX ; ++j)
                    {
                        values[i][j].weight = weights[i][j];
                        values[i][j].used = false;
                    }
                }
            }
        }               xkrt_router_cfs_map_t;

    public:

        /**
         *  A graph where weights[i][j] is the weight of the edge between node 'i' and 'j'
         *  If weights[i][j] is UINT8_MAX, then there is no edge
         *  If weights[i][j] = weights[i'][j'] / x then the bw on link (i,j) is
         *      'x' times greater than the bw of link (i,j) - i.e. the lower weights[i][j] the greater the BW
         *  If used[i][j], then a communication is already occuring on link (i,j)
         */
        const xkrt_router_cfs_map_t map;

    public:

        RouterCFS(const uint8_t weights[XKRT_DEVICES_MAX][XKRT_DEVICES_MAX]) : weights(weights) {}
        ~RouterCFS() {}

        /* @override */
        xkrt_device_global_id_t
        get_source(
            const xkrt_device_global_id_t dst,
            const xkrt_device_global_id_bitfield_t valid
        ) const {

            /* fast way out: valid on that device already */
            if (valid & (1 << dst))
                return dst;

            // TODO

            /* get any random device */
            return (xkrt_device_global_id_t) (__random_set_bit(valid) - 1);
        }

};

# endif /* __ROUTER_CFS_HPP__ */
