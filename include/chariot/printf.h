#pragma once

// some super basic prink stuffs

#ifdef __cplusplus
#include <types.h>

// use the host's headers as the information within should be header-only :)
#include <arch.h>
#include <asm.h>
#include <stddef.h>


// #include <ck/string.h>
//
//
namespace arch {
  void dump_backtrace(void);
};

typedef i64 acpi_native_int;

#endif

#include <stdarg.h>

extern "C" {
void putchar(char);
int puts(char*);
void printf_nolock(const char* format, ...);
int printf(const char* format, ...);
int sprintf(char* buffer, const char* format, ...);
int snprintf(char* buffer, size_t count, const char* format, ...);
int vsnprintf(char* buffer, size_t count, const char* format, va_list va);

int sscanf(const char* buf, const char* fmt, ...);


// process printk, if you are running in the kernel context of a thread of /bin/sh
// pprintf("hello, world\n") outputs:
// [/bin/sh %d] hello, world
//
int pprintf(const char* format, ...);

const char* human_size(uint64_t bytes, char* buf);
}



#define BLK "\e[0;30m"
#define RED "\e[0;31m"
#define GRN "\e[0;32m"
#define YEL "\e[0;33m"
#define BLU "\e[0;34m"
#define MAG "\e[0;35m"
#define CYN "\e[0;36m"
#define WHT "\e[0;37m"

#define GRY "\e[0;90m"

#define RESET "\e[0m"

#define KLOG(PREFIX, ...) KINFO(PREFIX RESET, __VA_ARGS__)
#define PFXLOG(PREFIX, ...) printf(PREFIX RESET ": " __VA_ARGS__)

#define KERN_ERROR "\0010"
#define KERN_WARN "\0011"
#define KERN_INFO "\0012"
#define KERN_DEBUG "\0013"
#define KERN_DISABLE "\0014"

#define debug(fmt, args...) printf(KERN_DEBUG fmt, ##args)
#define KERR(fmt, args...) printf(KERN_ERROR fmt, ##args)
#define KWARN(fmt, args...) printf(KERN_WARN fmt, ##args)
#define KINFO(fmt, args...) printf(KERN_INFO fmt, ##args)

extern void debug_die(void);

template <typename... T>
inline void do_panic(const char* fmt, T&&... args) {
  printf(fmt, args...);
  printf("\n");
  debug_die();
}


void debug_dump_addr2line();

#define panic(fmt, args...)                   \
  do {                                        \
    KERR("PANIC: %s\n", __PRETTY_FUNCTION__); \
    do_panic(KERN_ERROR fmt, ##args);         \
  } while (0);

#define assert(val)                          \
  do {                                       \
    if (!(val)) {                            \
      panic("assertion failed: %s\n", #val); \
    }                                        \
  } while (0);

#define UNIMPL() printf(KERN_WARN "'%s' NOT IMPLEMENTED\n", __PRETTY_FUNCTION__)

#define LOG_TIME time_logger __TLOGGER(__PRETTY_FUNCTION__)
