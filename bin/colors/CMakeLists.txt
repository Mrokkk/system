file(GLOB SRC "*.c")

add_executable(
    colors
    ${SRC}
    $<TARGET_OBJECTS:common>
)

target_link_libraries(
    colors
    PRIVATE
        libc
        "-Wl,-Map=colors.map"
)
