file(GLOB src "*.c")

add_executable(
    loop
    ${src}
    $<TARGET_OBJECTS:common>
)

target_link_libraries(
    loop
    PRIVATE
        "-Wl,-Map=loop.map"
)
