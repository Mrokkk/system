file(GLOB src "*.c")

set_property(SOURCE ${PROJECT_SOURCE_DIR}/../lib/start.S PROPERTY LANGUAGE C)

add_executable(
    test2
    ${src}
    $<TARGET_OBJECTS:common>
    ${PROJECT_SOURCE_DIR}/../lib/start.S
)

target_link_libraries(
    test2
    PRIVATE
        libc
        "-Wl,-Map=test2.map"
)
