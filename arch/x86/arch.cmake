set(LINKER_SCRIPT ${PROJECT_SOURCE_DIR}/arch/x86/linker.ld)
set(LINKER_FLAGS_ARCH "-Wl,-T${LINKER_SCRIPT} -m32 -Wl,--no-warn-rwx-segments")
set(C_FLAGS_ARCH "-m32 -mno-sse -mno-sse2 -mno-sse3 -mno-sse4 -mno-avx")
