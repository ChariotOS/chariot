#include <module.h>
#include <printf.h>

void initialize_builtin_modules(void) {
  extern struct kernel_module_info __start__kernel_modules[];
  extern struct kernel_module_info __stop__kernel_modules[];

  {
    // printf(KERN_DEBUG "Kernel Modules being Loaded:\n");
    struct kernel_module_info *mod = __start__kernel_modules;
    int i = 0;
    while (mod != __stop__kernel_modules) {
      // printf(KERN_DEBUG "%p - %s\n", mod->initfn, mod->name);
      mod = &(__start__kernel_modules[++i]);
    }
  }

  {
    struct kernel_module_info *mod = __start__kernel_modules;
    int i = 0;
    while (mod != __stop__kernel_modules) {
      // printf(KERN_DEBUG "%p - %s\n", mod->initfn, mod->name);
      mod->initfn();
      // printf(KERN_DEBUG "Module '%s' done.\n", mod->name);
      mod = &(__start__kernel_modules[++i]);
    }
  }
}
