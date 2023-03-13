set_property(SOURCE head.S base.S PROPERTY LANGUAGE C)
file(GLOB SRC "*.c")
add_library(arch OBJECT ${SRC} head.S base.S)
