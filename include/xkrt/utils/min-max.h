/* ************************************************************************** */
/*                                                                            */
/*   min-max.h                                                    .-*-.       */
/*                                                              .'* *.'       */
/*   Created: 2024/07/10 17:00:08 by Romain Pereira          __/_*_*(_        */
/*   Updated: 2025/06/03 18:07:08 by Romain PEREIRA         / _______ \       */
/*                                                          \_)     (_/       */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/*   Author: Thierry GAUTIER <thierry.gautier@inrialpes.fr>                   */
/*   Author: Romain PEREIRA <romain.pereira@inria.fr>                         */
/*                                                                            */
/*   Copyright: see AUTHORS                                                   */
/*                                                                            */
/* ************************************************************************** */

#ifndef __MIN_MAX_H__
# define __MIN_MAX_H__

# ifndef MIN
#  define MIN(X, Y) ((Y) < (X) ? (Y) : (X))
# endif /* MIN */

# ifndef MAX
#  define MAX(X, Y) ((X) < (Y) ? (Y) : (X))
# endif /* MAX */

#endif
