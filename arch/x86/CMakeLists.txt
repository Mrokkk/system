if(BITS EQUAL 64)
    # TODO
    set(ASM_SRC "")
else()
    set(ASM_SRC "head_32.S;base.S;bios.S")
endif()

set_property(SOURCE ${ASM_SRC} PROPERTY LANGUAGE C)
file(GLOB SRC "*.c")
add_library(arch OBJECT ${SRC} ${ASM_SRC})
