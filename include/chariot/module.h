#ifndef __MODULE_H__
#define __MODULE_H__

#include <kshell.h>

typedef void (*init_function_type)(void);

struct kernel_module_info {
  const char *name;
  const char *file;
  init_function_type initfn;
};

#define module_init(name, initfn)                                                      \
  static char __kmod_info_name[] = name;                                               \
  static char __kmod_info_file[] = __FILE__;                                           \
  static struct kernel_module_info __kmod_info__ __attribute__((__used__))             \
  __attribute__((unused, __section__("_kernel_modules"), aligned(sizeof(void *)))) = { \
      __kmod_info_name,                                                                \
      __kmod_info_file,                                                                \
      initfn,                                                                          \
  };


void initialize_builtin_modules(void);



#define _MOD_CAT_(a, b) a##b
#define _MOD_CAT(a, b) _MOD_CAT_(a, b)
#define _MOD_VARNAME(Var) _MOD_CAT(Var, __LINE__)


// to make creating kernel shell functions easier.
#define module_init_kshell(cmd, usage, func) \
  void _MOD_VARNAME(kshell_init)(void) { kshell::add(cmd, usage, func); } \
	module_init(cmd, _MOD_VARNAME(kshell_init));
#endif
