file(GLOB src "*.c")

add_executable(
    display
    ${src}
    $<TARGET_OBJECTS:common>
)

target_link_libraries(
    display
    PRIVATE
        libc
        "-Wl,-Map=display.map"
)
