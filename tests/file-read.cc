/* ************************************************************************** */
/*                                                                            */
/*   file-read.cc                                                 .-*-.       */
/*                                                              .'* *.'       */
/*   Created: 2025/02/11 14:59:33 by Romain PEREIRA          __/_*_*(_        */
/*   Updated: 2025/06/23 15:37:21 by Romain PEREIRA         / _______ \       */
/*                                                          \_)     (_/       */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/*   Author: Thierry GAUTIER <thierry.gautier@inrialpes.fr>                   */
/*   Author: Romain PEREIRA <rpereira@anl.gov>                                */
/*                                                                            */
/*   Copyright: see AUTHORS                                                   */
/*                                                                            */
/* ************************************************************************** */

# include <fcntl.h>
# include <unistd.h>
# include <stdlib.h>
# include <stdio.h>
# include <string.h>
# include <errno.h>

# include <new>

# include <xkrt/xkrt.h>
# include <xkrt/logger/logger.h>
# include <xkrt/logger/metric.h>

constexpr const char * filename = "file.bin";             // filename
constexpr size_t buffer_size = (1024 * 1024);             // 1MB
// constexpr size_t total_size  = (1L * 1024 * 1024 * 1024); // 1GB
constexpr size_t total_size  = (8L * 1024 * 1024);      // 8MB
constexpr int    nchunks      = 4;                        // tasks that reads the file

static xkrt_runtime_t runtime;

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

    assert(xkrt_init(&runtime) == 0);

    unsigned char * buffer = (unsigned char *) malloc(total_size);
    assert(buffer);

    int fd = open(filename, O_RDONLY, 0644);
    if (fd < 0)
    {
        perror("Failed to open file");
        exit(EXIT_FAILURE);
    }

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
                    LOGGER_INFO("Chunk [%lu, %lu] is read", a, b);
                    for (uintptr_t x = a ; x < b ; ++x)
                        assert(*((unsigned char *) x) == 1);
                }
            );
        }
    );

    /* wait for all tasks completion */
    runtime.task_wait();

    close(fd);
    if (remove(filename) == 0)
        LOGGER_INFO("Deleted file: %s", filename);
    else
        perror("remove");

    assert(xkrt_deinit(&runtime) == 0);

    return 0;
}
