#include "test.h"

#include <string.h>

const char* const RUN_MSG       = "\033[32m[  RUN   ]\033[0m";
const char* const FAIL_MSG      = "\033[31m[  FAIL  ]\033[0m";
const char* const PASS_MSG      = "\033[32m[  PASS  ]\033[0m";
const char* const GEN_GREEN_MSG = "\033[32m[========]\033[0m";
const char* const GEN_RED_MSG   = "\033[31m[========]\033[0m";
const char* const EXPECT_MSG    = "\033[31m[ EXPECT ]\033[0m";
const char* const ACTUAL_MSG    = "\033[31m[ ACTUAL ]\033[0m";

static test_suite_t* suites[TEST_SUITES_COUNT];
static size_t suites_count;

static void config_read(int argc, char* argv[], config_t* config)
{
    memset(config, 0, sizeof(*config));

    for (int i = 0; i < argc; ++i)
    {
        size_t len = strlen(argv[i]);
        if (len == 9)
        {
            if (!strncmp(argv[i], "--forever", 9))
            {
                config->run_forever = true;
            }
        }
        else if (len > 7)
        {
            if (!strncmp(argv[i], "--test=", 7))
            {
                config->test_to_run = argv[i] + 7;
            }
        }
        else if (len == 2)
        {
            if (!strncmp(argv[i], "-f", 2))
            {
                config->run_forever = true;
            }
            if (!strncmp(argv[i], "-v", 2))
            {
                config->verbose = true;
            }
        }
    }
}

static int __tc_run(test_case_t* tc, config_t* config)
{
    int assert_failed = 0;

    if (config->verbose)
    {
        printf("%s %s\n", RUN_MSG, tc->name);
    }

    tc->test(&assert_failed, config);

    if (config->verbose)
    {
        if (assert_failed)
        {
            printf("%s %s\n", FAIL_MSG, tc->name);
        }
        else
        {
            printf("%s %s\n", PASS_MSG, tc->name);
        }
    }

    return assert_failed;
}

static void verdict_print(int passed, int failed, test_case_t* test_cases, int count)
{
    test_case_t* tc;

    printf("%s Passed %u\n", GEN_GREEN_MSG, passed);

    if (LIKELY(!failed))
    {
        return;
    }

    printf("%s Failed %u\n", GEN_RED_MSG, failed);
    for (int i = 0; i < count; ++i)
    {
        tc = test_cases + i;
        if (tc->failed)
        {
            printf("%s %s\n", GEN_RED_MSG, tc->name);
        }
    }
}

int __tcs_run(test_case_t* test_cases, int count, config_t* config)
{
    int assert_failed = 0, passed = 0, failed = 0;
    test_case_t* tc;

    for (int i = 0; i < count; ++i)
    {
        tc = test_cases + i;
        if (config->test_to_run && strcmp(config->test_to_run, tc->name))
        {
            continue;
        }

        if (config->test_to_run && config->run_forever)
        {
            while (!(assert_failed = __tc_run(tc, config)));
        }
        else
        {
            assert_failed = __tc_run(tc, config);
        }

        if (assert_failed == 0)
        {
            ++passed;
        }
        else
        {
            ++failed;
            tc->failed = 1;
        }
    }

    verdict_print(passed, failed, test_cases, count);
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
        printf("%s Configuration:\n"
            "  * verbose: %s\n"
            "  * run forever: %s\n"
            "  * test to run: %s\n",
            GEN_GREEN_MSG,
            config.verbose ? "true" : "false",
            config.run_forever ? "true" : "false",
            config.test_to_run ? config.test_to_run : "all");
    }

    for (size_t i = 0; i < suites_count; ++i)
    {
        test_suite_t* suite = suites[i];

        printf("%s Running %u tests from suite: \"%s\"\n",
            RUN_MSG,
            (unsigned int)suite->test_cases_count,
            suite->name);

        failed += __tcs_run(suite->test_cases, suite->test_cases_count, &config);
    }

    return failed;
}
