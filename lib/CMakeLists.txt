set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -nostdinc")
set_property(SOURCE start.S PROPERTY LANGUAGE C)

include_directories(
   ${PROJECT_SOURCE_DIR}/lib/include
)

file(GLOB lib_src "*.c")

add_library(libc STATIC
    ${lib_src}
    start.S
)
