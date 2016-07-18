file(GLOB kernel_src "*.c")
add_library(kernel OBJECT ${kernel_src})
