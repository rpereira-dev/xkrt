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

# include <atomic>
# include <cassert>
# include <cstring>

# include <xkrt/logger/logger.h>
# include <xkrt/namespace.h>
# include <xkrt/task/format.h>

extern "C"
void
xkrt_task_formats_init(xkrt_task_formats_t * formats)
{
    memset(formats, 0, sizeof(xkrt_task_formats_t));

    xkrt_task_format_t format;
    memset(&format.f, 0, sizeof(format.f));
    snprintf(format.label, sizeof(format.label), "(null)");

    xkrt_task_format_id_t id = xkrt_task_format_create(formats, &format);
    assert(id == XKRT_TASK_FORMAT_NULL);
}

extern "C"
xkrt_task_format_t *
xkrt_task_format_get(xkrt_task_formats_t * formats, xkrt_task_format_id_t fmtid)
{
    return formats->list + fmtid;
}

extern "C"
xkrt_task_format_id_t
xkrt_task_format_create(xkrt_task_formats_t * formats, const xkrt_task_format_t * format)
{
    const xkrt_task_format_id_t fmtid = formats->next_fmtid++;
    assert(fmtid < XKRT_TASK_FORMAT_MAX);
    memcpy(formats->list + fmtid, format, sizeof(xkrt_task_format_t));
    LOGGER_DEBUG("Created new task format `%d` named `%s`", fmtid, format->label);
    return fmtid;
}

extern "C"
xkrt_task_format_id_t
xkrt_task_format_put(xkrt_task_formats_t * formats, const char * label)
{
    const xkrt_task_format_id_t fmtid = formats->next_fmtid++;
    assert(fmtid < XKRT_TASK_FORMAT_MAX);
    snprintf(formats->list[fmtid].label, sizeof(formats->list[fmtid].label), "%s", label);
    LOGGER_DEBUG("Created new task format `%d` named `%s`", fmtid, formats->list[fmtid].label);
    return fmtid;
}

extern "C"
int
xkrt_task_format_set(
    xkrt_task_formats_t * formats,
    xkrt_task_format_id_t fmtid,
    xkrt_task_format_target_t target,
    xkrt_task_format_func_t func
) {
    xkrt_task_format_t * format = xkrt_task_format_get(formats, fmtid);
    format->f[target] = func;
    return 0;
}
