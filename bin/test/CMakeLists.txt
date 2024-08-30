set(TESTS
    libc.c
    kernel.c
    mman.c
)

list(LENGTH TESTS TESTS_LEN)

add_application(
    ${TESTS}
    argparse.c
    main.c
    mman_utils.c
    test.c
)

target_compile_options(test PRIVATE
    -fno-builtin
    -DTEST_SUITES_COUNT=${TESTS_LEN})
