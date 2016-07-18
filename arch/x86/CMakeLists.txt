set_property(SOURCE head.S x86_base.S PROPERTY LANGUAGE C)
file(GLOB arch_src "*.c")
add_library(arch OBJECT ${arch_src} head.S x86_base.S)
