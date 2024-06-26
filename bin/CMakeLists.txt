project(bin)

set(CMAKE_SKIP_RPATH ON)

set(CMAKE_C_FLAGS
    "-pipe \
    ${C_FLAGS_STANDARD} \
    ${C_FLAGS_OPTIMIZATION} \
    ${C_FLAGS_WARNINGS} \
    ${C_FLAGS_DEBUG} \
    -m32 -mno-sse -mno-sse2 -mno-sse3 -mno-sse4 -mno-avx")

set(DISABLED_APPS "env" "date")

function(add_application)

    get_filename_component(name ${CMAKE_CURRENT_SOURCE_DIR} NAME)
    add_executable(${name} ${ARGN})
    target_compile_options(${name} PRIVATE -fdata-sections -ffunction-sections)
    target_link_libraries(${name} c -Wl,-gc-sections)
    add_dependencies(rootfs ${name})
    add_dependencies(${name} libc_install)

endfunction()

file(GLOB SUBDIRS LIST_DIRECTORIES true "*")

foreach(dir ${SUBDIRS})
    get_filename_component(name ${dir} NAME)
    list(FIND DISABLED_APPS ${name} id)
    if(IS_DIRECTORY ${dir} AND ${id} EQUAL -1)
        add_subdirectory(${dir})
    endif()
endforeach()
