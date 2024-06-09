set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -include ${PROJECT_SOURCE_DIR}/lib/libc.h")

include_directories(
   ${PROJECT_SOURCE_DIR}/lib/include
)

add_library(c STATIC
    abort.c
    atexit.c
    atoi.c
    bss.c
    dirent.c
    env.c
    errno.c
    error.c
    fcntl.c
    getcwd.c
    gethostname.c
    grp.c
    inttypes.c
    isatty.c
    libc_main.c
    locale.c
    malloc.c
    math.c
    mntent.c
    pipe.c
    poll.c
    pwd.c
    qsort.c
    rand.c
    resource.c
    setjmp.c
    signal.c
    sleep.c
    stdio.c
    syscall.c
    termios.c
    time.c
    ttyname.c

    string/memchr.c
    string/memcmp.c
    string/memcpy.c
    string/memmove.c
    string/memset.c
    string/strcat.c
    string/strchr.c
    string/strcmp.c
    string/strcpy.c
    string/strlen.c
    string/strpbrk.c
    string/strspn.c
    string/strstr.c
    string/strtok.c

    ../kernel/lib/bitset.c
)

add_library(crt0 STATIC crt0.c)

add_custom_target(
    libc_headers_install
    COMMAND "${CMAKE_COMMAND}" -E copy_directory_if_different "${PROJECT_SOURCE_DIR}/lib/include" ../sysroot/usr/include
    COMMENT "Installing libc headers"
    DEPENDS kernel_headers_install
)

add_custom_target(
    crt0_install
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different $<TARGET_OBJECTS:crt0> ../sysroot/usr/lib/crt0.o
    DEPENDS crt0 libc_headers_install
    COMMENT "Installing crt0"
)

add_custom_target(
    libc_install
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different libc.a ../sysroot/usr/lib/libc.a
    DEPENDS crt0_install c
    COMMENT "Installing libc")
