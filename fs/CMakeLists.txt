file(GLOB fs_src "*.c")
add_library(fs OBJECT file_system.c files.c inode.c open.c read_write.c rootfs/root.c)
