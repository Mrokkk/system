add_application(
    main.c
)

target_compile_options(shared PRIVATE
    -fpie
    -pie
    -fPIC
)

add_library(test2 SHARED test.c)

target_compile_options(test2 PRIVATE -fdata-sections -ffunction-sections)
target_link_libraries(test2 c -Wl,-gc-sections)

target_link_libraries(shared
    -pie
    test2)
