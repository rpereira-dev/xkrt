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

#ifndef __QUEUE_TYPE_HPP__
# define __QUEUE_TYPE_HPP__

XKRT_NAMESPACE_BEGIN

    /* DONT CHANGE ORDER HERE !! Can have side effects (in the Offloader class for instance) */
    typedef enum    queue_type_t
    {
        QUEUE_TYPE_H2D        = 0,    /* from CPU to GPU */
        QUEUE_TYPE_D2H        = 1,    /* from GPU to CPU */
        QUEUE_TYPE_D2D        = 2,    /* from GPU to GPU */
        QUEUE_TYPE_KERN       = 3,
        QUEUE_TYPE_FD_READ    = 4,
        QUEUE_TYPE_FD_WRITE   = 5,
        QUEUE_TYPE_ALL                /* internal purpose */

    }               queue_type_t;

    const char * queue_type_to_str(queue_type_t type);

XKRT_NAMESPACE_END

# endif /* __QUEUE_TYPE_HPP__ */
