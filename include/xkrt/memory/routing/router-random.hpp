/* ************************************************************************** */
/*                                                                            */
/*   router-random.hpp                                            .-*-.       */
/*                                                              .'* *.'       */
/*   Created: 2025/02/11 14:59:33 by Romain PEREIRA          __/_*_*(_        */
/*   Updated: 2025/06/03 18:04:50 by Romain PEREIRA         / _______ \       */
/*                                                          \_)     (_/       */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/*   Author: Thierry GAUTIER <thierry.gautier@inrialpes.fr>                   */
/*   Author: Romain PEREIRA <rpereira@anl.gov>                                */
/*                                                                            */
/*   Copyright: see AUTHORS                                                   */
/*                                                                            */
/* ************************************************************************** */

# ifndef __ROUTER_RANDOM_HPP__
#  define __ROUTER_RANDOM_HPP__

# include <xkrt/memory/routing/router.hpp>
# include <xkrt/sync/bits.h>

/** The higher the rank, the lower the performance */
class RouterRandom : public Router
{
    # if 0
    private:
        /** The lower the rank, the higher the performance */
        const xkrt_device_global_id_bitfield_t affinity[XKRT_DEVICES_PERF_RANK_MAX];
    # endif /* 0 */

    public:
        RouterRandom() {}
        ~RouterRandom() {}

        xkrt_device_global_id_t
        get_source(
            const xkrt_device_global_id_t dst,
            const xkrt_device_global_id_bitfield_t valid
        ) const {
            /* get any random device */
            return (xkrt_device_global_id_t) (__random_set_bit(valid) - 1);
        }

};

# endif /* __ROUTER_RANDOM_HPP__ */
