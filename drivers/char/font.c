#define log_fmt(fmt) "font: " fmt
#include "font.h"

#include <arch/multiboot.h>

#include <kernel/fs.h>
#include <kernel/page.h>
#include <kernel/kernel.h>

#define PSF1_FONT_MAGIC 0x0436
#define PSF2_FONT_MAGIC 0x864ab572

font_t font;

typedef struct
{
    uint16_t magic;
    uint8_t mode;
    uint8_t size;
} PACKED psf1_t;

typedef struct
{
    uint32_t magic;
    uint32_t version;
    uint32_t header_size;
    uint32_t flags;
    uint32_t glyph_count;
    uint32_t bytes_per_glyph;
    uint32_t height;
    uint32_t width;
} PACKED psf2_t;

int font_load(void)
{
    int errno;
    scoped_file_t* file = NULL;
    page_t* pages;
    unsigned pages_needed;
    const char* filename = "/usr/share/font.psf";

    if ((errno = do_open(&file, filename, O_RDONLY, 0)))
    {
        return errno;
    }

    if (unlikely(!file->ops || !file->ops->read))
    {
        return -ENOSYS;
    }

    pages_needed = page_align(file->inode->size) / PAGE_SIZE;

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

    file->ops->read(file, page_virt_ptr(pages), file->inode->size);

    uint32_t* magic = page_virt_ptr(pages);

    if (*magic == PSF2_FONT_MAGIC)
    {
        psf2_t* psf = (psf2_t*)magic;

        font.height = psf->height;
        font.width = psf->width;
        font.bytes_per_glyph = psf->bytes_per_glyph;
        font.glyphs_count = psf->glyph_count;
        font.glyphs = ptr(addr(psf) + psf->header_size);

        return 0;
    }
    else if ((*magic & 0xffff) == PSF1_FONT_MAGIC)
    {
        psf1_t* psf = (psf1_t*)magic;

        font.height = psf->size;
        font.width = 8;
        font.bytes_per_glyph = psf->size;
        font.glyphs_count = psf->mode & 0x1 ? 512 : 256;
        font.glyphs = ptr(addr(psf) + sizeof(psf1_t));

        return 0;
    }

    log_warning("unrecognized format of font");
    return -EINVAL;
}
