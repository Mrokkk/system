file(GLOB src "*.c")

add_executable(
    pipe
    ${src}
    $<TARGET_OBJECTS:common>
)

target_link_libraries(
    pipe
    PRIVATE
        c
        "-Wl,-Map=pipe.map"
)
