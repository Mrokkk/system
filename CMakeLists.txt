cmake_minimum_required(VERSION 3.0)
project(system C)

set(VERSION_MAJ 0)
set(VERSION_MIN 0)

if (NOT EXISTS ${PROJECT_SOURCE_DIR}/CMake/config.cmake)
    message(FATAL_ERROR "Run configure first")
endif()

include(CMake/config.cmake)
include(arch/${ARCH}/arch.cmake)

set(C_FLAGS_STANDARD "-std=gnu99")
set(C_FLAGS_OPTIMIZATION "-O2")
set(C_FLAGS_FREESTANDING "-ffreestanding -fno-builtin -nostdlib")
set(C_FLAGS_WARNINGS "-Wall -Wextra")
set(C_FLAGS_OTHER "-fno-asynchronous-unwind-tables -fomit-frame-pointer")
if (NOT "${CMAKE_C_COMPILER_ID}" STREQUAL "Clang")
    set(C_FLAGS_WARNINGS "${C_FLAGS_WARNINGS} -Werror")
    set(C_FLAGS_OTHER "${C_FLAGS_OTHER} -fno-strength-reduce")
endif()

set(CMAKE_C_FLAGS "${C_FLAGS_STANDARD} ${C_FLAGS_FREESTANDING} ${C_FLAGS_OPTIMIZATION} ${C_FLAGS_OTHER} ${C_FLAGS_WARNINGS} ${C_FLAGS_ARCH}")

# Export compile commands to JSON file
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_EXE_LINKER_FLAGS
    "-Wl,-Map=kernel.map ${LINKER_FLAGS_ARCH}"
)

execute_process(COMMAND date +%X OUTPUT_VARIABLE COMPILE_TIME OUTPUT_STRIP_TRAILING_WHITESPACE)
execute_process(COMMAND date +%x OUTPUT_VARIABLE COMPILE_DATE OUTPUT_STRIP_TRAILING_WHITESPACE)
execute_process(COMMAND uname -s OUTPUT_VARIABLE HOST_OS OUTPUT_STRIP_TRAILING_WHITESPACE)
execute_process(COMMAND uname -m OUTPUT_VARIABLE HOST_ARCH OUTPUT_STRIP_TRAILING_WHITESPACE)
execute_process(COMMAND whoami OUTPUT_VARIABLE HOST_USER OUTPUT_STRIP_TRAILING_WHITESPACE)
execute_process(COMMAND hostname OUTPUT_VARIABLE HOSTNAME OUTPUT_STRIP_TRAILING_WHITESPACE)
execute_process(COMMAND ${CMAKE_C_COMPILER} -dumpversion OUTPUT_VARIABLE COMPILER OUTPUT_STRIP_TRAILING_WHITESPACE)

configure_file(${CMAKE_SOURCE_DIR}/include/kernel/compile.h.in ${CMAKE_SOURCE_DIR}/include/kernel/compile.h)
configure_file(${CMAKE_SOURCE_DIR}/include/kernel/version.h.in ${CMAKE_SOURCE_DIR}/include/kernel/version.h)

if(CI_BUILD)
    add_definitions(-DCI)
endif()

include_directories(
   ${PROJECT_SOURCE_DIR}/include
   ${PROJECT_SOURCE_DIR}/arch/${ARCH}/include
)

add_subdirectory(arch/${ARCH})
add_subdirectory(drivers)
add_subdirectory(fs)
add_subdirectory(lib)
add_subdirectory(init)
add_subdirectory(kernel)

file(GLOB MAIN_INCLUDES include/kernel/*.h)
file(GLOB ARCH_INCLUDES arch/${ARCH}/include/arch/*.h)

add_executable(
   system
   $<TARGET_OBJECTS:kernel>
   $<TARGET_OBJECTS:init>
   $<TARGET_OBJECTS:drivers>
   $<TARGET_OBJECTS:arch>
   $<TARGET_OBJECTS:lib>
   $<TARGET_OBJECTS:fs>
   ${MAIN_INCLUDES}
   ${ARCH_INCLUDES}
)

add_custom_target(
   iso
   COMMAND bash ${PROJECT_SOURCE_DIR}/CMake/isomaker.sh
   DEPENDS ${PROJECT_NAME}
   COMMENT "Building bootable ISO image"
)

add_custom_target(
    runqemu
    COMMAND echo exec `which qemu-system-i386 || which qemu-system-x86_64` -cdrom os.iso -serial stdio -no-reboot \\$$@ > ${CMAKE_BINARY_DIR}/runqemu
    COMMAND echo exec `which qemu-system-i386 || which qemu-system-x86_64` -cdrom os.iso -display none -serial stdio -no-reboot \\$$@ > ${CMAKE_BINARY_DIR}/runqemu-nographic
    COMMAND chmod +x runqemu*
    DEPENDS iso
    COMMENT "Creating runqemu executables"
)

add_custom_target(
    style-check
    COMMAND ${CMAKE_SOURCE_DIR}/CMake/style-checker.sh ${CMAKE_SOURCE_DIR}
)

