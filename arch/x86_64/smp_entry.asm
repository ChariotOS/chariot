; CR4 bitmasks
CR4_PAE equ 0x20
CR4_PSE equ 0x10

KERNEL_VMA equ 0xffff880000000000
MSR_EFER equ 0xC0000080
EFER_LM equ 0x100
EFER_NX equ 0x800
EFER_SCE equ 0x001
CR0_PAGING equ 0x80000000


%define ARG(n) (0x6000 + (8 * (n)))
AP_READY equ ARG(0)
AP_STACK equ ARG(1)
AP_APICID equ ARG(2)
AP_GDTR32 equ ARG(3)
AP_GDTR64 equ ARG(4)
AP_BOOTPT equ ARG(5)

;; where we jump to at the end
extern mpentry
extern boot_p4 ;; boot page table

[bits 16]
[section .init_ap]

global smp_trampoline_start
smp_trampoline_start:
	cli
	mov eax, [AP_GDTR32]
	lgdt [eax]
	mov eax, cr0
	or al, 1
	mov cr0, eax
	jmp 08h:.trampoline_32

[bits 32]
.trampoline_32:
  ; enable PAE and PSE
  mov eax, cr4
  or eax, (CR4_PAE + CR4_PSE)
  mov cr4, eax

	; enable long mode and the NX bit
  mov ecx, MSR_EFER
  rdmsr
  or eax, (EFER_LM | EFER_NX | EFER_SCE)
  wrmsr

  ; set cr3 to a pointer to pml4
  mov eax, [AP_BOOTPT]
  mov cr3, eax

  ; enable paging
  mov eax, cr0
  or eax, CR0_PAGING
  mov cr0, eax

  ; leave compatibility mode and enter long mode
	mov eax, [AP_GDTR64]
  lgdt [eax]
  mov ax, 0x10
  mov ss, ax
  mov ax, 0x0
  mov ds, ax
  mov es, ax
  mov fs, ax
  mov gs, ax

  jmp 0x08:.trampoline_64


; some 64-bit code in the lower half used to jump to the higher half
[bits 64]
.trampoline_64:
	mov rsp, [AP_STACK]
	mov rbp, rsp

  ; push 0x0
  ; popf

	mov rdi, [AP_APICID]
	mov rax, mpentry
  call rax

