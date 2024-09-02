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
        -fPIC
        -fpie
        -pie
        -ggdb3
        -ffunction-sections
        -fdata-sections
)

target_link_libraries(loader c_static
    -Wl,--gc-sections
    -Wl,-nostdlib
    -nostartfiles
    -Wl,-pie
    -Wl,-no-dynamic-linker
)

add_custom_target(
    loader_install
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different loader ../../sysroot/lib/ld.so
    DEPENDS loader
    COMMENT "Installing loader"
)

add_dependencies(rootfs loader_install)
