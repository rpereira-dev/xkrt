Examples
==============

C++
-------------------

Please refer to **include/xkrt/runtime.h** for more details.

Forking teams of threads
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: c++

    # ifndef _GNU_SOURCE
    #  define _GNU_SOURCE
    # endif
    # include <sched.h>

    # include <xkrt/xkrt.h>

    using namespace xkrt;

    int main(void)
    {
        // Initialize the runtime
        runtime_t runtime;
        runtime.init();

        // Create a team of threads that will run parallel-fors on all available cpus
        {
            // Describe the team
            team_t team = XKRT_TEAM_STATIC_INITIALIZER;
            team.desc.routine = XKRT_TEAM_ROUTINE_PARALLEL_FOR;

            // Spawn threads and run
            runtime.team_create(&team);
            runtime.team_parallel_for(&team, [&counter] (team_t * team, thread_t * thread) {
                printf("Thread `%3d` running on `sched_getcpu() -> %3d`", thread->tid, sched_getcpu());
            });
            runtime.team_join(&team);
        }

        // Create a team with 1x threads per device
        {
            // Fork 1 new thread for each device but the host device
            team_t team = XKRT_TEAM_STATIC_INITIALIZER;
            team.desc.routine           = func;
            team.desc.nthreads          = runtime.drivers.devices.n - 1;
            team.desc.binding.mode      = XKRT_TEAM_BINDING_MODE_COMPACT;
            team.desc.binding.places    = XKRT_TEAM_BINDING_PLACES_DEVICE;
            team.desc.binding.flags     = XKRT_TEAM_BINDING_FLAG_EXCLUDE_HOST;
            team.desc.master_is_member  = false;

            runtime.team_create(&team);
            runtime.team_join(&team);
        }

        // Deinitialize the runtime
        runtime.deinit();
        return 0;
    }

Spawning tasks
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: c++

    # include <xkrt/xkrt.h>

    int main(void)
    {
        // Initialize the runtime
        runtime_t runtime;
        runtime.init();

        void * a = 0x1234;
        void * b = 0x5678;

        // spawn a task with 1x access that reads segment [a..b]
        runtime.task_spawn<1>(
            [a, b] (task_t * task, access_t * accesses) {
                new (access) access_t(task, a, b, ACCESS_MODE_R);
            },

            [] (task_t * task) {
                puts("Segment [a..b] is ready, and task executed");
            }
        );

        // Deinitialize the runtime
        runtime.deinit();
        return 0;
    }

C
---------------------

The C API follows the C++ api prefixing types and methods with `xkrt_`.

.. code-block:: c

    # include <xkrt/xkrt.h>

    static void f(xkrt_task_t * task, void * data)
    {
        *((int *) data) = 42;
    }

    int main(void)
    {
        xkrt_runtime_t runtime;

        xkrt_init(&runtime);

        int x = 0;
        xkrt_task_spawn(&runtime, f, &x);
        xkrt_task_wait(&runtime);

        xkrt_deinit(&runtime);

        assert(x == 42);

        return 0;
    }


Julia
---------------------
TODO
