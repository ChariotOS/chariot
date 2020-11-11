extern kmain ;; c entry point
[extern high_kern_end]

; Declare constants for the multiboot header.
MBALIGN  equ  1 << 0            ; align loaded modules on page boundaries
MEMINFO  equ  1 << 1            ; provide memory map
MBVIDEO equ  1 << 2
FLAGS    equ  MBALIGN | MEMINFO | MBVIDEO ; this is the Multiboot 'flag' field
MAGIC    equ  0x1BADB002        ; 'magic number' lets bootloader find the header
CHECKSUM equ -(MAGIC + FLAGS)   ; checksum of above, to prove we are multiboot


section .multiboot
align 8
global mbheader
mbheader:
	dd MAGIC
	dd FLAGS
	dd CHECKSUM

;; other required headers
	dd 0
	dd 0
	dd 0
	dd 0
	dd 0
;; video header
	dd 0
	dd CONFIG_FRAMEBUFFER_WIDTH
	dd CONFIG_FRAMEBUFFER_HEIGHT
	dd 32



; higher-half virtual memory address
KERNEL_VMA equ 0xffff880000000000

; MSR numbers
MSR_EFER equ 0xC0000080

; EFER bitmasks
EFER_LM equ 0x100
EFER_NX equ 0x800
EFER_SCE equ 0x001

; CR0 bitmasks
CR0_PAGING equ 0x80000000

; CR4 bitmasks
CR4_PAE equ 0x20
CR4_PSE equ 0x10

; page flag bitmasks
PG_PRESENT  equ 0x1
PG_WRITABLE equ 0x2
PG_USER     equ 0x4
PG_BIG      equ 0x80
PG_NO_EXEC  equ 0x8000000000000000

; page and table size constants
LOG_TABLE_SIZE equ 9
LOG_PAGE_SIZE  equ 12
PAGE_SIZE  equ (1 << LOG_PAGE_SIZE)
TABLE_SIZE equ (1 << LOG_TABLE_SIZE)

; bootstrap stack size and alignment
STACK_SIZE  equ 0x1000
STACK_ALIGN equ 16





; paging structures
align PAGE_SIZE
[global boot_p4]
boot_p4:
	dq (boot_p3 + PG_PRESENT + PG_WRITABLE)
	times 271 dq 0
	dq (high_p3 + PG_PRESENT + PG_WRITABLE)
	times 239 dq 0

boot_p3:
	dq (boot_p2 + PG_PRESENT + PG_WRITABLE)
	times 511 dq 0

boot_p2:
	dq (boot_p1 + PG_PRESENT + PG_WRITABLE)
	times 511 dq 0

;; ID map the first bit 512 pages of memory
boot_p1:
	%assign pg 0
  %rep 512
    dq (pg + PG_PRESENT + PG_WRITABLE)
    %assign pg pg+PAGE_SIZE
  %endrep


high_p3:
	dq (high_p2 + PG_PRESENT + PG_WRITABLE)
	times 511 dq 0

high_p2:
	%assign pg 0
  %rep 512
    dq (pg + PG_PRESENT + PG_BIG + PG_WRITABLE)
    %assign pg pg+PAGE_SIZE*512
  %endrep

BOOT_STACK_SIZE equ 256
boot_stack:
	times BOOT_STACK_SIZE db 0

; the global descriptor table
gdt:
    dq 0
    dq 0x00AF98000000FFFF
    dq 0x00CF92000000FFFF
gdt_end:
  dq 0 ; some extra padding so the gdtr is 16-byte aligned
gdtr:
  dw gdt_end - gdt - 1
  dq gdt


global gdtr
global gdtr32

gdt32:
		dq 0x0000000000000000
		dq 0x00cf9a000000ffff
		dq 0x00cf92000000ffff
gdtr32:
	dw 23
	dd gdt32





[bits 32] ALIGN 8
section .init
;; Kernel entrypoint, 32bit code
global _start
_start:

	;; load up the boot stack
	mov esp, boot_stack + BOOT_STACK_SIZE
	mov ebp, esp

	;; move the info that grub passes into the kernel into
	;; arguments that we will use when calling kmain later
	mov edi, ebx
	mov esi, eax

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
  mov eax, boot_p4
  mov cr3, eax

  ; enable paging
  mov eax, cr0
  or eax, CR0_PAGING
  mov cr0, eax

  ; leave compatibility mode and enter long mode
  lgdt [gdtr]
  mov ax, 0x10
  mov ss, ax
  mov ax, 0x0
  mov ds, ax
  mov es, ax
  mov fs, ax
  mov gs, ax

  jmp 0x08:.trampoline

; some 64-bit code in the lower half used to jump to the higher half
[bits 64]
.trampoline:
  ; enter the higher half now that we loaded up that half (somewhat)
  mov rax, qword .next
  jmp rax

; the higher-half code
[bits 64]
[section .init_high]
.next:
	; re-load the GDTR with a virtual base address
	mov rax, [gdtr + 2]
	mov rbx, KERNEL_VMA
	add rax, rbx
	mov [gdtr + 2], rax
	mov rax, gdtr + KERNEL_VMA
	lgdt [rax]

  mov rbp, 0
  mov rsp, qword stack + STACK_SIZE
  push 0x0
  popf

  call kmain

; memory reserved for the kernel's stck
[section .bss align=STACK_ALIGN]
stack:
  resb STACK_SIZE




















