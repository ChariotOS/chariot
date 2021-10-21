#pragma once



/* SBI Extension IDs */
#define SBI_EXT_0_1_SET_TIMER 0x0
#define SBI_EXT_0_1_CONSOLE_PUTCHAR 0x1
#define SBI_EXT_0_1_CONSOLE_GETCHAR 0x2
#define SBI_EXT_0_1_CLEAR_IPI 0x3
#define SBI_EXT_0_1_SEND_IPI 0x4
#define SBI_EXT_0_1_REMOTE_FENCE_I 0x5
#define SBI_EXT_0_1_REMOTE_SFENCE_VMA 0x6
#define SBI_EXT_0_1_REMOTE_SFENCE_VMA_ASID 0x7
#define SBI_EXT_0_1_SHUTDOWN 0x8
#define SBI_EXT_BASE 0x10
#define SBI_EXT_TIME 0x54494D45
#define SBI_EXT_IPI 0x735049
#define SBI_EXT_RFENCE 0x52464E43
#define SBI_EXT_HSM 0x48534D
#define SBI_EXT_SRST 0x53525354

/* SBI function IDs for BASE extension*/
#define SBI_EXT_BASE_GET_SPEC_VERSION 0x0
#define SBI_EXT_BASE_GET_IMP_ID 0x1
#define SBI_EXT_BASE_GET_IMP_VERSION 0x2
#define SBI_EXT_BASE_PROBE_EXT 0x3
#define SBI_EXT_BASE_GET_MVENDORID 0x4
#define SBI_EXT_BASE_GET_MARCHID 0x5
#define SBI_EXT_BASE_GET_MIMPID 0x6

/* SBI function IDs for TIME extension*/
#define SBI_EXT_TIME_SET_TIMER 0x0

/* SBI function IDs for IPI extension*/
#define SBI_EXT_IPI_SEND_IPI 0x0

/* SBI function IDs for RFENCE extension*/
#define SBI_EXT_RFENCE_REMOTE_FENCE_I 0x0
#define SBI_EXT_RFENCE_REMOTE_SFENCE_VMA 0x1
#define SBI_EXT_RFENCE_REMOTE_SFENCE_VMA_ASID 0x2
#define SBI_EXT_RFENCE_REMOTE_HFENCE_GVMA 0x3
#define SBI_EXT_RFENCE_REMOTE_HFENCE_GVMA_VMID 0x4
#define SBI_EXT_RFENCE_REMOTE_HFENCE_VVMA 0x5
#define SBI_EXT_RFENCE_REMOTE_HFENCE_VVMA_ASID 0x6

/* SBI function IDs for HSM extension */
#define SBI_EXT_HSM_HART_START 0x0
#define SBI_EXT_HSM_HART_STOP 0x1
#define SBI_EXT_HSM_HART_GET_STATUS 0x2

#define SBI_HSM_HART_STATUS_STARTED 0x0
#define SBI_HSM_HART_STATUS_STOPPED 0x1
#define SBI_HSM_HART_STATUS_START_PENDING 0x2
#define SBI_HSM_HART_STATUS_STOP_PENDING 0x3

/* SBI function IDs for SRST extension */
#define SBI_EXT_SRST_RESET 0x0

#define SBI_SRST_RESET_TYPE_SHUTDOWN 0x0
#define SBI_SRST_RESET_TYPE_COLD_REBOOT 0x1
#define SBI_SRST_RESET_TYPE_WARM_REBOOT 0x2
#define SBI_SRST_RESET_TYPE_LAST SBI_SRST_RESET_TYPE_WARM_REBOOT

#define SBI_SRST_RESET_REASON_NONE 0x0
#define SBI_SRST_RESET_REASON_SYSFAIL 0x1

#define SBI_SPEC_VERSION_MAJOR_OFFSET 24
#define SBI_SPEC_VERSION_MAJOR_MASK 0x7f
#define SBI_SPEC_VERSION_MINOR_MASK 0xffffff
#define SBI_EXT_VENDOR_START 0x09000000
#define SBI_EXT_VENDOR_END 0x09FFFFFF
#define SBI_EXT_FIRMWARE_START 0x0A000000
#define SBI_EXT_FIRMWARE_END 0x0AFFFFFF

/* SBI return error codes */
#define SBI_SUCCESS 0
#define SBI_ERR_FAILED -1
#define SBI_ERR_NOT_SUPPORTED -2
#define SBI_ERR_INVALID_PARAM -3
#define SBI_ERR_DENIED -4
#define SBI_ERR_INVALID_ADDRESS -5
#define SBI_ERR_ALREADY_AVAILABLE -6

#define SBI_LAST_ERR SBI_ERR_ALREADY_AVAILABLE

#include <types.h>

struct sbiret {
  unsigned long error;
  unsigned long value;
};

#define SBI_SET_TIMER 0x00, 0
#define SBI_CONSOLE_PUTCHAR 0x01, 0
#define SBI_CONSOLE_GETCHAR 0x02, 0
#define SBI_CLEAR_IPI 0x03, 0
#define SBI_SEND_IPI 0x04, 0
#define SBI_REMOTE_FENCEI 0x05, 0
#define SBI_REMOTE_SFENCE_VMA 0x06, 0
#define SBI_REMOTE_SFENCE_VMA_ASID 0x07, 0
#define SBI_SHUTDOWN 0x08, 0

#define SBI_GET_SBI_SPEC_VERSION 0x10, 0
#define SBI_GET_SBI_IMPL_ID 0x10, 1
#define SBI_GET_SBI_IMPL_VERSION 0x10, 2
#define SBI_PROBE_EXTENSION 0x10, 3
#define SBI_GET_MVENDORID 0x10, 4
#define SBI_GET_MARCHID 0x10, 5
#define SBI_GET_MIMPID 0x10, 6

#define SBI_EXT_TIMER_SIG 0x54494d45
#define SBI_EXT_IPI_SIG 0x00735049
#define SBI_EXT_RFENCE_SIG 0x52464e43
#define SBI_EXT_HSM_SIG 0x0048534d
#define SBI_EXT_SRST_SIG 0x53525354

#define SBI_EXT_CONSOLE_PUTCHAR 0x01


#ifdef __cplusplus

void sbi_early_init(void);
void sbi_init(void);

void sbi_set_timer(uint64_t stime_value);
void sbi_send_ipis(const unsigned long *hart_mask);
void sbi_clear_ipi(void);
// status_t sbi_boot_hart(uint hartid, paddr_t start_addr, ulong arg);

bool sbi_probe_extension(unsigned long extension);

struct sbiret sbi_generic_call_2(unsigned long extension, unsigned long function);
struct sbiret sbi_generic_call_3(unsigned long extension, unsigned long function);

#endif


// make a SBI call according to the SBI spec at https://github.com/riscv/riscv-sbi-doc
// Note: it seems ambigious whether or not a2-a7 are trashed in the call, but the
// OpenSBI and linux implementations seem to assume that all of the regs are restored
// aside from a0 and a1 which are used for return values.
#define _sbi_call(extension, function, arg0, arg1, arg2, arg3, arg4, arg5, ...)                                   \
  ({                                                                                                              \
    register unsigned long a0 asm("a0") = (unsigned long)arg0;                                                    \
    register unsigned long a1 asm("a1") = (unsigned long)arg1;                                                    \
    register unsigned long a2 asm("a2") = (unsigned long)arg2;                                                    \
    register unsigned long a3 asm("a3") = (unsigned long)arg3;                                                    \
    register unsigned long a4 asm("a4") = (unsigned long)arg4;                                                    \
    register unsigned long a5 asm("a5") = (unsigned long)arg5;                                                    \
    register unsigned long a6 asm("a6") = (unsigned long)function;                                                \
    register unsigned long a7 asm("a7") = (unsigned long)extension;                                               \
    asm volatile("ecall" : "+r"(a0), "+r"(a1) : "r"(a2), "r"(a3), "r"(a4), "r"(a5), "r"(a6), "r"(a7) : "memory"); \
    (struct sbiret){.error = a0, .value = a1};                                                                    \
  })

#define sbi_call(...) _sbi_call(__VA_ARGS__, 0, 0, 0, 0, 0, 0, 0)
