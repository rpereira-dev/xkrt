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

# include <xkrt/task/task-format.h>
# include <xkrt/logger/logger.h>

# include <atomic>
# include <cassert>
# include <cstring>

void
task_formats_init(task_formats_t * formats)
{
    memset(formats, 0, sizeof(task_formats_t));

    task_format_t format;
    memset(&format.f, 0, sizeof(format.f));
    snprintf(format.label, sizeof(format.label), "(null)");

    task_format_id_t id = task_format_create(formats, &format);
    assert(id == TASK_FORMAT_NULL);
}

task_format_t *
task_format_get(task_formats_t * formats, task_format_id_t id)
{
    return formats->list + id;
}

task_format_id_t
task_format_create(task_formats_t * formats, task_format_t * format)
{
    task_format_id_t fmtid = (task_format_id_t) formats->next_fmtid.fetch_add(1, std::memory_order_relaxed);
    memcpy(formats->list + fmtid, format, sizeof(task_format_t));
    assert(fmtid < TASK_FORMAT_MAX);
    LOGGER_INFO("Created new task format `%d` named `%s`", fmtid, format->label);
    return fmtid;
}
