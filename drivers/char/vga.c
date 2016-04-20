#include <kernel/string.h>
#include <kernel/module.h>
#include <kernel/device.h>

#include <arch/bios.h>
#include <arch/io.h>
#include <arch/real_mode.h>

#define VIDEO_SEGMENT 0xb8000

#define COLOR_BLACK         0
#define COLOR_BLUE          1
#define COLOR_GREEN         2
#define COLOR_CYAN          3
#define COLOR_RED           4
#define COLOR_MAGENTA       5
#define COLOR_BROWN         6
#define COLOR_GRAY          7
#define COLOR_DARKGRAY      8
#define COLOR_BRIGHTBLUE    9
#define COLOR_BRIGHTGREEN   10
#define COLOR_BRIGHTCYAN    11
#define COLOR_BRIGHT_RED    12
#define COLOR_BRIGHTMAGENTA 13
#define COLOR_YELLOW        14
#define COLOR_WHITE         15

#define forecolor(x)    x
#define backcolor(x)    ((x << 4) & 0x7f)
#define blink           (1 << 7)

#define RESX 80
#define RESY 25

void cls();
int video_init();
void display_print(const char *text);

unsigned short *pointer[4] = {
        (unsigned short *)0xb8000,
        (unsigned short *)(0xb8000+4096),
        (unsigned short *)(0xb8000+2*4096),
        (unsigned short *)(0xb8000+3*4096)
};

unsigned char attrib = forecolor(COLOR_GRAY) | backcolor(COLOR_BLACK);
unsigned char csr_x[4] = {0, 0, 0, 0};
unsigned char csr_y[4] = {0, 0, 0, 0};

unsigned char current_page = 0;

void scroll(void) {

    unsigned short blank;
    unsigned short temp;

    /* Spacja na czarnym tle */
    blank = 0x20 | (attrib << 8);

    if(csr_y[current_page] >= RESY) {
        /* Move the current text chunk that makes up the screen
         * back in the buffer by a line */
        temp = csr_y[current_page] - RESY + 1;
        memcpy(pointer[current_page], pointer[current_page] + temp * RESX, (RESY - temp) * RESX * 2);

        /* Finally, we set the chunk of memory that occupies
         * the last line of text to our 'blank' character */
        memsetw(pointer[current_page] + (RESY - temp) * RESX, blank, RESX);
        csr_y[current_page] = RESY - 1;
    }

}

void move_csr(void) {

    unsigned short offset;

    offset = csr_y[current_page] * RESX + csr_x[current_page];

    outb(14, 0x3D4);
    outb(offset >> 8, 0x3D5);
    outb(15, 0x3D4);
    outb(offset, 0x3D5);

}

void cls() {

    unsigned short blank = 0x20 | (attrib << 8);

    memsetw((unsigned short*) pointer[current_page], blank, RESX * RESY);
    csr_x[current_page] = 0;
    csr_y[current_page] = 0;
    move_csr();

}


void putch(unsigned char c) {

    unsigned short *where;
    unsigned short att = attrib << 8;

    /* Backspace */
    if (c == 0x08) {
        if(csr_x[current_page] != 0) {
            csr_x[current_page]--;
            putch(' ');
            csr_x[current_page]--;
        }
    }
    /* Tabulator */
    else if (c == 0x09) {
        csr_x[current_page] = (csr_x[current_page] + 8) & ~(8 - 1);
    }
    /* CR */
    else if (c == '\r') {
        csr_x[current_page] = 0;
    }
    /* Znak nowej linii - LF */
    else if (c == '\n') {
        csr_x[current_page] = 0;
        csr_y[current_page]++;
    }
    /* Any character greater than and including a space, is a
    *  printable character. The equation for finding the index
    *  in a linear chunk of memory can be represented by:
    *  Index = [(y * width) + x] */
    else if (c >= ' ') {
        where = pointer[current_page] + (csr_y[current_page] * RESX + csr_x[current_page]);
        *where = c | att;    /* Character AND attributes: color */
        csr_x[current_page]++;
    }

    /* If the cursor has reached the edge of the screen's width, we
    *  insert a new line in there */
    if (csr_x[current_page] >= RESX) {
        csr_x[current_page] = 0;
        csr_y[current_page]++;
    }

    /* Scroll the screen if needed, and finally move the cursor */
    scroll();
    move_csr();

}


void display_print(const char *text) {

    unsigned int i = 0;

    for (i = 0; text[i]!=0; i++) {
        putch(text[i]);
        /*write_serial(text[i]);*/
    }
}

int display_write(struct inode *inode, struct file *file, const char *buffer, unsigned int size) {

    (void)inode; (void)file;

    while (size--)
        putch(*buffer++);

    return size;

}

int video_init() {

    struct regs_struct param;

    param.ax = 3;
    param.bx = 0;
    param.cx = 0;
    param.dx = 0;
    execute_in_real_mode((unsigned int)bios_int10h, &param);
    param.ax = 0x1001;
    param.bx = 100 << 8;
    execute_in_real_mode((unsigned int)bios_int10h, &param);
    pointer[0] = (unsigned short*)VIDEO_SEGMENT;
    pointer[1] = (unsigned short*)(VIDEO_SEGMENT+4096);

    cls();

    return 0;

}

void disp_set_page(unsigned char page) {

    struct regs_struct param;

    param.ax = 0;
    param.bx = 0;
    param.cx = 0;
    param.dx = 0;

    current_page = page;
    param.ax = 0x0500 | page;
    execute_in_real_mode((unsigned int)bios_int10h, &param);

}

void disp_switch_page(int page) {

    struct regs_struct param;

    param.ax = 0;
    param.bx = 0;
    param.cx = 0;
    param.dx = 0;

    current_page = page;
    param.ax = 0x0500 | page;
    execute_in_real_mode((unsigned int)bios_int10h, &param);
    move_csr();

}

void disp_putch(unsigned char c, unsigned char attr) {

    unsigned short *where;
    unsigned short att = attr << 8;

    /* Backspace */
    if(c == 0x08) {
        if(csr_x[current_page] != 0) {
            csr_x[current_page]--;
            disp_putch(' ', att);
            csr_x[current_page]--;
        }
    }
    /* Tabulator */
    else if(c == 0x09) {
        csr_x[current_page] = (csr_x[current_page] + 8) & ~(8 - 1);
    }
    /* CR */
    else if(c == '\r') {
        csr_x[current_page] = 0;
    }

    /* LF */
    else if(c == '\n') {
        csr_x[current_page] = 0;
        csr_y[current_page]++;
    }
    /* Any character greater than and including a space, is a
    *  printable character. The equation for finding the index
    *  in a linear chunk of memory can be represented by:
    *  Index = [(y * width) + x] */
    else if(c >= ' ') {
        where = pointer[current_page] + (csr_y[current_page] * RESX + csr_x[current_page]);
        *where = c | att;    /* Character AND attributes: color */
        csr_x[current_page]++;
    }

    /* If the cursor has reached the edge of the screen's width, we
    *  insert a new line in there */
    if(csr_x[current_page] >= RESX) {
        csr_x[current_page] = 0;
        csr_y[current_page]++;
    }

    /* Scroll the screen if needed, and finally move the cursor */
    scroll();
    move_csr();

}

/* Uses the above routine to output a string... */
void disp_puts(char *text, unsigned char attr) {

    unsigned int i;

    for (i = 0; text[i]!=0; i++) {
        disp_putch((unsigned char)text[i], attr);
        /*write_serial(text[i]);*/
    }
}

unsigned char get_csr_x() {
    return csr_x[current_page];
}

unsigned char get_csr_y() {
    return csr_y[current_page];
}

unsigned char get_current_page() {
    return current_page;
}

unsigned char disp_goto(unsigned char x, unsigned char y) {
    csr_x[current_page] = x;
    csr_y[current_page] = y;
    return 0;
}

void disp_change_color(unsigned char color) {

    unsigned short *where;
    where = pointer[current_page] + (csr_y[current_page] * RESX + csr_x[current_page]);
    *where = (*where & 0xff) | (color << 8);
    csr_x[current_page]++;
    move_csr();

}

void clear_screen(unsigned char c, unsigned char att) {

    unsigned short blank = c | (att << 8);
    memsetw((unsigned short*) pointer[current_page], blank, RESX * RESY);

    csr_x[current_page] = 0;
    csr_y[current_page] = 0;
    move_csr();

}
