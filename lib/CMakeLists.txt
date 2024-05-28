set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -nostdinc -include ${PROJECT_SOURCE_DIR}/lib/libc.h")

include_directories(
   ${PROJECT_SOURCE_DIR}/lib/include
)

add_library(libc STATIC
    abort.c
    bss.c
    errno.c
    libc_main.c
    malloc.c
    signal.c
    stdio.c
    string.c
    syscall.c
    crt0.c
)
