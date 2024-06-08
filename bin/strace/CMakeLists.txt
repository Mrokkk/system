file(GLOB src "*.c")

add_executable(
    strace
    ${src}
    $<TARGET_OBJECTS:common>
)

target_link_libraries(
    strace
    PRIVATE
        "-Wl,-Map=snake.map"
)
