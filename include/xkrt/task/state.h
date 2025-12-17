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

#ifndef __XKRT_TASK_STATE_H__
# define __XKRT_TASK_STATE_H__

/* task states */
typedef enum    xkrt_task_state_t
{
    TASK_STATE_ALLOCATED        = 0,    // task_t is allocated
    TASK_STATE_READY            = 1,    // task_t data can be fetched
    TASK_STATE_DATA_FETCHING    = 2,    // task_t data is being fetched
    TASK_STATE_DATA_FETCHED     = 3,    // task_t data is fetched, routine can execute
    TASK_STATE_EXECUTING        = 4,    // task_t routine executes
    TASK_STATE_COMPLETED        = 5,    // task_t completed, dependences can be resolved (kernel executed)
    TASK_STATE_DEALLOCATED      = 6,    // task_t is deallocated (virtual state, never set)
    TASK_STATE_MAX              = 7,
}               xkrt_task_state_t;

static inline const char *
xkrt_task_state_to_str(xkrt_task_state_t state)
{
    switch (state)
    {
        case (TASK_STATE_ALLOCATED):
            return "allocated";
        case (TASK_STATE_READY):
            return "ready";
        case (TASK_STATE_DATA_FETCHING):
            return "fetching";
        case (TASK_STATE_DATA_FETCHED):
            return "fetched";
        case (TASK_STATE_EXECUTING):
            return "executing";
        case (TASK_STATE_COMPLETED):
            return "completed";
        case (TASK_STATE_DEALLOCATED):
            return "deallocated";
        default:
            return "unk";
    }
}

#endif /* __XKRT_TASK_STATE_H__ */
