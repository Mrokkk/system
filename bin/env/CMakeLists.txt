file(GLOB src "*.c")

add_executable(
    env
    ${src}
    $<TARGET_OBJECTS:common>
)

target_link_libraries(
    env
    PRIVATE
        c
        "-Wl,-Map=env.map"
)
