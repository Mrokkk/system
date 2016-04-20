#ifndef DRIVERS_BLOCK_FLOPPY_H_
#define DRIVERS_BLOCK_FLOPPY_H_

#define FLOPPY_BASE     0x3f0
#define FLOPPY_DOR      (FLOPPY_BASE+2)
#define FLOPPY_MSR      (FLOPPY_BASE+4)
#define FLOPPY_FIFO     (FLOPPY_BASE+5)
#define FLOPPY_CCR      (FLOPPY_BASE+7)

#define DOR_MOTD        (1 << 7)
#define DOR_MOTC        (1 << 6)
#define DOR_MOTB        (1 << 5)
#define DOR_MOTA        (1 << 4)
#define DOR_IRQ         (1 << 3)
#define DOR_RESET       (1 << 2)
#define DOR_DSEL(x)     ((x) & 3)

#define MSR_RQM         (1 << 7)
#define MSR_DIO         (1 << 6)
#define MSR_NDMA        (1 << 5)
#define MSR_CB          (1 << 4)
#define MSR_ACTD        (1 << 3)
#define MSR_ACTC        (1 << 2)
#define MSR_ACTB        (1 << 1)
#define MSR_ACTA        (1 << 0)

#define CMD_SPECIFY         3
#define CMD_WRITE_DATA      5
#define CMD_READ_DATA       6
#define CMD_RECALIBRATE     7
#define CMD_SENSE_INTERRUPT 8
#define CMD_SEEK            15
#define CMD_VERSION         16

#define FLOPPY_144_SECTORS_PER_TRACK 18
#define FLOPPY_DMA_BUFFER_SIZE 0x4800

enum { floppy_motor_off = 0, floppy_motor_on, floppy_motor_wait };

typedef enum {
    floppy_dir_read,
    floppy_dir_write
} floppy_dir;

#endif /* DRIVERS_BLOCK_FLOPPY_H_ */
