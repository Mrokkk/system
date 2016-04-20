#include <kernel/module.h>

KERNEL_MODULE(fat);
module_init(fat_init);
module_exit(fat_deinit);

int fat_init() {

    return 0;

}

int fat_deinit() {

    return 0;

}

