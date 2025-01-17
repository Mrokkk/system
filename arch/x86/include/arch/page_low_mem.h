#pragma once

#include <kernel/page_types.h>

void page_low_mem_map(pgd_t** pgd);
void page_low_mem_unmap(pgd_t* pgd);
