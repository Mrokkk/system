#include "test.h"

#include <stdlib.h>
#include <string.h>

const char* const RUN_MSG           = "\033[32m[  RUN   ]\033[0m";
const char* const FAIL_MSG          = "\033[31m[  FAIL  ]\033[0m";
const char* const PASS_MSG          = "\033[32m[  PASS  ]\033[0m";
const char* const GEN_GREEN_MSG     = "\033[32m[========]\033[0m";
const char* const GEN_YELLOW_MSG    = "\033[33m[========]\033[0m";
const char* const GEN_RED_MSG       = "\033[31m[========]\033[0m";
const char* const EXPECT_MSG        = "\033[31m[ EXPECT ]\033[0m";
const char* const ACTUAL_MSG        = "\033[31m[ ACTUAL ]\033[0m";

static test_suite_t* suites[TEST_SUITES_COUNT];
static size_t suites_count;

static void config_read(int argc, char* argv[], config_t* config)
{
    __builtin_memset(config, 0, sizeof(*config));

    for (int i = 1; i < argc; ++i)
    {
        char* token;

        if (UNLIKELY(!(token = strtok(argv[i], "-"))))
        {
            printf("%s Empty argument passed: \"%s\"\n", GEN_YELLOW_MSG, argv[i]);
            exit(EXIT_FAILURE);
        }

        if (!strcmp(token, "f") || !strcmp(token, "forever"))
        {
            config->run_until_failure = true;
        }
        else if (!strcmp(token, "v") || !strcmp(token, "verbose"))
        {
            config->verbose = true;
        }
        else if (!strncmp(token, "test=", 5))
        {
            if (!(token = strtok(token + 5, "=")))
            {
                printf("%s No test name given after --test=: \"%s\"\n", GEN_YELLOW_MSG, argv[i]);
                exit(EXIT_FAILURE);
            }
            config->test_to_run = token;
        }
        else
        {
            printf("%s Unrecognized option: \"%s\"\n", GEN_YELLOW_MSG, argv[i]);
            exit(EXIT_FAILURE);
        }
    }
}

static int test_run(test_case_t* test, const char* suite_name, config_t* config)
{
    int assert_failed = 0;

    if (config->verbose)
    {
        printf("%s %s.%s\n", RUN_MSG, suite_name, test->name);
    }

    test->test(&assert_failed, config);

    if (assert_failed)
    {
        printf("%s %s.%s\n", FAIL_MSG, suite_name, test->name);
    }
    else if (config->verbose)
    {
        printf("%s %s.%s\n", PASS_MSG, suite_name, test->name);
    }

    return assert_failed;
}

static void verdict_print(int passed, int failed, test_case_t* tests, int count)
{
    test_case_t* test;

    printf("%s Passed %u\n", GEN_GREEN_MSG, passed);

    if (LIKELY(!failed))
    {
        return;
    }

    printf("%s Failed %u\n", GEN_RED_MSG, failed);
    for (int i = 0; i < count; ++i)
    {
        test = tests + i;
        if (test->failed)
        {
            printf("%s %s\n", GEN_RED_MSG, test->name);
        }
    }
}

int suite_run(test_suite_t* suite, config_t* config)
{
    int assert_failed = 0, passed = 0, failed = 0;
    test_case_t* test;
    test_case_t* tests = suite->test_cases;
    size_t count = suite->test_cases_count;

    for (size_t i = 0; i < count; ++i)
    {
        test = tests + i;

        if (config->test_to_run && strcmp(config->test_to_run, test->name))
        {
            continue;
        }

        if (config->test_to_run && config->run_until_failure)
        {
            while (!(assert_failed = test_run(test, suite->name, config)));
        }
        else
        {
            assert_failed = test_run(test, suite->name, config);
        }

        if (assert_failed == 0)
        {
            ++passed;
        }
        else
        {
            ++failed;
            test->failed = 1;
        }
    }

    verdict_print(passed, failed, tests, count);
    return failed;
}

void __test_suite_register(test_suite_t* suite)
{
    suites[suites_count++] = suite;
}

int __test_suites_run(int argc, char* argv[])
{
    int failed = 0;
    config_t config;

    config_read(argc, argv, &config);

    if (config.verbose)
    {
        printf("%s config.verbose: %s\n"
            "%s config.run_until_failure: %s\n"
            "%s config.test_to_run: %s\n",
            GEN_GREEN_MSG,
            config.verbose ? "true" : "false",
            GEN_GREEN_MSG,
            config.run_until_failure ? "true" : "false",
            GEN_GREEN_MSG,
            config.test_to_run ? config.test_to_run : "all");
    }

    for (size_t i = 0; i < suites_count; ++i)
    {
        test_suite_t* suite = suites[i];

        printf("%s Running %u tests from suite: \"%s\"\n",
            RUN_MSG,
            (unsigned int)suite->test_cases_count,
            suite->name);

        failed += suite_run(suite, &config);
    }

    return failed;
}
