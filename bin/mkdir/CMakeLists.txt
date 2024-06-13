file(GLOB src "*.c")

add_executable(
    mkdir
    ${src}
    $<TARGET_OBJECTS:common>
)

target_link_libraries(
    mkdir
    PRIVATE
        c
        "-Wl,-Map=mkdir.map"
)
