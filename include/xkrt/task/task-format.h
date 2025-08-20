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

#ifndef __TASK_FORMAT_H__
# define __TASK_FORMAT_H__

# include <atomic>
# include <stdint.h>

typedef enum    task_format_target_t : uint8_t
{
    TASK_FORMAT_TARGET_HOST  = 0,
    TASK_FORMAT_TARGET_CUDA  = 1,
    TASK_FORMAT_TARGET_ZE    = 2,
    TASK_FORMAT_TARGET_CL    = 3,
    TASK_FORMAT_TARGET_HIP   = 4,
    TASK_FORMAT_TARGET_SYCL  = 5,
    TASK_FORMAT_TARGET_MAX   = 6

}               task_format_target_t;

typedef void (*task_format_func_t)();

typedef struct  task_format_t
{
    /* task launch */
    task_format_func_t f[TASK_FORMAT_TARGET_MAX];

    /* a label */
    char label[32];

} task_format_t;

/* maximum number of task format */
typedef uint8_t task_format_id_t;

# define TASK_FORMAT_MAX ((1 << (sizeof(task_format_id_t) * 8)) - 1)
# define TASK_FORMAT_NULL 0

typedef struct  task_formats_t
{
    task_format_t list[TASK_FORMAT_MAX];
    std::atomic<task_format_id_t> next_fmtid;
}               task_formats_t;

void task_formats_init(task_formats_t * formats);
task_format_id_t task_format_create(task_formats_t * formats, task_format_t * format);
task_format_t * task_format_get(task_formats_t * formats, task_format_id_t id);

#endif /* __TASK_TARGET_H__ */
