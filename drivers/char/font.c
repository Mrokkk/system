#define log_fmt(fmt) "font: " fmt
#include "font.h"

#include <arch/multiboot.h>

#include <kernel/fs.h>
#include <kernel/kernel.h>
#include <kernel/page_alloc.h>

#define DEBUG_FONT      0
#define PSF1_FONT_MAGIC 0x0436
#define PSF2_FONT_MAGIC 0x864ab572
#define FONT_T_SIZE     (align(sizeof(font_t), CACHELINE_SIZE))

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

static void glyphs_endianess_fix(font_t* font)
{
    uint8_t* bytes = ptr(font->glyphs);
    switch (font->bytes_per_line)
    {
        case 1:
            break;
        case 2:
        case 3:
        {
            uint8_t* data = bytes;
            for (size_t i = 0; i < font->glyphs_count; ++i, data = shift(data, font->bytes_per_glyph))
            {
                for (size_t j = 0; j < font->bytes_per_glyph; j += font->bytes_per_line)
                {
                    uint8_t tmp = data[j];
                    data[j] = data[j + font->bytes_per_line - 1];
                    data[j + font->bytes_per_line - 1] = tmp;
                }
            }
            break;
        }
        case 4:
            log_warning("width %u is not yet supported", font->width);
    }
}

static int font_load_impl(font_t* font, void* font_data)
{
    uint32_t* magic = font_data;

    if (*magic == PSF2_FONT_MAGIC)
    {
        psf2_t* psf = (psf2_t*)magic;

        font->height = psf->height;
        font->width = psf->width;
        font->bytes_per_glyph = psf->bytes_per_glyph;
        font->bytes_per_line = align(font->width, 8) / 8;
        font->glyphs_count = psf->glyph_count;
        font->glyphs = ptr(addr(psf) + psf->header_size);

        goto success;
    }
    else if ((*magic & 0xffff) == PSF1_FONT_MAGIC)
    {
        psf1_t* psf = (psf1_t*)magic;

        font->height = psf->size;
        font->width = 8;
        font->bytes_per_glyph = psf->size;
        font->bytes_per_line = align(font->width, 8) / 8;
        font->glyphs_count = psf->mode & 0x1 ? 512 : 256;
        font->glyphs = ptr(addr(psf) + sizeof(psf1_t));

        goto success;
    }

    log_warning("unrecognized format of font: %p", *magic);

    return -EINVAL;

success:
    glyphs_endianess_fix(font);
    return 0;
}

int font_load_from_file(const char* path, font_t** font)
{
    int errno;
    scoped_file_t* file = NULL;
    page_t* pages;
    unsigned pages_needed;
    font_t* new_font;
    void* font_data;

    if ((errno = do_open(&file, path, O_RDONLY, 0)))
    {
        return errno;
    }

    if (unlikely(!file->ops || !file->ops->read))
    {
        return -ENOSYS;
    }

    pages_needed = page_align(FONT_T_SIZE + file->dentry->inode->size) / PAGE_SIZE;

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

    new_font = page_virt_ptr(pages);
    font_data = shift_as(void*, new_font, FONT_T_SIZE);

    int res = file->ops->read(file, font_data, file->dentry->inode->size);

    if (unlikely(errno = errno_get(res)))
    {
        goto failure;
    }

    new_font->pages = pages;

    if (unlikely(errno = font_load_impl(new_font, font_data)))
    {
        goto failure;
    }

    log_debug(DEBUG_FONT, "loaded font %s: h:%u w:%u bpg: %u glyphs: %u",
        path, new_font->height, new_font->width, new_font->bytes_per_glyph, new_font->glyphs_count);

    *font = new_font;

    return 0;

failure:
    if (pages)
    {
        pages_free(pages);
    }

    return errno;
}

int font_load_from_buffer(const void* buffer, size_t size, font_t** font)
{
    int errno;
    page_t* pages = page_alloc(page_align(FONT_T_SIZE + size) / PAGE_SIZE, PAGE_ALLOC_CONT);

    if (unlikely(!pages))
    {
        return -ENOMEM;
    }

    font_t* new_font = page_virt_ptr(pages);
    void* font_data = shift_as(void*, new_font, FONT_T_SIZE);

    memcpy(font_data, buffer, size);

    new_font->pages = pages;

    if (unlikely(errno = font_load_impl(new_font, font_data)))
    {
        pages_free(pages);
        return errno;
    }

    log_debug(DEBUG_FONT, "loaded font from %p: h:%u w:%u bpg: %u glyphs: %u",
        buffer, new_font->height, new_font->width, new_font->bytes_per_glyph, new_font->glyphs_count);

    *font = new_font;

    return 0;
}

void font_unload(font_t* font)
{
    pages_free(font->pages);
}
