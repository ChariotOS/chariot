#include "cpuid.h"
#include <asm.h>
#include <printk.h>
#include <util.h>

int cpuid::run(uint32_t func, cpuid::ret_t &ret) {
  asm volatile(
      "pushq %%rbx;"
      "movq %%rdi, %%rbx;"
      "cpuid ;"
      "movq %%rbx, %%rdi; "
      "popq %%rbx ;"
      : "=a"(ret.a), "=D"(ret.b), "=c"(ret.c), "=d"(ret.d)
      : "0"(func));

  return 0;
}



int cpuid::run_sub(uint32_t func, uint32_t sub_func, cpuid::ret_t &ret) {
  asm volatile(
      "pushq %%rbx;"
      "movq %%rdi, %%rbx;"
      "cpuid ;"
      "movq %%rbx, %%rdi; "
      "popq %%rbx ;"
      : "=a"(ret.a), "=D"(ret.b), "=c"(ret.c), "=d"(ret.d)
      : "0"(func), "2"(sub_func));

  return 0;
}


uint8_t cpuid::get_family(void) {
  uint8_t base_fam;
  uint8_t ext_fam;
  cpuid::ret_t ret;
  cpuid::run(CPUID_FEATURE_INFO, ret);
  base_fam = CPUID_GET_BASE_FAM(ret.a);

  if (base_fam < 0xfu) {
    return base_fam;
  }

  ext_fam = CPUID_GET_EXT_FAM(ret.a);

  return base_fam + ext_fam;
}


uint8_t cpuid::get_model(void) {
  uint8_t base_fam;
  uint8_t base_mod;
  uint8_t ext_mod;
  cpuid::ret_t ret;
  cpuid::run(CPUID_FEATURE_INFO, ret);
  base_fam = CPUID_GET_BASE_FAM(ret.a);
  base_mod = CPUID_GET_BASE_MOD(ret.a);

  if (base_fam < 0xfu) {
    return base_mod;
  }

  ext_mod = CPUID_GET_EXT_MOD(ret.a);

  return ext_mod << 4 | base_mod;
}


uint8_t cpuid::get_step(void) {
  cpuid::ret_t ret;
  cpuid::run(CPUID_FEATURE_INFO, ret);
  return CPUID_GET_PROC_STEP(ret.a);
}


uint32_t cpuid::leaf_max(void) {
  cpuid::ret_t ret;
  cpuid::run(0, ret);
  return ret.a & 0xff;
}


static void get_vendor_string(char buf[13]) {
  memset(buf, 0, 13);

  cpuid::ret_t id;
  cpuid::run(CPUID_LEAF_BASIC_INFO0, id);

  uint32_t *b = (uint32_t *)buf;
  b[0] = id.b;
  b[1] = id.d;
  b[2] = id.c;
  b[3] = 0;
}

void cpuid::detect_cpu(void) {
  char branding[13];
  get_vendor_string(branding);
  printk(KERN_INFO "Detected %s Processor\n", branding);
}



bool cpuid::is_amd(void) {
  char name[13];
  get_vendor_string(name);
  return !strcmp(name, "AuthenticAMD");
}

bool cpuid::is_intel(void) {
  char name[13];
  get_vendor_string(name);
  return !strcmp(name, "GenuineIntel");
}
