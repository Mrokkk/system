set(CMAKE_C_FLAGS
    "-pipe \
    ${C_FLAGS_STANDARD} \
    ${C_FLAGS_OPTIMIZATION} \
    ${C_FLAGS_WARNINGS} \
    ${C_FLAGS_DEBUG} \
    ${C_FLAGS_FRAME_POINTER} \
    -mno-sse -mno-sse2 -mno-sse3 -mno-sse4 -mno-avx \
    -nostdlib \
    -nostartfiles \
    -fno-builtin \
    -fno-asynchronous-unwind-tables -fno-stack-protector \
    -include ${PROJECT_SOURCE_DIR}/lib/libc.h \
    -fdata-sections \
    -ffunction-sections")

include_directories(
   ${PROJECT_SOURCE_DIR}/lib/include
)

set(LIBC_SRC
    abort.c
    assert.c
    atexit.c
    bsearch.c
    ctype.c
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
    sysconf.c
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

add_library(c SHARED ${LIBC_SRC})
add_library(c_static STATIC ${LIBC_SRC})

target_compile_options(c
    PRIVATE
        -fpie
        -pie
        -fPIC
)

add_library(crt0 OBJECT crt0.c)
add_library(crt0_shared OBJECT crt0_shared.c)

add_subdirectory(loader)

add_custom_target(
    libc_headers_install
    COMMAND "${CMAKE_COMMAND}" -E copy_directory_if_different "${PROJECT_SOURCE_DIR}/lib/include" ../sysroot/usr/include
    DEPENDS kernel_headers_install
    COMMENT "Installing libc headers"
)

add_dependencies(c libc_headers_install)
add_dependencies(c_static libc_headers_install)

add_custom_target(
    crt0_install
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different $<TARGET_OBJECTS:crt0> ../sysroot/usr/lib/crt0.o
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different $<TARGET_OBJECTS:crt0_shared> ../sysroot/usr/lib/crt0_shared.o
    DEPENDS crt0 crt0_shared libc_headers_install
    COMMENT "Installing crt0"
)

add_custom_target(
    libc_install
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different libc_static.a ../sysroot/usr/lib/libc.a
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different libc.so ../sysroot/usr/lib/libc.so
    DEPENDS crt0_install c c_static loader
    COMMENT "Installing libc")

add_dependencies(rootfs libc_install)
