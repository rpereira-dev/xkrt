/* ************************************************************************** */
/*                                                                            */
/*   router-affinity.hpp                                          .-*-.       */
/*                                                              .'* *.'       */
/*   Created: 2025/02/11 14:59:33 by Romain PEREIRA          __/_*_*(_        */
/*   Updated: 2025/08/22 22:44:42 by Romain PEREIRA         / _______ \       */
/*                                                          \_)     (_/       */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/*   Author: Thierry GAUTIER <thierry.gautier@inrialpes.fr>                   */
/*   Author: Romain PEREIRA <rpereira@anl.gov>                                */
/*                                                                            */
/*   Copyright: see AUTHORS                                                   */
/*                                                                            */
/* ************************************************************************** */

# ifndef __ROUTER_AFFINITY_HPP__
#  define __ROUTER_AFFINITY_HPP__

# include <xkrt/consts.h>
# include <xkrt/memory/routing/router.hpp>
# include <xkrt/sync/bits.h>

XKRT_NAMESPACE_BEGIN

/** The higher the rank, the lower the performance */
class RouterAffinity : public Router
{
    public:
        /**
         *  Given a destinatary device global id 'i' - affinity[i][j] is a
         *  bitmask of with '1' on the source devices of affinity 'j'.  The
         *  lowest the affinity, the higher the performance.
         */
        device_global_id_bitfield_t affinity[XKRT_DEVICES_MAX][XKRT_DEVICES_PERF_RANK_MAX];

    public:

        RouterAffinity() {}
        ~RouterAffinity() {}

        /* @override */
        device_global_id_t
        get_source(
            const device_global_id_t dst,
            const device_global_id_bitfield_t valid
        ) const override {

            /* fast way out: valid on that device already */
            if (valid & (1 << dst))
                return dst;

            /* find a device for P2P transfer - lowest rank <=> best performance */
            for (int rank = 0 ; rank < XKRT_DEVICES_PERF_RANK_MAX - 1 ; ++rank)
            {
                /* get valid devices for this perf */
                const device_global_id_bitfield_t mask = valid & this->affinity[dst][rank];
                if (mask == 0)
                    continue ;

                /* return a random device with this affinity */
                return __random_set_bit(mask) - 1;
            }

            /* get any random device */
            return (device_global_id_t) (__random_set_bit(valid) - 1);
        }

};

XKRT_NAMESPACE_END

# endif /* __ROUTER_AFFINITY_HPP__ */
