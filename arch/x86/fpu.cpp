#include "fpu.h"
#include <arch.h>
#include <printk.h>
#include <sched.h>
#include "cpuid.h"
#include "msr.h"

struct fpu::fpu_caps fpu::caps;


// #define FPU_DO_DEBUG


#ifdef FPU_DO_DEBUG
#define FPU_DEBUG(fmt, args...) printk(KERN_INFO "[FPU] " fmt, ##args)
#define FPU_WARN(fmt, args...) printk(KERN_WARN "[FPU] " fmt, ##args)
#else
#define FPU_DEBUG(fmt, args...)
#define FPU_WARN(fmt, args...)
#endif


#define _INTEL_FPU_FEAT_QUERY(r, feat)   \
  ({                                     \
    cpuid::ret_t ret;                    \
    struct cpuid::e##r##x_flags f;       \
    cpuid::run(CPUID_FEATURE_INFO, ret); \
    f.val = ret.r;                       \
    f.feat;                              \
  })

#define _AMD_FPU_FEAT_QUERY(r, feat)         \
  ({                                         \
    cpuid::ret_t ret;                        \
    struct cpuid::amd_e##r##x_flags f;       \
    cpuid::run(CPUID_AMD_FEATURE_INFO, ret); \
    f.val = ret.r;                           \
    f.feat;                                  \
  })

#define _INTEL_FPU_EXT_FEAT_QUERY(r, feat)                                   \
  ({                                                                         \
    cpuid::ret_t ret;                                                        \
    struct cpuid::ext_feat_flags_e##r##x f;                                  \
    cpuid::run_sub(CPUID_EXT_FEATURE_INFO, CPUID_EXT_FEATURE_SUB_INFO, ret); \
    f.val = ret.r;                                                           \
    f.feat;                                                                  \
  })

#define FPU_ECX_FEAT_QUERY(feat) _INTEL_FPU_FEAT_QUERY(c, feat)
#define FPU_EDX_FEAT_QUERY(feat) _INTEL_FPU_FEAT_QUERY(d, feat)
#define FPU_EBX_EXT_FEAT_QUERY(feat) _INTEL_FPU_EXT_FEAT_QUERY(b, feat)

#define AMD_FPU_ECX_FEAT_QUERY(feat) _AMD_FPU_FEAT_QUERY(c, feat)
#define AMD_FPU_EDX_FEAT_QUERY(feat) _AMD_FPU_FEAT_QUERY(d, feat)

#define DEFAULT_FUN_CHECK(fun, str)    \
  if (fun()) {                         \
    FPU_DEBUG(#str " Detected!\n");    \
  } else {                             \
    FPU_WARN(#str " NOT Detected!\n"); \
  }

uint16_t fpu::get_x87_status(void) {
  uint16_t status;
  asm volatile("fnstsw %[_a]" : [ _a ] "=a"(status) : :);

  return status;
}

void fpu::set_x87_ctrl(uint16_t val) {
  asm volatile("fldcw %[_m]" ::[_m] "m"(val) : "memory");
}


void fpu::clear_x87_excp(void) { asm volatile("fnclex" :::); }


bool fpu::has_x87() { return FPU_EDX_FEAT_QUERY(fpu); }


void fpu::enable_x87(void) {
  unsigned long r;

  r = read_cr0();
  r |= CR0_MP |  // If TS flag is set, WAIT/FWAIT generate #NM exception (for
                 // lazy switching)
       CR0_NE;   // Generate FP exceptions internally, not on a PIC

  write_cr0(r);

  asm volatile("fninit" ::: "memory");

  /*
   * - mask all x87 exceptions
   * - double extended precision
   * - round to nearest (even)
   * - infinity ctrl N/A
   *
   * (fninit should set to this value,
   * but being paranoid)
   */
  set_x87_ctrl(0x037f);
}


static uint8_t has_clflush(void) { return FPU_EDX_FEAT_QUERY(clfsh); }
static uint8_t has_mmx(void) { return FPU_EDX_FEAT_QUERY(mmx); }
static uint8_t amd_has_mmx_ext(void) { return AMD_FPU_EDX_FEAT_QUERY(mmx_ext); }
static uint8_t amd_has_3dnow(void) { return AMD_FPU_EDX_FEAT_QUERY(amd3dnow); }
static uint8_t amd_has_3dnow_ext(void) {
  return AMD_FPU_EDX_FEAT_QUERY(amd3dnowext);
}
static uint8_t has_sse(void) { return FPU_EDX_FEAT_QUERY(sse); }
static uint8_t has_sse2(void) { return FPU_EDX_FEAT_QUERY(sse2); }
static uint8_t has_sse3(void) { return FPU_ECX_FEAT_QUERY(sse3); }
static uint8_t has_ssse3(void) { return FPU_ECX_FEAT_QUERY(ssse3); }
static uint8_t has_sse4d1(void) { return FPU_ECX_FEAT_QUERY(sse4dot1); }
static uint8_t has_sse4d2(void) { return FPU_ECX_FEAT_QUERY(sse4dot2); }
static uint8_t amd_has_sse4a(void) { return AMD_FPU_ECX_FEAT_QUERY(sse4a); }
static uint8_t amd_has_prefetch(void) {
  return AMD_FPU_ECX_FEAT_QUERY(prefetch3d);
}
static uint8_t amd_has_misal_sse(void) {
  return AMD_FPU_ECX_FEAT_QUERY(misalignsse);
}
static uint8_t has_fma4(void) { return FPU_ECX_FEAT_QUERY(fma); }
static uint8_t amd_has_fma4(void) { return AMD_FPU_ECX_FEAT_QUERY(fma4); }
static uint8_t has_cvt16(void) { return FPU_ECX_FEAT_QUERY(f16c); }
static uint8_t has_cx16(void) { return FPU_ECX_FEAT_QUERY(cx16); }
static uint8_t has_avx(void) { return FPU_ECX_FEAT_QUERY(avx); }
static uint8_t has_avx2(void) { return FPU_EBX_EXT_FEAT_QUERY(avx2); }
static uint8_t has_avx512f(void) { return FPU_EBX_EXT_FEAT_QUERY(avx512f); }
static uint8_t has_fxsr(void) { return FPU_EDX_FEAT_QUERY(fxsr); }
static uint8_t has_xsave(void) { return FPU_ECX_FEAT_QUERY(xsave); }


static uint32_t get_xsave_features(void) {
  cpuid::ret_t r;
  cpuid::run_sub(0x0d, 0, r);
  return r.a;
}

static void set_osxsave(void) {
  unsigned long r = read_cr4();
  r |= CR4_OSXSAVE;
  write_cr4(r);
}

static void enable_xsave(void) {
	return;
  fpu::caps.xsave = true;
  /* Enables XSAVE features by reading CR4 and writing OSXSAVE bit */
  set_osxsave();

  /* XSAVE feature setup done later after we figure out support of AVX, AVX512f
   * and SSE */
}

static void enable_sse(void) {
  fpu::caps.sse = true;
  unsigned long r = read_cr4();
  uint32_t m;

  /* NOTE: assuming EM and MP bit in CR0 are already set */

  r |= CR4_OSFXSR |     // OS supports save/restore of FPU regs
       CR4_OSXMMEXCPT;  // OS can handle SIMD floating point exceptions

  write_cr4(r);

  /* bang on the EFER so we dont' get fast FXSAVE */
  r = msr_read(MSR_EFER);
  r &= ~(EFER_FFXSR);
  msr_write(MSR_EFER, r);

  m = 0x00001f80;  // mask all FPU exceptions, no denormals are zero
  asm volatile("ldmxcsr %[_m]" ::[_m] "m"(m) : "memory");
}



static void fpu_init_common() {
  uint8_t x87_ready = 0;
  uint8_t sse_ready = 0;
  uint8_t xsave_ready = 0;
  uint8_t avx_ready = 0;
  uint8_t avx2_ready = 0;
  uint8_t avx512f_ready = 0;
  uint32_t xsave_support = 0;

  if (fpu::has_x87()) {
    FPU_DEBUG("[x87]\n");
    x87_ready = 1;
  }

  if (has_sse()) {
    ++sse_ready;
    fpu::caps.sse = true;
    FPU_DEBUG("[SSE]\n");
  }

  if (has_clflush()) {
    ++sse_ready;
    fpu::caps.sse = true;
    FPU_DEBUG("[CLFLUSH]\n");
  }

  DEFAULT_FUN_CHECK(has_sse2, SSE2)
  DEFAULT_FUN_CHECK(has_sse3, SSE3)

  if (has_fxsr()) {
    ++sse_ready;
    fpu::caps.sse = true;
    FPU_DEBUG("[FXSAVE/RESTORE]\n");
  } else {
    panic("No FXSAVE/RESTORE support. Thread switching will be broken\n");
  }

  if (has_xsave()) {
    ++xsave_ready;
    FPU_DEBUG("[XSAVE/RESTORE]\n");
  }

  if (has_avx()) {
    ++avx_ready;
    FPU_DEBUG("[AVX]\n");
  }

  if (has_avx2()) {
    ++avx2_ready;
    FPU_DEBUG("[AVX2]\n");
  }

  if (has_avx512f()) {
    ++avx512f_ready;
    FPU_DEBUG("[AVX512]\n");
  }

  DEFAULT_FUN_CHECK(has_sse4d1, SSE4 .1)
  DEFAULT_FUN_CHECK(has_sse4d2, SSE4 .2)
  DEFAULT_FUN_CHECK(has_mmx, MMX)

  /* should we turn on x87? */
  if (x87_ready) {
    FPU_DEBUG("Initializing legacy x87 FPU\n");
    fpu::enable_x87();
  }

  /* does processor have xsave instructions? */
  if (xsave_ready) {
    FPU_DEBUG("Initializing XSAVE instructions\n");
    enable_xsave();
    /* x87 support is non-optional if xsave enabled */
    xsave_support |= 0x1;
  }

  /* did we meet SSE requirements? */
  if (sse_ready >= 3) {
    FPU_DEBUG("Initializing SSE extensions\n");
    enable_sse();
    /* If we want XSAVE to save SSE registers, add it to bit mask */
    if (xsave_support >= 1) {
      xsave_support |= 0x2;
    }
  }

  /* Does processor have AVX registers? */
  if (avx_ready) {
    /* Can only enable XSAVE AVX support if processor has SSE support */
    if (xsave_support >= 3) {
      FPU_DEBUG("Initializing XSAVE AVX support\n");
      xsave_support |= 0x4;
    }
  }

  /* Does processor have AVX2 registers? */
  if (avx2_ready) {
    FPU_DEBUG("Initializing AVX2 support\n");
  }

  // Does processor have AVX512f registers?
  if (avx512f_ready) {
    /* Can only enable AVX512f if processor has SSE and AVX support */
    if (xsave_support >= 7 && avx_ready) {
      FPU_DEBUG("Initializing AVX512f support\n");
      /* Bits correspond to AVX512 opmasks, top half of lower ZMM regs, and
       * upper ZMM regs */
      xsave_support |= 0xe0;
    }
  }

  /* Configure XSAVE Support */
  if (xsave_ready) {
    fpu::caps.xsave = true;
    xsave_support &= get_xsave_features();
    asm volatile(
        "xor %%rcx, %%rcx ;"
        "xsetbv ;"
        :
        : "a"(xsave_support)
        : "rcx", "memory");
  }
}

void fpu::init(void) {
  fpu_init_common();

  if (cpuid::is_intel()) {
    FPU_DEBUG("Probing for Intel specific FPU/SIMD extensions\n");
    DEFAULT_FUN_CHECK(has_cx16, CX16)
    DEFAULT_FUN_CHECK(has_cvt16, CVT16)
    DEFAULT_FUN_CHECK(has_fma4, FMA4)
    DEFAULT_FUN_CHECK(has_ssse3, SSSE3)
  } else if (cpuid::is_amd()) {
    FPU_DEBUG("Probing for AMD-specific FPU/SIMD extensions\n");
    DEFAULT_FUN_CHECK(amd_has_fma4, FMA4)
    DEFAULT_FUN_CHECK(amd_has_mmx_ext, AMDMMXEXT)
    DEFAULT_FUN_CHECK(amd_has_sse4a, SSE4A)
    DEFAULT_FUN_CHECK(amd_has_3dnow, 3DNOW)
    DEFAULT_FUN_CHECK(amd_has_3dnow_ext, 3DNOWEXT)
    DEFAULT_FUN_CHECK(amd_has_prefetch, PREFETCHW)
    DEFAULT_FUN_CHECK(amd_has_misal_sse, MISALSSE)
  }
}


extern "C" void __fpu_xsave64(void *);
extern "C" void __fpu_xrstor64(void *);

void arch::save_fpu(struct thread &thd) {
  if (fpu::caps.xsave) {
    __fpu_xsave64(thd.fpu.state);
  } else {
    asm volatile("fxsave64 (%0);" ::"r"(thd.fpu.state));
  }
}

void arch::restore_fpu(struct thread &thd) {

	// printk("cr4=%p\n", read_cr4());

  if (!thd.fpu.initialized) {
    asm volatile("fninit");

    if (fpu::caps.xsave) {
      __fpu_xsave64(thd.fpu.state);
    } else {
      asm volatile("fxsave64 (%0);" ::"r"(thd.fpu.state));
    }
    thd.fpu.initialized = true;
  } else {
    if (fpu::caps.xsave) {
      __fpu_xrstor64(thd.fpu.state);
    } else {
      asm volatile("fxrstor64 (%0);" ::"r"(thd.fpu.state));
    }
  }
}
