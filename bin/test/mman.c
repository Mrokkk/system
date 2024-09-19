#include "mman.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>

TEST_SUITE(mman);

TEST(program_headers_are_immutable)
{
    MAPPINGS_FOR_EACH(m)
    {
        if (!m->path || *m->path == '[')
        {
            continue;
        }
        EXPECT_EQ(MPROTECT(m->ptr, m->size, PROT_NONE), -1);
        EXPECT_EQ(errno, EPERM);
    }
}

TEST(mmap_fail)
{
    EXPECT_EQ(mmap(PTR(0x20), 0x1000, PROT_READ, MAP_FIXED | MAP_PRIVATE | MAP_ANONYMOUS, -1, 0), -1);
    EXPECT_EQ(errno, EINVAL);

    EXPECT_EQ(mmap(NULL, 0x1000, PROT_READ, MAP_FIXED | MAP_PRIVATE | MAP_ANONYMOUS, -1, 0), -1);
    EXPECT_EQ(errno, EINVAL);

    EXPECT_EQ(mmap(NULL, 0x1000, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0), -1);
    EXPECT_EQ(errno, EINVAL);

    EXPECT_EQ(mmap(PTR(0xc0000000), 0x1000, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0), -1);
    EXPECT_EQ(errno, EFAULT);
}

TEST(mmap_and_mprotect)
{
    EXPECT_EXIT_WITH(0)
    {
        void* ptr = MUST_SUCCEED(MMAP(NULL, 0x10000, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
        uintptr_t base = ADDR(ptr);

        EXPECT_MAPPING(base, 0x10000, PROT_NONE, 0, NULL);

        MUST_SUCCEED(MPROTECT(ptr + 0x2000, 0x2000, PROT_READ | PROT_WRITE));

        EXPECT_MAPPING(base,          0x2000, PROT_NONE, 0, NULL);
        EXPECT_MAPPING(base + 0x2000, 0x2000, PROT_READ | PROT_WRITE, 0, NULL);
        EXPECT_MAPPING(base + 0x4000, 0xc000, PROT_NONE, 0, NULL);

        MUST_SUCCEED(MPROTECT(ptr, 0x1000, PROT_READ));

        EXPECT_MAPPING(base,          0x1000, PROT_READ, 0, NULL);
        EXPECT_MAPPING(base + 0x1000, 0x1000, PROT_NONE, 0, NULL);
        EXPECT_MAPPING(base + 0x2000, 0x2000, PROT_READ | PROT_WRITE, 0, NULL);
        EXPECT_MAPPING(base + 0x4000, 0xc000, PROT_NONE, 0, NULL);

        MUST_SUCCEED(MPROTECT(ptr + 0xe000, 0x2000, PROT_READ));

        EXPECT_MAPPING_CHECK_ACCESS(base,          0x1000, PROT_READ, 0, NULL);
        EXPECT_MAPPING_CHECK_ACCESS(base + 0x1000, 0x1000, PROT_NONE, 0, NULL);
        EXPECT_MAPPING_CHECK_ACCESS(base + 0x2000, 0x2000, PROT_READ | PROT_WRITE, 0, NULL);
        EXPECT_MAPPING_CHECK_ACCESS(base + 0x4000, 0xa000, PROT_NONE, 0, NULL);
        EXPECT_MAPPING_CHECK_ACCESS(base + 0xe000, 0x2000, PROT_READ, 0, NULL);

        MUST_SUCCEED(MPROTECT(ptr, 0x10000, PROT_NONE));

        EXPECT_MAPPING(base, 0x10000, PROT_NONE, 0, NULL);

        exit(FAILED_EXPECTATIONS());
    }
}

TEST(oom)
{
    EXPECT_KILLED_BY(SIGKILL)
    {
        const size_t size = 0x50000000;
        void* base = MUST_SUCCEED(MMAP(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
        uint8_t* end = base + size;

        for (uint8_t* ptr = base; ADDR(ptr) < ADDR(end); ptr += 0x1000)
        {
            *ptr = 32;
        }

        exit(0);
    }
}

__attribute__((naked)) void dummy_fn(const char*, size_t)
{
    asm volatile(
        "mov $1, %eax;"
        "ret;"
    );
}

static bool is_segmexec_disabled(void)
{
    char buffer[16];
    const char* path = "/sys/segmexec";
    int fd = open(path, O_RDONLY);

    if (UNLIKELY(fd == -1))
    {
        FAIL("cannot open %s: %s", path, strerror(errno));
        return false;
    }

    int bytes = read(fd, buffer, sizeof(buffer));
    close(fd);

    if (UNLIKELY(bytes == -1))
    {
        FAIL("cannot read %s: %s", path, strerror(errno));
        return false;
    }

    if (UNLIKELY(bytes == sizeof(buffer)))
    {
        FAIL("%s is too big", path);
        return false;
    }

    return *buffer == '0';
}

TEST(stack_exec)
{
    SKIP_WHEN(is_segmexec_disabled());

    EXPECT_KILLED_BY(SIGSEGV)
    {
        const char* const message = "Hello there!\n";
        uint8_t buffer[0x28];
        int (*function_on_stack)() = PTR(buffer);

        memcpy(buffer, &dummy_fn, sizeof(buffer));
        function_on_stack(message, __builtin_strlen(message));

        exit(0);
    }
}

TEST(syscall_permissions)
{
    EXPECT_KILLED_BY(SIGABRT)
    {
        asm volatile(
            "mov $24, %eax;"
            "int $0x80"
        );
        exit(0);
    }
}

TEST_SUITE_END(mman);
