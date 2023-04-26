file(GLOB SRC "*.c")

add_executable(
    cat
    ${SRC}
    $<TARGET_OBJECTS:common>
)

target_link_libraries(
    cat
    PRIVATE
        libc
        "-Wl,-Map=cat.map"
)
