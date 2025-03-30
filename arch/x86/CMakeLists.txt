if(BITS EQUAL 64)
    # TODO
    set(ASM_SRC "")
else()
    set(ASM_SRC "head_32.S;base.S;real_mode.S;multiboot.S;smp.S")
endif()

set_property(SOURCE ${ASM_SRC} PROPERTY LANGUAGE C)

set(SRC
    mm/cache.c
    mm/mtrr.c
    mm/page.c
    mm/page_mmio.c
    mm/page_print.c
    mm/pat.c

    acpi/acpi.c
    acpi/ec.c
    acpi/glue.c

    agp.c
    apic.c
    apm.c
    arch.cmake
    backtrace_frame.c
    backtrace_guess.c
    backtrace_user.c
    base.S
    base_64.S
    bios.c
    bios32.c
    CMakeLists.txt
    cpuid.c
    descriptor.c
    dmi.c
    earlycon.c
    elf.c
    exception.c
    head_32.S
    head_64.S
    hpet.c
    i8042.c
    i8253.c
    i8259.c
    init_64.c
    irq.c
    linker_32.ld
    linker_64.ld
    memory.c
    memory_dump.c
    mmap.c
    mp.c
    multiboot.c
    multiboot.S
    pci.c
    process.c
    real_mode.c
    real_mode.S
    reboot.c
    register.c
    rtc.c
    semaphore.c
    setup.c
    smp.c
    smp.S
    syscall.c
    tsc.c
    vbe.c
    vga.c
)

include(acpi/uACPI/uacpi.cmake)

add_library(uacpi OBJECT ${UACPI_SOURCES})

target_compile_options(uacpi PRIVATE -fno-analyzer -DUACPI_SIZED_FREES=1 -DUACPI_PHYS_ADDR_IS_32BITS=1)

add_library(arch OBJECT ${SRC} ${ASM_SRC})

target_include_directories(uacpi
    PRIVATE
    ${UACPI_INCLUDES}
)

target_include_directories(arch
    PRIVATE
    ${UACPI_INCLUDES}
)

target_compile_options(arch PRIVATE -DUACPI_SIZED_FREES=1 -DUACPI_PHYS_ADDR_IS_32BITS=1)
