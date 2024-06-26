add_executable(loader
    elf.c
    helpers.c
    loader.c
)

target_include_directories(loader
    PRIVATE ..
)

target_compile_options(loader
    PRIVATE
        -ffunction-sections
        -fdata-sections
)

target_link_libraries(loader c
    -Wl,-T${CMAKE_CURRENT_SOURCE_DIR}/loader.ld
    -Wl,--gc-sections
)

add_dependencies(rootfs loader)
