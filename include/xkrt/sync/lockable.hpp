/*
** Copyright 2024,2025 INRIA
**
** Contributors :
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

#ifndef __LOCKABLE_HPP__
# define __LOCKABLE_HPP__

# include <xkrt/support.h>
# include <xkrt/sync/spinlock.h>

/* an abstract object that can be locked */
class Lockable {

    public:
        spinlock_t spinlock;

        # if XKRT_SUPPORT_DEBUG
        volatile bool locked;
        # endif /* XKRT_SUPPORT_DEBUG */

    public:
        # if XKRT_SUPPORT_DEBUG
        Lockable() : spinlock(), locked(false) {}
        # else
        Lockable() : spinlock() {}
        # endif /* XKRT_SUPPORT_DEBUG */

        ~Lockable() {}

    public:

        inline void
        lock(void)
        {
            SPINLOCK_LOCK(this->spinlock);
            # if XKRT_SUPPORT_DEBUG
            this->locked = true;
            # endif /* XKRT_SUPPORT_DEBUG */
        }

        inline void
        unlock(void)
        {
            # if XKRT_SUPPORT_DEBUG
            this->locked = false;
            # endif /* XKRT_SUPPORT_DEBUG */
            SPINLOCK_UNLOCK(this->spinlock);
        }

        #if XKRT_SUPPORT_DEBUG
        bool
        is_locked(void) const
        {
            return this->locked;
        }
        # endif /* XKRT_SUPPORT_DEBUG */
};

#endif /* __LOCKABLE_HPP__ */
