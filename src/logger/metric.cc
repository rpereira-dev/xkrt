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

# include <xkrt/logger/metric.h>
# include <time.h>
# include <stdio.h>

XKRT_NAMESPACE_BEGIN

uint64_t
get_nanotime(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)(ts.tv_sec * 1000000000) + (uint64_t) ts.tv_nsec;
}

void
metric_byte(char * buffer, int bufsize, size_t nbytes)
{
    const char * suffixes[] = {"B", "KB", "MB", "GB", "TB", "PB", "EB"};
    unsigned int i = 0;
    double size = (double) nbytes;
    while (size >= 1024 && i < sizeof(suffixes) / sizeof(*suffixes))
    {
        size /= 1024;
        i++;
    }
    snprintf(buffer, bufsize, "%.2lf%s", size, suffixes[i]);
}

void
metric_time(char * buffer, int bufsize, uint64_t ns)
{
    const char * suffixes[] = {"ns", "us", "ms", "s"};
    unsigned int i = 0;
    double size = (double) ns;
    while (size >= 1000 && i < sizeof(suffixes) / sizeof(*suffixes))
    {
        size /= 1000;
        i++;
    }
    snprintf(buffer, bufsize, "%.2lf%s", size, suffixes[i]);
}

void
metric_bandwidth(char * buffer, int bufsize, size_t byte_per_sec)
{
    const char * suffixes[] = {"B", "KB", "MB", "GB", "TB", "PB", "EB"};
    unsigned int i = 0;
    double size = (double) byte_per_sec;
    while (size >= 1024 && i < sizeof(suffixes) / sizeof(*suffixes))
    {
        size /= 1024;
        i++;
    }
    snprintf(buffer, bufsize, "%.2lf%s/s", size, suffixes[i]);
}

XKRT_NAMESPACE_END
