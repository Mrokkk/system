#pragma once

#include <arch/io.h>
#include <kernel/virtio.h>

#define VIRTIO_GPU_CONTROLQ  0
#define VIRTIO_GPU_CURSORQ   1
#define VIRTIO_GPU_NUMQUEUES 2

#define VIRTIO_GPU_F_VIRGL       (1 << 0)
#define VIRTIO_GPU_F_EDID        (1 << 1)
#define VIRTIO_GPU_EVENT_DISPLAY (1 << 0)

typedef struct virtio_gpu_rect virtio_gpu_rect_t;
typedef struct virtio_gpu_config virtio_gpu_config_t;
typedef struct virtio_gpu_ctrl_hdr virtio_gpu_ctrl_hdr_t;
typedef struct virtio_gpu_mem_entry virtio_gpu_mem_entry_t;
typedef struct virtio_gpu_set_scanout virtio_gpu_set_scanout_t;
typedef struct virtio_gpu_resource_flush virtio_gpu_resource_flush_t;
typedef struct virtio_gpu_resource_unref virtio_gpu_resource_unref_t;
typedef struct virtio_gpu_resp_display_info virtio_gpu_resp_display_info_t;
typedef struct virtio_gpu_resource_create_2d virtio_gpu_resource_create_2d_t;
typedef struct virtio_gpu_transfer_to_host_2d virtio_gpu_transfer_to_host_2d_t;
typedef struct virtio_gpu_resource_attach_backing virtio_gpu_resource_attach_backing_t;

struct virtio_gpu_config
{
    le32 events_read;
    le32 events_clear;
    le32 num_scanouts;
    le32 reserved;
};

enum virtio_gpu_ctrl_type
{
    // 2d commands
    VIRTIO_GPU_CMD_GET_DISPLAY_INFO = 0x0100,
    VIRTIO_GPU_CMD_RESOURCE_CREATE_2D,
    VIRTIO_GPU_CMD_RESOURCE_UNREF,
    VIRTIO_GPU_CMD_SET_SCANOUT,
    VIRTIO_GPU_CMD_RESOURCE_FLUSH,
    VIRTIO_GPU_CMD_TRANSFER_TO_HOST_2D,
    VIRTIO_GPU_CMD_RESOURCE_ATTACH_BACKING,
    VIRTIO_GPU_CMD_RESOURCE_DETACH_BACKING,
    VIRTIO_GPU_CMD_GET_CAPSET_INFO,
    VIRTIO_GPU_CMD_GET_CAPSET,
    VIRTIO_GPU_CMD_GET_EDID,

    // cursor commands
    VIRTIO_GPU_CMD_UPDATE_CURSOR = 0x0300,
    VIRTIO_GPU_CMD_MOVE_CURSOR,

    // success responses
    VIRTIO_GPU_RESP_OK_NODATA = 0x1100,
    VIRTIO_GPU_RESP_OK_DISPLAY_INFO,
    VIRTIO_GPU_RESP_OK_CAPSET_INFO,
    VIRTIO_GPU_RESP_OK_CAPSET,
    VIRTIO_GPU_RESP_OK_EDID,

    // error responses
    VIRTIO_GPU_RESP_ERR_UNSPEC = 0x1200,
    VIRTIO_GPU_RESP_ERR_OUT_OF_MEMORY,
    VIRTIO_GPU_RESP_ERR_INVALID_SCANOUT_ID,
    VIRTIO_GPU_RESP_ERR_INVALID_RESOURCE_ID,
    VIRTIO_GPU_RESP_ERR_INVALID_CONTEXT_ID,
    VIRTIO_GPU_RESP_ERR_INVALID_PARAMETER,
};

#define VIRTIO_GPU_FLAG_FENCE (1 << 0)

struct virtio_gpu_ctrl_hdr
{
    le32 type;
    le32 flags;
    le64 fence_id;
    le32 ctx_id;
    le32 padding;
};

#define VIRTIO_GPU_MAX_SCANOUTS 16

struct virtio_gpu_rect
{
    le32 x;
    le32 y;
    le32 width;
    le32 height;
};

struct virtio_gpu_resp_display_info
{
    virtio_gpu_ctrl_hdr_t hdr;
    struct virtio_gpu_display_one
    {
        virtio_gpu_rect_t r;
        le32 enabled;
        le32 flags;
    } pmodes[VIRTIO_GPU_MAX_SCANOUTS];
};

enum virtio_gpu_formats
{
    VIRTIO_GPU_FORMAT_B8G8R8A8_UNORM  = 1,
    VIRTIO_GPU_FORMAT_B8G8R8X8_UNORM  = 2,
    VIRTIO_GPU_FORMAT_A8R8G8B8_UNORM  = 3,
    VIRTIO_GPU_FORMAT_X8R8G8B8_UNORM  = 4,

    VIRTIO_GPU_FORMAT_R8G8B8A8_UNORM  = 67,
    VIRTIO_GPU_FORMAT_X8B8G8R8_UNORM  = 68,

    VIRTIO_GPU_FORMAT_A8B8G8R8_UNORM  = 121,
    VIRTIO_GPU_FORMAT_R8G8B8X8_UNORM  = 134,
};

struct virtio_gpu_resource_create_2d
{
    virtio_gpu_ctrl_hdr_t hdr;
    le32 resource_id;
    le32 format;
    le32 width;
    le32 height;
};

struct virtio_gpu_resource_attach_backing
{
    virtio_gpu_ctrl_hdr_t hdr;
    le32 resource_id;
    le32 nr_entries;
};

struct virtio_gpu_mem_entry
{
    le64 addr;
    le32 length;
    le32 padding;
};

struct virtio_gpu_set_scanout
{
    virtio_gpu_ctrl_hdr_t hdr;
    virtio_gpu_rect_t r;
    le32 scanout_id;
    le32 resource_id;
};

struct virtio_gpu_resource_flush
{
    virtio_gpu_ctrl_hdr_t hdr;
    virtio_gpu_rect_t r;
    le32 resource_id;
    le32 padding;
};

struct virtio_gpu_transfer_to_host_2d
{
    virtio_gpu_ctrl_hdr_t hdr;
    virtio_gpu_rect_t r;
    le64 offset;
    le32 resource_id;
    le32 padding;
};

struct virtio_gpu_resource_unref
{
    virtio_gpu_ctrl_hdr_t hdr;
    le32 resource_id;
    le32 padding;
};
