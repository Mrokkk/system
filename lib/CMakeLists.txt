set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -include ${PROJECT_SOURCE_DIR}/lib/libc.h")

include_directories(
   ${PROJECT_SOURCE_DIR}/lib/include
)

add_library(c STATIC
    abort.c
    assert.c
    atexit.c
    bss.c
    dirent.c
    env.c
    errno.c
    error.c
    getcwd.c
    gethostname.c
    getpagesize.c
    grp.c
    inttypes.c
    isatty.c
    libc_main.c
    locale.c
    malloc.c
    math.c
    mntent.c
    poll.c
    pwd.c
    qsort.c
    rand.c
    resource.c
    setjmp.c
    signal.c
    sleep.c
    strtod.c
    strtol.c
    syscall.c
    termios.c
    time.c
    ttyname.c
    wchar.c

    stdio/fclose.c
    stdio/fflush.c
    stdio/fgetc.c
    stdio/file.c
    stdio/fopen.c
    stdio/fprintf.c
    stdio/fpurge.c
    stdio/fputc.c
    stdio/fputs.c
    stdio/fread.c
    stdio/fscanf.c
    stdio/fseek.c
    stdio/fwrite.c
    stdio/gnulib.c
    stdio/setvbuf.c
    stdio/sprintf.c
    stdio/stdio.c
    stdio/stdio_ext.c
    stdio/vsprintf.c

    string/memchr.c
    string/memcmp.c
    string/memcpy.c
    string/memmove.c
    string/memset.c
    string/strcat.c
    string/strchr.c
    string/strcmp.c
    string/strcpy.c
    string/strdup.c
    string/strlen.c
    string/strpbrk.c
    string/strsignal.c
    string/strspn.c
    string/strstr.c
    string/strtok.c

    ../kernel/lib/bitset.c
)

add_library(crt0 OBJECT crt0.c)
add_library(crt0_shared OBJECT crt0_shared.c)

add_custom_target(
    libc_headers_install
    COMMAND "${CMAKE_COMMAND}" -E copy_directory_if_different "${PROJECT_SOURCE_DIR}/lib/include" ../sysroot/usr/include
    DEPENDS kernel_headers_install
    COMMENT "Installing libc headers"
)

add_dependencies(c libc_headers_install)

add_custom_target(
    crt0_install
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different $<TARGET_OBJECTS:crt0> ../sysroot/usr/lib/crt0.o
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different $<TARGET_OBJECTS:crt0_shared> ../sysroot/usr/lib/crt0_shared.o
    DEPENDS crt0 crt0_shared libc_headers_install
    COMMENT "Installing crt0"
)

add_custom_target(
    libc_install
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different libc.a ../sysroot/usr/lib/libc.a
    DEPENDS crt0_install c
    COMMENT "Installing libc")
