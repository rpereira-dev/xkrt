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

# include <xkrt/support.h>
# if XKRT_SUPPORT_STATS

# include <xkrt/runtime.h>
# include <xkrt/task/task.hpp>
# include <xkrt/task/task-format.h>
# include <xkrt/logger/logger.h>
# include <xkrt/logger/metric.h>

# include <string.h>

typedef struct  device_stats_t
{
    struct {
        stats_int_t freed;
        struct {
            stats_int_t total;
            stats_int_t currently;
        } allocated;
        stats_int_t registered;
        stats_int_t unregistered;
    } memory;

    struct {
        stats_int_t n;
        stats_int_t transfered;
    } streams[XKRT_STREAM_TYPE_ALL];

    struct {
        stats_int_t commited;
        stats_int_t completed;
    } instructions[XKRT_STREAM_INSTR_TYPE_MAX];

}               device_stats_t;

static void
xkrt_runtime_stats_device_agg_gather(xkrt_runtime_t * runtime, device_stats_t * agg)
{
    agg->memory.registered += runtime->stats.memory.registered;
    agg->memory.unregistered += runtime->stats.memory.unregistered;
}

static void
xkrt_runtime_stats_device_agg(device_stats_t * src, device_stats_t * agg)
{
    agg->memory.freed += src->memory.freed;
    agg->memory.allocated.total += src->memory.allocated.total;
    agg->memory.allocated.currently += src->memory.allocated.currently;

    for (int stype = 0 ; stype < XKRT_STREAM_TYPE_ALL ; ++stype)
    {
        agg->streams[stype].n += src->streams[stype].n;
        agg->streams[stype].transfered += src->streams[stype].transfered;
    }

    for (int instr_type = 0 ; instr_type < XKRT_STREAM_INSTR_TYPE_MAX ; ++instr_type)
    {
        agg->instructions[instr_type].commited += src->instructions[instr_type].commited;
        agg->instructions[instr_type].completed += src->instructions[instr_type].completed;
    }
}

static void
xkrt_runtime_stats_device_report(device_stats_t * stats)
{
    char buffer[128];

    LOGGER_WARN("  Memory");

    xkrt_metric_byte(buffer, sizeof(buffer), stats->memory.allocated.total.load());
    LOGGER_WARN("    Allocated (total): %s", buffer);

    xkrt_metric_byte(buffer, sizeof(buffer), stats->memory.allocated.currently.load());
    LOGGER_WARN("    Allocated (currently): %s", buffer);

    xkrt_metric_byte(buffer, sizeof(buffer), stats->memory.freed.load());
    LOGGER_WARN("    Freed: %s", buffer);

    if (stats->memory.registered.load() || stats->memory.unregistered.load())
    {
        xkrt_metric_byte(buffer, sizeof(buffer), stats->memory.registered.load());
        LOGGER_WARN("    Registered: %s", buffer);

        xkrt_metric_byte(buffer, sizeof(buffer), stats->memory.unregistered.load());
        LOGGER_WARN("    Unregistered: %s", buffer);
    }


    LOGGER_WARN("  Streams");
    for (int stype = 0 ; stype < XKRT_STREAM_TYPE_ALL ; ++stype)
    {
        # if 0
        xkrt_metric_byte(buffer, sizeof(buffer), stats->streams[stype].transfered.load());
        LOGGER_WARN("    `%4s` - with %2lu streams - transfered %s", xkrt_stream_type_to_str((xkrt_stream_type_t) stype), stats->streams[stype].n.load(), buffer);
        # else
        LOGGER_WARN("    `%4s` - with %2lu streams - transfered %zuB", xkrt_stream_type_to_str((xkrt_stream_type_t) stype), stats->streams[stype].n.load(), stats->streams[stype].transfered.load());
        # endif
    }

    LOGGER_WARN("  Instructions");
    for (int instr_type = 0 ; instr_type < XKRT_STREAM_INSTR_TYPE_MAX ; ++instr_type)
    {
        LOGGER_WARN(
            "    `%12s` - commited %6zu - completed %6zu",
            xkrt_stream_instruction_type_to_str((xkrt_stream_instruction_type_t) instr_type),
            stats->instructions[instr_type].commited.load(),
            stats->instructions[instr_type].completed.load()
        );
    }
}

static void
xkrt_runtime_stats_device_gather(
    xkrt_device_t * device,
    device_stats_t * stats
) {
    memset(stats, 0, sizeof(device_stats_t));

    stats->memory.freed = device->stats.memory.freed.load();
    stats->memory.allocated.total = device->stats.memory.allocated.total.load();
    stats->memory.allocated.currently = device->stats.memory.allocated.currently.load();

    for (uint8_t device_tid = 0 ; device_tid < device->nthreads ; ++device_tid)
    {
        for (int stype = 0 ; stype < XKRT_STREAM_TYPE_ALL ; ++stype)
        {
            for (int stream_id = 0 ; stream_id < device->count[stype] ; ++stream_id)
            {
                xkrt_stream_t * stream = device->streams[device_tid][stype][stream_id];
                for (int instr_type = 0 ; instr_type < XKRT_STREAM_INSTR_TYPE_MAX ; ++instr_type)
                {
                    stats->instructions[instr_type].commited += stream->stats.instructions[instr_type].commited.load();
                    stats->instructions[instr_type].completed += stream->stats.instructions[instr_type].completed.load();
                }
                stats->streams[stype].transfered += stream->stats.transfered.load();
            }
            stats->streams[stype].n += device->count[stype];
        }
    }
}

static void
xkrt_runtime_stats_tasks_report(xkrt_runtime_t * runtime)
{
    for (size_t i = 0 ; i < TASK_FORMAT_MAX ; ++i)
    {
        task_format_t * format = task_format_get(&runtime->formats.list, (task_format_id_t) i);
        if (format == NULL)
            break ;
        if (runtime->stats.tasks[i].commited)
            LOGGER_WARN("  `%16s` - %6zu commited - %6zu submitted - %6zu completed",
                format->label,
                runtime->stats.tasks[i].commited.load(),
                runtime->stats.tasks[i].submitted.load(),
                runtime->stats.tasks[i].completed.load()
            );
    }

    # ifndef NDEBUG
    xkrt_thread_t * thread = xkrt_thread_t::get_tls();
    int counter[TASK_STATE_MAX];
    memset(counter, 0, sizeof(counter));
    for (task_t * & task : thread->tasks)
    {
        assert(task->state.value >= TASK_STATE_ALLOCATED && task->state.value < TASK_STATE_MAX);
        ++counter[task->state.value];
    }

    for (int i = 0 ; i < TASK_STATE_MAX ; ++i)
        LOGGER_WARN("  `%8d` tasks in state `%12s`", counter[i], task_state_to_str((task_state_t)i));

    # endif /* NDEBUG */
}

void
xkrt_runtime_stats_report(xkrt_runtime_t * runtime)
{
    LOGGER_WARN("----------------- STATS -----------------");
    device_stats_t agg;
    memset(&agg, 0, sizeof(agg));

    for (xkrt_device_global_id_t device_global_id = 0 ; device_global_id < runtime->drivers.devices.n ; ++device_global_id)
    {
        xkrt_device_t * device = runtime->drivers.devices.list[device_global_id];

        xkrt_driver_t * driver = runtime->driver_get(device->driver_type);
        LOGGER_WARN("Device %u", device->global_id);

        char info[512];
        driver->f_device_info(device->driver_id, info, sizeof(info));
        LOGGER_WARN("  Info: %s", info);

        device_stats_t stats;
        xkrt_runtime_stats_device_gather(device, &stats);
        xkrt_runtime_stats_device_report(&stats);
        xkrt_runtime_stats_device_agg(&stats, &agg);
    }
    xkrt_runtime_stats_device_agg_gather(runtime, &agg);

    LOGGER_WARN("-----------------------------------------");
    LOGGER_WARN("All Devices");
    xkrt_runtime_stats_device_report(&agg);
    LOGGER_WARN("-----------------------------------------");
    LOGGER_WARN("Tasks");
    xkrt_runtime_stats_tasks_report(runtime);
    LOGGER_WARN("-----------------------------------------");
}

# endif /* XKRT_SUPPORT_STATS */
