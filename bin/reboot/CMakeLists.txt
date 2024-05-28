file(GLOB SRC "*.c")

add_executable(
    reboot
    ${SRC}
    $<TARGET_OBJECTS:common>
)

target_link_libraries(
    reboot
    PRIVATE
        "-Wl,-Map=reboot.map"
)
