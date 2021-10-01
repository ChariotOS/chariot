#ifndef __MODULE_H__
#define __MODULE_H__

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

#endif
