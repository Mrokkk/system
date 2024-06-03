set(TESTS
    kernel.c
    libc.c
)

list(LENGTH TESTS TESTS_LEN)

add_executable(test
    ${TESTS}
    argparse.c
    main.c
    test.c
    $<TARGET_OBJECTS:common>
)

target_compile_options(test PRIVATE
    -fno-builtin
    -DTEST_SUITES_COUNT=${TESTS_LEN})

target_link_libraries(
    test
    PRIVATE
        c
        "-Wl,-Map=test.map"
)
