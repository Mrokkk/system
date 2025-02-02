if(BITS EQUAL 64)

    set(LINKER_SCRIPT ${PROJECT_SOURCE_DIR}/arch/x86/linker_64.ld)
    set(EMULATION "-Wl,-m -Wl,elf_x86_64")
    set(C_FLAGS_ARCH "-m${BITS} -mcmodel=large -mno-red-zone -mno-sse -mno-sse2 -mno-sse3 -mno-sse4 -mno-avx -DCONFIG_X86=64")

elseif(BITS EQUAL 32)

    if(DEFINED CPU)
        if(CPU STREQUAL "i386")
            set(CONFIG_CPU "-DCONFIG_X86=3")
        elseif(CPU STREQUAL "i486")
            set(CONFIG_CPU "-DCONFIG_X86=4")
        elseif(CPU STREQUAL "i586")
            set(CONFIG_CPU "-DCONFIG_X86=5")
        else()
            set(CONFIG_CPU "-DCONFIG_X86=6")
        endif()
        set(CPU_ARCH "-march=${CPU}")
    else()
        set(CONFIG_CPU "-DCONFIG_X86=6")
    endif()

    set(LINKER_SCRIPT ${PROJECT_SOURCE_DIR}/arch/x86/linker_32.ld)
    set(EMULATION "-Wl,-m -Wl,elf_i386")
    set(C_FLAGS_ARCH "-m${BITS} -mno-sse -mno-sse2 -mno-sse3 -mno-sse4 -mno-avx ${CPU_ARCH} ${CONFIG_CPU}")

else()

    message(FATAL_ERROR "Incorrect architecture")

endif()

set(LINKER_FLAGS_ARCH "-Wl,-T${LINKER_SCRIPT} ${EMULATION} -Wl,--no-warn-rwx-segments")
