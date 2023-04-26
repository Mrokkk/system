cmake_minimum_required(VERSION 3.0)
project(system C)

set(ARCH x86)
set(FRAME_POINTER 1)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(arch/${ARCH}/arch.cmake)

set(C_FLAGS_STANDARD "-std=gnu11")
set(C_FLAGS_OPTIMIZATION "-O2")
set(C_FLAGS_FREESTANDING "-ffreestanding -fno-builtin -nostdlib -no-pie -fno-PIC -nostartfiles -fno-asynchronous-unwind-tables -fno-stack-protector") # -fno-strength-reduce
set(C_FLAGS_WARNINGS "-Wall -Wextra -Werror")
set(C_DEBUG "-ggdb3")

if(FRAME_POINTER)
    set(C_FLAGS_FRAME_POINTER "-fno-omit-frame-pointer -fno-optimize-sibling-calls")
else()
    set(C_FLAGS_FRAME_POINTER "-fomit-frame-pointer")
endif()

set(CMAKE_C_FLAGS
    "${C_FLAGS_STANDARD} \
    ${C_FLAGS_FREESTANDING} \
    ${C_FLAGS_OPTIMIZATION} \
    ${C_FLAGS_WARNINGS} \
    ${C_FLAGS_ARCH} \
    ${C_DEBUG} \
    ${C_FLAGS_FRAME_POINTER} \
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

add_subdirectory(lib)
add_subdirectory(bin)

add_custom_target(
    ksyms
    COMMAND nm -S ${CMAKE_BINARY_DIR}/${PROJECT_NAME} | sort > ${CMAKE_BINARY_DIR}/kernel.map
    DEPENDS ${PROJECT_NAME}
    COMMENT "Creating kernel.map"
)

add_custom_target(
    font
    COMMAND "${PROJECT_SOURCE_DIR}/scripts/font_get.sh"
    COMMENT "Copying font"
)

add_custom_target(
   iso
   COMMAND "${PROJECT_SOURCE_DIR}/scripts/isomaker.sh"
           "-o" "${PROJECT_NAME}"
           "--args" "console=/dev/tty0 syslog=/dev/ttyS0"
           "--module" "${CMAKE_BINARY_DIR}/bin/sh/sh"
           "--module" "${CMAKE_BINARY_DIR}/bin/test/test"
           "--module" "${CMAKE_BINARY_DIR}/bin/test2/test2"
           "--module" "${CMAKE_BINARY_DIR}/bin/reboot/reboot"
           "--module" "${CMAKE_BINARY_DIR}/bin/ls/ls"
           "--module" "${CMAKE_BINARY_DIR}/bin/cat/cat"
           "--module" "${CMAKE_BINARY_DIR}/bin/init/init"
           "--module" "${CMAKE_BINARY_DIR}/bin/colors/colors"
           "--module" "${CMAKE_BINARY_DIR}/kernel.map"
           "--module" "${CMAKE_SOURCE_DIR}/tux.tga"
           "--module" "${CMAKE_BINARY_DIR}/font.psf"
   DEPENDS ${PROJECT_NAME} test test2 sh cat reboot ls colors init ksyms font
   COMMENT "Building ISO image"
)

add_custom_target(
    runqemu
    COMMAND cp ${PROJECT_SOURCE_DIR}/scripts/runqemu.sh runqemu
    COMMAND ./runqemu -k
    DEPENDS iso
    COMMENT "Running qemu"
)
