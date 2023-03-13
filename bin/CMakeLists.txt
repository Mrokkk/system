project(bin)

set(BIN_LINKER_SCRIPT ${PROJECT_SOURCE_DIR}/../lib/start.ld)
set(BIN_LINKER_FLAGS "-Wl,-T${BIN_LINKER_SCRIPT} -Wl,--oformat -Wl,elf32-i386 -m32 -Wl,-m -Wl,elf_i386 -Wl,--build-id=none")
set(CMAKE_EXE_LINKER_FLAGS "${BIN_LINKER_FLAGS}")

set(BIN_C_FLAGS "${C_FLAGS_STANDARD} ${C_FLAGS_FREESTANDING} -Os ${C_FLAGS_OTHER} ${C_FLAGS_WARNINGS} -DUSER -ggdb3")
set(CMAKE_C_FLAGS "${BIN_C_FLAGS} -m32 -msse -mno-sse2 -mno-sse3 -mno-sse4 -mno-avx -nostdinc")

include_directories(
   ${PROJECT_SOURCE_DIR}/../lib/include
)

file(GLOB bin_common_src "*.c")

add_library(common OBJECT ${bin_common_src} "${PROJECT_SOURCE_DIR}/../kernel/lib/bitset.c")

add_subdirectory(ls)
add_subdirectory(sh)
add_subdirectory(init)
add_subdirectory(test)
add_subdirectory(test2)
add_subdirectory(reboot)
