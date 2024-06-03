#include "test.h"
#include <stdlib.h>
#include <string.h>

static int data;

TEST_SUITE(libc);

TEST(bss_is_zeroed)
{
    EXPECT_EQ(data, 0);
}

TEST(strlen)
{
    EXPECT_EQ(strlen("test"), 4);
    EXPECT_EQ(strlen(""), 0);
    EXPECT_EQ(strlen("test123"), 7);
}

TEST(strnlen)
{
    EXPECT_EQ(strnlen("test", 4), 4);
    EXPECT_EQ(strnlen("", 4), 0);
    EXPECT_EQ(strnlen("test123", 7), 7);
}

TEST(strcmp)
{
    static const char* string = "test";

    EXPECT_EQ(strcmp(string, "test"), 0);
    EXPECT_EQ(strcmp(string, "tes"), 't');
    EXPECT_EQ(strcmp(string, "est"), 't' - 'e');
    EXPECT_EQ(strcmp(string, "test1"), 0 - '1');
    EXPECT_EQ(strcmp(string, "\0"), 't' - 0);
}

TEST(strncmp)
{
    static const char* string = "test";

    EXPECT_EQ(strncmp(string, "test", 4), 0);
    EXPECT_EQ(strncmp(string, "test", SIZE_MAX), 0);
    EXPECT_EQ(strncmp(string, "tes", SIZE_MAX), 't');
    EXPECT_EQ(strncmp(string, "est", 4), 't' - 'e');
    EXPECT_EQ(strncmp(string, "test1", 5), 0 - '1');
    EXPECT_EQ(strncmp(string, "test1", 4), 0);
}

TEST(strspn)
{
    static const char* string = "test123_test456\xff";

    EXPECT_EQ(strspn(string, "_"), 0);
    EXPECT_EQ(strspn(string, "t"), 1);
    EXPECT_EQ(strspn(string, "e"), 0);
    EXPECT_EQ(strspn(string, "est"), 4);
    EXPECT_EQ(strspn(string, "est_"), 4);
    EXPECT_EQ(strspn(string, "test"), 4);
    EXPECT_EQ(strspn(string, "tsea"), 4);
    EXPECT_EQ(strspn(string, "ste1"), 5);
    EXPECT_EQ(strspn(string, "456est123_"), 15);
    EXPECT_EQ(strspn(string, "\xff" "456est123_"), 16);
}

TEST(strcspn)
{
    static const char* string = "test123_test456\xff";
    EXPECT_EQ(strcspn(string, "e"), 1);
    EXPECT_EQ(strcspn(string, "s"), 2);
    EXPECT_EQ(strcspn(string, "ufw1"), 4);
    EXPECT_EQ(strcspn(string, "_"), 7);
    EXPECT_EQ(strcspn(string, "abc"), 16);
    EXPECT_EQ(strcspn(string, "abc"), 16);
}

TEST(strcpy)
{
    char buf[16] = {0, };
    static const char* string = "test";
    buf[4] = 0xff;
    EXPECT_EQ(strcpy(buf, string), buf);
    EXPECT_EQ(buf[0], 't');
    EXPECT_EQ(buf[1], 'e');
    EXPECT_EQ(buf[2], 's');
    EXPECT_EQ(buf[3], 't');
    EXPECT_EQ(buf[4], 0);
}

TEST(strncpy)
{
    {
        char buf[16] = {0, };
        static const char* string = "test";
        buf[4] = 0xff;
        EXPECT_EQ(strncpy(buf, string, 16), buf);
        EXPECT_EQ(buf[0], 't');
        EXPECT_EQ(buf[1], 'e');
        EXPECT_EQ(buf[2], 's');
        EXPECT_EQ(buf[3], 't');
        EXPECT_EQ(buf[4], 0);
    }
    {
        char buf[16] = {0, };
        static const char* string = "test";
        buf[4] = 0xff;
        EXPECT_EQ(strncpy(buf, string, 3), buf);
        EXPECT_EQ(buf[0], 't');
        EXPECT_EQ(buf[1], 'e');
        EXPECT_EQ(buf[2], 's');
        EXPECT_EQ(buf[3], 0);
        EXPECT_EQ(buf[4], 0xff);
    }
}

TEST(strtok)
{
    char buffer[32];
    __builtin_strcpy(buffer, "this;is;a;;test=kkk");
    EXPECT_EQ(strtok(buffer, ";"), buffer);
    EXPECT_EQ(buffer[0], 't');
    EXPECT_EQ(buffer[3], 's');
    EXPECT_EQ(buffer[4], 0);
    EXPECT_EQ(buffer[5], 'i');
    EXPECT_EQ(strtok(NULL, ";"), buffer + 5);
    EXPECT_EQ(buffer[6], 's');
    EXPECT_EQ(buffer[7], 0);
    EXPECT_EQ(buffer[8], 'a');
    EXPECT_EQ(strtok(NULL, ";"), buffer + 8);
    EXPECT_EQ(buffer[9], 0);
    EXPECT_EQ(buffer[10], ';');
    EXPECT_EQ(buffer[11], 't');
    EXPECT_EQ(strtok(NULL, ";="), buffer + 11);
    EXPECT_EQ(strtok(NULL, ";="), buffer + 16);
    EXPECT_EQ(strtok(NULL, ";="), NULL);
}

TEST(strstr)
{
    const char* string = "test_string_est";
    EXPECT_EQ(strstr(string, "est"), string + 1);
    EXPECT_EQ(strstr(string, "not_existing"), NULL);
    EXPECT_EQ(strstr(string, ""), string);
    EXPECT_EQ(strstr(string, "test_string_est"), string);
    EXPECT_EQ(strstr(string, "test_string_esta"), NULL);
    EXPECT_EQ(strstr(string, "string"), string + 5);
}

TEST(atoi)
{
    EXPECT_EQ(atoi("154"), 154);
    EXPECT_EQ(atoi("    4952tkskg "), 4952);
    EXPECT_EQ(atoi("abc"), 0);
    EXPECT_EQ(atoi("z930"), 0);
    EXPECT_EQ(atoi("\0"), 0);
    EXPECT_EQ(atoi("-1"), -1);
    EXPECT_EQ(atoi("-23450"), -23450);
    EXPECT_EQ(atoi("2147483647"), 2147483647);
    EXPECT_EQ(atoi("-2147483648"), -2147483648);
}

TEST_SUITE_END(libc);
