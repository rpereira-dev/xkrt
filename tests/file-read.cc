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

# include <fcntl.h>
# include <unistd.h>
# include <stdlib.h>
# include <stdio.h>
# include <string.h>
# include <errno.h>

# include <new>

# include <xkrt/runtime.h>
# include <xkrt/logger/logger.h>
# include <xkrt/logger/metric.h>

XKRT_NAMESPACE_USE;

constexpr const char * filename = "file.bin";             // filename
constexpr size_t buffer_size = (1024 * 1024);             // 1MB
//constexpr size_t total_size  = (1L * 1024 * 1024 * 1024); // 1GB
constexpr size_t total_size  = (8L * 1024 * 1024);      // 8MB
constexpr int    nchunks      = 4;                        // tasks that reads the file

static runtime_t runtime;

static void
createfile(void)
{
    LOGGER_INFO("Creating %luGB file", total_size/1024/1024/1024);
    int fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0)
    {
        perror("Failed to open file");
        exit(EXIT_FAILURE);
    }

    char * buffer = (char *) malloc(buffer_size);
    memset(buffer, 1, buffer_size);
    if (!buffer)
    {
        perror("Failed to allocate buffer");
        close(fd);
        exit(EXIT_FAILURE);
    }

    long long bytes_written = 0;
    while (bytes_written < total_size)
    {
        size_t to_write = buffer_size;
        if (total_size - bytes_written < buffer_size)
            to_write = total_size - bytes_written;

        ssize_t written = write(fd, buffer, to_write);
        if (written < 0)
        {
            perror("Write error");
            exit(EXIT_FAILURE);
        }

        bytes_written += written;
    }
    LOGGER_INFO("Created");
}

int
main(void)
{
    createfile();

    assert(runtime.init() == 0);

    unsigned char * buffer = (unsigned char *) malloc(total_size);
    assert(buffer);

    int fd = open(filename, O_RDONLY, 0644);
    if (fd < 0)
    {
        perror("Failed to open file");
        exit(EXIT_FAILURE);
    }

    # if 0
    uint64_t t0 = get_nanotime();
    # endif

    /* spawn tasks that read the file */
    runtime.file_read_async(fd, buffer, total_size, nchunks);

    /* spawn a successor for each read task */
    runtime.file_foreach_chunk(buffer, total_size, nchunks, [] (const uintptr_t a, const uintptr_t b) {
            runtime.task_spawn<1>(
                [a, b] (task_t * task, access_t * accesses) {
                    access_t * access = accesses + 0;
                    new (access) access_t(task, a, b, ACCESS_MODE_R);
                },

                [a, b] (task_t * task) {
                    LOGGER_INFO("File chunk [%lu, %lu] is read", a, b);
                    for (uintptr_t x = a ; x < b ; ++x)
                        assert(*((unsigned char *) x) == 1);
                }
            );
        }
    );

    /* wait for all tasks completion */
    runtime.task_wait();

    # if 0
    uint64_t tf = get_nanotime();
    double dt_ns = (double) (tf - t0);
    LOGGER_INFO("Read with %.2lf GB/s", total_size/dt_ns);
    # endif

    close(fd);
    if (remove(filename) == 0)
        LOGGER_INFO("Deleted file: %s", filename);
    else
        perror("remove");

    assert(runtime.deinit() == 0);

    return 0;
}
