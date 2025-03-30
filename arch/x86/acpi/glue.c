#define log_fmt(fmt) "acpi: " fmt
#include <arch/io.h>
#include <arch/pci.h>
#include <arch/acpi.h>
#include <arch/bios.h>

#include <kernel/div.h>
#include <kernel/irq.h>
#include <kernel/time.h>
#include <kernel/mutex.h>
#include <kernel/kernel.h>
#include <kernel/process.h>
#include <kernel/compiler.h>
#include <kernel/spinlock.h>
#include <kernel/byteorder.h>
#include <kernel/page_mmio.h>
#include <kernel/semaphore.h>
#include <kernel/page_alloc.h>

#include <uacpi/event.h>
#include <uacpi/sleep.h>
#include <uacpi/uacpi.h>
#include <uacpi/status.h>
#include <uacpi/resources.h>
#include <uacpi/utilities.h>

#define DEBUG_UACPI    0
#define RSDP_SIGNATURE U32('R', 'S', 'D', ' ')

uacpi_status uacpi_kernel_get_rsdp(uacpi_phys_addr* out_rsdp_address)
{
    void* rsdp = bios_find(RSDP_SIGNATURE);

    if (unlikely(!rsdp))
    {
        log_notice("no RSDP found");
        return UACPI_STATUS_NOT_FOUND;
    }

    *out_rsdp_address = addr(rsdp);

    return UACPI_STATUS_OK;
}

void* uacpi_kernel_alloc(uacpi_size size)
{
    // FIXME: add proper allocator which handles all sizes
    if (size > 1024)
    {
        size = page_align(size);
        page_t* pages = page_alloc(size / PAGE_SIZE, PAGE_ALLOC_CONT);

        if (unlikely(!pages))
        {
            return NULL;
        }

        return page_virt_ptr(pages);
    }
    void* block = slab_alloc(size);

    if (unlikely(!block))
    {
        log_warning("cannot allocate %u bytes", size);
        return block;
    }

    return block;
}

void uacpi_kernel_free(void* mem, uacpi_size size_hint)
{
    if (unlikely(!mem))
    {
        return;
    }

    if (size_hint > 1024)
    {
        return;
    }

    return slab_free(mem, size_hint);
}

void* uacpi_kernel_map(uacpi_phys_addr addr, uacpi_size len)
{
    uintptr_t aligned = addr & PAGE_ADDRESS;
    uintptr_t offset = addr & PAGE_MASK;

    len += offset;

    void* mapped = mmio_map_uc(aligned, len, "uAPIC");

    if (unlikely(!mapped))
    {
        return mapped;
    }

    return mapped + offset;
}

void uacpi_kernel_unmap(void* addr, uacpi_size)
{
    if (unlikely(!addr))
    {
        return;
    }
    mmio_unmap(ptr(addr(addr) & PAGE_ADDRESS));
}

uacpi_handle uacpi_kernel_create_event(void)
{
    semaphore_t* s = alloc(semaphore_t);

    if (unlikely(!s))
    {
        return NULL;
    }

    semaphore_init(s, 0);

    return s;
}

void uacpi_kernel_free_event(uacpi_handle handle)
{
    semaphore_t* s = handle;
    delete(s);
}

uacpi_bool uacpi_kernel_wait_for_event(uacpi_handle handle, uacpi_u16)
{
    semaphore_down(handle);
    return UACPI_TRUE;
}

void uacpi_kernel_signal_event(uacpi_handle handle)
{
    semaphore_up(handle);
}

void uacpi_kernel_reset_event(uacpi_handle handle)
{
    semaphore_init(handle, 0);
}

uacpi_handle uacpi_kernel_create_spinlock(void)
{
    spinlock_t* s = alloc(spinlock_t);

    if (unlikely(!s))
    {
        return s;
    }

    spinlock_init(s);

    return s;
}

void uacpi_kernel_free_spinlock(uacpi_handle opaque)
{
    spinlock_t* s = opaque;
    delete(s);
}

uacpi_cpu_flags uacpi_kernel_lock_spinlock(uacpi_handle handle)
{
    flags_t flags;
    spinlock_t* s = handle;

    irq_save(flags);
    spinlock_lock(s);

    return flags;
}

void uacpi_kernel_unlock_spinlock(uacpi_handle handle, uacpi_cpu_flags flags)
{
    spinlock_t* s = handle;
    spinlock_unlock(s);
    irq_restore(flags);
}

uacpi_handle uacpi_kernel_create_mutex(void)
{
    mutex_t* mutex = alloc(mutex_t);

    if (unlikely(!mutex))
    {
        return NULL;
    }

    mutex_init(mutex);
    return mutex;
}

void uacpi_kernel_free_mutex(uacpi_handle opaque)
{
    mutex_t* mutex = opaque;
    delete(mutex);
}

uacpi_status uacpi_kernel_acquire_mutex(uacpi_handle opaque, uacpi_u16 timeout)
{
    mutex_t* mutex = opaque;
    mutex_lock(mutex);
    UNUSED(timeout); // TODO
    return UACPI_STATUS_OK;
}

void uacpi_kernel_release_mutex(uacpi_handle opaque)
{
    mutex_t* mutex = opaque;
    mutex_unlock(mutex);
}

void uacpi_kernel_stall(uacpi_u8 usec)
{
    // FIXME: make i8253 delay work properly
    udelay(usec * 10);
}

void uacpi_kernel_sleep(uacpi_u64 msec)
{
    if (msec > 0xffffffff)
    {
        log_error("invalid delay value");
    }
    mdelay((size_t)msec);
}

uacpi_u64 uacpi_kernel_get_nanoseconds_since_boot(void)
{
    timeval_t t;
    timestamp_update();
    timestamp_get(&t);
    return (uint64_t)t.tv_sec * NSEC_IN_SEC + (uint64_t)t.tv_usec * 1000;
}

static inline loglevel_t loglevel_get(uacpi_log_level lvl)
{
    switch (lvl)
    {
        case UACPI_LOG_ERROR: return KERN_ERR;
        case UACPI_LOG_WARN:  return KERN_WARN;
        case UACPI_LOG_INFO:  return KERN_INFO;
        case UACPI_LOG_TRACE: return KERN_DEBUG;
        case UACPI_LOG_DEBUG: return KERN_DEBUG;
        default:              return KERN_INFO;
    }
}

void uacpi_kernel_log(uacpi_log_level lvl, const uacpi_char* string)
{
    loglevel_t loglevel = loglevel_get(lvl);

    size_t len = strlen(string);

    // uACPI puts newline at the end
    ((char*)string)[len - 1] = 0;

    log(loglevel, "uacpi: %s", string);
}

uacpi_status uacpi_kernel_handle_firmware_request(uacpi_firmware_request*)
{
    log_warning("%s: not implemented", __func__);
    return UACPI_STATUS_UNIMPLEMENTED;
}

uacpi_status uacpi_kernel_io_map(uacpi_io_addr base, uacpi_size, uacpi_handle* handle)
{
    if (unlikely(base > 0xffff))
    {
        return UACPI_STATUS_INVALID_ARGUMENT;
    }

    *handle = ptr(base & 0xffff);

    return UACPI_STATUS_OK;
}

void uacpi_kernel_io_unmap(uacpi_handle)
{
}

uacpi_status uacpi_kernel_io_read8(uacpi_handle handle, uacpi_size offset, uacpi_u8* out_value)
{
    uint16_t base = (uintptr_t)handle;
    *out_value = inb(base + offset);
    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_io_read16(uacpi_handle handle, uacpi_size offset, uacpi_u16* out_value)
{
    uint16_t base = (uintptr_t)handle;
    *out_value = inw(base + offset);
    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_io_read32(uacpi_handle handle, uacpi_size offset, uacpi_u32* out_value)
{
    uint16_t base = (uintptr_t)handle;
    *out_value = inl(base + offset);
    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_io_write8(uacpi_handle handle, uacpi_size offset, uacpi_u8 in_value)
{
    uint16_t base = (uintptr_t)handle;
    outb(in_value, base + offset);
    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_io_write16(uacpi_handle handle, uacpi_size offset, uacpi_u16 in_value)
{
    uint16_t base = (uintptr_t)handle;
    outw(in_value, base + offset);
    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_io_write32(uacpi_handle handle, uacpi_size offset, uacpi_u32 in_value)
{
    uint16_t base = (uintptr_t)handle;
    outl(in_value, base + offset);
    return UACPI_STATUS_OK;
}

#define UACPI_PRIVATE ((void*)(-1))

uacpi_status uacpi_kernel_pci_device_open(uacpi_pci_address address, uacpi_handle* out_handle)
{
    pci_device_t* device = pci_device_get_at(address.bus, address.device, address.function);

    log_debug(DEBUG_UACPI, "[%02x:%02x.%x]", address.bus, address.device, address.function);

    if (unlikely(!device))
    {
        device = zalloc(pci_device_t);

        if (unlikely(!device))
        {
            return UACPI_STATUS_OUT_OF_MEMORY;
        }

        device->bus = address.bus;
        device->slot = address.device;
        device->func = address.function;
        device->private = UACPI_PRIVATE;

        return UACPI_STATUS_OK;
    }

    *out_handle = device;

    return UACPI_STATUS_OK;
}

void uacpi_kernel_pci_device_close(uacpi_handle handle)
{
    pci_device_t* device = handle;

    log_debug(DEBUG_UACPI, "[%02x:%02x.%x]", device->bus, device->slot, device->func);

    if (device->private == UACPI_PRIVATE)
    {
        delete(device);
    }
}

uacpi_status uacpi_kernel_pci_read8(uacpi_handle handle, uacpi_size offset, uacpi_u8* value)
{
    int status = 0;
    pci_device_t* device = handle;
    uint8_t data = pci_config_readb(device, offset, &status);

    log_debug(DEBUG_UACPI, "[%02x:%02x.%x] @ %#x = %#x", device->bus, device->slot, device->func, offset, data);

    if (unlikely(status))
    {
        return UACPI_STATUS_INVALID_ARGUMENT;
    }

    *value = data;
    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_pci_read16(uacpi_handle handle, uacpi_size offset, uacpi_u16* value)
{
    int status = 0;
    pci_device_t* device = handle;
    uint16_t data = pci_config_readw(device, offset, &status);

    log_debug(DEBUG_UACPI, "[%02x:%02x.%x] @ %#x = %#x", device->bus, device->slot, device->func, offset, data);

    if (unlikely(status))
    {
        return UACPI_STATUS_INVALID_ARGUMENT;
    }

    *value = data;
    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_pci_read32(uacpi_handle handle, uacpi_size offset, uacpi_u32* value)
{
    int status = 0;
    pci_device_t* device = handle;
    uint32_t data = pci_config_readl(device, offset, &status);

    log_debug(DEBUG_UACPI, "[%02x:%02x.%x] @ %#x = %#x", device->bus, device->slot, device->func, offset, data);

    if (unlikely(status))
    {
        return UACPI_STATUS_INVALID_ARGUMENT;
    }

    *value = data;
    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_pci_write8(uacpi_handle handle, uacpi_size offset, uacpi_u8 value)
{
    pci_device_t* device = handle;
    log_debug(DEBUG_UACPI, "[%02x:%02x.%x] @ %#x = %#x", device->bus, device->slot, device->func, offset, value);

    if (unlikely(pci_config_writeb(device, offset, value)))
    {
        return UACPI_STATUS_INVALID_ARGUMENT;
    }

    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_pci_write16(uacpi_handle handle, uacpi_size offset, uacpi_u16 value)
{
    pci_device_t* device = handle;
    log_debug(DEBUG_UACPI, "[%02x:%02x.%x] @ %#x = %#x", device->bus, device->slot, device->func, offset, value);

    if (unlikely(pci_config_writew(device, offset, value)))
    {
        return UACPI_STATUS_INVALID_ARGUMENT;
    }

    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_pci_write32(uacpi_handle handle, uacpi_size offset, uacpi_u32 value)
{
    pci_device_t* device = handle;
    log_debug(DEBUG_UACPI, "[%02x:%02x.%x] @ %#x = %#x", device->bus, device->slot, device->func, offset, value);

    if (unlikely(pci_config_writel(device, offset, value)))
    {
        return UACPI_STATUS_INVALID_ARGUMENT;
    }

    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_schedule_work(uacpi_work_type, uacpi_work_handler, uacpi_handle)
{
    log_warning("%s: not implemented", __func__);
    return UACPI_STATUS_UNIMPLEMENTED;
}

uacpi_status uacpi_kernel_wait_for_work_completion(void)
{
    log_warning("%s: not implemented", __func__);
    return UACPI_STATUS_UNIMPLEMENTED;
}

uacpi_thread_id uacpi_kernel_get_thread_id(void)
{
    return ptr(process_current->pid);
}

struct uacpi_irq
{
    uint32_t                nr;
    uacpi_handle            ctx;
    uacpi_interrupt_handler handler;
};

typedef struct uacpi_irq uacpi_irq_t;

static void uacpi_irq_handler(uint32_t nr, void* private, pt_regs_t*)
{
    uacpi_irq_t* irq = private;
    log_debug(DEBUG_UACPI, "%u", nr);
    irq->handler(irq->ctx);
}

uacpi_status uacpi_kernel_install_interrupt_handler(
    uacpi_u32 irq,
    uacpi_interrupt_handler handler,
    uacpi_handle ctx,
    uacpi_handle* out_irq_handle)
{
    if (unlikely(!handler))
    {
        return UACPI_STATUS_INVALID_ARGUMENT;
    }

    log_debug(DEBUG_UACPI, "%u", irq);

    uacpi_irq_t* uacpi_irq = zalloc(uacpi_irq_t);

    if (unlikely(!uacpi_irq))
    {
        return UACPI_STATUS_OUT_OF_MEMORY;
    }

    uacpi_irq->nr = irq;
    uacpi_irq->ctx = ctx;
    uacpi_irq->handler = handler;

    if (irq_register(irq, &uacpi_irq_handler, "ACPI", IRQ_DEFAULT, uacpi_irq))
    {
        return UACPI_STATUS_ALREADY_EXISTS;
    }

    *out_irq_handle = uacpi_irq;

    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_uninstall_interrupt_handler(uacpi_interrupt_handler, uacpi_handle irq_handle)
{
    uacpi_irq_t* irq = irq_handle;
    irq_disable(irq->nr);
    delete(irq); // TODO: Add real unregister
    return UACPI_STATUS_OK;
}

unsigned int __atomic_fetch_add_4(volatile void *ptr, unsigned int val, int)
{
    asm volatile(
        "lock xaddl %0, %1"
        : "+r" (val), "+m" (*(unsigned int*)ptr)
        :: "memory");
    return val;
}

unsigned int __atomic_fetch_sub_4(volatile void *ptr, unsigned int val, int)
{
    unsigned int orig = val;
    val = -val;
    asm volatile(
        "lock xaddl %0, %1"
        : "+r" (val), "+m" (*(unsigned int*)ptr)
        :: "memory");
    return orig;
}

bool __atomic_compare_exchange_4(volatile void* ptr, void* expected, unsigned int desired, bool, int, int)
{
    char success;
    asm volatile (
        "lock cmpxchgl %3, %1;"
        "sete %0"
        : "=q" (success), "+m" (*(volatile unsigned int*)ptr), "+a" (*(unsigned int*)expected)
        : "r" (desired)
        : "memory");
    return success;
}

#ifdef __i386__
uint64_t __udivdi3(uint64_t a, uint64_t b)
{
    do_div(a, b);
    return a;
}

uint64_t __udivmoddi4(uint64_t a, uint64_t b, uint64_t *c)
{
    *c = do_div(a, b);
    return a;
}

uint64_t __umoddi3(uint64_t a, uint64_t b)
{
    return do_div(a, b);
}

int __ffsdi2(uint64_t x)
{
    return (x == 0) ? 0 : __builtin_ctz(x) + 1;
}
#endif
