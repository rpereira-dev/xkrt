/* ************************************************************************** */
/*                                                                            */
/*   file-read.cc                                                 .-*-.       */
/*                                                              .'* *.'       */
/*   Created: 2025/02/11 14:59:33 by Romain PEREIRA          __/_*_*(_        */
/*   Updated: 2025/06/20 21:27:50 by Romain PEREIRA         / _______ \       */
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

# include <xkrt/xkrt.h>
# include <xkrt/logger/logger.h>
# include <xkrt/logger/metric.h>

constexpr const char * filename = "file.bin";             // filename
constexpr size_t buffer_size = (1024 * 1024);             // 1MB
//constexpr size_t total_size  = (1L * 1024 * 1024 * 1024); // 1GB
constexpr size_t total_size  = (1L * 1024 * 1024); // 1MB
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

    runtime.file_read_async(fd, buffer, total_size, nchunks);
    for (int i = 0 ; i < nchunks ; ++i)
    {
        runtime.task_spawn<1>(
            [&i] (task_t * task) {
                LOGGER_INFO("Chunk %d is read");
            },

            // TODO
            [] (access_t * accesses) {
                access_t * access = accesses + 0;
                new (access) access_t();
            }
        );
    }
    // TODO : spawn successor tasks on each chunks
    runtime.task_wait();

    close(fd);
    if (remove(filename) == 0)
        LOGGER_INFO("Deleted file: %s", filename);
    else
        perror("remove");

    assert(xkrt_deinit(&runtime) == 0);

    return 0;
}
