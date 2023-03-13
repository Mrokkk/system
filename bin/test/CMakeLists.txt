file(GLOB src "*.c")

add_executable(
    test
    ${src}
    $<TARGET_OBJECTS:common>
)

target_link_libraries(
    test
    PRIVATE
        libc
        "-Wl,-Map=test.map"
)
