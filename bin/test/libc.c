#include "test.h"

#include <errno.h>
#include <stdio.h>
#include <getopt.h>
#include <limits.h>
#include <setjmp.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdio_ext.h>

TEST_SUITE(libc);

TEST(memset_zero)
{
    char buf[32];
    __builtin_memset(buf, 0xfe, sizeof(buf));
    memset(buf, 0, 7);
    EXPECT_EQ(buf[0], 0);
    EXPECT_EQ(buf[1], 0);
    EXPECT_EQ(buf[5], 0);
    EXPECT_EQ(buf[6], 0);
    EXPECT_EQ(buf[7], 0xfe);
    EXPECT_EQ(buf[31], 0xfe);
}

TEST(bzero)
{
    char buf[32];
    __builtin_memset(buf, 0xfe, sizeof(buf));
    bzero(buf, 7);
    EXPECT_EQ(buf[0], 0);
    EXPECT_EQ(buf[1], 0);
    EXPECT_EQ(buf[5], 0);
    EXPECT_EQ(buf[6], 0);
    EXPECT_EQ(buf[7], 0xfe);
    EXPECT_EQ(buf[31], 0xfe);
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
    EXPECT_EQ(strncmp(string, "tes", 3), 0);
    EXPECT_EQ(strncmp(string, "aaa", 0), 0);
    EXPECT_EQ(strncmp(string, "test", SIZE_MAX), 0);
    EXPECT_EQ(strncmp(string, "tes", SIZE_MAX), 't');
    EXPECT_EQ(strncmp(string, "est", 4), 't' - 'e');
    EXPECT_EQ(strncmp(string, "test1", 5), 0 - '1');
    EXPECT_EQ(strncmp(string, "test1", 4), 0);
}

TEST(strcasecmp)
{
    static const char* string = "Test";

    EXPECT_EQ(strcasecmp(string, "test"), 0);
    EXPECT_EQ(strcasecmp(string, "TEST"), 0);
    EXPECT_EQ(strcasecmp(string, "teS"), 't');
    EXPECT_EQ(strcasecmp(string, "Est"), 't' - 'e');
    EXPECT_EQ(strcasecmp(string, "test1"), 0 - '1');
    EXPECT_EQ(strcasecmp(string, "\0"), 't' - 0);
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

TEST(stpcpy)
{
    char buf[16] = {0, };
    static const char* string = "test";
    buf[4] = 0xff;
    EXPECT_EQ(stpcpy(buf, string), buf + 4);
    EXPECT_EQ(buf[0], 't');
    EXPECT_EQ(buf[1], 'e');
    EXPECT_EQ(buf[2], 's');
    EXPECT_EQ(buf[3], 't');
    EXPECT_EQ(buf[4], 0);
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

TEST(stpncpy)
{
    {
        char buf[16] = {0, };
        static const char* string = "test";
        buf[4] = 0xff;
        EXPECT_EQ(stpncpy(buf, string, 16), buf + 4);
        EXPECT_EQ(buf[0], 't');
        EXPECT_EQ(buf[1], 'e');
        EXPECT_EQ(buf[2], 's');
        EXPECT_EQ(buf[3], 't');
        EXPECT_EQ(buf[4], 0);
    }
    {
        char buf[16] = {0, };
        static const char* string = "test";
        buf[3] = 0xff;
        buf[4] = 0xff;
        EXPECT_EQ(stpncpy(buf, string, 3), buf + 3);
        EXPECT_EQ(buf[0], 't');
        EXPECT_EQ(buf[1], 'e');
        EXPECT_EQ(buf[2], 's');
        EXPECT_EQ(buf[3], 0xff);
        EXPECT_EQ(buf[4], 0xff);
    }
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
        buf[3] = 0xff;
        buf[4] = 0xff;
        EXPECT_EQ(strncpy(buf, string, 3), buf);
        EXPECT_EQ(buf[0], 't');
        EXPECT_EQ(buf[1], 'e');
        EXPECT_EQ(buf[2], 's');
        EXPECT_EQ(buf[3], 0xff);
        EXPECT_EQ(buf[4], 0xff);
    }
}

TEST(strchr)
{
    static const char* string = "this_is_test_string";
    EXPECT_EQ(strchr(string, 'x'), NULL);
    EXPECT_EQ(strchr(string, '\0'), string + 19);
    EXPECT_EQ(strchr(string, 'g'), string + 18);
    EXPECT_EQ(strchr(string, 'i'), string + 2);
    EXPECT_EQ(strchr(string, 's'), string + 3);
    EXPECT_EQ(strchr(string, 'h'), string + 1);
}

TEST(strrchr)
{
    static const char* string = "{this_is_test_string}";
    EXPECT_EQ(strrchr(string, 'x'), NULL);
    EXPECT_EQ(strrchr(string + 1, '{'), NULL);
    EXPECT_EQ(strrchr(string, '{'), string);
    EXPECT_EQ(strrchr(string, '\0'), string + 21);
    EXPECT_EQ(strrchr(string, 'g'), string + 19);
    EXPECT_EQ(strrchr(string, 'i'), string + 17);
    EXPECT_EQ(strrchr(string, 's'), string + 14);
    EXPECT_EQ(strrchr(string, 'h'), string + 2);
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

TEST(strcat)
{
    char buf[128];
    strcpy(buf, "test");
    EXPECT_EQ(strcat(buf, "_test"), buf);
    EXPECT_STR_EQ(buf, "test_test");
}

TEST(strncat)
{
    char buf[32] = "this";
    strncat(buf, "_is_test", 3);
    EXPECT_STR_EQ(buf, "this_is");
}

TEST(strpbrk)
{
    const char* string = "test_string";
    EXPECT_EQ(strpbrk(string, "string"), string);
    EXPECT_EQ(strpbrk(string, "sing"), string + 2);
    EXPECT_EQ(strpbrk(string, "ig"), string + 8);
    EXPECT_EQ(strpbrk(string, "xyz"), NULL);
    EXPECT_EQ(strpbrk(string, ""), NULL);
    EXPECT_EQ(strpbrk("", ""), NULL);
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

TEST(memchr)
{
    const char* string = "test123";
    EXPECT_EQ(memchr(string, 't', 7), string);
    EXPECT_EQ(memchr(string, 'e', 7), string + 1);
    EXPECT_EQ(memchr(string, '2', 7), string + 5);
    EXPECT_EQ(memchr(string, 'w', 7), NULL);
    EXPECT_EQ(memchr(string, 't', 0), NULL);
    EXPECT_EQ(memrchr(string, 't', 7), string + 3);
    EXPECT_EQ(memrchr(string, 't', 0), NULL);
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
    EXPECT_EQ(setenv("PHOENIX", "TEST", 1), 0);
    EXPECT_STR_EQ(getenv("PHOENIX"), "TEST");

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

static int compar(const void* lhs, const void* rhs)
{
    return *(int*)lhs - *(int*)rhs;
}

static int compar2(const void* lhs, const void* rhs)
{
    return *(uint64_t*)lhs - *(uint64_t*)rhs;
}

struct packed_data
{
    uint8_t a;
    uint8_t b;
    uint8_t c;
} __attribute__((packed));

typedef struct packed_data packed_t;

static int compar3(const void* lhs, const void* rhs)
{
    const packed_t* a = lhs;
    const packed_t* b = rhs;
    uint32_t lhsv = (uint32_t)a->a << 16 | (uint32_t)a->b << 8 | (uint32_t)a->c;
    uint32_t rhsv = (uint32_t)b->a << 16 | (uint32_t)b->b << 8 | (uint32_t)b->c;
    return lhsv - rhsv;
}

TEST(qsort)
{
    // 3-bytes data
    {
        packed_t table[] = {{0xff, 0x02, 0xb3}, {0x00, 0x03, 0xff}, {0x23, 0x22, 0x01}, {0x00, 0x00, 0x01}};

        qsort(table, 4, sizeof(*table), &compar3);

        EXPECT_EQ(table[0].a, 0x00);
        EXPECT_EQ(table[0].b, 0x00);
        EXPECT_EQ(table[0].c, 0x01);
        EXPECT_EQ(table[1].a, 0x00);
        EXPECT_EQ(table[1].b, 0x03);
        EXPECT_EQ(table[1].c, 0xff);
        EXPECT_EQ(table[2].a, 0x23);
        EXPECT_EQ(table[2].b, 0x22);
        EXPECT_EQ(table[2].c, 0x01);
        EXPECT_EQ(table[3].a, 0xff);
        EXPECT_EQ(table[3].b, 0x02);
        EXPECT_EQ(table[3].c, 0xb3);
    }

    // 4-bytes data
    {
        int table[] = {205, 2, 5, 203, -2, 29, 4};

        qsort(table, 7, sizeof(*table), &compar);

        EXPECT_EQ(table[0], -2);
        EXPECT_EQ(table[1], 2);
        EXPECT_EQ(table[2], 4);
        EXPECT_EQ(table[3], 5);
        EXPECT_EQ(table[4], 29);
        EXPECT_EQ(table[5], 203);
        EXPECT_EQ(table[6], 205);
    }
    // 8-bytes data
    {
        uint64_t table[] = {1294, 49532, 23, 456, 30495930294LL};

        qsort(table, 5, sizeof(*table), &compar2);

        EXPECT_EQ(table[0], 23);
        EXPECT_EQ(table[1], 456);
        EXPECT_EQ(table[2], 1294);
        EXPECT_EQ(table[3], 49532);
        EXPECT_EQ(table[4], 30495930294LL);
    }
}

TEST(isatty)
{
    EXPECT_GE(isatty(0), 1);
    EXPECT_GE(isatty(1), 1);
    EXPECT_GE(isatty(2), 1);
    int fd = open("/proc/uptime", O_RDONLY);
    EXPECT_GT(fd, 0);
    EXPECT_EQ(isatty(fd), 0);
    close(fd);
}

TEST(strtol)
{
    // Invalid bases
    {
        errno = 0;
        EXPECT_EQ(strtol("932", NULL, 27), 0);
        EXPECT_EQ(errno, EINVAL);

        errno = 0;
        EXPECT_EQ(strtol("932", NULL, 1), 0);
        EXPECT_EQ(errno, EINVAL);

        errno = 0;
        EXPECT_EQ(strtol("932", NULL, -2), 0);
        EXPECT_EQ(errno, EINVAL);
    }

    char* end;

    EXPECT_EQ(strtol("73",         NULL, 0),  73);
    EXPECT_EQ(strtol("-928",       NULL, 0), -928);
    EXPECT_EQ(strtol("073",        NULL, 0),  073);
    EXPECT_EQ(strtol("073",        NULL, 8),  073);
    EXPECT_EQ(strtol("073",        NULL, 16), 0x73);
    EXPECT_EQ(strtol("   0x203  ", NULL, 0),  0x203);
    EXPECT_EQ(strtol(" 0x203",     NULL, 16), 0x203);
    EXPECT_EQ(strtol("P5hC",       NULL, 26), 443234);
    EXPECT_EQ(strtol("\t-0xfb43",  &end, 0), -0xfb43);
    EXPECT_EQ(*end, 0);
    EXPECT_EQ(strtol("  -0xfb43g", &end, 0), -0xfb43);
    EXPECT_EQ(*end, 'g');
    EXPECT_EQ(strtol("  +3953bsd", &end, 10), 3953);
    EXPECT_EQ(*end, 'b');

    errno = 0;
    EXPECT_EQ(strtol("0xbff04000", NULL, 16), LONG_MAX);
    EXPECT_EQ(errno, ERANGE);

    errno = 0;
    EXPECT_EQ(strtol("0xabd1045bdfe", NULL, 0), LONG_MAX);
    EXPECT_EQ(errno, ERANGE);

    errno = 0;
    EXPECT_EQ(strtol("-0x9482bf92dda![", NULL, 0), LONG_MIN);
    EXPECT_EQ(errno, ERANGE);
}

TEST(strtoul)
{
    EXPECT_EQ(strtoul("0xbff04000", NULL, 16), 0xbff04000);
}

extern int __fmode(FILE* stream);

TEST(fopen)
{
    FILE* f;
    {
        f = fopen("/dev/null", "w");
        EXPECT_NE(f, NULL);
        EXPECT_EQ(__fmode(f), O_WRONLY | O_CREAT | O_TRUNC);
        fclose(f);
    }
    {
        f = fopen("/dev/null", "r");
        EXPECT_NE(f, NULL);
        EXPECT_EQ(__fmode(f), O_RDONLY);
        fclose(f);
    }
    {
        f = fopen("/dev/null", "a");
        EXPECT_NE(f, NULL);
        EXPECT_EQ(__fmode(f), O_WRONLY | O_CREAT | O_APPEND);
        fclose(f);
    }
    {
        f = fopen("/dev/null", "a+");
        EXPECT_NE(f, NULL);
        EXPECT_EQ(__fmode(f), O_RDWR | O_CREAT | O_APPEND);
        fclose(f);
    }
    {
        f = fopen("/dev/null", "x");
        EXPECT_EQ(f, NULL);
        EXPECT_EQ(errno, EINVAL);
    }
}

TEST(fdopen)
{
    FILE* f;
    int fd = open("/dev/null", O_RDONLY);

    {
        f = fdopen(fd, "w");
        EXPECT_EQ(f, NULL);
        EXPECT_EQ(errno, EINVAL);
    }
    {
        f = fdopen(fd, "r+");
        EXPECT_EQ(f, NULL);
        EXPECT_EQ(errno, EINVAL);
    }
    {
        f = fdopen(fd, "r");
        EXPECT_NE(f, NULL);
        EXPECT_EQ(__fmode(f), O_RDONLY);
        fclose(f);
    }
}

#define _GETOPT_CONDITION() \
    ({ \
        int res = _; _ = 0; \
        if (!res) { optind = 0; opterr = 0; argc = ARRAY_SIZE(argv); } \
        !res; \
    })

#define GETOPT_TEST_CASE(...) \
    for (char* argv[] = {__VA_ARGS__}; _GETOPT_CONDITION(); ++_)

#define GETOPT_INIT() \
    int _ = 0, argc

TEST(getopt)
{
    GETOPT_INIT();

    // Reading positional argument
    GETOPT_TEST_CASE("app", "test")
    {
        EXPECT_EQ(getopt(argc, argv, "-"), 1);
        EXPECT_EQ(optind, 2);
        EXPECT_STR_EQ(optarg, "test");

        EXPECT_EQ(getopt(argc, argv, "-"), -1);
        EXPECT_EQ(optind, 2);
        EXPECT_EQ(optarg, NULL);
    }

    // Option with argument in the same argv entry
    GETOPT_TEST_CASE("app", "-otest")
    {
        const char* optstring = "o:";

        EXPECT_EQ(getopt(argc, argv, optstring), 'o');
        EXPECT_EQ(optind, 2);
        EXPECT_STR_EQ(optarg, "test");

        EXPECT_EQ(getopt(argc, argv, optstring), -1);
        EXPECT_EQ(optind, 2);
        EXPECT_EQ(optarg, NULL);
    }

    // Option with argument in the next argv entry
    GETOPT_TEST_CASE("app", "-o", "test")
    {
        const char* optstring = "o:";

        EXPECT_EQ(getopt(argc, argv, optstring), 'o');
        EXPECT_EQ(optind, 3);
        EXPECT_STR_EQ(optarg, "test");

        EXPECT_EQ(getopt(argc, argv, optstring), -1);
        EXPECT_EQ(optind, 3);
        EXPECT_EQ(optarg, NULL);
    }

    // Missing argument
    GETOPT_TEST_CASE("app", "-o")
    {
        const char* optstring = "o:";

        EXPECT_EQ(getopt(argc, argv, optstring), '?');
        EXPECT_EQ(optind, 2);
        EXPECT_EQ(optopt, 'o');
        EXPECT_EQ(optarg, NULL);

        EXPECT_EQ(getopt(argc, argv, optstring), -1);
        EXPECT_EQ(optind, 2);
        EXPECT_EQ(optopt, 'o');
        EXPECT_EQ(optarg, NULL);
    }

    // Unknown option
    GETOPT_TEST_CASE("app", "-i")
    {
        const char* optstring = "o";

        EXPECT_EQ(getopt(argc, argv, optstring), '?');
        EXPECT_EQ(optind, 2);
        EXPECT_EQ(optarg, NULL);
        EXPECT_EQ(optopt, 'i');

        EXPECT_EQ(getopt(argc, argv, optstring), -1);
        EXPECT_EQ(optind, 2);
        EXPECT_EQ(optarg, NULL);
        EXPECT_EQ(optopt, 'i');
    }

    // Option with optional argument; without argument
    GETOPT_TEST_CASE("app", "-o")
    {
        const char* optstring = "o::";

        EXPECT_EQ(getopt(argc, argv, optstring), 'o');
        EXPECT_EQ(optind, 2);
        EXPECT_EQ(optarg, NULL);

        EXPECT_EQ(getopt(argc, argv, optstring), -1);
        EXPECT_EQ(optind, 2);
        EXPECT_EQ(optarg, NULL);
    }

    // Option with optional argument; with argument
    GETOPT_TEST_CASE("app", "-otest")
    {
        const char* optstring = "o::";

        EXPECT_EQ(getopt(argc, argv, optstring), 'o');
        EXPECT_EQ(optind, 2);
        EXPECT_STR_EQ(optarg, "test");

        EXPECT_EQ(getopt(argc, argv, optstring), -1);
        EXPECT_EQ(optind, 2);
        EXPECT_EQ(optarg, NULL);
    }

    // Option with ignored positional arguments
    GETOPT_TEST_CASE("app", "arg1", "arg2", "arg3", "-a")
    {
        const char* optstring = "a";

        EXPECT_EQ(getopt(argc, argv, optstring), 'a');
        EXPECT_EQ(optind, 2);
        EXPECT_EQ(optarg, NULL);

        EXPECT_EQ(getopt(argc, argv, optstring), -1);
        EXPECT_EQ(optarg, NULL);
    }

    // Option with not ignored positional arguments
    GETOPT_TEST_CASE("app", "arg1", "arg2", "-o")
    {
        const char* optstring = "-o:";

        EXPECT_EQ(getopt(argc, argv, optstring), 1);
        EXPECT_STR_EQ(optarg, "arg1");

        EXPECT_EQ(getopt(argc, argv, optstring), 1);
        EXPECT_STR_EQ(optarg, "arg2");

        EXPECT_EQ(getopt(argc, argv, optstring), '?');

        EXPECT_EQ(getopt(argc, argv, optstring), -1);
    }

    // Option with ignored positional arguments; mixed
    GETOPT_TEST_CASE("app", "-o", "arg1", "arg2", "arg3", "-o")
    {
        const char* optstring = "o";

        EXPECT_EQ(getopt(argc, argv, optstring), 'o');
        EXPECT_EQ(getopt(argc, argv, optstring), 'o');
        EXPECT_EQ(getopt(argc, argv, optstring), -1);
        EXPECT_EQ(getopt(argc, argv, optstring), -1);
    }

    // Multiple options in single argv entry
    GETOPT_TEST_CASE("app", "-abc")
    {
        const char* optstring = "abc";

        EXPECT_EQ(getopt(argc, argv, optstring), 'a');
        EXPECT_EQ(getopt(argc, argv, optstring), 'b');
        EXPECT_EQ(getopt(argc, argv, optstring), 'c');
        EXPECT_EQ(getopt(argc, argv, optstring), -1);
    }

    // Multiple options intertwined with random - and --
    GETOPT_TEST_CASE("app", "-i-o", "--", "-i", "-")
    {
        const char* optstring = "io";

        EXPECT_EQ(getopt(argc, argv, optstring), 'i');
        EXPECT_EQ(optind, 1);
        EXPECT_EQ(getopt(argc, argv, optstring), '?');
        EXPECT_EQ(optind, 1);
        EXPECT_EQ(getopt(argc, argv, optstring), 'o');
        EXPECT_EQ(getopt(argc, argv, optstring), -1);
        EXPECT_EQ(getopt(argc, argv, optstring), 'i');
        EXPECT_EQ(getopt(argc, argv, optstring), -1);
        EXPECT_EQ(getopt(argc, argv, optstring), -1);
        EXPECT_EQ(getopt(argc, argv, optstring), -1);
    }

    // Positonal argument when option required
    GETOPT_TEST_CASE("app", "abc")
    {
        const char* optstring = "io";

        EXPECT_EQ(getopt(argc, argv, optstring), -1);
        EXPECT_EQ(optind, 1);
        EXPECT_EQ(optarg, NULL);
    }

    // argc lower than 2
    GETOPT_TEST_CASE("app")
    {
        EXPECT_EQ(getopt(0, argv, ""), -1);
        EXPECT_EQ(getopt(1, argv, ""), -1);
    }
}

TEST(getopt_long)
{
    GETOPT_INIT();

    GETOPT_TEST_CASE("app", "--test=aaa", "--test2", "bbb", "--test3")
    {
        const char* optstring = "";
        int optindex = -1;

        const struct option longopts[] = {
            {"test", required_argument, 0, 0},
            {"test2", required_argument, 0, 0},
            {"test3", optional_argument, 0, 0},
            {0, 0, 0, 0}
        };

        EXPECT_EQ(getopt_long(argc, argv, optstring, longopts, &optindex), 0);
        EXPECT_EQ(optindex, 0);
        EXPECT_EQ(optind, 2);
        EXPECT_STR_EQ(optarg, "aaa");

        EXPECT_EQ(getopt_long(argc, argv, optstring, longopts, &optindex), 0);
        EXPECT_EQ(optindex, 1);
        EXPECT_EQ(optind, 4);
        EXPECT_STR_EQ(optarg, "bbb");

        EXPECT_EQ(getopt_long(argc, argv, optstring, longopts, &optindex), 0);
        EXPECT_EQ(optindex, 2);
        EXPECT_EQ(optind, 5);
        EXPECT_STR_EQ(optarg, NULL);

        EXPECT_EQ(getopt_long(argc, argv, optstring, longopts, &optindex), -1);
        EXPECT_EQ(optind, 5);
    }

    GETOPT_TEST_CASE("app", "--test=aaa", "-tbbb", "ccc")
    {
        const char* optstring = "-t:";
        int optindex = -1;

        const struct option longopts[] = {
            {"test", required_argument, 0, 0},
            {0, 0, 0, 0}
        };

        EXPECT_EQ(getopt_long(argc, argv, optstring, longopts, &optindex), 0);
        EXPECT_EQ(optindex, 0);
        EXPECT_STR_EQ(optarg, "aaa");

        EXPECT_EQ(getopt_long(argc, argv, optstring, longopts, &optindex), 't');
        EXPECT_STR_EQ(optarg, "bbb");

        EXPECT_EQ(getopt_long(argc, argv, optstring, longopts, &optindex), 1);
        EXPECT_STR_EQ(optarg, "ccc");

        EXPECT_EQ(getopt_long(argc, argv, optstring, longopts, &optindex), -1);
    }
}

TEST(getopt_long_only)
{
    GETOPT_INIT();

    GETOPT_TEST_CASE("app", "-test=aaa", "-test2", "bbb", "-test3")
    {
        const char* optstring = "";
        int optindex = -1;

        const struct option longopts[] = {
            {"test", required_argument, 0, 0},
            {"test2", required_argument, 0, 0},
            {"test3", optional_argument, 0, 0},
            {0, 0, 0, 0}
        };

        EXPECT_EQ(getopt_long_only(argc, argv, optstring, longopts, &optindex), 0);
        EXPECT_EQ(optindex, 0);
        EXPECT_STR_EQ(optarg, "aaa");

        EXPECT_EQ(getopt_long_only(argc, argv, optstring, longopts, &optindex), 0);
        EXPECT_EQ(optindex, 1);
        EXPECT_STR_EQ(optarg, "bbb");

        EXPECT_EQ(getopt_long_only(argc, argv, optstring, longopts, &optindex), 0);
        EXPECT_EQ(optindex, 2);
        EXPECT_STR_EQ(optarg, NULL);

        EXPECT_EQ(getopt_long_only(argc, argv, optstring, longopts, &optindex), -1);
    }
}

TEST(confstr)
{
    char buf[32];
    EXPECT_EQ(confstr(_CS_PATH, NULL, 0), 14);
    EXPECT_EQ(errno, 0);
    EXPECT_EQ(confstr(33, NULL, 0), 0);
    EXPECT_EQ(errno, EINVAL);
    EXPECT_EQ(confstr(_CS_PATH, buf, 32), 14);
    EXPECT_STR_EQ(buf, "/bin:/usr/bin");
    EXPECT_EQ(errno, 0);
}

TEST_SUITE_END(libc);
