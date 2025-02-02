set(CMAKE_C_FLAGS
    "-pipe \
    ${C_FLAGS_STANDARD} \
    ${C_FLAGS_OPTIMIZATION} \
    ${C_FLAGS_WARNINGS} \
    ${C_FLAGS_DEBUG} \
    ${C_FLAGS_FRAME_POINTER} \
    ${C_FLAGS_ADDITIONAL} \
    ${C_FLAGS_ARCH} \
    -nostdlib \
    -nostartfiles \
    -fno-builtin \
    -fno-asynchronous-unwind-tables \
    -include ${PROJECT_SOURCE_DIR}/lib/libc.h \
    -fdata-sections \
    -ffunction-sections")

include_directories(
   ${PROJECT_SOURCE_DIR}/lib/include
)

set_property(SOURCE vsyscall.S PROPERTY LANGUAGE C)

add_common_library(libc_common "__libc_" ON)

set(LIBC_SRC
    abort.c
    assert.c
    atexit.c
    bsearch.c
    confstr.c
    ctype.c
    dirent.c
    dlfcn.c
    env.c
    errno.c
    error.c
    exec.c
    ftruncate.c
    getcwd.c
    gethostname.c
    getopt.c
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
    sched.c
    setjmp.c
    signal.c
    sleep.c
    ssp.c
    strtod.c
    strtol.c
    syscall.c
    sysconf.c
    symlink.c
    syslog.c
    system.c
    termios.c
    time.c
    ttyname.c
    uname.c
    vsyscall.S
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
    stdio/remove.c
    stdio/scanf.c
    stdio/setvbuf.c
    stdio/sprintf.c
    stdio/stdio.c
    stdio/stdio_ext.c
    stdio/tmpnam.c
    stdio/vsprintf.c

    string/bzero.c
    string/memchr.c
    string/strcasecmp.c
    string/strcat.c
    string/strcoll.c
    string/strdup.c
    string/strpbrk.c
    string/strsignal.c
    string/strtok.c
    string/strxfrm.c

    $<TARGET_OBJECTS:libc_common>
)

add_library(c SHARED ${LIBC_SRC})
add_library(c_static STATIC ${LIBC_SRC})

target_link_libraries(c PRIVATE "${EMULATION}")
target_link_libraries(c_static PRIVATE "${EMULATION}")

target_compile_options(c
    PRIVATE
        -fpie
        -pie
        -fPIC
)

target_compile_options(c_static
    PRIVATE
        -fPIC
        -pie
)

add_library(crt0 OBJECT crt0.c)

target_compile_options(crt0
    PRIVATE
        -fpie
        -pie
        -fPIC
)

add_library(crt0_shared OBJECT crt0_shared.c)

set_property(SOURCE "crti.S" PROPERTY LANGUAGE C)
set_property(SOURCE "crtn.S" PROPERTY LANGUAGE C)

add_library(crti OBJECT crti.S)
add_library(crtn OBJECT crtn.S)

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
    crt_install
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different $<TARGET_OBJECTS:crt0> ../sysroot/usr/lib/crt0.o
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different $<TARGET_OBJECTS:crt0_shared> ../sysroot/usr/lib/crt0_shared.o
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different $<TARGET_OBJECTS:crti> ../sysroot/usr/lib/crti.o
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different $<TARGET_OBJECTS:crtn> ../sysroot/usr/lib/crtn.o
    DEPENDS crt0 crti crtn crt0_shared libc_headers_install
    COMMENT "Installing crt files"
)

add_custom_target(
    libc_install
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different libc_static.a ../sysroot/lib/libc.a
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different libc.so ../sysroot/lib/libc.so
    DEPENDS crt_install c c_static loader
    COMMENT "Installing libc")

add_dependencies(rootfs libc_install)
