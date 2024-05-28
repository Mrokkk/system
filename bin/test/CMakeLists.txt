file(GLOB src "*.c")

add_executable(
    test
    ${src}
    $<TARGET_OBJECTS:common>
)

target_link_libraries(
    test
    PRIVATE
        "-Wl,-Map=test.map"
)
