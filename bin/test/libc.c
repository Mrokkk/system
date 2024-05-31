#include "test.h"
#include <stdlib.h>
#include <string.h>

static int data;

TEST_SUITE(libc);

TEST(bss_is_zeroed)
{
    EXPECT_EQ(data, 0);
}

TEST(strspn)
{
    char buf[128];
    {
        const char* string = "test123_test456\xff";
        strcpy(buf, string);
    }

    EXPECT_EQ(strspn(buf, "_"), 0);
    EXPECT_EQ(strspn(buf, "t"), 1);
    EXPECT_EQ(strspn(buf, "e"), 0);
    EXPECT_EQ(strspn(buf, "est"), 4);
    EXPECT_EQ(strspn(buf, "est_"), 4);
    EXPECT_EQ(strspn(buf, "test"), 4);
    EXPECT_EQ(strspn(buf, "tsea"), 4);
    EXPECT_EQ(strspn(buf, "ste1"), 5);
    EXPECT_EQ(strspn(buf, "456est123_"), 15);
    EXPECT_EQ(strspn(buf, "\xff" "456est123_"), 16);
}

TEST(atoi)
{
    char buf[128];
    {
        sprintf(buf, "154");
        EXPECT_EQ(atoi(buf), 154);
    }
    {
        sprintf(buf, "    4952tkskg ");
        EXPECT_EQ(atoi(buf), 4952);
    }
    {
        sprintf(buf, "abc");
        EXPECT_EQ(atoi(buf), 0);
    }
    {
        sprintf(buf, "z930");
        EXPECT_EQ(atoi(buf), 0);
    }
    {
        buf[0] = 0;
        EXPECT_EQ(atoi(buf), 0);
    }
    {
        sprintf(buf, "-1");
        EXPECT_EQ(atoi(buf), -1);
    }
    {
        sprintf(buf, "-23450");
        EXPECT_EQ(atoi(buf), -23450);
    }
    {
        sprintf(buf, "%d", INT32_MIN);
        EXPECT_EQ(atoi(buf), INT32_MIN);
    }
    {
        sprintf(buf, "40934950");
        EXPECT_EQ(atoi(buf), 40934950);
    }
}

TEST_SUITE_END(libc);
