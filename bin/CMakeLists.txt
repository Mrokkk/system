project(bin)

set(CMAKE_C_FLAGS
    "-pipe \
    ${C_FLAGS_STANDARD} \
    ${C_FLAGS_OPTIMIZATION} \
    ${C_FLAGS_WARNINGS} \
    ${C_FLAGS_DEBUG} \
    -m32 -mno-sse -mno-sse2 -mno-sse3 -mno-sse4 -mno-avx")

file(GLOB bin_common_src "*.c")

add_library(common OBJECT ${bin_common_src} "${PROJECT_SOURCE_DIR}/../kernel/lib/bitset.c")

add_dependencies(common libc_install)

add_subdirectory(ls)
add_subdirectory(sh)
add_subdirectory(cat)
add_subdirectory(date)
add_subdirectory(init)
add_subdirectory(loop)
add_subdirectory(test)
add_subdirectory(snake)
add_subdirectory(test2)
add_subdirectory(colors)
add_subdirectory(insmod)
add_subdirectory(reboot)
add_subdirectory(strace)
add_subdirectory(display)
add_subdirectory(poweroff)
