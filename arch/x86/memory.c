#define log_fmt(fmt) "e820: " fmt
#include <arch/register.h>
#include <kernel/memory.h>
#include <kernel/kernel.h>
#include <kernel/page_types.h>

#include <arch/bios.h>

struct e820_data
{
    uint64_t base;
    uint64_t len;
    uint32_t type;
    uint32_t acpi_attr;
} PACKED;

typedef struct e820_data e820_data_t;

#define E820_PRINT          1
#define E820_MAGIC          0x534d4150
#define E820_USABLE         0x1
#define E820_RESERVED       0x2
#define E820_ENTRIES_COUNT  MEMORY_AREAS_SIZE
#define E820_DATA_START     0x1000

#define swap(a, b) \
    { \
        typeof(a) __a = a; \
        typeof(b) __b = b; \
        a = __b; \
        b = __a; \
    }

// On QEMU there will be only up to 8 entries, so I'm ok with bubble sort
static void e820_entries_sort(int* sorted, e820_data_t* map, size_t count)
{
    size_t n = count;
    do
    {
        for (size_t i = 0; i < count - 1; ++i)
        {
            if (map[sorted[i]].base > map[sorted[i + 1]].base)
            {
                swap(sorted[i], sorted[i + 1]);
            }
        }
    }
    while (n--);
}

int e820_entries_read(e820_data_t* map, int sorted[])
{
    int count;
    regs_t regs = {0};
    e820_data_t* data = map;

    for (count = 0; count < E820_ENTRIES_COUNT; ++data, ++count)
    {
        sorted[count] = count;

        regs.eax = 0xe820;
        regs.ecx = sizeof(e820_data_t);
        regs.edx = E820_MAGIC;
        regs.di = addr(data);
        bios_call(BIOS_SYSTEM, &regs);

        if (regs.eax != E820_MAGIC || (regs.eflags & EFL_CF))
        {
            log_warning("E820 call failed");
            return 0;
        }

        e820_data_t* e = map + count;
#if E820_PRINT
        log_notice("[mem %#018llx - %#018llx] %s(%#x)",
            e->base,
            e->base + e->len - 1,
            e->type == E820_USABLE ? "usable" : "reserved",
            e->type);
#endif

        if (unlikely(count > 1 && map[count - 1].base == e->base && map[count - 1].len == e->len))
        {
            log_warning("E820 broken");
            return 0;
        }

        if (!regs.ebx)
        {
            break;
        }
    }

    return count + 1;
}

static int memory_mm_type(int type)
{
    switch (type)
    {
        case E820_USABLE:   return MMAP_TYPE_AVL;
        case E820_RESERVED: return MMAP_TYPE_RES;
        default:            return MMAP_TYPE_RES;
    }
}

static void memory_detect_legacy(void)
{
    regs_t regs = {.ah = 0x88};
    bios_call(BIOS_SYSTEM, &regs);

    if (regs.eflags & EFL_CF)
    {
        panic("failed to determine memory map");
    }

    memory_area_add(0, MiB, MMAP_TYPE_LOW);
    memory_area_add(MiB, regs.ax * KiB, MMAP_TYPE_AVL);
    memory_area_add(regs.ax * KiB, 0x100000000ULL, MMAP_TYPE_RES);
}

void memory_detect(void)
{
    int count;
    int sorted[E820_ENTRIES_COUNT];
    e820_data_t* map = ptr(E820_DATA_START);

    if (unlikely(!(count = e820_entries_read(map, sorted))))
    {
        log_warning("fallback to legacy INT 0x10 AH=0x88");
        memory_detect_legacy();
        return;
    }

    e820_entries_sort(sorted, map, count);

    for (int i = 0; i < count; ++i)
    {
        e820_data_t* current = map + sorted[i];
        e820_data_t* next = i < count - 1 ? map + sorted[i + 1] : NULL;
        uint64_t current_start = current->base;
        uint64_t current_end = current->base + current->len;
        uint64_t next_start = next ? next->base : 0;

        if (next && current->type != next->type)
        {
            if (current_end < next->base)
            {
                if (current->type != E820_USABLE)
                {
                    current_end = next->base;
                }
                else if (next->type != E820_USABLE)
                {
                    next->len += next_start - current_end;
                    next->base = current_end;
                }
            }
        }

        if (next && current->type != next->type)
        {
            if (current->type != E820_USABLE)
            {
                uint32_t page_alignment = PAGE_SIZE - (next->base & PAGE_MASK);
                next->base -= page_alignment;
                next->len += page_alignment;
                current_end = next->base;
            }
            else if (next->type != E820_USABLE)
            {
                uint32_t page_alignment = next->base & PAGE_MASK;
                current_end -= page_alignment;
                next->len += next_start - current_end;
                next->base = current_end;
            }
        }

        int mm_type = memory_mm_type(current->type);

        for (; i < count - 1; ++i)
        {
            next = map + sorted[i + 1];
            if (memory_mm_type(next->type) != mm_type)
            {
                e820_data_t* temp = map + sorted[i];
                next = temp != current ? temp : next;
                break;
            }
        }

        if (next && mm_type == memory_mm_type(next->type))
        {
            current_end = next->base + next->len;
        }

        if (current_start == 0 && mm_type == MMAP_TYPE_AVL)
        {
            mm_type = MMAP_TYPE_LOW;
        }

        memory_area_add(current_start, current_end, mm_type);
    }
}
