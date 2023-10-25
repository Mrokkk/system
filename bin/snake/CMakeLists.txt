file(GLOB src "*.c")

add_executable(
    snake
    ${src}
    $<TARGET_OBJECTS:common>
)

target_link_libraries(
    snake
    PRIVATE
        libc
        "-Wl,-Map=snake.map"
)
