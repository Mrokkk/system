#include "test.h"

#include <stdio.h>
#include <setjmp.h>
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

TEST(memmove)
{
    char buf[16] = {0, };
    const char* data1 = "test123";
    EXPECT_EQ(memmove(buf, data1, 8), buf);
    EXPECT_STR_EQ(buf, "test123");
    EXPECT_EQ(memmove(buf, buf + 2, 6), buf);
    EXPECT_STR_EQ(buf, "st123");
    memcpy(buf, data1, 8);
    EXPECT_EQ(memmove(buf + 2, buf, 6), buf + 2);
    EXPECT_STR_EQ(buf, "tetest12");
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

TEST(env)
{
    EXPECT_EXIT_WITH(0)
    {
        extern char** environ;

        // Set value with same length
        {
            EXPECT_EQ(setenv("PHOENIX", "TEST", 1), 0);
            EXPECT_STR_EQ(getenv("PHOENIX"), "TEST");
        }

        // Set value with longer length
        {
            char* old_var = getenv("PHOENIX");
            EXPECT_EQ(setenv("PHOENIX", "__PHOENIX__", 1), 0);
            EXPECT_STR_EQ(getenv("PHOENIX"), "__PHOENIX__");
            EXPECT_NE(getenv("PHOENIX"), old_var);
        }

        // Set value for new variable (reallocate environ)
        {
            char** old_environ = environ;
            EXPECT_STR_EQ(getenv("TEST"), NULL);
            EXPECT_EQ(setenv("TEST", "AAABBB", 0), 0);
            EXPECT_STR_EQ(getenv("TEST"), "AAABBB");
            EXPECT_NE(environ, old_environ);
        }

        // Set value for new variable (no reallocation)
        {
            char** old_environ = environ;
            EXPECT_STR_EQ(getenv("TEST2"), NULL);
            EXPECT_EQ(setenv("TEST2", "BBBB", 0), 0);
            EXPECT_STR_EQ(getenv("TEST2"), "BBBB");
            EXPECT_EQ(environ, old_environ);
        }

        // Trigger another reallocation
        {
            char** old_environ = environ;
            for (int i = 0; i < 31; ++i)
            {
                char buf[12];
                sprintf(buf, "TEST%u", i);
                EXPECT_EQ(setenv(buf, "VALUE", 0), 0);
            }
            EXPECT_NE(environ, old_environ);
            EXPECT_STR_EQ(getenv("TEST30"), "VALUE");
        }

        // Check that variables are passed to child process
        EXPECT_EXIT_WITH(0)
        {
            EXPECT_STR_EQ(getenv("TEST30"), "VALUE");
            exit(FAILED_EXPECTATIONS());
        }

        exit(FAILED_EXPECTATIONS());
    }
}

static int i = 0;

TEST(jmp)
{
    jmp_buf buf;
    int ret = setjmp(buf);

    if (ret == 1)
    {
        ++i;
    }
    else
    {
        longjmp(buf, 1);
    }

    EXPECT_EQ(i, 1);
}

TEST_SUITE_END(libc);
