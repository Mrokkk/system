#include "test.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>

#include "argparse.h"

#define RED                 "\033[31m"
#define GREEN               "\033[32m"
#define YELLOW              "\033[33m"
#define BLUE                "\033[34m"
#define RESET               "\033[0m"

#define VERDICT(p, f)       (((p) << 16) | (f))
#define PASSED_GET(v)       ((v) >> 16)
#define FAILED_GET(v)       ((v) & 0xffff)

typedef uint32_t verdict_t;

static const char* const RUN_MSG           = GREEN  "[ RUN  ]" RESET;
static const char* const FAIL_MSG          = RED    "[ FAIL ]" RESET;
static const char* const PASS_MSG          = GREEN  "[ PASS ]" RESET;
static const char* const GEN_GREEN_MSG     = GREEN  "[======]" RESET;
static const char* const GEN_RED_MSG       = RED    "[======]" RESET;

static test_suite_t* suites[TEST_SUITES_COUNT];
static size_t suites_count;
int __assert_failed;

static void verbose_set(config_t* config)
{
    config->verbose = true;
}

static void forever_set(config_t* config)
{
    config->run_until_failure = true;
}

static void test_set(config_t* config, const char* test)
{
    config->test_to_run = test;
}

static option_t options[] = {
    {"-f", "--forever", OPT_BOOL,  "Loop test until failure", &forever_set},
    {"-v", "--verbose", OPT_BOOL,  "Turn on verbose logging", &verbose_set},
    {"-t", "--test",    OPT_VALUE, "Select test to run",      &test_set},
};

static int test_run(test_case_t* test, test_suite_t* suite)
{
    const char* suite_name = suite->name;

    fprintf(stdout, "%s %s.%s\n", RUN_MSG, suite_name, test->name);

    __assert_failed = 0;

    test->test();

    fprintf(stdout, "%s %s.%s\n", __assert_failed ? FAIL_MSG : PASS_MSG, suite_name, test->name);

    return __assert_failed;
}

static verdict_t suite_run(test_suite_t* suite, list_head_t* failed_tests, config_t* config)
{
    int assert_failed = 0, passed = 0, failed = 0;
    test_case_t* test;
    test_case_t* tests = suite->test_cases;
    size_t count = suite->test_cases_count;

    *suite->config = config;

    fprintf(stdout, "%s %u tests from %s\n", GEN_GREEN_MSG, count, suite->name);

    for (size_t i = 0; i < count; ++i)
    {
        test = tests + i;

        if (config->test_to_run && strcmp(config->test_to_run, test->name))
        {
            continue;
        }

        if (config->test_to_run && config->run_until_failure)
        {
            while (!(assert_failed = test_run(test, suite)));
        }
        else
        {
            assert_failed = test_run(test, suite);
        }

        if (LIKELY(!assert_failed))
        {
            ++passed;
        }
        else
        {
            ++failed;
            list_add_tail(&test->failed_tests, failed_tests);
        }
    }

    fprintf(stdout, "%s %u tests from %s\n\n", GEN_GREEN_MSG, count, suite->name);

    return VERDICT(passed, failed);
}

void __test_suite_register(test_suite_t* suite)
{
    suites[suites_count++] = suite;
}

static int print_value(FILE* stream, void* data, int type)
{
    switch (type)
    {
        case TYPE_CHAR: return fprintf(stream, "char{%d}", *(char*)data);
        case TYPE_SHORT: return fprintf(stream, "short{%d}", *(short*)data);
        case TYPE_INT: return fprintf(stream, "int{%d}", *(int*)data);
        case TYPE_LONG: return fprintf(stream, "long{%ld}", *(long*)data);
        case TYPE_LONG_LONG: return fprintf(stream, "long long{%lld}", *(long long*)data);
        case TYPE_UNSIGNED_CHAR: return fprintf(stream, "unsigned char{%u}", *(unsigned char*)data);
        case TYPE_UNSIGNED_SHORT: return fprintf(stream, "unsigned short{%u}", *(unsigned short*)data);
        case TYPE_UNSIGNED_INT: return fprintf(stream, "unsigned int{%u}", *(unsigned int*)data);
        case TYPE_UNSIGNED_LONG: return fprintf(stream, "unsigned long{%lu}", *(unsigned long*)data);
        case TYPE_UNSIGNED_LONG_LONG: return fprintf(stream, "unsigned long long{%llu}", *(unsigned long long*)data);
        case TYPE_INT8_T: return fprintf(stream, "int8_t{%d}", *(int8_t*)data);
        case TYPE_INT16_T: return fprintf(stream, "int16_t{%d}", *(int16_t*)data);
        case TYPE_INT32_T: return fprintf(stream, "int32_t{%d}", *(int32_t*)data);
        case TYPE_INT64_T: return fprintf(stream, "int64_t{%lld}", *(int64_t*)data);
        case TYPE_UINT8_T: return fprintf(stream, "uint8_t{%u}", *(uint8_t*)data);
        case TYPE_UINT16_T: return fprintf(stream, "uint16_t{%u}", *(uint16_t*)data);
        case TYPE_UINT32_T: return fprintf(stream, "uint32_t{%u}", *(uint32_t*)data);
        case TYPE_UINT64_T: return fprintf(stream, "uint64_t{%llu}", *(uint64_t*)data);
        case TYPE_VOID_PTR: return fprintf(stream, "void*{%p}", *(void**)data);
        case TYPE_CHAR_PTR: return fprintf(stream, "char*{%p}", *(char**)data);
        default: return fprintf(stream, "unrecognized{%p}", *(void**)data);
    }
}

static const char* comp_string_get(comp_t comp)
{
    switch (comp)
    {
        case COMP_EQ: return "equal to";
        case COMP_NE: return "not equal to";
        case COMP_GT: return "greater than";
        case COMP_GE: return "greater than or equal to";
        case COMP_LT: return "less than";
        case COMP_LE: return "less than or equal to";
        default: return "< unknown comparison >";
    }
}

static const char* FAILED_EXPECTATION_MSG = "  Failed expectation from " BLUE "%s" RESET ":%u\n";
static const char* USER_FAILURE_MSG = "  Failure from " BLUE "%s" RESET ":%u\n    ";

void failure_print(
    value_t* actual,
    value_t* expected,
    comp_t comp,
    const char* file,
    size_t line)
{
    fprintf(stdout, FAILED_EXPECTATION_MSG, file, line);
    fprintf(stdout, "    expected: %s\n"
                    "        which is ", actual->name);
    print_value(stdout, actual->value, actual->type);
    fprintf(stdout, "\n"
                    "    to be %s %s\n"
                    "        which is ", comp_string_get(comp), expected->name);
    print_value(stdout, expected->value, expected->type);
    fprintf(stdout, "\n");
}

static void string_failure_print(
    value_t* actual,
    value_t* expected,
    const char* file,
    size_t line)
{
    fprintf(stdout, FAILED_EXPECTATION_MSG, file, line);
    fprintf(stdout, "    expected: %s\n"
                    "        which is ",
                    actual->name);

    if (*(char**)actual->value)
    {
        fprintf(stdout, "\"%s\"", *(char**)actual->value);
    }
    else
    {
        print_value(stdout, actual->value, actual->type);
    }

    fprintf(stdout, "\n"
                    "    to be equal to %s\n"
                    "        which is ",
                    expected->name);

    if (*(char**)expected->value)
    {
        fprintf(stdout, "\"%s\"", *(char**)expected->value);
    }
    else
    {
        print_value(stdout, expected->value, expected->type);
    }

    putc('\n', stdout);
}

void string_check(
    value_t* actual,
    value_t* expected,
    const char* file,
    size_t line)
{
    const char* a = *(char**)actual->value;
    const char* e = *(char**)expected->value;

    if (!a || !e)
    {
        if (UNLIKELY(a != e))
        {
            string_failure_print(actual, expected, file, line);
            __assert_failed++;
        }
    }
    else if (UNLIKELY(strcmp(a, e)))
    {
        string_failure_print(actual, expected, file, line);
        __assert_failed++;
    }
}

void user_failure_print(const char* file, size_t line, const char* fmt, ...)
{
    va_list args;

    fprintf(stdout, USER_FAILURE_MSG, file, line);

    va_start(args, fmt);
    vfprintf(stdout, fmt, args);
    va_end(args);

    fputc('\n', stdout);
    fputc('\n', stdout);
}

enum
{
    EXPECTED_EXIT_WITH = 1,
    EXPECTED_KILLED_BY = 2,
    ACTUAL_EXIT_WITH = 4,
    ACTUAL_KILLED_BY = 8,
    ACTUAL_WAITPID_FAILED = 16,
};

static void exit_failure_print(int error, int expected_status, int status, const char* file, size_t line)
{
    fprintf(stdout, FAILED_EXPECTATION_MSG, file, line);

    switch (error & 3)
    {
        case EXPECTED_EXIT_WITH:
            fprintf(stdout,
                "    expected: process exit with %d\n",
                expected_status);
            break;
        case EXPECTED_KILLED_BY:
            fprintf(stdout,
                "    expected: process killed by SIG%s\n",
                sigabbrev_np(expected_status));
            break;
    }

    switch (error & 0x1c)
    {
        case ACTUAL_WAITPID_FAILED:
            fprintf(stdout,
                "      actual: waitpid failed with %d\n",
                errno);
            break;
        case ACTUAL_EXIT_WITH:
            fprintf(stdout,
                "      actual: process exit with %d\n",
                WEXITSTATUS(status));
            break;
        case ACTUAL_KILLED_BY:
            fprintf(stdout,
                "      actual: process killed by SIG%s\n",
                sigabbrev_np(WTERMSIG(status)));
            break;
    }
}

int expect_exit_with(int pid, int expected_error_code, const char* file, size_t line)
{
    int error = 0, status = 0;

    if (UNLIKELY(waitpid(pid, &status, 0) < 0))
    {
        error = EXPECTED_EXIT_WITH | ACTUAL_WAITPID_FAILED;
    }
    else if (UNLIKELY(WIFEXITED(status) <= 0 || WIFSIGNALED(status)))
    {
        error = EXPECTED_EXIT_WITH | ACTUAL_KILLED_BY;
    }
    else if (UNLIKELY(WEXITSTATUS(status) != expected_error_code))
    {
        error = EXPECTED_EXIT_WITH | ACTUAL_EXIT_WITH;
    }

    if (UNLIKELY(error))
    {
        exit_failure_print(error, expected_error_code, status, file, line);
    }

    return !!error;
}

int expect_killed_by(int pid, int signal, const char* file, size_t line)
{
    int error = 0, status = 0;

    if (UNLIKELY(waitpid(pid, &status, 0) < 0))
    {
        error = EXPECTED_KILLED_BY | ACTUAL_WAITPID_FAILED;
    }
    else if (UNLIKELY(WIFEXITED(status) != 0))
    {
        error = EXPECTED_KILLED_BY | ACTUAL_EXIT_WITH;
    }
    else if (WTERMSIG(status) != signal)
    {
        error = EXPECTED_KILLED_BY | ACTUAL_KILLED_BY;
    }

    if (UNLIKELY(error))
    {
        exit_failure_print(error, signal, status, file, line);
    }

    return !!error;
}

static void final_verdict_print(int passed, int failed, list_head_t* failed_tests)
{
    test_case_t* test;

    fprintf(stdout, "%s Passed %u\n", GEN_GREEN_MSG, passed);

    if (LIKELY(!failed))
    {
        return;
    }

    fprintf(stdout, "%s Failed %u\n", GEN_RED_MSG, failed);
    list_for_each_entry(test, failed_tests, failed_tests)
    {
        fprintf(stdout, "%s %s.%s\n", FAIL_MSG, test->suite->name, test->name);
    }
}

int __test_suites_run(int argc, char* argv[])
{
    int failed = 0, passed = 0;
    config_t config = {};
    list_head_t failed_tests = LIST_INIT(failed_tests);

    args_parse(argc, argv, options, sizeof(options) / sizeof(*options), &config);

    for (size_t i = 0; i < suites_count; ++i)
    {
        verdict_t verdict;
        test_suite_t* suite = suites[i];

        verdict = suite_run(suite, &failed_tests, &config);

        passed += PASSED_GET(verdict);
        failed += FAILED_GET(verdict);
    }

    final_verdict_print(passed, failed, &failed_tests);

    return failed;
}
