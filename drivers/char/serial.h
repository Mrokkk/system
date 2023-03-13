#pragma once

int serial_init();
void write_serial(char a);
void serial_print(const char *string);
void serial_log_print(const char *string);
int serial_printf(const char *fmt, ...);
