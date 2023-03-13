file(GLOB_RECURSE SRC "*.c")
add_library(kernel OBJECT ${SRC})
