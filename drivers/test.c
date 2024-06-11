#define log_fmt(fmt) "test: " fmt
#include <kernel/init.h>
#include <kernel/kernel.h>
#include <kernel/module.h>
#include <kernel/process.h>

int a = 2;

static int test_init(void);
static int test_deinit(void);

KERNEL_MODULE(test);
module_init(test_init);
module_exit(test_deinit);

static int test_init(void)
{
    file_t* stdout;

    log_info("hello world: %u, %s, %u", a, cmdline, jiffies);

    if (process_fd_get(process_current, 1, &stdout))
    {
        return -1;
    }

    const char* message = "Hello there!\n";
    stdout->ops->write(stdout, message, strlen(message));

    return 0;
}

static int test_deinit(void)
{
    log_info("goodbye!");
    return 0;
}
