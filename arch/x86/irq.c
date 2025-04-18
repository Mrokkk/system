#define log_fmt(fmt) "irq: " fmt
#include <arch/irq.h>
#include <arch/descriptor.h>

#include <kernel/irq.h>
#include <kernel/time.h>
#include <kernel/kernel.h>

typedef struct
{
    irq_handler_t handler;
    void*         private;
    const char*   name;
    int           flags;
} irq_t;

static irq_chip_t* chips[IRQ_CHIPS_COUNT];
static irq_chip_t* used_chip;
static irq_t irq_list[IRQ_COUNT];

int irq_register(uint32_t nr, irq_handler_t handler, const char* name, int flags, void* private)
{
    if (unlikely(irq_list[nr].handler))
    {
        return -EBUSY;
    }

    log_debug(DEBUG_IRQ, "%u:%s flags: %#x", nr, name, flags);

    irq_list[nr].name    = name;
    irq_list[nr].flags   = flags;
    irq_list[nr].private = private;
    irq_list[nr].handler = handler;

    if (flags & IRQ_ENABLE)
    {
        used_chip->irq_enable(nr, flags);
    }

    return 0;
}

int irq_register_naked(uint32_t nr, irq_naked_handler_t handler, const char* name, int flags)
{
    if (unlikely(irq_list[nr].handler))
    {
        return -EBUSY;
    }

    log_debug(DEBUG_IRQ, "%u:%s flags: %#x", nr, name, flags);

    irq_list[nr].name    = name;
    irq_list[nr].flags   = flags;
    irq_list[nr].private = NULL;
    irq_list[nr].handler = NULL;

    idt_set(nr + used_chip->vector_offset, addr(handler));

    if (flags & IRQ_ENABLE)
    {
        used_chip->irq_enable(nr, flags);
    }

    return 0;
}

int irq_allocate(irq_handler_t handler, const char* name, int flags, void* private, int* irq_vector)
{
    int errno;

    for (size_t i = 16; i < IRQ_COUNT; ++i)
    {
        if (!irq_list[i].handler)
        {
            if (unlikely(errno = irq_register(i, handler, name, flags, private)))
            {
                return errno;
            }

            *irq_vector = i + used_chip->vector_offset;

            return 0;
        }
    }

    return -ENOMEM;
}

void do_irq(uint32_t nr, pt_regs_t* regs)
{
    log_debug(DEBUG_IRQ, "%u", nr);

    if (unlikely(!irq_list[nr].handler))
    {
        log_error("not handled IRQ %u", nr);
        used_chip->eoi(nr);
        return;
    }

    irq_list[nr].handler(nr, irq_list[nr].private, regs);
    used_chip->eoi(nr);
}

void irq_eoi(uint32_t irq)
{
    log_debug(DEBUG_IRQ, "%u", irq);
    used_chip->eoi(irq);
}

int irq_enable(uint32_t irq)
{
    log_debug(DEBUG_IRQ, "%u", irq);
    used_chip->irq_enable(irq, irq_list[irq].flags);
    return 0;
}

int irq_disable(uint32_t irq)
{
    if (unlikely(!irq_list[irq].handler))
    {
        log_warning("disabling not registred IRQ: %u", irq);
        return -EINVAL;
    }

    log_debug(DEBUG_IRQ, "%u", irq);

    if (used_chip->irq_disable)
    {
        used_chip->irq_disable(irq, irq_list[irq].flags);
    }
    return 0;
}

int irq_chip_disable(void)
{
    log_debug(DEBUG_IRQ, "");
    if (used_chip && used_chip->disable)
    {
        used_chip->disable();
    }
    return 0;
}

UNMAP_AFTER_INIT int irq_chip_register(irq_chip_t* chip)
{
    for (int i = 0; i < IRQ_CHIPS_COUNT; ++i)
    {
        if (!chips[i])
        {
            chips[i] = chip;
            return 0;
        }
    }

    return -EBUSY;
}

UNMAP_AFTER_INIT int irqs_initialize(void)
{
    log_info("available chips:");
    for (int i = 0; i < IRQ_CHIPS_COUNT; ++i)
    {
        if (!chips[i])
        {
            continue;
        }

        if (!strcmp(chips[i]->name, "ioapic"))
        {
            used_chip = chips[i];
        }
        else
        {
            used_chip = used_chip ? used_chip : chips[i];
        }

        log_continue(" %s", chips[i]->name);
    }

    if (!used_chip)
    {
        panic("no chip!");
    }

    log_continue("; selected: %s", used_chip->name);

    if (used_chip->initialize)
    {
        used_chip->initialize();
    }

    return 0;
}
