cmake_minimum_required(VERSION 3.0)
project(system C)

if (NOT EXISTS ${PROJECT_SOURCE_DIR}/CMake/config.cmake)
    message(FATAL_ERROR "Run configure first")
endif()

include(CMake/config.cmake)
include(CMake/generate_version_h.cmake)
include(CMake/generate_compile_h.cmake)

set(LINKER_SCRIPT ${PROJECT_SOURCE_DIR}/arch/${ARCH}/linker.ld)

set(C_FLAGS_STANDARD "-std=gnu99")
set(C_FLAGS_OPTIMIZATION "-O2")
set(C_FLAGS_FREESTANDING "-ffreestanding -fno-builtin -nostdinc -nostdlib")
set(C_FLAGS_OTHER "-fno-asynchronous-unwind-tables -fomit-frame-pointer -fno-strength-reduce")

set(CMAKE_C_FLAGS "${C_FLAGS_STANDARD} ${C_FLAGS_FREESTANDING} ${C_FLAGS_OPTIMIZATION} ${C_FLAGS_OTHER} -m32")

set(CMAKE_EXE_LINKER_FLAGS
    "-Wl,-T${LINKER_SCRIPT} -Wl,--oformat -Wl,elf32-i386 -m32 -Wl,-m -Wl,elf_i386 -Wl,-Map=kernel.map -Wl,--build-id=none"
)

add_definitions(
   -Wall
   -Wextra
   -Werror
   -DARCH=${ARCH}
)

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

