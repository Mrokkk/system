file(GLOB src "*.c")

add_executable(
    display
    ${src}
    $<TARGET_OBJECTS:common>
)

target_link_libraries(
    display
    PRIVATE
        "-Wl,-Map=display.map"
)
