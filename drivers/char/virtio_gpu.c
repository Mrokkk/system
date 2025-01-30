#define log_fmt(fmt) "virtio-gpu: " fmt
#include "virtio_gpu.h"

#include <arch/pci.h>
#include <kernel/timer.h>
#include <kernel/kernel.h>
#include <kernel/minmax.h>
#include <kernel/string.h>
#include <kernel/page_mmio.h>
#include <kernel/page_alloc.h>
#include <kernel/framebuffer.h>

#include "virtio.h"

#define VIRTIO_GPU_FB_SIZE (16 * MiB)

static virtio_device_t* gpu;
static int error;
static size_t fb_resx, fb_resy;
static page_t* fb_pages;
static timer_t timer;
static volatile bool dirty;
static volatile fb_rect_t dirty_rect;

static void virtio_gpu_fb_dirty_set(const fb_rect_t* rect);
static void virtio_gpu_fb_refresh(void);
static int virtio_gpu_fb_mode_set(int resx, int resy, int bpp);

READONLY static fb_ops_t fb_ops = {
    .dirty_set = &virtio_gpu_fb_dirty_set,
    .mode_set = &virtio_gpu_fb_mode_set,
    .refresh = &virtio_gpu_fb_refresh,
};

static const char* virtio_gpu_response_string(int type)
{
    switch (type)
    {
        case VIRTIO_GPU_RESP_OK_NODATA: return "OK (no data)";
        case VIRTIO_GPU_RESP_OK_DISPLAY_INFO: return "OK (Display Info)";
        case VIRTIO_GPU_RESP_OK_CAPSET_INFO: return "OK (CapSet Info)";
        case VIRTIO_GPU_RESP_OK_EDID: return "OK (EDID)";
        case VIRTIO_GPU_RESP_ERR_UNSPEC: return "unspecified error";
        case VIRTIO_GPU_RESP_ERR_OUT_OF_MEMORY: return "out of memory";
        case VIRTIO_GPU_RESP_ERR_INVALID_SCANOUT_ID: return "invalid scanout ID";
        case VIRTIO_GPU_RESP_ERR_INVALID_RESOURCE_ID: return "invalid resource ID";
        case VIRTIO_GPU_RESP_ERR_INVALID_CONTEXT_ID: return "invalid context ID";
        case VIRTIO_GPU_RESP_ERR_INVALID_PARAMETER: return "invalid parameter";
        default: return "unknown value";
    }
}

static int virtio_gpu_command(int queue_id, const void* req, size_t req_size, void** resp, size_t resp_size)
{
    virtio_queue_t* queue = &gpu->queues[queue_id];

    virtio_buffer_t input  = virtio_buffer_create(queue, req_size, 0);
    virtio_buffer_t output = virtio_buffer_create(queue, resp_size, VIRTQ_DESC_F_WRITE);

    memcpy(input.buffer, req, req_size);

    virtio_buffers_submit(queue, &input, &output, NULL);

    *resp = output.buffer;

    return req_size;
}

static int virtio_gpu_get_display_info(virtio_gpu_resp_display_info_t** resp)
{
    const virtio_gpu_ctrl_hdr_t req = {.type = VIRTIO_GPU_CMD_GET_DISPLAY_INFO};

    virtio_gpu_command(VIRTIO_GPU_CONTROLQ, &req, sizeof(req), (void**)resp, sizeof(**resp));

    if (unlikely((*resp)->hdr.type != VIRTIO_GPU_RESP_OK_DISPLAY_INFO))
    {
        error = (*resp)->hdr.type;
        return -EIO;
    }

    return 0;
}

static int virtio_gpu_resource_create_2d(size_t w, size_t h)
{
    virtio_gpu_resource_create_2d_t req = {
        .hdr = {.type = VIRTIO_GPU_CMD_RESOURCE_CREATE_2D},
        .format = VIRTIO_GPU_FORMAT_B8G8R8A8_UNORM,
        .width  = w,
        .height = h,
        .resource_id = 1
    };
    virtio_gpu_ctrl_hdr_t* resp;

    virtio_gpu_command(VIRTIO_GPU_CONTROLQ, &req, sizeof(req), (void**)&resp, sizeof(*resp));

    if (unlikely(resp->type != VIRTIO_GPU_RESP_OK_NODATA))
    {
        error = resp->type;
        return -EIO;
    }

    return 0;
}

static int virtio_gpu_resource_unref(void)
{
    virtio_gpu_resource_unref_t req = {
        .hdr = {.type = VIRTIO_GPU_CMD_RESOURCE_UNREF},
        .resource_id = 1
    };
    virtio_gpu_ctrl_hdr_t* resp;

    virtio_gpu_command(VIRTIO_GPU_CONTROLQ, &req, sizeof(req), (void**)&resp, sizeof(*resp));

    if (unlikely(resp->type != VIRTIO_GPU_RESP_OK_NODATA))
    {
        error = resp->type;
        return -EIO;
    }

    return 0;
}

static int virtio_gpu_resource_attach_backing(void)
{
    struct
    {
        virtio_gpu_resource_attach_backing_t req;
        virtio_gpu_mem_entry_t data;
    } req = {
        .req = {
            .hdr = {.type = VIRTIO_GPU_CMD_RESOURCE_ATTACH_BACKING},
            .resource_id = 1,
            .nr_entries = 1,
        },
        .data = {
            .addr = page_phys(fb_pages),
            .length = VIRTIO_GPU_FB_SIZE,
        }
    };
    virtio_gpu_ctrl_hdr_t* resp;

    virtio_gpu_command(VIRTIO_GPU_CONTROLQ, &req, sizeof(req), (void*)&resp, sizeof(*resp));

    if (unlikely(resp->type != VIRTIO_GPU_RESP_OK_NODATA))
    {
        error = resp->type;
        return -EIO;
    }

    return 0;
}

static int virtio_gpu_set_scanout(size_t w, size_t h)
{
    virtio_gpu_set_scanout_t req = {
        .hdr = {.type = VIRTIO_GPU_CMD_SET_SCANOUT},
        .scanout_id = 0,
        .resource_id = 1,
        .r = {
            .width = w,
            .height = h,
        }
    };
    virtio_gpu_ctrl_hdr_t* resp;

    virtio_gpu_command(VIRTIO_GPU_CONTROLQ, &req, sizeof(req), (void**)&resp, sizeof(*resp));

    if (unlikely(resp->type != VIRTIO_GPU_RESP_OK_NODATA))
    {
        error = resp->type;
        return -EIO;
    }

    return 0;
}

static int virtio_gpu_transfer_to_host_2d(size_t x, size_t y, size_t w, size_t h)
{
    virtio_gpu_transfer_to_host_2d_t req = {
        .hdr = {.type = VIRTIO_GPU_CMD_TRANSFER_TO_HOST_2D},
        .offset = x * 4 + y * fb_resx * 4,
        .resource_id = 1,
        .r = {
            .x = x,
            .y = y,
            .width = w,
            .height = h,
        }
    };
    virtio_gpu_ctrl_hdr_t* resp;

    virtio_gpu_command(VIRTIO_GPU_CONTROLQ, &req, sizeof(req), (void**)&resp, sizeof(*resp));

    if (unlikely(resp->type != VIRTIO_GPU_RESP_OK_NODATA))
    {
        error = resp->type;
        return -EIO;
    }

    return 0;
}

static int virtio_gpu_resource_flush(size_t x, size_t y, size_t w, size_t h)
{
    virtio_gpu_resource_flush_t req = {
        .hdr = {.type = VIRTIO_GPU_CMD_RESOURCE_FLUSH},
        .resource_id = 1,
        .r = {
            .x = x,
            .y = y,
            .width = w,
            .height = h,
        }
    };
    virtio_gpu_ctrl_hdr_t* resp;

    virtio_gpu_command(VIRTIO_GPU_CONTROLQ, &req, sizeof(req), (void*)&resp, sizeof(*resp));

    if (unlikely(resp->type != VIRTIO_GPU_RESP_OK_NODATA))
    {
        error = resp->type;
        return -EIO;
    }

    return 0;
}

static void virtio_gpu_fb_setup(void)
{
    framebuffer.size = fb_resx * fb_resy * 4;
    framebuffer.accel = false;
    framebuffer.bpp = 32;
    framebuffer.paddr = page_phys(fb_pages);
    framebuffer.vaddr = page_virt_ptr(fb_pages);
    framebuffer.width = fb_resx;
    framebuffer.height = fb_resy;
    framebuffer.id = "VirtIO GPU FB";
    framebuffer.type = FB_TYPE_PACKED_PIXELS;
    framebuffer.type_aux = 0;
    framebuffer.visual = FB_VISUAL_TRUECOLOR;
    framebuffer.pitch = fb_resx * 4;
    framebuffer.flags = FB_FLAGS_VIRTFB;
    framebuffer.delay.tv_sec = 0;
    framebuffer.delay.tv_usec = 6000;
    framebuffer.ops = &fb_ops;
}

static void virtio_gpu_fb_dirty_set(const fb_rect_t* rect)
{
    fb_rect_enlarge(ptr(&dirty_rect), rect);
    dirty = true;
}

static void virtio_gpu_fb_refresh(void)
{
    virtio_gpu_transfer_to_host_2d(0, 0, fb_resx, fb_resy);
    virtio_gpu_resource_flush(0, 0, fb_resx, fb_resy);
    fb_rect_clear(ptr(&dirty_rect));
    dirty = false;
}

static int virtio_gpu_fb_mode_set(int resx, int resy, int bpp)
{
    if (resx < 0 || resy < 0 || resx > 0xffff || resy > 0xffff || bpp != 32)
    {
        return -EINVAL;
    }

    log_info("setting %u x %u", resx, resy);

    dirty = false;

    fb_resx = resx;
    fb_resy = resy;

    if (virtio_gpu_resource_unref() ||
        virtio_gpu_resource_create_2d(resx, resy) ||
        virtio_gpu_resource_attach_backing() ||
        virtio_gpu_set_scanout(resx, resy))
    {
        log_warning("failed with %s (%#x)", virtio_gpu_response_string(error), error);
    }

    virtio_gpu_fb_setup();

    dirty = true;

    return 0;
}

static void refresh_callback(ktimer_t*)
{
    if (!dirty)
    {
        return;
    }
    virtio_gpu_transfer_to_host_2d(dirty_rect.x, dirty_rect.y, dirty_rect.w, dirty_rect.h);
    virtio_gpu_resource_flush(dirty_rect.x, dirty_rect.y, dirty_rect.w, dirty_rect.h);
    memset(ptr(&dirty_rect), 0, sizeof(dirty_rect));
    dirty = false;
}

UNMAP_AFTER_INIT int virtio_gpu_init(void)
{
    int errno;
    pci_device_t* pci_device;

    if (unlikely(!(pci_device = pci_device_get(PCI_DISPLAY, PCI_DISPLAY_CONTROLLER))))
    {
        log_info("no device");
        return -ENODEV;
    }

    if (unlikely(errno = virtio_initialize("virtio-gpu", pci_device, VIRTIO_GPU_NUMQUEUES, &gpu)))
    {
        return errno;
    }

    virtio_gpu_resp_display_info_t* resp;

    if (unlikely(virtio_gpu_get_display_info(&resp)))
    {
        log_warning("VIRTIO_GPU_CMD_GET_DISPLAY_INFO failed with %s (%#x)",
            virtio_gpu_response_string(error), error);
        return -ENODEV;
    }

    fb_resx = resp->pmodes[0].r.width;
    fb_resy = resp->pmodes[0].r.height;

    if (!fb_resx || !fb_resy)
    {
        log_info("[display info] no preffered resolution; using 1024x768");
        fb_resx = 1024;
        fb_resy = 768;
    }

    size_t framebuffer_size = VIRTIO_GPU_FB_SIZE;
    fb_pages = page_alloc(framebuffer_size / PAGE_SIZE, PAGE_ALLOC_CONT | PAGE_ALLOC_ZEROED);

    if (unlikely(!fb_pages))
    {
        return -ENOMEM;
    }

    if (virtio_gpu_get_display_info(&resp) ||
        virtio_gpu_resource_create_2d(fb_resx, fb_resy) ||
        virtio_gpu_resource_attach_backing() ||
        virtio_gpu_set_scanout(fb_resx, fb_resy))
    {
        log_warning("failed with %s (%#x)", virtio_gpu_response_string(error), error);
        return -EIO;
    }

    timer = ktimer_create_and_start(
        KTIMER_REPEATING,
        (timeval_t){.tv_sec = 0, .tv_usec = 10000},
        &refresh_callback,
        NULL);

    if (unlikely(errno = errno_get(timer)))
    {
        log_warning("cannot start refresh timer: %s", errno_name(errno));
        timer = 0;
        return errno;
    }

    virtio_gpu_fb_setup();

    dirty = true;

    return 0;
}
