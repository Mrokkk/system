add_executable(loader loader.c)

target_link_libraries(loader c)

add_dependencies(rootfs loader)
