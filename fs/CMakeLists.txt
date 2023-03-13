file(GLOB fs_src "*.c")
add_library(fs OBJECT ${fs_src} ramfs/ramfs.c mbfs/mbfs.c devfs/devfs.c)
