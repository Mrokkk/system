#ifndef INCLUDE_KERNEL_PRINTK_H_
#define INCLUDE_KERNEL_PRINTK_H_

void console_register(void (*func)(const char *string));
int printk(const char *, ...) __attribute__ ((format (printf, 1, 2)));
void panic(const char *fmt, ...);

#endif /* INCLUDE_KERNEL_PRINTK_H_ */
