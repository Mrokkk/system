cmake_minimum_required(VERSION 3.5)
project(system C)

set(ARCH x86)
set(FRAME_POINTER 1)
set(STACK_PROTECTOR ON)
set(STATIC_ANALYZER ON)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_program(CCACHE ccache)
if(CCACHE)
    message(STATUS "Using ccache: ${CCACHE}")
    set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE "${CCACHE}")
endif()

include(arch/${ARCH}/arch.cmake)

set(C_FLAGS_STANDARD "-std=gnu11")
set(C_FLAGS_OPTIMIZATION "-O2")
set(C_FLAGS_FREESTANDING "-ffreestanding -fno-builtin -nostdlib -Wl,-no-pie -fno-pie -fno-PIC -nostartfiles -fno-asynchronous-unwind-tables")
set(C_FLAGS_WARNINGS "-Wall -Wextra -Werror")
set(C_FLAGS_DEBUG "-ggdb3")

if(STATIC_ANALYZER)
    set(C_FLAGS_ADDITIONAL "${C_FLAGS_ADDITIONAL} -fanalyzer -Wno-analyzer-va-arg-type-mismatch -Wno-analyzer-va-list-exhausted")
endif()

if(STACK_PROTECTOR)
    set(C_FLAGS_ADDITIONAL "${C_FLAGS_ADDITIONAL} -fstack-protector-strong")
endif()

if(FRAME_POINTER)
    set(C_FLAGS_FRAME_POINTER "-fno-omit-frame-pointer -fno-optimize-sibling-calls")
else()
    set(C_FLAGS_FRAME_POINTER "-fomit-frame-pointer")
endif()

set(CMAKE_C_FLAGS
    "-pipe \
    ${C_FLAGS_STANDARD} \
    ${C_FLAGS_FREESTANDING} \
    ${C_FLAGS_OPTIMIZATION} \
    ${C_FLAGS_WARNINGS} \
    ${C_FLAGS_ARCH} \
    ${C_FLAGS_DEBUG} \
    ${C_FLAGS_FRAME_POINTER} \
    ${C_FLAGS_ADDITIONAL} \
    -DFRAME_POINTER=${FRAME_POINTER}"
)

include_directories(
   ${PROJECT_SOURCE_DIR}/include
   ${PROJECT_SOURCE_DIR}/common/include
   ${PROJECT_SOURCE_DIR}/arch/${ARCH}/include
)

add_subdirectory(common)
add_subdirectory(arch/${ARCH})
add_subdirectory(drivers)
add_subdirectory(fs)
add_subdirectory(kernel)

add_common_library(kernel_common "" OFF)

if(BITS EQUAL 64)
    set_property(SOURCE arch/x86/head_64.S arch/x86/multiboot.S arch/x86/base_64.S arch/x86/real_mode.S PROPERTY LANGUAGE C)
    add_executable(
        system
        kernel/lib/printf.c
        arch/x86/head_64.S
        arch/x86/init_64.c
        arch/x86/multiboot.c
        arch/x86/multiboot.S
        arch/x86/descriptor.c
        arch/x86/pci.c
        arch/x86/cpuid.c
        arch/x86/bios32.c
        arch/x86/register.c
        arch/x86/real_mode.c
        arch/x86/memory.c
        arch/x86/base_64.S
        arch/x86/real_mode.S
        kernel/memory.c
        kernel/lib/bitflag.c
        kernel/sections.c
        $<TARGET_OBJECTS:kernel_common>
    )
else()
    add_executable(
        system
        $<TARGET_OBJECTS:kernel_common>
        $<TARGET_OBJECTS:kernel>
        $<TARGET_OBJECTS:drivers>
        $<TARGET_OBJECTS:arch>
        $<TARGET_OBJECTS:fs>
    )
endif()

target_link_libraries(
    system
    PRIVATE "-Wl,-Map=system.map -Wl,--no-print-map-discarded -Wl,--print-map-locals ${LINKER_FLAGS_ARCH}"
)

add_custom_target(
    kernel_headers_install
    COMMAND "${CMAKE_COMMAND}" -E copy_directory_if_different "${PROJECT_SOURCE_DIR}/include" sysroot/usr/include
    COMMAND "${CMAKE_COMMAND}" -E copy_directory_if_different "${PROJECT_SOURCE_DIR}/common/include/common/" sysroot/usr/include/common
    COMMENT "Installing kernel headers"
)

add_custom_target(
    kernel_symbols
    COMMAND nm -S ${CMAKE_BINARY_DIR}/${PROJECT_NAME} | sort > ${CMAKE_BINARY_DIR}/kernel.map
    DEPENDS ${PROJECT_NAME}
    COMMENT "Creating kernel.map"
)

add_custom_target(
    rootfs
    COMMAND "${PROJECT_SOURCE_DIR}/scripts/build_filesystem.sh"
    DEPENDS ${PROJECT_NAME} kernel_symbols
    COMMENT "Preparing rootfs"
)

if(NOT BITS EQUAL 64)
    add_subdirectory(lib)
    add_subdirectory(bin)
endif()

add_custom_target(
   iso
   COMMAND "${PROJECT_SOURCE_DIR}/scripts/isomaker.sh"
           "-o" "${PROJECT_NAME}"
           "--args" "console=/dev/tty0 syslog=/dev/debug0 earlycon"
           "--module" "${CMAKE_BINARY_DIR}/kernel.map"
   DEPENDS rootfs
   COMMENT "Building ISO image"
)

if (BITS EQUAL 64)
add_custom_target(
    runqemu
    COMMAND ${CMAKE_COMMAND} -E create_symlink ${PROJECT_SOURCE_DIR}/scripts/runqemu.sh ${CMAKE_BINARY_DIR}/runqemu
    COMMAND ${CMAKE_COMMAND} -E create_symlink ${PROJECT_SOURCE_DIR}/scripts/debugcon.sh ${CMAKE_BINARY_DIR}/debugcon
    COMMAND ${CMAKE_COMMAND} -E create_symlink ${PROJECT_SOURCE_DIR}/scripts/rungdb.py ${CMAKE_BINARY_DIR}/rungdb
    COMMAND ./runqemu -k --isa-debugcon
    DEPENDS rootfs
    COMMENT "Running qemu"
)
else()
add_custom_target(
    runqemu
    COMMAND ${CMAKE_COMMAND} -E create_symlink ${PROJECT_SOURCE_DIR}/scripts/runqemu.sh ${CMAKE_BINARY_DIR}/runqemu
    COMMAND ${CMAKE_COMMAND} -E create_symlink ${PROJECT_SOURCE_DIR}/scripts/debugcon.sh ${CMAKE_BINARY_DIR}/debugcon
    COMMAND ${CMAKE_COMMAND} -E create_symlink ${PROJECT_SOURCE_DIR}/scripts/rungdb.py ${CMAKE_BINARY_DIR}/rungdb
    COMMAND ./runqemu -k
    DEPENDS rootfs
    COMMENT "Running qemu"
)
endif()

add_custom_target(
    runbochs
    COMMAND ${CMAKE_COMMAND} -E create_symlink ${PROJECT_SOURCE_DIR}/scripts/runbochs.sh ${CMAKE_BINARY_DIR}/runbochs
    COMMAND ./runbochs
    DEPENDS rootfs
    COMMENT "Running bochs"
)

add_custom_target(
    run86box
    COMMAND ${CMAKE_COMMAND} -E create_symlink ${PROJECT_SOURCE_DIR}/scripts/run86box.sh ${CMAKE_BINARY_DIR}/run86box
    COMMAND ./run86box
    DEPENDS rootfs
    COMMENT "Running 86box"
)
