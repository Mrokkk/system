file(GLOB src "*.c")

add_executable(
    test2
    ${src}
    $<TARGET_OBJECTS:common>
)

target_link_libraries(
    test2
    PRIVATE
        libc
        "-Wl,-Map=test2.map"
)
