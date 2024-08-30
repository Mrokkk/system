#include "mman.h"

#include <errno.h>

TEST_SUITE(mman);

TEST(mmap_fail)
{
    EXPECT_EQ(mmap((void*)0x20, 0x1000, PROT_READ, MAP_FIXED | MAP_PRIVATE | MAP_ANONYMOUS, -1, 0), -1);
    EXPECT_EQ(errno, EINVAL);

    EXPECT_EQ(mmap(NULL, 0x1000, PROT_READ, MAP_FIXED | MAP_PRIVATE | MAP_ANONYMOUS, -1, 0), -1);
    EXPECT_EQ(errno, EINVAL);

    EXPECT_EQ(mmap(NULL, 0x1000, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0), -1);
    EXPECT_EQ(errno, EINVAL);
}

TEST(mmap_and_mprotect)
{
    EXPECT_EXIT_WITH(0)
    {
        void* ptr = MUST_SUCCEED(MMAP(NULL, 0x10000, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
        uintptr_t base = ADDR(ptr);

        EXPECT_MAPPING(base, 0x10000, PROT_NONE);

        MUST_SUCCEED(MPROTECT(ptr + 0x2000, 0x2000, PROT_READ | PROT_WRITE));

        EXPECT_MAPPING(base,          0x2000, PROT_NONE);
        EXPECT_MAPPING(base + 0x2000, 0x2000, PROT_READ | PROT_WRITE);
        EXPECT_MAPPING(base + 0x4000, 0xc000, PROT_NONE);

        MUST_SUCCEED(MPROTECT(ptr, 0x1000, PROT_READ));

        EXPECT_MAPPING(base,          0x1000, PROT_READ);
        EXPECT_MAPPING(base + 0x1000, 0x1000, PROT_NONE);
        EXPECT_MAPPING(base + 0x2000, 0x2000, PROT_READ | PROT_WRITE);
        EXPECT_MAPPING(base + 0x4000, 0xc000, PROT_NONE);

        MUST_SUCCEED(MPROTECT(ptr + 0xe000, 0x2000, PROT_READ));

        EXPECT_MAPPING_CHECK_ACCESS(base,          0x1000, PROT_READ);
        EXPECT_MAPPING_CHECK_ACCESS(base + 0x1000, 0x1000, PROT_NONE);
        EXPECT_MAPPING_CHECK_ACCESS(base + 0x2000, 0x2000, PROT_READ | PROT_WRITE);
        EXPECT_MAPPING_CHECK_ACCESS(base + 0x4000, 0xa000, PROT_NONE);
        EXPECT_MAPPING_CHECK_ACCESS(base + 0xe000, 0x2000, PROT_READ);

        MUST_SUCCEED(MPROTECT(ptr, 0x10000, PROT_NONE));

        EXPECT_MAPPING(base, 0x10000, PROT_NONE);

        exit(FAILED_EXPECTATIONS());
    }
}

TEST(oom)
{
    EXPECT_KILLED_BY(SIGKILL)
    {
        const size_t size = 0xb0000000;
        void* base = MUST_SUCCEED(MMAP(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
        uint8_t* end = base + size;

        for (uint8_t* ptr = base; ADDR(ptr) < ADDR(end); ptr += 0x1000)
        {
            *ptr = 32;
        }

        exit(0);
    }
}

TEST_SUITE_END(mman);
