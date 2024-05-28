file(GLOB SRC "*.c")

add_executable(
    ls
    ${SRC}
    $<TARGET_OBJECTS:common>
)

target_link_libraries(
    ls
    PRIVATE
        "-Wl,-Map=ls.map"
)
