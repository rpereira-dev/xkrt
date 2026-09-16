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

# include <xkrt/support.h>

# if XKRT_SUPPORT_TOOLS

# include <xkrt/logger/logger.h>
# include <xkrt/runtime.h>
# include <xkrt/tool.h>

# include <dlfcn.h>
# include <stdlib.h>
# include <string.h>

XKRT_NAMESPACE_BEGIN

/* type of the entry point exported by a tool shared library */
typedef xkrt_tool_result_t * (*xkrt_tool_start_t)(void);

xkrt_set_result_t
runtime_t::tool_set_callback(xkrt_callback_t event, xkrt_callback_generic_t callback)
{
    if (event <= 0 || event >= XKRT_CALLBACK_MAX)
        return XKRT_SET_ERROR;
    this->tool.callbacks[event] = callback;
    return XKRT_SET_ALWAYS;
}

int
runtime_t::tool_get_callback(xkrt_callback_t event, xkrt_callback_generic_t * callback)
{
    if (event <= 0 || event >= XKRT_CALLBACK_MAX || callback == NULL)
        return 0;
    xkrt_callback_generic_t cb = this->tool.callbacks[event];
    if (cb == NULL)
        return 0;
    *callback = cb;
    return 1;
}

uint64_t
runtime_t::tool_unique_id(void)
{
    return this->tool.next_unique_id.fetch_add(1, std::memory_order_relaxed);
}

void
runtime_t::tool_connect(xkrt_tool_result_t * result)
{
    this->tool.result = result;
}

void
runtime_t::tool_init(void)
{
    this->tool.enabled   = false;
    memset(this->tool.callbacks, 0, sizeof(this->tool.callbacks));
    this->tool.dl_handle = NULL;

    /* 1. an in-process tool registered through tool_connect() has priority */
    xkrt_tool_result_t * result = this->tool.result;
    void *               handle = NULL;

    /* 2. otherwise, try to load the tool passed through XKRT_TOOL_PATH */
    if (result == NULL)
    {
        const char * path = getenv("XKRT_TOOL_PATH");
        if (path == NULL || path[0] == '\0')
            return ; /* no tool requested */

        handle = dlopen(path, RTLD_NOW | RTLD_LOCAL);
        if (handle == NULL)
        {
            LOGGER_WARN("XKRT_TOOL_PATH='%s' but dlopen failed: %s", path, dlerror());
            return ;
        }

        xkrt_tool_start_t start = (xkrt_tool_start_t) dlsym(handle, "xkrt_tool_start");
        if (start == NULL)
        {
            LOGGER_WARN("Tool '%s' exports no 'xkrt_tool_start' symbol, ignoring", path);
            dlclose(handle);
            return ;
        }

        result = start();
        if (result == NULL)
        {
            LOGGER_INFO("Tool '%s' declined activation", path);
            dlclose(handle);
            return ;
        }
    }

    /* 3. activate the tool by calling its initialize with this runtime */
    this->tool.result    = result;
    this->tool.dl_handle = handle;

    int keep = 1;
    if (result->initialize)
        keep = result->initialize(this, &result->tool_data);

    if (keep)
    {
        this->tool.enabled = true;
        LOGGER_INFO("XKRT-T: tool activated");
    }
    else
    {
        /* the tool declined during initialize: roll back */
        LOGGER_INFO("XKRT-T: tool declined activation during initialize");
        this->tool.result = NULL;
        if (handle)
            dlclose(handle);
        this->tool.dl_handle = NULL;
    }
}

void
runtime_t::tool_fini(void)
{
    /* stop dispatching before tearing the tool down */
    this->tool.enabled = false;

    if (this->tool.result)
    {
        if (this->tool.result->finalize)
            this->tool.result->finalize(&this->tool.result->tool_data);
        this->tool.result = NULL;
    }

    if (this->tool.dl_handle)
    {
        dlclose(this->tool.dl_handle);
        this->tool.dl_handle = NULL;
    }
}

XKRT_NAMESPACE_END

# endif /* XKRT_SUPPORT_TOOLS */
