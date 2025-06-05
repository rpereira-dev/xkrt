/*
** Copyright 2024,2025 INRIA
**
** Contributors :
** Thierry Gautier, thierry.gautier@inrialpes.fr
** Joao Lima joao.lima@inf.ufsm.br
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

# include <xkrt/task/task.hpp>
# include <xkrt/thread/deque.hpp>
# include <xkrt/logger/logger.h>
# include <xkrt/sync/mem.h>

// TODO : PROBLEM - this impl assumes `push` and `pop` are called from the same
// 'producer' thread and `steal` from any other thread.
// It does not support 'giving' tasks, that is, having a thread different from
// the producer pushing a task into this queue

template <typename T, int C>
void
xkrt_deque_t<T, C>::push(T const & task)
{
    int idx = _t++;
    tasks[idx%C] = task;
}

template <typename T, int C>
T
xkrt_deque_t<T,C>::pop(void)
{
    task_t * task;
    int idx = --_t;
    if (_h > _t)
    {
        ++_t;
        SPINLOCK_LOCK(lock);
        {
            int idx = --_t;
            if (_h > idx || ((task = tasks[idx%C]) == NULL))
            {
                ++_t;
                SPINLOCK_UNLOCK(lock);
                return NULL; // FAILURE
            }
            else
                tasks[idx%C] = NULL;
        }
        SPINLOCK_UNLOCK(lock);
    }
    else
    {
        task = tasks[idx%C];
        tasks[idx%C] = NULL;
    }
    return task; // SUCCESS
}

template <typename T, int C>
T
xkrt_deque_t<T,C>::steal(void)
{
    task_t * task;
    SPINLOCK_LOCK(lock);
    {
        int idx = _h++;
        if (idx >= _t || ((task = tasks[idx%C]) == NULL))
        {
            --_h;
            SPINLOCK_UNLOCK(lock);
            return NULL;  // FAILURE
        }
        tasks[idx%C] = NULL;
    }
    SPINLOCK_UNLOCK(lock);
    return task;  // SUCCESS
}

// Explicit instantiation
template struct xkrt_deque_t<task_t *, 4096>;
