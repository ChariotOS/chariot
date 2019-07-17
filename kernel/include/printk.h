// some super basic prink stuffs

#include <types.h>

// use the host's headers as the information within should be header-only :)
#include <stdarg.h>
#include <stddef.h>


typedef i64 acpi_native_int;


void putchar(char);
int puts(char*);
int printk(const char* format, ...);
int sprintk(char* buffer, const char* format, ...);
int snprintk(char* buffer, size_t count, const char* format, ...);
int vsnprintk(char* buffer, size_t count, const char* format, va_list va);
