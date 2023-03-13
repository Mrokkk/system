#include <arch/page.h>
#include <arch/multiboot.h>

#include <kernel/font.h>
#include <kernel/kernel.h>

font_t font;

typedef struct
{
#define PSF_FONT_MAGIC 0x864ab572
    uint32_t magic;
    uint32_t version;
    uint32_t header_size;
    uint32_t flags;
    uint32_t glyph_count;
    uint32_t bytes_per_glyph;
    uint32_t height;
    uint32_t width;
} psf_t;

int font_load(void* ptr, size_t size)
{
    psf_t* psf = ptr;

    if (psf->magic != PSF_FONT_MAGIC)
    {
        log_warning("incorrect format of font");
        return -1;
    }

    font.height = psf->height;
    font.width = psf->width;
    font.bytes_per_glyph = psf->bytes_per_glyph;
    font.glyphs_count = psf->glyph_count;
    font.glyphs = ptr(addr(ptr) + psf->header_size);

    not_used(size);

    return 0;
}
