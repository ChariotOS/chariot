#pragma once

#define APIC_SPUR_INT_VEC      0xff
#define APIC_TIMER_INT_VEC     0xf0
#define APIC_ERROR_INT_VEC     0xf1
#define APIC_THRML_INT_VEC     0xf2
#define APIC_PC_INT_VEC        0xf3
#define APIC_CMCR_INT_VEC      0xf4
#define APIC_EXT_LVT_DUMMY_VEC 0xf5
#define APIC_NULL_KICK_VEC     0xfc
    

#define APIC_TIMER_DIV_1   0xb
#define APIC_TIMER_DIV_2   0x0
#define APIC_TIMER_DIV_4   0x1
#define APIC_TIMER_DIV_8   0x2
#define APIC_TIMER_DIV_16  0x3
#define APIC_TIMER_DIV_32  0x8
#define APIC_TIMER_DIV_64  0x9
#define APIC_TIMER_DIV_128 0xa


#define APIC_TIMER_DIV 16
#define APIC_TIMER_DIVCODE APIC_TIMER_DIV_16
    
#define APIC_BASE_MSR        0x0000001b
    
#define IA32_APIC_BASE_MSR_BSP    0x100 
#define IA32_APIC_BASE_MSR_ENABLE 0x800
    
#define APIC_BASE_ADDR_MASK 0xfffffffffffff000ULL
#define APIC_IS_BSP(x)      ((x) & (1 << 8))
#define APIC_VERSION(x)     ((x) & 0xffu)
    
#define APIC_IPI_SELF          0x40000
#define APIC_IPI_ALL           0x80000
#define APIC_IPI_OTHERS        0xC0000
#define APIC_GLOBAL_ENABLE     (1u << 11)
#define APIC_SPIV_SW_ENABLE    (1u << 8)
#define APIC_SPIV_VEC_MASK     0xffu
#define APIC_SPIV_CORE_FOCUS  (1u << 9)
#define APIC_SPIC_EOI_BROADCAST_DISABLE (1u << 12)
    #define ICR_DEL_MODE_LOWEST  (1 << 8)
#define ICR_DEL_MODE_SMI     (2 << 8)
#define ICR_DEL_MODE_NMI     (4 << 8)
#define ICR_DEL_MODE_INIT    (5 << 8)
#define ICR_DEL_MODE_STARTUP (6 << 8)
    
#define ICR_DST_MODE_LOG      (1 << 11)
#define ICR_SEND_PENDING      (1 << 12)
#define ICR_LEVEL_ASSERT      (1 << 14)
#define ICR_TRIG_MODE_LEVEL   (1 << 15)
    
    
    
// TODO: on a xeon phi, this is 16 instead.
#ifdef CONFIG_XEON_PHI
#define APIC_ID_SHIFT 16
#define APIC_ICR2_DST_SHIFT 16
#else
#define APIC_ID_SHIFT 24
#define APIC_ICR2_DST_SHIFT 24
#endif

    
#define APIC_REG_ID       0x20
#define   APIC_GET_ID(x) (((x) >> 24) & 0xffu)
#define APIC_REG_LVR      0x30
#define     APIC_LVR_VER(x) ((x) & 0xffu)
#define     APIC_LVR_MAX(x) (((x) >> 16) & 0xffu)
#define     APIC_LVR_HAS_DIRECTED_EOI(x) ((x)>>24 & 0x1) // "broadcast suppression"
#define     APIC_LVR_HAS_EXT_LVT(x) (x & (1U<<31))
#define APIC_REG_TPR      0x80
#define APIC_REG_APR      0x90
#define APIC_REG_PPR      0xa0
#define APIC_REG_EOI      0xb0
#define APIC_REG_RRR      0xc0
#define APIC_REG_LDR      0xd0
#define APIC_REG_DFR      0xe0
#define APIC_REG_SPIV     0xf0
#define APIC_REG_ISR      0x100
#define     APIC_GET_ISR(x)  (APIC_REG_ISR + 0x10*(x))
#define APIC_REG_TMR      0x180
#define APIC_REG_IRR      0x200
#define     APIC_GET_IRR(x)  (APIC_REG_IRR + 0x10*(x))
#define APIC_REG_ESR      0x280
#define APIC_REG_LVT_CMCI 0x2f0
#define APIC_REG_ICR      0x300
#define     APIC_ICR_BUSY 0x01000
#define APIC_REG_ICR2     0x310
#define APIC_REG_LVTT     0x320
#define APIC_REG_LVTTHMR  0x330
#define APIC_REG_LVTPC    0x340
#define APIC_REG_LVT0     0x350
#define APIC_REG_LVT1     0x360
#define APIC_REG_LVTERR   0x370
#define APIC_REG_TMICT    0x380
#define APIC_REG_TMCCT    0x390
#define APIC_REG_TMDCR    0x3e0
#define APIC_REG_SELF_IPI 0x3f0
    /* AMD */
#define APIC_REG_EXFR     0x400
#define   APIC_EXFR_GET_LVT(x)   (((x) >> 16) & 0xffu)
#define   APIC_EXFR_GET_XAIDC(x) (!!((x) & 0x4))
#define   APIC_EXFR_GET_SNIC(x)  (!!((x) & 0x2))
#define   APIC_EXFR_GET_INC(x)   (!!((x) & 0x1))
#define APIC_REG_EXFC     0x410
#define   APIC_EXFC_XAIDC_EN     (1u << 2)
#define   APIC_EXFC_SN_EN        0x2
#define   APIC_EXFC_IERN         0x1
#define   APIC_GET_EXT_ID(x)     (((x) >> 24) & 0xffu)

/* Extended LVT entries */
#define APIC_REG_EXTLVT(n) (0x500 + 0x10*(n))



#define APIC_REG_IER 0x480
#define     APIC_GET_IER(x)  (APIC_REG_IER + 0x10*(x))


/* X2APIC support 
     - This is an Intel thing
     - CPUID 0x1, ECX.21 tells you if X2APIC is supported
     - APIC_BASE_MSR.11 is APIC/XAPIC enable, .12 is X2APIC enable
       disable both first, then enable, then X2APIC
     - X2APIC regs are accessed via MSR
     - the MSR base is 0x800
     - Generally, an MMIO register at 0xA0 appears
       at 0x800 + 0xA 
     - All 32 bits of ID are now the ID (not just 8 bits)
     - ICR is now a single 64 bit register at 0x30
     - DFR is not available/supported
     - LDR is not writeable and has different semantics
         The logical id of an apic is now:
            (ID[31:4] << 16) | (1 << ID[3:0])
     - SELF IPI is now available
     - MSRREAD/WRITE is of EDX:EAX, where
       EDX is only meaninful for 64 bit registers


*/
#define X2APIC_MSR_ACCESS_BASE        0x800
#define X2APIC_MMIO_REG_OFFSET_TO_MSR(reg) (X2APIC_MSR_ACCESS_BASE+(reg>>4))

    /* for LVT entries */
#define APIC_DEL_MODE_FIXED  0x00000
#define APIC_DEL_MODE_LOWEST 0x00100
#define APIC_DEL_MODE_SMI    0x00200
#define APIC_DEL_MODE_REMRD  0x00300
#define APIC_DEL_MODE_NMI    0x00400
#define APIC_DEL_MODE_INIT   0x00500
#define APIC_DEL_MODE_SIPI   0x00600
#define APIC_DEL_MODE_EXTINT 0x00700
#define APIC_GET_DEL_MODE(x) (((x) >> 8) & 0x7)
#define APIC_LVT_VEC_MASK    0xffu
#define APIC_LVT_DISABLED 0x10000
    

#define APIC_DFR_FLAT     0xfffffffful
#define APIC_DFR_CLUSTER  0x0ffffffful
    
#ifdef CONFIG_XEON_PHI
#define APIC_LDR_MASK       (0xffu<<16)
#else
#define APIC_LDR_MASK       (0xffu<<24)
#endif

// LDR is not writeable in X2APIC mode
// It's a 32-bit hardware-set quantity that
// breaks down into a cluster and an id in the cluster
#define APIC_LDR_X2APIC_CLUSTER(x) (((x)>>16)&0xffff)
#define APIC_LDR_X2APIC_LOGID(x) ((x)&0xffff)

#ifdef CONFIG_XEON_PHI
#define GET_APIC_LOGICAL_ID(x)  (((x)>>16)&0xffffu)
#define SET_APIC_LOGICAL_ID(x)  (((x)<<16))
#define APIC_ALL_CPUS       0xffffu
#else
#define GET_APIC_LOGICAL_ID(x)  (((x)>>24)&0xffu)
#define SET_APIC_LOGICAL_ID(x)  (((x)<<24))
#define APIC_ALL_CPUS       0xffu
#endif


#define APIC_TIMER_MASK     0x10000
#define APIC_TIMER_ONESHOT  0x00000
#define APIC_TIMER_PERIODIC 0x20000
#define APIC_TIMER_TSCDLINE 0x40000


#define APIC_ERR_ILL_REG(x) ((x) & (1U<<7))
#define APIC_ERR_ILL_VECRCV(x) ((x) & (1U<<6))
#define APIC_ERR_ILL_VECSEN(x) ((x) & (1U<<5))
#define APIC_ERR_RED_IPI(x)    ((x) & (1U<<4))
#define APIC_ERR_RCV_ACC(x)    ((x) & (1U<<3))
#define APIC_ERR_SND_ACC(x)    ((x) & (1U<<2))
#define APIC_ERR_RCV_CKSUM(x)  ((x) & (1U<<1))
#define APIC_ERR_SND_CKSUM(x)  ((x) & 1)

#define IPI_VEC_XCALL 0xf3

