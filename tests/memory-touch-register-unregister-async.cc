/* ************************************************************************** */
/*                                                                            */
/*   touch-register-unregister-async.cc                                       */
/*                                                                   .-*-.    */
/*   Author: Romain PEREIRA <rpereira@anl.gov>                     .'* *.'    */
/*                                                              __/_*_*(_     */
/*   Created: 2025/03/03 01:28:08 by Romain PEREIRA            / _______ \    */
/*   Updated: 2025/06/02 20:11:29 by Romain PEREIRA            \_)     (_/    */
/*                                                                            */
/*   License: ???                                                             */
/*                                                                            */
/* ************************************************************************** */

# include <random>

# include <xkrt/runtime.h>
# include <xkrt/logger/logger.h>
# include <xkrt/logger/metric.h>

XKRT_NAMESPACE_USE;

int
main(int argc, char ** argv)
{
    runtime_t runtime;

    if (runtime.init())
        LOGGER_FATAL("ERROR INIT");

    driver_t * driver = runtime.driver_get(XKRT_DRIVER_TYPE_HOST);
    assert(driver);

    std::mt19937 rng(std::random_device{}());
    int done[3] = {0, 0, 0};

    if (argc == 2)
    {
        done[0] = 1;
        done[1] = 1;
        done[2] = 1;
        done[atoi(argv[1])] = 0;
    }

    int dumped = 0;

    while (done[0] + done[1] + done[2] < 3)
    {
        int i;
        do { i = rng() % 3; } while (done[i]);
        done[i] = 1;

        LOGGER_INFO("------------------------------");

        # include "memory-register-async.conf.cc"
        if (dumped == 0)
        {
            LOGGER_INFO("Size is %.1f GB in %zu chunks using %u threads",
                size/1e9, nchunks, team->priv.nthreads);
            dumped = 1;
        }

        uint64_t t0 = get_nanotime();

        if (i == 0 || i == 1)
        {
            if (i == 0)
            {
                LOGGER_INFO("Running with touch>sync>register>sync>unregister>sync");

                uint64_t t0 = get_nanotime();
                runtime.memory_touch_async(team, ptr, size, nchunks);
                runtime.task_wait();
                uint64_t tf = get_nanotime();
                LOGGER_INFO("      Touch took %lf s.", (tf - t0) / 1e9);
            }
            else if (i == 1)
            {
                LOGGER_INFO("Running with register>sync>unregister>sync");
            }

            {
                uint64_t t0 = get_nanotime();
                runtime.memory_register_async(team, ptr, size, nchunks);
                runtime.task_wait();
                uint64_t tf = get_nanotime();
                LOGGER_INFO("    Pinning took %lf s.", (tf - t0) / 1e9);
            }
        }
        else if (i == 2)
        {
            LOGGER_INFO("Running with touch>register>sync>unregister>sync");
            uint64_t t0 = get_nanotime();
            runtime.memory_touch_async(team, ptr, size, nchunks);
            runtime.memory_register_async(team, ptr, size, nchunks);
            runtime.task_wait();
            uint64_t tf = get_nanotime();
            LOGGER_INFO("  Touch+Pin took %lf s.", (tf - t0) / 1e9);
        }

        {
            uint64_t t0 = get_nanotime();
            runtime.memory_unregister_async(team, ptr, size, nchunks);
            runtime.task_wait();
            uint64_t tf = get_nanotime();
            LOGGER_INFO("  Unpinning took %lf s.", (tf - t0) / 1e9);
        }

        uint64_t tf = get_nanotime();
        LOGGER_INFO("Total took %lf s.", (tf - t0) / 1e9);
    }

    if (runtime.deinit())
        LOGGER_FATAL("ERROR DEINIT");

    return 0;
}
