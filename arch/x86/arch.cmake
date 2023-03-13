set(LINKER_SCRIPT ${PROJECT_SOURCE_DIR}/arch/x86/linker.ld)
set(LINKER_FLAGS_ARCH "-Wl,-T${LINKER_SCRIPT} -Wl,--oformat -Wl,elf32-i386 -m32 -Wl,-m -Wl,elf_i386 -Wl,--build-id=none")
set(C_FLAGS_ARCH "-m32 -mno-sse -mno-sse2 -mno-sse3 -mno-sse4 -mno-avx")
