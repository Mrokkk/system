#include <kernel/init.h>

#include "console_driver.h"

static int dummycon_probe(framebuffer_t* fb);
static int dummycon_init(console_driver_t* driver, console_config_t* config, size_t* resx, size_t* resy);
static void dummycon_glyph_draw(console_driver_t* driver, size_t x, size_t y, glyph_t* glyph);
static void dummycon_defcolor(console_driver_t* driver, uint32_t* fgcolor, uint32_t* bgcolor);
static void dummycon_deinit(console_driver_t* driver);

static console_driver_ops_t dummycon_ops = {
    .name         = "Dummy FB",
    .probe        = &dummycon_probe,
    .init         = &dummycon_init,
    .deinit       = &dummycon_deinit,
    .glyph_draw   = &dummycon_glyph_draw,
    .defcolor     = &dummycon_defcolor,
};

static void dummycon_glyph_draw(console_driver_t*, size_t, size_t, glyph_t*)
{
}

static void dummycon_defcolor(console_driver_t*, uint32_t*, uint32_t*)
{
}

static int dummycon_init(console_driver_t*, console_config_t*, size_t* resx, size_t* resy)
{
    *resx = 80;
    *resy = 25;
    return 0;
}

static void dummycon_deinit(console_driver_t*)
{
}

static int dummycon_probe(framebuffer_t* fb)
{
    return fb->vaddr != NULL;
}

UNMAP_AFTER_INIT static int dummycon_initialize(void)
{
    console_driver_register(&dummycon_ops);
    return 0;
}

premodules_initcall(dummycon_initialize);
