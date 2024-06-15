set(TESTS
    kernel.c
    libc.c
)

list(LENGTH TESTS TESTS_LEN)

add_application(
    ${TESTS}
    argparse.c
    main.c
    test.c
)

target_compile_options(test PRIVATE
    -fno-builtin
    -DTEST_SUITES_COUNT=${TESTS_LEN})
