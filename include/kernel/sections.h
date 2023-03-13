#pragma once

extern char _stext[], _etext[];
extern char _sdata[], _edata[];
extern char _smodules_data[], _emodules_data[];
extern char _sbss[], _ebss[];
extern char _end[];

#define is_kernel_text(addr) \
    ((uint32_t)(addr) >= (uint32_t)_stext && (uint32_t)(addr) <= (uint32_t)_etext)
