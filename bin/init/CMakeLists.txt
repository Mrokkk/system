file(GLOB SRC "*.c")

add_executable(
    init
    ${SRC}
    $<TARGET_OBJECTS:common>
)

target_link_libraries(
    init
    PRIVATE
        libc
        "-Wl,-Map=init.map"
)
