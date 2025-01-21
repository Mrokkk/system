#pragma once

#ifndef __ASSEMBLER__

#if defined __i386__
#define BITS_PER_LONG 32
#elif defined __x86_64__
#define BITS_PER_LONG 64
#endif

#endif
