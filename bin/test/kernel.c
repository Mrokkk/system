#include <errno.h>
#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>

#include "test.h"

static int data;
static int data2 = 13;
static int data3[1028];
static int sigreceived;

static void sighan()
{
    sigreceived = 1;
}

TEST_SUITE(kernel);

#define TEST_CRASH_WRITE_ADDRESS(name, address) \
    TEST(name) \
    { \
        EXPECT_KILLED_BY(SIGSEGV) \
        { \
            int* ptr = (int*)address; \
            *ptr = 2; \
            exit(0); \
        } \
    }

#define TEST_CRASH_READ_ADDRESS(name, address) \
    TEST(name) \
    { \
        EXPECT_KILLED_BY(SIGSEGV) \
        { \
            int* ptr = (int*)address; \
            printf("%u\n", *ptr); \
            exit(0); \
        } \
    }

TEST(sse)
{
    EXPECT_EXIT_WITH(154)
    {
        asm volatile(
            "movapd %0, %%xmm0"
            :: "m" (data3)
        );
        exit(154);
    }
}

TEST(getpid)
{
    EXPECT_GT(getpid(), 0);
}

TEST(getppid)
{
    int parent_pid = getpid();
    EXPECT_GT(getppid(), 0);
    EXPECT_EXIT_WITH(0)
    {
        EXPECT_EQ(getppid(), parent_pid);
        exit(FAILED_EXPECTATIONS());
    }
}

TEST(getdents_bad_ptr1)
{
    EXPECT_EXIT_WITH(0)
    {
        char buf[128];
        int dirfd, count;

        getcwd(buf, 128);

        dirfd = open(buf, O_RDONLY | O_DIRECTORY, 0);

        if (dirfd < 0)
        {
            printf("failed open %d\n", dirfd);
        }

        count = getdents(dirfd, (void*)0xc0000000, 0x1000);

        EXPECT_EQ(count, -1);
        EXPECT_EQ(errno, EFAULT);

        exit(FAILED_EXPECTATIONS());
    }
}

TEST(getdents_bad_ptr2)
{
    EXPECT_EXIT_WITH(0)
    {
        char buf[128];
        int dirfd, count;

        getcwd(buf, 128);

        dirfd = open(buf, O_RDONLY | O_DIRECTORY, 0);

        if (dirfd < 0)
        {
            printf("failed open %d\n", dirfd);
        }

        count = getdents(dirfd, (void*)0xcfffff00, 0x1000);

        EXPECT_EQ(count, -1);
        EXPECT_EQ(errno, EFAULT);

        exit(FAILED_EXPECTATIONS());
    }
}

TEST(signal)
{
    int status, pid = fork();
    if (pid > 0)
    {
        waitpid(pid, &status, WUNTRACED);
        kill(pid, SIGUSR1);
        kill(pid, SIGCONT);
        waitpid(pid, &status, 0);
        EXPECT_GT(WIFEXITED(status), 0);
        EXPECT_EQ(WEXITSTATUS(status), 0);
    }
    else
    {
        signal(SIGUSR1, &sighan);
        kill(getpid(), SIGSTOP);
        EXPECT_EQ(sigreceived, 1);
        exit(FAILED_EXPECTATIONS());
    }
}

int received;

void handler(int sig)
{
    received |= 1 << sig;
    ++received;
    switch (sig)
    {
        case SIGUSR1:
            raise(SIGUSR2);
            break;
        case SIGUSR2:
            raise(SIGPIPE);
            break;
    }
}

TEST(signal_nested)
{
    EXPECT_EXIT_WITH(0)
    {
        signal(SIGUSR1, &handler);
        signal(SIGUSR2, &handler);
        signal(SIGPIPE, &handler);
        raise(SIGUSR1);
        EXPECT_EQ(received, 3 | (1 << SIGUSR1) | (1 << SIGUSR2) | (1 << SIGPIPE));
        exit(FAILED_EXPECTATIONS());
    }
}

TEST(copy_on_write)
{
    data = 93;
    data3[1027] = 2221;
    EXPECT_EXIT_WITH(0)
    {
        data = 58;
        data3[1027] = 29304;
        EXPECT_EQ(data, 58);
        EXPECT_EQ(data3[1027], 29304);
        exit(FAILED_EXPECTATIONS());
    }
    EXPECT_EQ(data, 93);
    EXPECT_EQ(data3[1027], 2221);
}

TEST(sbrk1)
{
    EXPECT_EXIT_WITH(0)
    {
        uint8_t* data = sbrk(0x333);
        data[2] = 29;
        EXPECT_EQ(data[2], 29);
        exit(FAILED_EXPECTATIONS());
    }
}

TEST(sbrk2)
{
    EXPECT_EXIT_WITH(0)
    {
        uint8_t* data = sbrk(0x1000);
        data[2] = 29;
        EXPECT_EQ(data[2], 29);

        uint8_t* data2 = sbrk(0x1000);

        data2[0xfff] = 28;
        EXPECT_EQ(data2[0xfff], 28);

        EXPECT_EQ((uint32_t)data2 - (uint32_t)data, 0x1000);

        uint8_t* data3 = sbrk(0x10000);
        data3[2] = 254;
        data3[0x10000 - 1] = 34;
        EXPECT_EQ(data3[2], 254);
        EXPECT_EQ(data3[0x10000 - 1], 34);

        exit(FAILED_EXPECTATIONS());
    }
}

TEST(sbrk3)
{
    EXPECT_KILLED_BY(SIGSEGV)
    {
        uint32_t* data = sbrk(0) - 4;
        *data = 2;
        sbrk(-0x1000);
        *data = 29;
        exit(FAILED_EXPECTATIONS());
    }
}

TEST(own_copy_of_program)
{
    EXPECT_EQ(data2, 13);
    data = 34;
    EXPECT_EXIT_WITH(0)
    {
        char* argv[] = {"/bin/test", "--test=modify_data", 0};
        close(STDOUT_FILENO);
        close(STDERR_FILENO);
        execvp(argv[0], argv);
        exit(1);
    }
    EXPECT_EQ(data, 34);
    EXPECT_EQ(data2, 13);
}

TEST(modify_data)
{
    EXPECT_EQ(data2, 13);
    data2 = 99;
    EXPECT_EQ(data2, 99);
}

TEST_CRASH_READ_ADDRESS(crash_read_nullptr, 0)
TEST_CRASH_READ_ADDRESS(crash_read_kernel_space_1, 0xc0000000)
TEST_CRASH_READ_ADDRESS(crash_read_kernel_space_2, 0xc0010000)
TEST_CRASH_READ_ADDRESS(crash_read_kernel_space_3, 0xc0020000)
TEST_CRASH_READ_ADDRESS(crash_read_kernel_space_4, 0xc011759c)
TEST_CRASH_READ_ADDRESS(crash_read_kernel_space_5, 0xfffff000)

TEST_CRASH_WRITE_ADDRESS(crash_write_nullptr, 0)
TEST_CRASH_WRITE_ADDRESS(crash_write_code, 0x1000)
TEST_CRASH_WRITE_ADDRESS(crash_write_kernel_space_1, 0xc0000000)
TEST_CRASH_WRITE_ADDRESS(crash_write_kernel_space_2, 0xc0010000)
TEST_CRASH_WRITE_ADDRESS(crash_write_kernel_space_3, 0xc0020000)
TEST_CRASH_WRITE_ADDRESS(crash_write_kernel_space_4, 0xc011759c)

TEST(bad_syscall)
{
    int ret = syscall(999, 0, 0);
    EXPECT_EQ(ret, -1);
    EXPECT_EQ(errno, ENOSYS);
}

TEST_SUITE_END(kernel);
