cmake_minimum_required(VERSION 3.5)
project(system C)

set(ARCH x86)
set(FRAME_POINTER 1)
set(STATIC_ANALYZER 0)
set(STACK_PROTECTOR 1)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
    message(STATUS "Using ccache: ${CCACHE_PROGRAM}")
    set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE "${CCACHE_PROGRAM}")
endif()

include(arch/${ARCH}/arch.cmake)

set(C_FLAGS_STANDARD "-std=gnu11")
set(C_FLAGS_OPTIMIZATION "-O2")
set(C_FLAGS_FREESTANDING "-ffreestanding -fno-builtin -nostdlib -Wl,-no-pie -fno-pie -fno-PIC -nostartfiles -fno-asynchronous-unwind-tables") # -fno-strength-reduce
set(C_FLAGS_WARNINGS "-Wall -Wextra -Werror")
set(C_FLAGS_DEBUG "-ggdb3")

if(STATIC_ANALYZER)
    set(C_FLAGS_WARNINGS "${C_FLAGS_WARNINGS} -fanalyzer -Wno-analyzer-va-arg-type-mismatch -Wno-analyzer-va-list-exhausted -Wno-error")
endif()

if(STACK_PROTECTOR)
    set(C_FLAGS_ADDITIONAL "${C_FLAGS_ADDITIONAL} -fstack-protector-strong")
endif()

if(CMAKE_C_COMPILER_ID STREQUAL "Clang")
    set(C_FLAGS_ADDITIONAL "-Wno-error -Wno-unused-command-line-argument -Wno-c2x-extensions -Wno-strict-prototypes -Wno-address-of-packed-member")
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
    -DFRAME_POINTER=${FRAME_POINTER}")

include_directories(
   ${PROJECT_SOURCE_DIR}/include
   ${PROJECT_SOURCE_DIR}/arch/${ARCH}/include
)

add_subdirectory(arch/${ARCH})
add_subdirectory(drivers)
add_subdirectory(fs)
add_subdirectory(kernel)

add_executable(
    system
    $<TARGET_OBJECTS:kernel>
    $<TARGET_OBJECTS:drivers>
    $<TARGET_OBJECTS:arch>
    $<TARGET_OBJECTS:fs>
)

target_link_libraries(
    system
    PRIVATE "-Wl,-Map=system.map ${LINKER_FLAGS_ARCH}"
)

add_custom_target(
    kernel_headers_install
    COMMAND "${CMAKE_COMMAND}" -E copy_directory_if_different "${PROJECT_SOURCE_DIR}/include" sysroot/usr/include
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

add_subdirectory(lib)
add_subdirectory(bin)

add_custom_target(
   iso
   COMMAND "${PROJECT_SOURCE_DIR}/scripts/isomaker.sh"
           "-o" "${PROJECT_NAME}"
           "--args" "console=/dev/tty0 syslog=/dev/debug0 earlycon"
           "--module" "${CMAKE_BINARY_DIR}/kernel.map"
   DEPENDS rootfs
   COMMENT "Building ISO image"
)

add_custom_target(
    runqemu
    COMMAND ${CMAKE_COMMAND} -E create_symlink ${PROJECT_SOURCE_DIR}/scripts/runqemu.sh ${CMAKE_BINARY_DIR}/runqemu
    COMMAND ${CMAKE_COMMAND} -E create_symlink ${PROJECT_SOURCE_DIR}/scripts/debugcon.sh ${CMAKE_BINARY_DIR}/debugcon
    COMMAND ${CMAKE_COMMAND} -E create_symlink ${PROJECT_SOURCE_DIR}/scripts/rungdb.py ${CMAKE_BINARY_DIR}/rungdb
    COMMAND ./runqemu -k
    DEPENDS rootfs
    COMMENT "Running qemu"
)

add_custom_target(
    runbochs
    COMMAND ${CMAKE_COMMAND} -E create_symlink ${PROJECT_SOURCE_DIR}/scripts/runbochs.sh ${CMAKE_BINARY_DIR}/runbochs
    COMMAND ./runbochs
    DEPENDS iso
    COMMENT "Running bochs"
)
