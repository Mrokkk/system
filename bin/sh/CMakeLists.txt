file(GLOB bin_sh_src "*.c")

add_executable(
    sh
    ${bin_sh_src}
    $<TARGET_OBJECTS:common>
)

target_link_libraries(
    sh
    PRIVATE
        libc
        "-Wl,-Map=sh.map"
)
