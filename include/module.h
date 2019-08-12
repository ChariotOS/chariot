#ifndef __MODULE_H__
#define __MODULE_H__

typedef void (*init_function_type)(void);

struct kernel_module_info {
  const char *name;
  init_function_type initfn;
};

#define module_init(name, initfn)                                          \
  static char __kmod_info_name[] = name;                                   \
  static struct kernel_module_info __kmod_info__ __attribute__((__used__)) \
      __attribute__((unused, __section__("_kernel_modules"),               \
                     aligned(sizeof(void *)))) = {                         \
          __kmod_info_name,                                                \
          initfn,                                                          \
  };

#endif
