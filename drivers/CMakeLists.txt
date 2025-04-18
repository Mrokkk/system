file(GLOB_RECURSE BLOCK_SRC "block/*.c")
file(GLOB_RECURSE CHAR_SRC "char/*.c")
file(GLOB_RECURSE USB_SRC "usb/*.c")
file(GLOB_RECURSE VIDEO_SRC "video/*.c")

add_library(drivers OBJECT ${BLOCK_SRC} ${CHAR_SRC} ${USB_SRC} ${VIDEO_SRC})

file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/modules")

function(module_build name)

    add_library(mod_${name} OBJECT ${ARGN})

    target_compile_options(mod_${name}
        PRIVATE -fpie -pie
    )

    add_custom_target(module_${name}
        COMMAND cp $<TARGET_OBJECTS:mod_${name}> ${CMAKE_BINARY_DIR}/modules/${name}.ko
        DEPENDS mod_${name}
    )

endfunction()

module_build(test test.c)
module_build(debugmon debugmon.c)

add_dependencies(drivers module_test module_debugmon)
