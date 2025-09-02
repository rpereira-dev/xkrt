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

# ifndef __METRICS_H__
#  define __METRICS_H__

#  include <stddef.h>
#  include <stdint.h>

# include <xkrt/namespace.h>
XKRT_NAMESPACE_BEGIN

uint64_t get_nanotime(void);
void metric_byte(char * buffer, int bufsize, size_t nbytes);
void metric_time(char * buffer, int bufsize, uint64_t ns);
void metric_bandwidth(char * buffer, int bufsize, size_t byte_per_sec);

typedef enum    metric_t
{
    METRIC_BYTE,
    METRIC_TIME,
    METRIC_BW,
    METRIC_MAX
}               metric_t;

typedef struct  power_counter_t
{
    uint64_t b1, b2, b3 ,b4;
}               power_counter_t;

typedef struct  power_t
{
    struct {
        /* start/stop times */
        uint64_t t1, t2;

        /* power values */
        power_counter_t c1, c2;
    } priv;

    /* delta time between a start/stop */
    double dt;

    /* power (J/s <=> Watt) between a start/stop */
    double P;

}               power_t;

XKRT_NAMESPACE_END

# endif /* __METRICS_H__ */
