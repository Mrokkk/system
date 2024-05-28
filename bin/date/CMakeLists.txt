file(GLOB src "*.c")

add_executable(
    date
    ${src}
    $<TARGET_OBJECTS:common>
)

target_link_libraries(
    date
    PRIVATE
        "-Wl,-Map=test2.map"
)
