#pragma once

// some super basic prink stuffs

#include <types.h>

// use the host's headers as the information within should be header-only :)
#include <asm.h>
#include <dev/RTC.h>
#include <stdarg.h>
#include <stddef.h>
#include <arch.h>
// #include <string.h>

typedef i64 acpi_native_int;

void putchar(char);
int puts(char*);
void printk_nolock(const char *format, ...);
int printk(const char* format, ...);
int sprintk(char* buffer, const char* format, ...);
int snprintk(char* buffer, size_t count, const char* format, ...);
int vsnprintk(char* buffer, size_t count, const char* format, va_list va);

int sscanf(const char* buf, const char* fmt, ...);

const char* human_size(uint64_t bytes, char* buf);

#define BLK "\e[0;30m"
#define RED "\e[0;31m"
#define GRN "\e[0;32m"
#define YEL "\e[0;33m"
#define BLU "\e[0;34m"
#define MAG "\e[0;35m"
#define CYN "\e[0;36m"
#define WHT "\e[0;37m"

#define RESET "\e[0m"

#define KLOG(PREFIX, fmt, args...)   \
  do {                               \
    printk(PREFIX ": " fmt, ##args); \
  } while (0);



#define KERN_ERROR "\0010"
#define KERN_WARN "\0011"
#define KERN_INFO "\0012"
#define KERN_DEBUG "\0013"

#define KERR(fmt, args...) printk(KERN_ERROR fmt, ##args)
#define KWARN(fmt, args...) printk(KERN_WARN fmt, ##args)
#define KINFO(fmt, args...) printk(KERN_INFO fmt, ##args)

template <typename... T>
inline void do_panic(const char* fmt, T&&... args) {
  // disable interrupts
  arch::cli();
  printk(fmt, args...);
  printk("\n");
  while (1) {
    arch::halt();
  }
}

#define panic(fmt, args...)                   \
  do {                                        \
    KERR("PANIC: %s\n", __PRETTY_FUNCTION__); \
    do_panic(fmt, ##args);                    \
  } while (0);

#define assert(val)                          \
  do {                                       \
    if (!(val)) {                            \
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

class time_logger {
  const char* const name;
  uint64_t start;

 public:
  time_logger(const char* const name);
  ~time_logger(void);
};

#define LOG_TIME time_logger __TLOGGER(__PRETTY_FUNCTION__)
