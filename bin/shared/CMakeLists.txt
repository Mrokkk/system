add_application(
    main.c
)

add_library(test2 SHARED test.c)

target_compile_options(test2 PRIVATE -fdata-sections -ffunction-sections)
target_link_libraries(test2 c -Wl,-gc-sections)

target_link_libraries(shared
    test2
)

add_custom_target(
    test2_install
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different libtest2.so ../../sysroot/usr/lib/libtest2.so
    DEPENDS test2
)

add_dependencies(rootfs test2_install)
