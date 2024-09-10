#include <stdlib.h>

#include "mman.h"
#include "test.h"

TEST_SUITE(malloc);

#define ENSURE(x) \
    ({ \
        typeof(x) val = x; \
        if (UNLIKELY(!val)) \
        { \
            FAIL(#x " is null"); \
            exit(FAILED_EXPECTATIONS()); \
        } \
        val; \
    })

DIAG_IGNORE("-Wanalyzer-double-free");

TEST(small_allocation)
{
    EXPECT_EXIT_WITH(0)
    {
        uintptr_t* mem = ENSURE(malloc(42));

        *mem = 345;
        EXPECT_EQ(*mem, 345);

        uintptr_t* mem2 = ENSURE(malloc(42));

        *mem2 = 25;
        EXPECT_EQ(*mem2, 25);

        uintptr_t* mem3 = ENSURE(malloc(16384));

        *mem3 = 94853;
        EXPECT_EQ(*mem3, 94853);

        // Not aligned ptr
        EXPECT_KILLED_BY(SIGABRT)
        {
            free(SHIFT(mem, 1));
            exit(0);
        }

        free(mem);
        free(mem2);
        free(mem3);

        // Double frees
        EXPECT_KILLED_BY(SIGABRT)
        {
            free(mem);
            exit(0);
        }

        EXPECT_KILLED_BY(SIGABRT)
        {
            free(mem2);
            exit(0);
        }

        EXPECT_KILLED_BY(SIGABRT)
        {
            free(mem2);
            exit(0);
        }

        DIAG_IGNORE("-Wanalyzer-malloc-leak");
        for (int i = 0; i < 100000; ++i)
        {
            uintptr_t* ptr;
            if (!(ptr = malloc(16)))
            {
                FAIL("cannot allocate %u", i);
                break;
            }
            *ptr = 2;
        }
        DIAG_RESTORE();

        exit(FAILED_EXPECTATIONS());
    }
}

TEST(large_allocation)
{
    EXPECT_EXIT_WITH(0)
    {
        uintptr_t* mem = ENSURE(malloc(0x20000));

        *mem = 345;
        EXPECT_EQ(*mem, 345);

        mappings_read(LOCATION());
        EXPECT_MAPPING(ADDR(mem) - 0x1000, 0x1000, PROT_NONE, 0, NULL);
        EXPECT_MAPPING(ADDR(mem), 0x20000, PROT_READ | PROT_WRITE, 0, NULL);
        EXPECT_MAPPING(ADDR(mem) + 0x20000, 0x1000, PROT_NONE, 0, NULL);

        free(mem);

        mappings_read(LOCATION());
        EXPECT_NO_MAPPING(ADDR(mem) - 0x1000, 0x22000);

        EXPECT_KILLED_BY(SIGABRT)
        {
            free(mem);
            exit(0);
        }

        exit(FAILED_EXPECTATIONS());
    }
}

TEST(realloc_small_to_small)
{
    EXPECT_EXIT_WITH(0)
    {
        uintptr_t* mem = ENSURE(malloc(9));

        *mem = 49282;
        EXPECT_EQ(*mem, 49282);

        uintptr_t* new_mem = ENSURE(realloc(mem, 257));

        EXPECT_EQ(*new_mem, 49282);
        EXPECT_NE(new_mem, mem);

        free(new_mem);

        EXPECT_KILLED_BY(SIGABRT)
        {
            free(mem);
            exit(0);
        }

        exit(FAILED_EXPECTATIONS());
    }
}

TEST(realloc_small_to_small_same_size_class)
{
    EXPECT_EXIT_WITH(0)
    {
        uintptr_t* mem = ENSURE(malloc(8));

        *mem = 49282;
        EXPECT_EQ(*mem, 49282);

        uintptr_t* new_mem = ENSURE(realloc(mem, 15));

        EXPECT_EQ(*new_mem, 49282);
        EXPECT_EQ(new_mem, mem);

        free(new_mem);

        exit(FAILED_EXPECTATIONS());
    }
}

TEST(realloc_small_to_small_smaller_size_class)
{
    EXPECT_EXIT_WITH(0)
    {
        uintptr_t* mem = ENSURE(malloc(2048));

        *mem = 49282;
        EXPECT_EQ(*mem, 49282);

        uintptr_t* new_mem = ENSURE(realloc(mem, 64));

        EXPECT_EQ(*new_mem, 49282);
        EXPECT_NE(new_mem, mem);

        exit(FAILED_EXPECTATIONS());
    }
}

TEST(realloc_small_to_large)
{
    EXPECT_EXIT_WITH(0)
    {
        uintptr_t* mem = ENSURE(malloc(1024));

        *mem = 49282;
        EXPECT_EQ(*mem, 49282);

        uintptr_t* new_mem = ENSURE(realloc(mem, 0x30000));

        EXPECT_EQ(*new_mem, 49282);
        EXPECT_NE(new_mem, mem);

        free(new_mem);

        EXPECT_KILLED_BY(SIGABRT)
        {
            free(mem);
            exit(0);
        }

        exit(FAILED_EXPECTATIONS());
    }
}

TEST(realloc_large_to_small)
{
    EXPECT_EXIT_WITH(0)
    {
        uintptr_t* mem = ENSURE(malloc(1024));

        *mem = 49282;
        EXPECT_EQ(*mem, 49282);

        uintptr_t* new_mem = ENSURE(realloc(mem, 0x30000));

        EXPECT_EQ(*new_mem, 49282);
        EXPECT_NE(new_mem, mem);
        EXPECT_EQ(ADDR(new_mem) % 0x1000, 0);

        free(new_mem);

        EXPECT_KILLED_BY(SIGABRT)
        {
            free(mem);
            exit(0);
        }

        exit(FAILED_EXPECTATIONS());
    }
}

DIAG_RESTORE();

TEST_SUITE_END(malloc);
