#pragma once

// some super basic prink stuffs

#include <types.h>

// use the host's headers as the information within should be header-only :)
#include <asm.h>
#include <dev/RTC.h>
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


#define KLOG(CLR, fmt, args...)                                             \
  do {                                                                      \
    struct tm t;                                                            \
    dev::RTC::localtime(t);                                                 \
    printk("[" CLR "%02d:%02d:%02d" RESET "] " fmt, t.tm_hour, t.tm_min, t.tm_sec, ##args); \
  } while (0);


#define KINFO(fmt, args...) KLOG("", fmt, ##args)
#define KWARN(fmt, args...) KLOG(YEL, fmt, ##args)
#define KERR(fmt, args...) KLOG(RED, fmt, ##args)



template <typename... T>
inline void do_panic(const char* fmt, T&&... args) {
  // disable interrupts
  cli();
  printk(fmt, args...);
  printk("\n");
  while (1) {
    halt();
  }
}


#define panic(fmt, args...) \
  do { KERR("PANIC: %s\n", __PRETTY_FUNCTION__); \
  do_panic(fmt, ##args); } while(0);


#define assert(val)                          \
  do {                                       \
    if (!(val)) {                            \
      cli();                                 \
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

