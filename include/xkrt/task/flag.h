/*
** Copyright 2024,2025 INRIA
**
** Contributors :
** Thierry Gautier, thierry.gautier@inrialpes.fr
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

#ifndef __XKRT_TASK_FLAG_H__
# define __XKRT_TASK_FLAG_H__

/**
 *  Task flags. Constraints:
 *      - cannot have both TASK_FLAG_DOMAIN and TASK_FLAG_DEVICE
 */
typedef enum    xkrt_task_flags_t
{
    TASK_FLAG_ZERO          = 0,
    TASK_FLAG_DEPENDENT     = (1 << 0), // may have dependencies
    TASK_FLAG_DETACHABLE    = (1 << 1), // completion is associated with the completion of events
    TASK_FLAG_DEVICE        = (1 << 2), // may execute on a device
    TASK_FLAG_DOMAIN        = (1 << 3), // may have dependent children tasks - in such case, it will have a dependency and a memory domain
    TASK_FLAG_MOLDABLE      = (1 << 4), // the task may be split

    // support me in the future
  // TASK_FLAG_CANCEL        = (1 << 5), // cancelled
  // TASK_FLAG_UNDEFERED     = (1 << X), // suspend the current task execution until that task completed
  // TASK_FLAG_PERSISTENT    = (1 << Y), // persistence

    TASK_FLAG_REQUEUE       = (1 << 7), // will be re-queued after returning from its body
    TASK_FLAG_MAX           = (1 << 8)
}               xkrt_task_flags_t;

typedef uint8_t xkrt_task_flag_bitfield_t;
_Static_assert(TASK_FLAG_MAX <= (1 << 8*sizeof(xkrt_task_flag_bitfield_t)), "");

#endif /* __XKRT_TASK_FLAG_H__ */
