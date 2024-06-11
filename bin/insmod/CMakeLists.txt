file(GLOB src "*.c")

add_executable(
    insmod
    ${src}
    $<TARGET_OBJECTS:common>
)

target_link_libraries(
    insmod
    PRIVATE
        c
        "-Wl,-Map=insmod.map"
)
