#ifndef ARCH_X86_SERIAL_H_
#define ARCH_X86_SERIAL_H_

int serial_init();
void write_serial(char a);
void serial_print(const char *string);

#endif /* ARCH_X86_SERIAL_H_ */
