#define log_fmt(fmt) "font: " fmt
#include "font.h"

#include <arch/multiboot.h>

#include <kernel/fs.h>
#include <kernel/kernel.h>
#include <kernel/page_alloc.h>

#define PSF1_FONT_MAGIC 0x0436
#define PSF2_FONT_MAGIC 0x864ab572

font_t font;

typedef struct psf1
{
    uint16_t magic;
    uint8_t  mode;
    uint8_t  size;
} psf1_t;

typedef struct psf2
{
    uint32_t magic;
    uint32_t version;
    uint32_t header_size;
    uint32_t flags;
    uint32_t glyph_count;
    uint32_t bytes_per_glyph;
    uint32_t height;
    uint32_t width;
} psf2_t;

static int font_load_impl(page_t* pages)
{
    uint32_t prev_height = font.height;
    uint32_t prev_width = font.width;

    uint32_t* magic = page_virt_ptr(pages);

    if (*magic == PSF2_FONT_MAGIC)
    {
        psf2_t* psf = (psf2_t*)magic;

        if ((prev_height && prev_height != psf->height) ||
            (prev_width && prev_width != psf->width))
        {
            return -EINVAL;
        }

        scoped_irq_lock();

        font.height = psf->height;
        font.width = psf->width;
        font.bytes_per_glyph = psf->bytes_per_glyph;
        font.glyphs_count = psf->glyph_count;
        font.glyphs = ptr(addr(psf) + psf->header_size);
        font.pages = pages;

        return 0;
    }
    else if ((*magic & 0xffff) == PSF1_FONT_MAGIC)
    {
        psf1_t* psf = (psf1_t*)magic;

        if ((prev_height && prev_height != psf->size) ||
            (prev_width && prev_width != 8))
        {
            return -EINVAL;
        }

        scoped_irq_lock();

        font.height = psf->size;
        font.width = 8;
        font.bytes_per_glyph = psf->size;
        font.glyphs_count = psf->mode & 0x1 ? 512 : 256;
        font.glyphs = ptr(addr(psf) + sizeof(psf1_t));
        font.pages = pages;

        return 0;
    }

    log_warning("unrecognized format of font: %p", *magic);

    return -EINVAL;
}

int font_load_from_file(const char* path)
{
    int errno;
    scoped_file_t* file = NULL;
    page_t* pages;
    unsigned pages_needed;
    page_t* prev_pages = font.pages;

    if ((errno = do_open(&file, path, O_RDONLY, 0)))
    {
        return errno;
    }

    if (unlikely(!file->ops || !file->ops->read))
    {
        return -ENOSYS;
    }

    pages_needed = page_align(file->dentry->inode->size) / PAGE_SIZE;

    log_info("allocating %u pages for font", pages_needed);

    if (unlikely(!pages_needed))
    {
        return -ENOENT;
    }

    pages = page_alloc(pages_needed, PAGE_ALLOC_CONT);

    if (unlikely(!pages))
    {
        return -ENOMEM;
    }

    int res = file->ops->read(file, page_virt_ptr(pages), file->dentry->inode->size);

    if (unlikely(errno = errno_get(res)))
    {
        goto failure;
    }

    if (unlikely(errno = font_load_impl(pages)))
    {
        goto failure;
    }

    if (prev_pages)
    {
        pages_free(prev_pages);
    }

    return 0;

failure:
    if (pages)
    {
        pages_free(pages);
    }

    return errno;
}

int font_load_from_buffer(const void* buffer, size_t size)
{
    int errno;
    page_t* prev_pages = font.pages;
    page_t* pages = page_alloc(page_align(size) / PAGE_SIZE, PAGE_ALLOC_CONT);

    if (unlikely(!pages))
    {
        return -ENOMEM;
    }

    memcpy(page_virt_ptr(pages), buffer, size);

    if (unlikely(errno = font_load_impl(pages)))
    {
        pages_free(pages);
        return errno;
    }

    if (prev_pages)
    {
        pages_free(prev_pages);
    }

    return 0;
}
