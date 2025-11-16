#include <iostream>
#include <fstream>
#include <chrono>
#include <fcntl.h>
#include <unistd.h>
#include <vector>
#include <cmath>

#include <xkrt/runtime.h>

XKRT_NAMESPACE_USE;

// Helper to compute mean and standard deviation
void stats(const std::vector<double> &v, double &mean, double &stdev) {
    mean = 0.0;
    int n = v.size();
    for (double x : v)
    {
        LOGGER_INFO("%lf", x);
        mean += x;
    }
    mean /= n;
    stdev = 0.0;
    for (double x : v)
        stdev += (x - mean) * (x - mean);
    stdev = std::sqrt(stdev / n);
}

double median(std::vector<double> &nums) {
    if (nums.empty()) {
        throw std::runtime_error("List is empty");
    }

    std::sort(nums.begin(), nums.end()); // sort the list

    size_t n = nums.size();
    if (n % 2 == 1) {
        // odd number of elements, return the middle one
        return nums[n / 2];
    } else {
        // even number of elements, return the average of the two middle ones
        return (nums[n / 2 - 1] + nums[n / 2]) / 2.0;
    }
}

int main(int argc, char **argv) {
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << " <filename> [size_in_bytes] [ntasks] \n";
        return 1;
    }

    const char *filename = argv[1];
    size_t size = std::stoul(argv[2]);
    const int runs = 10;
    int NTASKS = atoi(argv[3]);

    // Allocate page-aligned host buffer
    void *buffer = nullptr;
    posix_memalign(&buffer, 4096, size);
    if (!buffer) {
        std::cerr << "Failed to allocate host buffer\n";
        return 1;
    }
    memset(buffer, 0, size);

    // Open file for reading
    int fd = open(filename, O_RDONLY);
    if (fd < 0) {
        perror("open");
        return 1;
    }

    runtime_t runtime;
    runtime.init();

    const device_global_id_t device_global_id = 1;

    // Vectors to store results
    std::vector<double> t_alloc, t_read, t_register, t_memcpy, t_total;

    for (int iter = 0; iter <= runs ; ++iter) {

        // invalidate caches
        runtime.reset();

        // allocate device memory
        area_chunk_t * chunk = runtime.memory_device_allocate(device_global_id, size);
        assert(chunk);
        assert(chunk->ptr);

        // --- 1. read ---
        lseek(fd, 0, SEEK_SET);
        runtime.file_read_async(fd, buffer, size, NTASKS);
        // read(fd, buffer, size);

        // --- 2. register ---
        runtime.memory_register_async(buffer, size, NTASKS);
        runtime.task_wait();
        auto t_start_total = std::chrono::high_resolution_clock::now();

        // --- 3. H2D ---
        const uintptr_t dst_device_addr = (const uintptr_t) chunk->ptr;
        const uintptr_t src_device_addr = (const uintptr_t) buffer;
        runtime.memory_copy_async(device_global_id, size, device_global_id, dst_device_addr, HOST_DEVICE_GLOBAL_ID, src_device_addr, NTASKS);
        runtime.task_wait();
        auto t_end_total = std::chrono::high_resolution_clock::now();

        // sync
        runtime.task_wait();

        // Compute durations
        double total_ms = std::chrono::duration<double, std::milli>(t_end_total - t_start_total).count();

        runtime.memory_unregister(buffer, size);
        runtime.memory_device_deallocate(device_global_id, chunk);

        if (iter == 0)
            continue ;

        // Store times
        t_total.push_back(total_ms);
    }

    // Compute stats
    double mean_total, stdev_total;

    stats(t_total, mean_total, stdev_total);

    // Print results
    std::cout << "Benchmark results (size = " << size / (1024.0 * 1024.0) << " MB, "
              << runs << " runs):\n";
    std::cout << "  Total time          : avg = " << mean_total << " ms, stdev = " << stdev_total << " ms\n";
    std::cout << "  Total time          : med = " << median(t_total) <<  " ms\n";

    // Cleanup
    free(buffer);
    close(fd);

    runtime.deinit();
    return 0;
}
