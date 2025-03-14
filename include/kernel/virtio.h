#pragma once

#include <stdint.h>
#include <arch/io.h>
#include <arch/pci.h>
#include <kernel/page_types.h>

// References:
// https://docs.oasis-open.org/virtio/virtio/v1.1/cs01/virtio-v1.1-cs01.html

typedef volatile uint64_t le64;
typedef volatile uint32_t le32;
typedef volatile uint16_t le16;
typedef volatile uint8_t u8;

typedef struct virtq_desc virtq_desc_t;
typedef struct virtq_used virtq_used_t;
typedef struct virtq_avail virtq_avail_t;
typedef struct virtio_queue virtio_queue_t;
typedef struct virtio_buffer virtio_buffer_t;
typedef struct virtio_device virtio_device_t;
typedef struct virtio_pci_cap virtio_pci_cap_t;
typedef struct virtq_used_elem virtq_used_elem_t;
typedef struct virtio_pci_common_cfg virtio_pci_common_cfg_t;

#define VIRTIO_ACKNOWLEDGE          1
#define VIRTIO_DRIVER               2
#define VIRTIO_FEATURES_OK          8
#define VIRTIO_DRIVER_OK            4
#define VIRTIO_DEVICE_NEEDS_RESET   64
#define VIRTIO_FAILED               128

#define VIRTIO_PCI_CAP_COMMON_CFG        1 // Common configuration
#define VIRTIO_PCI_CAP_NOTIFY_CFG        2 // Notifications
#define VIRTIO_PCI_CAP_ISR_CFG           3 // ISR Status
#define VIRTIO_PCI_CAP_DEVICE_CFG        4 // Device specific configuration
#define VIRTIO_PCI_CAP_PCI_CFG           5 // PCI configuration access

struct virtio_pci_cap
{
    u8   cap_vndr;  // Generic PCI field: PCI_CAP_ID_VNDR
    u8   cap_next;  // Generic PCI field: next ptr.
    u8   cap_len;   // Generic PCI field: capability length
    u8   cfg_type;  // Identifies the structure
    u8   bar;       // Where to find it
    u8   pad[3];    // Pad to full dword
    le32 offset;    // Offset within bar
    le32 length;    // Length of the structure, in bytes
};

struct virtio_pci_common_cfg
{
    // About the whole device
    io32 device_feature_select;     // read-write
    io32 device_feature;            // read-only
    io32 driver_feature_select;     // read-write
    io32 driver_feature;            // read-write
    io16 msix_config;               // read-write
    io16 num_queues;                // read-only
    io8 device_status;              // read-write
    io8 config_generation;          // read-only

    // About a specific virtqueue
    io16 queue_select;              // read-write
    io16 queue_size;                // read-write
    io16 queue_msix_vector;         // read-write
    io16 queue_enable;              // read-write
    io16 queue_notify_off;          // read-only
    io64 queue_desc;                // read-write
    io64 queue_driver;              // read-write
    io64 queue_device;              // read-write
};

struct virtio_pci_notify_cap
{
    virtio_pci_cap_t cap;
    le32 notify_off_multiplier; // Multiplier for queue_notify_off
};

struct virtq_desc
{
    le64 addr;  // Address (guest-physical)
    le32 len;   // Length

#define VIRTQ_DESC_F_NEXT       1 // This marks a buffer as continuing via the next field
#define VIRTQ_DESC_F_WRITE      2 // This marks a buffer as device write-only (otherwise device read-only)
#define VIRTQ_DESC_F_INDIRECT   4 // This means the buffer contains a list of buffer descriptors

    le16 flags; // The flags as indicated above
    le16 next;  // Next field if flags & NEXT
};

struct virtq_avail
{
#define VIRTQ_AVAIL_F_NO_INTERRUPT 1
    le16 flags;
    le16 idx;
    le16 ring[ /* Queue Size */ ];
};

struct virtq_used_elem
{
    io32 id;  // Index of start of used descriptor chain
    io32 len; // Total length of the descriptor chain which was used (written to)
};

struct virtq_used
{
#define VIRTQ_USED_F_NO_NOTIFY 1
    io16 flags;
    io16 idx;
    virtq_used_elem_t ring[ /* Queue Size */];
};

struct virtio_queue
{
    int            queue_id;
    page_t*        buffers;
    char*          buffer_cur;
    virtq_desc_t*  desc;
    virtq_avail_t* avail;
    virtq_used_t*  used;
    uint16_t       size;
    io32*          notify;
};

struct virtio_device
{
    const char*              name;
    pci_device_t*            pci_device;
    virtio_pci_common_cfg_t* common_cfg;
    size_t                   config_offset;
    virtio_queue_t           queues[];
};

struct virtio_buffer
{
    void*     buffer;
    uintptr_t paddr;
    int       type;
    size_t    size;
};

#define VIRTQ_ALIGN        64
#define BUFFER_PAGES       4

int virtio_initialize(const char* name, pci_device_t* pci_device, size_t num_queues, virtio_device_t** result);
virtio_buffer_t virtio_buffer_create(virtio_queue_t* queue, size_t size, int type);
void virtio_buffers_submit(virtio_queue_t* queue, ... /*, NULL */);
void virtio_buffer_submit(virtio_queue_t* queue, virtio_buffer_t* buffer);
