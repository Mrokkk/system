#define log_fmt(fmt) "virtio: " fmt
#include <stdarg.h>
#include <stddef.h>
#include <kernel/time.h>
#include <kernel/kernel.h>
#include <kernel/malloc.h>
#include <kernel/virtio.h>
#include <kernel/execute.h>
#include <kernel/page_mmio.h>
#include <kernel/page_alloc.h>

#define BUFFER_ALLOC_FLAGS BUFFER_PAGES > 1 ? PAGE_ALLOC_CONT : PAGE_ALLOC_DISCONT

void virtio_buffers_submit(virtio_queue_t* queue, ...)
{
    va_list args;
    int prev_id = -1;
    int first_id = -1;

    va_start(args, queue);
    while (1)
    {
        virtio_buffer_t* buffer = va_arg(args, virtio_buffer_t*);

        if (!buffer)
        {
            break;
        }

        uint16_t id = queue->avail->idx % queue->size;

        if (prev_id != -1)
        {
            queue->desc[prev_id].flags |= VIRTQ_DESC_F_NEXT;
            queue->desc[prev_id].next = id;
        }

        queue->desc[id].addr = buffer->paddr;
        queue->desc[id].flags = buffer->type;
        queue->desc[id].len = buffer->size;
        queue->desc[id].next = 0;
        mb();

        queue->avail->idx++;
        mb();

        prev_id = id;
        if (first_id == -1)
        {
            first_id = id;
        }
    }

    va_end(args);

    queue->avail->ring[first_id] = first_id;
    mb();

    *queue->notify = queue->queue_id;

    while (queue->used->idx != queue->avail->idx);
    mb();
}

void virtio_buffer_submit(virtio_queue_t* queue, virtio_buffer_t* buffer)
{
    uint16_t id = queue->avail->idx % queue->size;
    queue->desc[id].addr = buffer->paddr;
    queue->desc[id].flags = 0;
    queue->desc[id].len = buffer->size;
    queue->desc[id].next = 0;

    queue->avail->ring[id] = id;
    mb();

    queue->avail->idx++;
    mb();

    *queue->notify = queue->queue_id;

    while (queue->used->idx != queue->avail->idx);
}

virtio_buffer_t virtio_buffer_create(virtio_queue_t* queue, size_t size, int type)
{
    char* buffer = queue->buffer_cur;

    queue->buffer_cur += size;

    if (queue->buffer_cur - (char*)page_virt_ptr(queue->buffers) >= BUFFER_PAGES * PAGE_SIZE)
    {
        buffer = page_virt_ptr(queue->buffers);
        queue->buffer_cur = buffer + size;
    }

    return (virtio_buffer_t){
        .buffer = buffer,
        .paddr = phys_addr(buffer),
        .type = type,
        .size = size
    };
}

static virtio_device_t* virtio_device_create(size_t num_queues)
{
    size_t size = sizeof(virtio_device_t) + num_queues * sizeof(virtio_queue_t);
    virtio_device_t* device = slab_alloc(size);

    if (unlikely(!device))
    {
        return NULL;
    }

    memset(device, 0, size);

    return device;
}

static int virtio_cap_read(virtio_device_t* device, void** notify_bar_ptr, uint32_t* notify_cap_off, uint32_t* notify_off_multiplier)
{
    int notify_bar = 0;
    pci_cap_t cap = {};
    void* mapped_bars[6] = {};
    virtio_pci_cap_t virtio_cap[5] = {};

    for (uint8_t ptr = device->pci_device->capabilities; ptr; ptr = cap.next)
    {
        if (pci_config_read(device->pci_device, ptr, &cap, sizeof(cap)))
        {
            log_warning("read failed");
            continue;
        }

        if (cap.id == PCI_CAP_ID_VNDR)
        {
            int index = cap.data - 1;
            pci_config_read(device->pci_device, ptr, &virtio_cap[index], sizeof(*virtio_cap));

            int bar = virtio_cap[index].bar;

            if (device->pci_device->bar[bar].region == PCI_IO ||
                !device->pci_device->bar[bar].addr)
            {
                continue;
            }

            if (!mapped_bars[bar])
            {
                mapped_bars[bar] = mmio_map_uc(
                    device->pci_device->bar[bar].addr,
                    device->pci_device->bar[bar].size,
                    "virtio_device");
            }

            switch (virtio_cap[index].cfg_type)
            {
                case VIRTIO_PCI_CAP_COMMON_CFG:
                    device->common_cfg = shift_as(
                        virtio_pci_common_cfg_t*,
                        mapped_bars[bar],
                        virtio_cap[index].offset);
                    break;
                case VIRTIO_PCI_CAP_NOTIFY_CFG:
                    pci_config_read(device->pci_device, ptr + sizeof(*virtio_cap), notify_off_multiplier, 4);
                    *notify_cap_off = virtio_cap[index].offset;
                    notify_bar = virtio_cap[index].bar;
                    break;
                case VIRTIO_PCI_CAP_DEVICE_CFG:
                    device->config_offset = ptr + sizeof(*virtio_cap);
                    break;
            }
        }
    }

    if (unlikely(!notify_bar || !device->common_cfg))
    {
        return -1;
    }

    *notify_bar_ptr = mapped_bars[notify_bar];

    return 0;
}

static bool virtio_failed(virtio_device_t* device)
{
    return device->common_cfg->device_status & VIRTIO_FAILED;
}

static int virtio_reset(virtio_device_t* device)
{
    unsigned time_elapsed = 0;

    device->common_cfg->device_status = 0;

    while (device->common_cfg->device_status != 0)
    {
        if (unlikely(time_elapsed > 1000))
        {
            log_warning("device reset timeout");
            return -1;
        }

        udelay(10);
    }

    if (unlikely(virtio_failed(device)))
    {
        log_warning("device failed after reset");
        return -1;
    }

    return 0;
}

static int virtio_initialize_internal(virtio_device_t* device)
{
    if (virtio_reset(device))
    {
        return 1;
    }

    device->common_cfg->device_status = VIRTIO_ACKNOWLEDGE;
    device->common_cfg->device_status |= VIRTIO_FEATURES_OK;

    if (device->common_cfg->device_status != (VIRTIO_ACKNOWLEDGE | VIRTIO_FEATURES_OK) ||
        virtio_failed(device))
    {
        log_warning("initialization failed; status: %x", device->common_cfg->device_status);
        return 1;
    }

    return 0;
}

static int virtio_queues_setup(virtio_device_t* device, size_t num_queues, void* notify_bar, uint32_t notify_cap_off, uint32_t notify_off_multiplier)
{
    page_t* buffer;
    page_t* control;
    size_t queue_size = device->common_cfg->queue_size;

    off_t desc_off = 0;

    off_t driver_off
        = desc_off
        + sizeof(virtq_desc_t) * queue_size;

    off_t device_off
        = driver_off
        + align(sizeof(virtq_avail_t) + queue_size * sizeof(uint16_t), VIRTQ_ALIGN);

    size_t required_size
        = device_off
        + align(sizeof(virtq_used_t) + queue_size * sizeof(virtq_used_elem_t), VIRTQ_ALIGN);

    log_info("[%s] required size of control data per queue: %u B, queue size: %u", device->name, required_size, queue_size);

    if (required_size > PAGE_SIZE)
    {
        log_warning("size is bigger than page size");
        return -1;
    }

    for (size_t i = 0; i < num_queues; ++i)
    {
        buffer = page_alloc(BUFFER_PAGES, BUFFER_ALLOC_FLAGS | PAGE_ALLOC_ZEROED);

        if (unlikely(!buffer))
        {
            return -ENOMEM;
        }

        control = page_alloc(1, PAGE_ALLOC_UNCACHED | PAGE_ALLOC_ZEROED);

        if (unlikely(!control))
        {
            pages_free(buffer);
            return -ENOMEM;
        }

        device->common_cfg->queue_select = i;
        device->common_cfg->queue_desc = page_phys(control) + desc_off;
        device->common_cfg->queue_driver = page_phys(control) + driver_off;
        device->common_cfg->queue_device = page_phys(control) + device_off;

        device->queues[i].queue_id = i;
        device->queues[i].buffers = buffer;
        device->queues[i].buffer_cur = page_virt_ptr(buffer);
        device->queues[i].buffer_cur = page_virt_ptr(buffer);
        device->queues[i].desc = shift_as(void*, page_virt(control), desc_off);
        device->queues[i].avail = shift_as(void*, page_virt(control), driver_off);
        device->queues[i].used = shift_as(void*, page_virt(control), device_off);

        device->queues[i].avail->flags = VIRTQ_AVAIL_F_NO_INTERRUPT;
        device->queues[i].used->flags = VIRTQ_USED_F_NO_NOTIFY;

        device->queues[i].size = queue_size;

        if (unlikely(virtio_failed(device)))
        {
            log_warning("queue setup failed");
            return -1;
        }
    }

    for (size_t i = 0; i < num_queues; ++i)
    {
        device->common_cfg->queue_select = i;
        device->common_cfg->queue_enable = 1;

        if (unlikely(virtio_failed(device)))
        {
            log_warning("queue setup failed");
            return -1;
        }

        device->queues[i].notify = shift_as(
            void*,
            notify_bar,
            notify_cap_off + device->common_cfg->queue_notify_off * notify_off_multiplier);
    }

    return 0;
}

int virtio_initialize(const char* name, pci_device_t* pci_device, size_t num_queues, virtio_device_t** result)
{
    virtio_device_t* device;

    if (unlikely(!(device = virtio_device_create(num_queues))))
    {
        return -ENOMEM;
    }

    device->name       = name;
    device->pci_device = pci_device;

    void* notify_bar = NULL;
    uint32_t notify_cap_off = 0;
    uint32_t notify_off_multiplier = 0;

    if (execute(pci_device_initialize(pci_device), "[%s] initializing PCI", name) ||
        execute(virtio_cap_read(device, &notify_bar, &notify_cap_off, &notify_off_multiplier), "[%s] reading cap", name) ||
        execute(virtio_initialize_internal(device), "[%s] initializing virtio", name) ||
        execute(virtio_queues_setup(device, num_queues, notify_bar, notify_cap_off, notify_off_multiplier), "[%s] initializing queues", name))
    {
        return -ENODEV;
    }

    device->common_cfg->device_status |= VIRTIO_DRIVER_OK;

    if (unlikely(virtio_failed(device)))
    {
        log_warning("failed");
        return -ENODEV;
    }

    *result = device;

    return 0;
}
