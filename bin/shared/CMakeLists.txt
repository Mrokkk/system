set(CMAKE_SHARED_LIBRARY_LINK_C_FLAGS "-rdynamic")

add_application(
    main.c
)

target_compile_options(shared PRIVATE
    -fpie
    -pie
    -fPIC
    -nostdlib
)

add_library(test2 SHARED test.c)

target_link_libraries(shared
    -pie
    test2)
