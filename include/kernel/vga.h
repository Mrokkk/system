#pragma once

#include <stddef.h>
#include <arch/io.h>

// References:
// http://www.osdever.net/FreeVGA/vga/vga.htm

// VGA data register ports
#define VGA_CRT_DC      0x3d5 // CRT Controller Data Register - color emulation
#define VGA_CRT_DM      0x3b5 // CRT Controller Data Register - mono emulation
#define VGA_ATT_R       0x3c1 // Attribute Controller Data Read Register
#define VGA_ATT_W       0x3c0 // Attribute Controller Data Write Register
#define VGA_GFX_D       0x3cf // Graphics Controller Data Register
#define VGA_SEQ_D       0x3c5 // Sequencer Data Register
#define VGA_MIS_R       0x3cc // Misc Output Read Register
#define VGA_MIS_W       0x3c2 // Misc Output Write Register
#define VGA_FTC_R       0x3ca // Feature Control Read Register
#define VGA_IS1_RC      0x3da // Input Status Register 1 - color emulation
#define VGA_IS1_RM      0x3ba // Input Status Register 1 - mono emulation
#define VGA_PEL_D       0x3c9 // PEL Data Register
#define VGA_PEL_MSK     0x3c6 // PEL mask register

// VGA index register ports
#define VGA_CRT_IC      0x3d4 // CRT Controller Index - color emulation
#define VGA_CRT_IM      0x3b4 // CRT Controller Index - mono emulation
#define VGA_ATT_IW      0x3c0 // Attribute Controller Index & Data Write Register
#define VGA_GFX_I       0x3ce // Graphics Controller Index
#define VGA_SEQ_I       0x3c4 // Sequencer Index
#define VGA_PEL_IW      0x3c8 // PEL Write Index
#define VGA_PEL_IR      0x3c7 // PEL Read Index

// Standard VGA indexes max counts
#define VGA_CRT_C       0x19 // Number of CRT Controller Registers
#define VGA_ATT_C       0x15 // Number of Attribute Controller Registers
#define VGA_GFX_C       0x09 // Number of Graphics Controller Registers
#define VGA_SEQ_C       0x05 // Number of Sequencer Registers
#define VGA_MIS_C       0x01 // Number of Misc Output Register

// VGA CRT controller register indices
#define VGA_CRTC_H_TOTAL       0
#define VGA_CRTC_H_DISP        1
#define VGA_CRTC_H_BLANK_START 2
#define VGA_CRTC_H_BLANK_END   3
#define VGA_CRTC_H_SYNC_START  4
#define VGA_CRTC_H_SYNC_END    5
#define VGA_CRTC_V_TOTAL       6
#define VGA_CRTC_OVERFLOW      7
#define VGA_CRTC_PRESET_ROW    8
#define VGA_CRTC_MAX_SCAN      9
#define VGA_CRTC_CURSOR_START  0x0a
#define VGA_CRTC_CURSOR_END    0x0b
#define VGA_CRTC_START_HI      0x0c
#define VGA_CRTC_START_LO      0x0d
#define VGA_CRTC_CURSOR_HI     0x0e
#define VGA_CRTC_CURSOR_LO     0x0f
#define VGA_CRTC_V_SYNC_START  0x10
#define VGA_CRTC_V_SYNC_END    0x11
#define VGA_CRTC_V_DISP_END    0x12
#define VGA_CRTC_OFFSET        0x13
#define VGA_CRTC_UNDERLINE     0x14
#define VGA_CRTC_V_BLANK_START 0x15
#define VGA_CRTC_V_BLANK_END   0x16
#define VGA_CRTC_MODE          0x17
#define VGA_CRTC_LINE_COMPARE  0x18
#define VGA_CRTC_REGS          VGA_CRT_C

#define VGA_CRTC_CURSOR_DISABLE (1 << 5)

// VGA graphics controller register indices
#define VGA_GFX_SR_VALUE      0x00
#define VGA_GFX_SR_ENABLE     0x01
#define VGA_GFX_COMPARE_VALUE 0x02
#define VGA_GFX_DATA_ROTATE   0x03
#define VGA_GFX_PLANE_READ    0x04
#define VGA_GFX_MODE          0x05
#define VGA_GFX_MISC          0x06
#define VGA_GFX_COMPARE_MASK  0x07
#define VGA_GFX_BIT_MASK      0x08

// VGA sequencer register indices
#define VGA_SEQ_RESET         0x00
#define VGA_SEQ_CLOCK_MODE    0x01
#define VGA_SEQ_PLANE_WRITE   0x02
#define VGA_SEQ_CHARACTER_MAP 0x03
#define VGA_SEQ_MEMORY_MODE   0x04

#define VGA_OUT16VAL(v, r) \
    (((v) << 8) | (r))

enum vga_palette
{
    VGA_COLOR_BLACK         = 0,
    VGA_COLOR_RED           = 4,
    VGA_COLOR_GREEN         = 2,
    VGA_COLOR_YELLOW        = 6,
    VGA_COLOR_BLUE          = 1,
    VGA_COLOR_MAGENTA       = 5,
    VGA_COLOR_CYAN          = 3,
    VGA_COLOR_WHITE         = 15,
    VGA_COLOR_BRIGHTBLACK   = 7,
    VGA_COLOR_BRIGHTRED     = 12,
    VGA_COLOR_BRIGHTGREEN   = 10,
    VGA_COLOR_BRIGHTYELLOW  = 14,
    VGA_COLOR_BRIGHTBLUE    = 9,
    VGA_COLOR_BRIGHTMAGENTA = 13,
    VGA_COLOR_BRIGHTCYAN    = 11,
    VGA_COLOR_BRIGHTWHITE   = 15,
};

static inline void vga_write(uint16_t port, uint8_t reg, uint8_t value)
{
    outw(VGA_OUT16VAL(value, reg), port);
}

static inline void vga_crt_write(uint8_t reg, uint8_t value)
{
    vga_write(VGA_CRT_IC, reg, value);
}

static inline void vga_gfx_write(uint8_t reg, uint8_t value)
{
    vga_write(VGA_GFX_I, reg, value);
}

static inline void vga_seq_write(uint8_t reg, uint8_t value)
{
    vga_write(VGA_SEQ_I, reg, value);
}

int vga_font_set(size_t width, size_t height, void* data, size_t bytes_per_glyph, size_t glyphs_count);
void vga_cursor_disable(void);

extern uint8_t vga_palette[];
