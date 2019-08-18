#pragma once

// some super basic prink stuffs

#include <types.h>

// use the host's headers as the information within should be header-only :)
#include <stdarg.h>
#include <stddef.h>
// #include <string.h>

typedef i64 acpi_native_int;

void putchar(char);
int puts(char*);
int printk(const char* format, ...);
int sprintk(char* buffer, const char* format, ...);
int snprintk(char* buffer, size_t count, const char* format, ...);
int vsnprintk(char* buffer, size_t count, const char* format, va_list va);



const char* human_size(uint64_t bytes, char* buf);

template <typename... T>
inline void panic(const char* fmt, T&&... args) {
  printk("[PANIC] ");
  printk(fmt, args...);
  printk("\n");
  while (1)
    ;
}

#define assert(val)                          \
  do {                                       \
    if (!(val)) {                              \
      panic("assertion failed: %s\n", #val); \
    }                                        \
  } while (0);

class scope_logger {
 public:
  template <typename... T>
  scope_logger(const char* fmt, T&&... args) {
    printk(fmt, args...);
    printk("...");
  }

  ~scope_logger() { printk("OK\n"); }
};

