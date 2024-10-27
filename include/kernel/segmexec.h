#pragma once

#define CONFIG_SEGMEXEC 1

#ifdef __x86_64__
#undef CONFIG_SEGMEXEC
#define CONFIG_SEGMEXEC 0
#endif

#if CONFIG_SEGMEXEC
#define CODE_START 0x60000000
#endif
