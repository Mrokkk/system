#include <kernel/kernel.h>
#include <arch/descriptor.h>
#include <arch/segment.h>
#include <arch/page.h>

struct page_directory kernel_directories[1024] __attribute__((aligned(4096)));
struct page_table kernel_tables1[1024] __attribute__((aligned(4096)));
struct page_table kernel_tables2[1024] __attribute__((aligned(4096)));

struct page_directory user_directories[1024] __attribute__((aligned(4096)));
struct page_table user_tables1[1024] __attribute__((aligned(4096)));
struct page_table user_tables2[1024] __attribute__((aligned(4096)));

/*===========================================================================*
 *                                paging_init                                *
 *===========================================================================*/
int paging_init() {

    int i;

    for(i = 0; i < 1024; i++) {
        kernel_directories[i].present = 0;
        kernel_directories[i].writeable = 1;
        kernel_tables1[i].present = 1;
        kernel_tables1[i].writeable = 1;
        kernel_tables1[i].address = (i * 0x1000) >> 12;
        kernel_tables2[i].present = 1;
        kernel_tables2[i].writeable = 1;
        kernel_tables2[i].address = (1024 * 0x1000 + (i * 0x1000)) >> 12;
    }

    for (i = 0; i < 1024; i++) {
        user_directories[i].present = 0;
        user_directories[i].writeable = 1;
        user_directories[i].user_access = 1;
        user_tables1[i].present = 1;
        user_tables1[i].writeable = 1;
        user_tables1[i].user_access = 1;
        user_tables1[i].address = (i * 0x1000) >> 12;
        user_tables2[i].present = 1;
        user_tables2[i].writeable = 1;
        user_tables2[i].user_access = 1;
        user_tables2[i].address = (1024 * 0x1000 + (i * 0x1000)) >> 12;
    }

    kernel_directories[0].present = 1;
    kernel_directories[0].address = ((unsigned int)kernel_tables1) >> 12;

    user_directories[0].present = 1;
    user_directories[0].address = ((unsigned int)user_tables1) >> 12;

    page_directory_load(kernel_directories);
    paging_enable();

    return 0;

}
