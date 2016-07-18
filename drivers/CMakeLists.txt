file(GLOB drivers_src "*.c" "char/*.c")
add_library(drivers OBJECT ${drivers_src} char)
