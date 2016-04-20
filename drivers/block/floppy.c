#include <kernel/kernel.h>
#include <kernel/module.h>
#include <kernel/device.h>
#include <kernel/irq.h>
#include <arch/io.h>
#include <arch/system.h>
#include "floppy.h"

int floppy_read_track(unsigned cyl, unsigned int address);
int floppy_open();
void floppy_timer();

KERNEL_MODULE(floppy);
module_init(floppy_init);
module_exit(floppy_deinit);

/*
static const char *drive_types[8] = {
    "none",
    "360kB 5.25\"",
    "1.2MB 5.25\"",
    "720kB 3.5\"",
    "1.44MB 3.5\"",
    "2.88MB 3.5\"",
    "unknown type",
    "unknown type"
};*/

static struct file_operations fops = {
    .open = &floppy_open,
};

unsigned char floppy_type = 0;
volatile char floppy_irq = 0;
volatile unsigned char floppy_dma_buffer[FLOPPY_DMA_BUFFER_SIZE] __aligned(0x8000);
static volatile int floppy_motor_ticks = 0;
static volatile int floppy_motor_state = 0;
static unsigned char fd_type[2];

int floppy_deinit() {

    return 0;

}

void lba_2_chs(unsigned int lba, unsigned short *cyl, unsigned short *head, unsigned short *sector) {

    *cyl    = lba / (2 * FLOPPY_144_SECTORS_PER_TRACK);
    *head   = ((lba % (2 * FLOPPY_144_SECTORS_PER_TRACK)) / FLOPPY_144_SECTORS_PER_TRACK);
    *sector = ((lba % (2 * FLOPPY_144_SECTORS_PER_TRACK)) % FLOPPY_144_SECTORS_PER_TRACK + 1);

}

void floppy_detect_drives() {

    unsigned int drives;

    outb(0x70, 0x10);
    drives = inb(0x71);

    fd_type[1] = drives & 0xf;
    fd_type[0] = (drives >> 4) & 0xf;

}

unsigned char floppy_status_read() {
    return inb(FLOPPY_MSR);
}

void __optimize(0) floppy_send_command(unsigned char cmd) {

    int i;

    for (i=0; i<4096; i++) {
        if (MSR_RQM & floppy_status_read()) {
            outb(cmd, FLOPPY_FIFO);
            return;
        }
        delay(10);
    }
    printk("cannot send cmd\n");
}

unsigned char __optimize(0) floppy_read_data() {

    int i;

    for (i=0; i<4096; i++) {
        if (MSR_RQM & floppy_status_read()) {
            return inb(FLOPPY_FIFO);
        }
        delay(10);
    }
    printk("cannot read data\n");
    return 0;
}

void floppy_check_interrupt(int *st0, int *cyl) {

    floppy_send_command(CMD_SENSE_INTERRUPT);

    *st0 = floppy_read_data();
    *cyl = floppy_read_data();

}

void floppy_isr() {

    asm volatile("movb $1, floppy_irq");

}

void floppy_irq_wait() {


    while (1) {

        if (floppy_irq) {
            return;
        }

    }

}

void floppy_motor(int onoff) {

    if(onoff) {
        if(!floppy_motor_state) {
            /* need to turn on */
            outb(0x1c, FLOPPY_DOR);
            delay(10); /* wait 500 ms = hopefully enough for modern drives */
        }
        floppy_motor_state = floppy_motor_on;
    } else {
        if(floppy_motor_state == floppy_motor_wait) {
            printk("floppy_motor: strange, fd motor-state already waiting..\n");
        }
        floppy_motor_ticks = 300; /* 3 seconds, see floppy_timer() below */
        floppy_motor_state = floppy_motor_wait;
        /*floppy_timer();*/
    }
}

void floppy_motor_kill() {
    outb(0x0c, FLOPPY_DOR);
    floppy_motor_state = floppy_motor_off;
}

void floppy_timer() {
    while(1) {
        /* delay for 500ms at a time, which gives around half
           a second jitter, but that's hardly relevant here. */
        delay(20);
        if(floppy_motor_state == floppy_motor_wait) {
            floppy_motor_ticks -= 50;
            if(floppy_motor_ticks <= 0) {
                floppy_motor_kill();
            }
        }
    }
}

int floppy_calibrate() {

    int i, st0, cyl = -1; /* set to bogus cylinder */

    floppy_motor(floppy_motor_on);

    for(i = 0; i < 10; i++) {
        /* Attempt to positions head to cylinder 0 */
        floppy_send_command(CMD_RECALIBRATE);
        floppy_send_command(0); /* argument is drive, we only support 0 */

        floppy_irq_wait();
        floppy_check_interrupt(&st0, &cyl);

        if(st0 & 0xc0) {
            static const char * status[] =
            { 0, "error", "invalid", "drive" };
            printk("floppy_calibrate: status = %s\n", status[st0 >> 6]);
            continue;
        }

        if(!cyl) { /* found cylinder 0 ? */
            floppy_motor(floppy_motor_off);
            return 0;
        }
    }

    printk("floppy_calibrate: 10 retries exhausted\n");
    floppy_motor(floppy_motor_off);

    return -EBUSY;

}

int floppy_init() {

    irq_register(0x06, &floppy_isr, "floppy");

    block_device_register(MAJOR_BLK_FLOPPY, "fd0", &fops);
    block_device_register(MAJOR_BLK_FLOPPY, "fd1", &fops);

    return 0;

}

int floppy_open(int minor) {

    int st0, cy0;

    (void)minor;

    floppy_detect_drives();

    /*if (fd_type[0] != 4 && fd_type[1] != 4)
        return -ENODEV;*/

    outb(0x00, FLOPPY_DOR);
    outb(DOR_RESET | DOR_IRQ | DOR_DSEL(0), FLOPPY_DOR);

    floppy_irq_wait();
    floppy_check_interrupt(&st0, &cy0);

    outb(0x00, FLOPPY_CCR); /* Prêdkoœæ 500kB/s */

    floppy_send_command(CMD_SPECIFY);
    floppy_send_command(0xdf); /* steprate = 3ms, unload time = 240ms */
    floppy_send_command(0x02); /* load time = 16ms, no-DMA = 0 */

    if (floppy_calibrate()) return -1;

    floppy_send_command(CMD_VERSION);
    floppy_type = floppy_read_data();

    return 0;

}

int floppy_seek(int cyli, int head) {

    int i, st0, cyl = -1;

    floppy_motor(floppy_motor_on);

    for(i = 0; i < 10; i++) {

        /* Attempt to position to given cylinder    *
         * 1st byte bit[1:0] = drive, bit[2] = head    *
         * 2nd byte is cylinder number                */
        floppy_send_command(CMD_SEEK);
        floppy_send_command(head<<2);
        floppy_send_command(cyli);

        floppy_irq_wait();
        floppy_check_interrupt(&st0, &cyl);

        if(st0 & 0xc0) {
            static const char * status[] =
            { "normal", "error", "invalid", "drive" };
            printk("floppy_seek: status = %s\n", status[st0 >> 6]);
            continue;
        }

        if(cyl == cyli) {
            floppy_motor(floppy_motor_off);
            return 0;
        }

    }

    printk("floppy_seek: 10 retries exhausted\n");
    floppy_motor(floppy_motor_off);
    return -1;
}

void floppy_dma_init(floppy_dir dir, unsigned int address) {

    union {
        unsigned char b[4];
        unsigned long l;
    } a, c;
    unsigned char mode;

    a.l = (unsigned long)address;
    c.l = (unsigned long)FLOPPY_DMA_BUFFER_SIZE-1;

    if((a.l >> 24) || (c.l >> 16) || (((a.l&0xffff)+c.l)>>16)) {
        printk("floppy_dma_init: static buffer problem\n");
        return;
    }

    switch(dir) {
        /* 01:0:0:01:10 = single/inc/no-auto/to-mem/chan2 */
        case floppy_dir_read:  mode = 0x46; break;
        /* 01:0:0:10:10 = single/inc/no-auto/from-mem/chan2 */
        case floppy_dir_write: mode = 0x4a; break;
        default: printk("floppy_dma_init: invalid direction\n");
            return; /* not reached, please "mode user uninitialized" */
    }

    outb(0x06, 0x0a);   /* mask chan 2 */

    outb(0xff, 0x0c);   /* reset flip-flop */
    outb(a.b[0], 0x04); /*  - address low byte */

    outb(a.b[1], 0x04); /*  - address high byte */

    outb(a.b[2], 0x81); /* external page register */

    outb(0xff, 0x0c);   /* reset flip-flop */

    outb(c.b[0], 0x05); /*  - count low byte */
    outb(c.b[1], 0x05); /*  - count high byte */

    outb(mode, 0x0b);   /* set mode (see above) */

    outb(0x02, 0x0a);   /* unmask chan 2 */

}

int floppy_do_track(unsigned cyl, floppy_dir dir, unsigned int address) {

    unsigned char cmd;
    int i;
    unsigned char st0, st1, st2, rcy, rhe, rse, bps;
    int error = 0;

    /* Read is MT:MF:SK:0:0:1:1:0, write MT:MF:0:0:1:0:1
    // where MT = multitrack, MF = MFM mode, SK = skip deleted
    //
    // Specify multitrack and MFM mode */
    static const int flags = 0xc0;
    switch(dir) {
        case floppy_dir_read:
            cmd = CMD_READ_DATA | flags;
            break;
        case floppy_dir_write:
            cmd = CMD_WRITE_DATA | flags;
            break;
        default:
            printk("floppy_do_track: invalid direction\n");
            return 0; /* not reached, but pleases "cmd used uninitialized" */
    }

    /* seek both heads */
    if (floppy_seek(cyl, 0)) return -1;
    if (floppy_seek(cyl, 1)) return -1;

    for(i = 0; i < 20; i++) {
        floppy_motor(floppy_motor_on);

        /* init dma.. */
        floppy_dma_init(dir, address);

        delay(10); /* give some time (100ms) to settle after the seeks */

        floppy_send_command(cmd);  /* set above for current direction */
        floppy_send_command(0);    /* 0:0:0:0:0:HD:US1:US0 = head and drive */
        floppy_send_command(cyl);  /* cylinder */
        floppy_send_command(0);    /* first head (should match with above) */
        floppy_send_command(1);    /* first sector, strangely counts from 1 */
        floppy_send_command(2);    /* bytes/sector, 128*2^x (x=2 -> 512) */
        floppy_send_command(18);   /* number of tracks to operate on */
        floppy_send_command(0x1b); /* GAP3 length, 27 is default for 3.5" */
        floppy_send_command(0xff); /* data length (0xff if B/S != 0) */

        floppy_irq_wait(); /* don't SENSE_INTERRUPT here! */

        /* first read status information */

        st0 = floppy_read_data();
        st1 = floppy_read_data();
        st2 = floppy_read_data();
        /*
         * These are cylinder/head/sector values, updated with some
         * rather bizarre logic, that I would like to understand.
         *
         */
        rcy = floppy_read_data();
        rhe = floppy_read_data();
        rse = floppy_read_data();
        (void)rcy; (void)rhe; (void)rse;

        /* bytes per sector, should be what we programmed in */
        bps = floppy_read_data();

        if(st0 & 0xC0) {
            static const char * status[] =
            { 0, "error", "invalid command", "drive not ready" };
            printk("floppy_do_sector: status = %s\n", status[st0 >> 6]);
            error = 1;
        }
        if(st1 & 0x80) {
            printk("floppy_do_sector: end of cylinder\n");
            error = 1;
        }
        if(st0 & 0x08) {
            printk("floppy_do_sector: drive not ready\n");
            error = 1;
        }
        if(st1 & 0x20) {
            printk("floppy_do_sector: CRC error\n");
            error = 1;
        }
        if(st1 & 0x10) {
            printk("floppy_do_sector: controller timeout\n");
            error = 1;
        }
        if(st1 & 0x04) {
            printk("floppy_do_sector: no data found\n");
            error = 1;
        }
        if((st1|st2) & 0x01) {
            printk("floppy_do_sector: no address mark found\n");
            error = 1;
        }
        if(st2 & 0x40) {
            printk("floppy_do_sector: deleted address mark\n");
            error = 1;
        }
        if(st2 & 0x20) {
            printk("floppy_do_sector: CRC error in data\n");
            error = 1;
        }
        if(st2 & 0x10) {
            printk("floppy_do_sector: wrong cylinder\n");
            error = 1;
        }
        if(st2 & 0x04) {
            printk("floppy_do_sector: uPD765 sector not found\n");
            error = 1;
        }
        if(st2 & 0x02) {
            printk("floppy_do_sector: bad cylinder\n");
            error = 1;
        }
        if(bps != 0x2) {
            printk("floppy_do_sector: wanted 512B/sector, got %d\n", (1<<(bps+7)));
            error = 1;
        }
        if(st1 & 0x02) {
            printk("floppy_do_sector: not writable\n");
            error = 2;
        }

        if(!error) {
            floppy_motor(floppy_motor_off);
            return 0;
        }

        if(error > 1) {
            printk("floppy_do_sector: not retrying..\n");
            floppy_motor(floppy_motor_off);
            return -2;
        }

    }

    printk("floppy_do_sector: 20 retries exhausted\n");
    floppy_motor(floppy_motor_off);

    /*floppy_motor_kill();*/
    return -1;

}

int floppy_read_track(unsigned cyl, unsigned int address) {

    /*if (fd_type[0] != 4) return 0;*/

    return floppy_do_track(cyl, floppy_dir_read, address);

}

int floppy_write_track(unsigned cyl) {
    return floppy_do_track(cyl, floppy_dir_write, 0);
}

