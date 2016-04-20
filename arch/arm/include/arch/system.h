#ifndef __SYSTEM_H_
#define __SYSTEM_H_

#include <kernel/compiler.h>
#include <kernel/process.h>
#include <arch/segment.h>

#define mb()

#define save_flags(x) (void)x

#define restore_flags(x) (void)x
#define sti()

#define cli()
#define nop()

#define halt()


/* Context switching */
#define process_switch(prev, next) (void)prev; (void)next

extern unsigned long loops_per_sec;
extern void do_delay();

extern inline void udelay(unsigned long usecs) {

    (void)usecs;

}

extern void irq_enable(unsigned int);
extern void switch_to_user();
extern void delay(unsigned int);
extern void reboot();
extern void shutdown();
extern void copy_from_user(void *dest, void *src, unsigned int size);
extern void copy_to_user(void *dest, void *src, unsigned int size);
extern void get_from_user(void *, void *);
extern void put_to_user(void *, void *);
unsigned int ram_get();

#endif /* __SYSTEM_H_ */
