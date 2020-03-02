#include <module.h>

void initialize_builtin_modules(void) {
  extern struct kernel_module_info __start__kernel_modules[];
  extern struct kernel_module_info __stop__kernel_modules[];
  struct kernel_module_info *mod = __start__kernel_modules;
  int i = 0;
  while (mod != __stop__kernel_modules) {
    mod->initfn();
    mod = &(__start__kernel_modules[++i]);
  }
}
