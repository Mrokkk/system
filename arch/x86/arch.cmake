
if(BITS EQUAL 64)
    set(LINKER_SCRIPT ${PROJECT_SOURCE_DIR}/arch/x86/linker_64.ld)
    set(EMULATION "")
    set(C_FLAGS_ARCH "-m${BITS} -mcmodel=large -mno-sse -mno-sse2 -mno-sse3 -mno-sse4 -mno-avx")
elseif(BITS EQUAL 32)
    set(LINKER_SCRIPT ${PROJECT_SOURCE_DIR}/arch/x86/linker_32.ld)
    set(EMULATION "-Wl,-m -Wl,elf_i386")
    set(C_FLAGS_ARCH "-m${BITS} -mno-sse -mno-sse2 -mno-sse3 -mno-sse4 -mno-avx")
else()
    message(FATAL_ERROR "Incorrect architecture")
endif()

set(LINKER_FLAGS_ARCH "-Wl,-T${LINKER_SCRIPT} ${EMULATION} -Wl,--no-warn-rwx-segments")
