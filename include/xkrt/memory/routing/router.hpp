/* ************************************************************************** */
/*                                                                            */
/*   router.hpp                                                   .-*-.       */
/*                                                              .'* *.'       */
/*   Created: 2025/02/11 14:59:33 by Romain PEREIRA          __/_*_*(_        */
/*   Updated: 2025/06/03 18:04:47 by Romain PEREIRA         / _______ \       */
/*                                                          \_)     (_/       */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/*   Author: Thierry GAUTIER <thierry.gautier@inrialpes.fr>                   */
/*   Author: Romain PEREIRA <rpereira@anl.gov>                                */
/*                                                                            */
/*   Copyright: see AUTHORS                                                   */
/*                                                                            */
/* ************************************************************************** */

# ifndef __ROUTER_HPP__
#  define __ROUTER_HPP__

# include <xkrt/consts.h>

class Router
{
    /**
     *  Retrieve the source to use for a data transfer to 'dst' where the
     *  valid sources are amongst the 'valid' bitfield
     */
    virtual xkrt_device_global_id_t
    get_source(
        const xkrt_device_global_id_t dst,
        const xkrt_device_global_id_bitfield_t valid
    ) const = 0;

};

# endif /* __ROUTER_HPP__ */
