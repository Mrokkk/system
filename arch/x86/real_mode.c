#include <arch/bios.h>
#include <arch/real_mode.h>
#include <arch/descriptor.h>

#include <kernel/kernel.h>

extern void real_mode_call(void);

static_assert(offsetof(low_mem_t, code) == CODE_ADDRESS);
static_assert(offsetof(low_mem_t, real_mode_code) == PROC_ADDRESS);
static_assert(offsetof(low_mem_t, idt) == IDT_ADDRESS);
static_assert(offsetof(low_mem_t, gdt) == GDT_ADDRESS);
static_assert(offsetof(low_mem_t, saved_stack) == SAVED_STACK_ADDRESS);
static_assert(offsetof(low_mem_t, params) == PARAMS_ADDRESS);

static low_mem_t* low_mem;
extern unsigned int real_mode_code_size;
extern void* real_mode_code_address;

void real_mode_call_init(void)
{
    // When low_mem is initialized to NULL, gcc treats
    // every access to it as an access to nullptr. Because
    // of that, memcpy of idtr_real is inlined to mov and ud2
    // instructions, the latter causing a #UD exception.
    // null_get implemented elsewhere is to trick gcc that it's
    // not a nullptr
    extern void* null_get(void);
    extern void protected_16();
    extern void real_mode_call_end();

    struct
    {
        uint16_t limit;
        uint32_t offset;
    } PACKED idtr_real = {
        .limit = 0x3ff,
        .offset = 0
    };

    low_mem = null_get();

    ASSERT(real_mode_code_size < 0x200);

    memcpy(low_mem->code, real_mode_code_address, real_mode_code_size);
    memcpy(low_mem->idt, &idtr_real, sizeof(idtr_real));
    memcpy(low_mem->gdt, &gdt, sizeof(gdt));
}

void bios_call(uint32_t function, regs_t* param)
{
    memcpy(low_mem->params, param, sizeof(*param));

    // int <function>; ret
    low_mem->real_mode_code[0] = 0xcd;
    low_mem->real_mode_code[1] = function;
    low_mem->real_mode_code[2] = 0xc3;

    real_mode_call();

    memcpy(param, low_mem->params, sizeof(*param));
}

void real_mode_reboot(void)
{
    // ljmp $0xffff, $0x0
    low_mem->real_mode_code[0] = 0xea;
    low_mem->real_mode_code[1] = 0x00;
    low_mem->real_mode_code[2] = 0x00;
    low_mem->real_mode_code[3] = 0xff;
    low_mem->real_mode_code[4] = 0xff;
    low_mem->real_mode_code[5] = 0x00;
    low_mem->real_mode_code[6] = 0x00;

    real_mode_call();
}
