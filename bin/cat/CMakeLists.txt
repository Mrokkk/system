file(GLOB SRC "*.c")

add_application(
    ${SRC}
)

target_compile_options(cat PRIVATE -std=c89 -pedantic)
