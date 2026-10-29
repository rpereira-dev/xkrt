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

# include <stdint.h>

# include <xkrt/data-structures/uthash.h>
# include <xkrt/logger/logger.h>
# include <xkrt/sync/spinlock.h>

# include <stdarg.h>
# include <stdio.h>
# include <stdlib.h>
# include <string.h>

volatile spinlock_t LOGGER_PRINT_MTX;

volatile double     LOGGER_TIME_ELAPSED = 0.0;
volatile uint64_t   LOGGER_LAST_TIME    = 0;

char const * LOGGER_PRINT_COLORS[LOGGER_PRINT_ID_MAX] = {
    "\033[1;31m",
    "\033[1;31m",
    "\033[1;33m",
    "\033[1;32m",
    "\033[1;35m",
    "\033[1;36m",
};

char const * LOGGER_PRINT_HEADERS[LOGGER_PRINT_ID_MAX] = {
    "FATAL",
    "ERROR",
    "WARN",
    "INFO",
    "IMPL",
    "DEBUG",
};

int LOGGER_VERBOSE = LOGGER_PRINT_INFO_ID;
int LOGGER_INITIALIZED = 0;

/* ------------------------------------------------------------------------ */
/*  Support for the LOGGER_*_ONCE macros                                    */
/* ------------------------------------------------------------------------ */

/** Initial size of the per-thread formatting buffer (grows if needed) */
# ifndef LOGGER_ONCE_BUFSZ
#  define LOGGER_ONCE_BUFSZ 1024
# endif

/**
 *  Upper bound on the number of distinct messages remembered.  It protects
 *  against unbounded growth when a message embeds a varying value (pointer,
 *  counter...).  Once reached, unknown messages are simply printed every time.
 */
# ifndef LOGGER_ONCE_MAX_ENTRIES
#  define LOGGER_ONCE_MAX_ENTRIES 4096
# endif

/* ---- per-thread formatting buffer --------------------------------------- */

/* the buffer, lazily malloc()'d by each thread on its first use, then reused */
static _Thread_local char *  LOGGER_TLS_BUF = NULL;
static _Thread_local size_t  LOGGER_TLS_CAP = 0;

/**
 *  Make sure the calling thread's buffer holds at least 'need' bytes.
 *  The previous content is NOT preserved.  Returns NULL if malloc() fails.
 */
static char *
logger_tls_buffer(size_t need)
{
    if (LOGGER_TLS_BUF && LOGGER_TLS_CAP >= need)
        return LOGGER_TLS_BUF;

    size_t cap = LOGGER_TLS_CAP ? LOGGER_TLS_CAP : (size_t) LOGGER_ONCE_BUFSZ;
    while (cap < need)
        cap *= 2;

    free(LOGGER_TLS_BUF);
    LOGGER_TLS_BUF = NULL;
    LOGGER_TLS_CAP = 0;

    char * buf = (char *) malloc(cap);
    if (buf == NULL)
        return NULL;

    LOGGER_TLS_BUF = buf;
    LOGGER_TLS_CAP = cap;
    return buf;
}

char const *
logger_once_format(char const * fmt, ...)
{
    char * buf = LOGGER_TLS_BUF;
    if (buf == NULL)
    {
        buf = logger_tls_buffer(LOGGER_ONCE_BUFSZ);
        if (buf == NULL)
            return NULL;
    }

    va_list ap, ap2;
    va_start(ap, fmt);
    va_copy(ap2, ap);

    int n = vsnprintf(buf, LOGGER_TLS_CAP, fmt, ap);
    va_end(ap);

    if (n < 0)
    {
        buf = NULL;
    }
    else if ((size_t) n >= LOGGER_TLS_CAP)  /* truncated: grow, then redo */
    {
        buf = logger_tls_buffer((size_t) n + 1);
        if (buf)
            vsnprintf(buf, LOGGER_TLS_CAP, fmt, ap2);
    }
    va_end(ap2);
    return buf;
}

void
logger_once_free_buffer(void)
{
    free(LOGGER_TLS_BUF);
    LOGGER_TLS_BUF = NULL;
    LOGGER_TLS_CAP = 0;
}

/* ---- set of already printed messages ------------------------------------ */

typedef struct  logger_once_entry_t
{
    char *          key;    /* NUL-terminated, stored right after the struct */
    UT_hash_handle  hh;
}               logger_once_entry_t;

static logger_once_entry_t *    LOGGER_ONCE_SETS[LOGGER_PRINT_ID_MAX] = { NULL };
static unsigned int             LOGGER_ONCE_COUNT = 0;
static volatile spinlock_t      LOGGER_ONCE_MTX = 0;

int
logger_once_register(int lvl, char const * msg)
{
    if (lvl < 0 || lvl >= LOGGER_PRINT_ID_MAX)
        return 1;

    logger_once_entry_t * e = NULL;

    SPINLOCK_LOCK(LOGGER_ONCE_MTX);
    HASH_FIND_STR(LOGGER_ONCE_SETS[lvl], msg, e);
    int full = (LOGGER_ONCE_COUNT >= LOGGER_ONCE_MAX_ENTRIES);
    SPINLOCK_UNLOCK(LOGGER_ONCE_MTX);

    if (e)
        return 0;
    if (full)
        return 1;

    size_t len = strlen(msg);
    logger_once_entry_t * n = (logger_once_entry_t *) malloc(sizeof(logger_once_entry_t) + len + 1);
    if (n == NULL)
        return 1;
    n->key = (char *) (n + 1);
    memcpy(n->key, msg, len + 1);

    int first = 1;
    SPINLOCK_LOCK(LOGGER_ONCE_MTX);
    {
        HASH_FIND_STR(LOGGER_ONCE_SETS[lvl], msg, e);
        if (e)
            first = 0;
        else if (LOGGER_ONCE_COUNT < LOGGER_ONCE_MAX_ENTRIES)
        {
            HASH_ADD_KEYPTR(hh, LOGGER_ONCE_SETS[lvl], n->key, len, n);
            ++LOGGER_ONCE_COUNT;
            n = NULL;
        }
    }
    SPINLOCK_UNLOCK(LOGGER_ONCE_MTX);

    free(n);
    return first;
}

void
logger_once_reset(void)
{
    logger_once_entry_t * old[LOGGER_PRINT_ID_MAX];

    SPINLOCK_LOCK(LOGGER_ONCE_MTX);
    for (int i = 0 ; i < LOGGER_PRINT_ID_MAX ; ++i)
    {
        old[i] = LOGGER_ONCE_SETS[i];
        LOGGER_ONCE_SETS[i] = NULL;
    }
    LOGGER_ONCE_COUNT = 0;
    SPINLOCK_UNLOCK(LOGGER_ONCE_MTX);

    for (int i = 0 ; i < LOGGER_PRINT_ID_MAX ; ++i)
    {
        logger_once_entry_t * e, * tmp;
        HASH_ITER(hh, old[i], e, tmp)
        {
            HASH_DEL(old[i], e);
            free(e);
        }
    }
}
