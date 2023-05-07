file(GLOB SRC "*.c")

add_executable(
    poweroff
    ${SRC}
    $<TARGET_OBJECTS:common>
)

target_link_libraries(
    poweroff
    PRIVATE
        libc
        "-Wl,-Map=poweroff.map"
)
