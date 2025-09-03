# include <assert.h>
# include <xkrt/xkrt.h>

static void
f(xkrt_task_t * task, void * data)
{
    *((int *) data) = 42;
}

int
main(void)
{
    xkrt_runtime_t runtime;

    assert(xkrt_init   (&runtime) == 0);

    int x = 0;
    xkrt_task_spawn(&runtime, f, &x);
    xkrt_task_wait(&runtime);

    assert(xkrt_deinit (&runtime) == 0);

    assert(x == 42);

    return 0;
}
