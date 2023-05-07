set_property(SOURCE head.S base.S bios.S PROPERTY LANGUAGE C)
file(GLOB SRC "*.c")
add_library(arch OBJECT ${SRC} head.S base.S bios.S)
