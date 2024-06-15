project(bin)

set(CMAKE_C_FLAGS
    "-pipe \
    ${C_FLAGS_STANDARD} \
    ${C_FLAGS_OPTIMIZATION} \
    ${C_FLAGS_WARNINGS} \
    ${C_FLAGS_DEBUG} \
    -m32 -mno-sse -mno-sse2 -mno-sse3 -mno-sse4 -mno-avx")

function(add_application)

    get_filename_component(name ${CMAKE_CURRENT_SOURCE_DIR} NAME)
    add_executable(${name} ${ARGN})
    target_link_libraries(${name} c)
    add_dependencies(rootfs ${name} libc_install)

endfunction()

file(GLOB SUBDIRS LIST_DIRECTORIES true "*")

foreach(dir ${SUBDIRS})
    if(IS_DIRECTORY ${dir})
        add_subdirectory(${dir})
    endif()
endforeach()
