extern kmain ;; c entry point
[extern high_kern_end]

section multiboot
;  global mbheader
;  mbheader:
;  	dd 0xe85250d6                                ; multiboot magic
;  	dd 0                                         ; architecture
;  	dd .hdr_end - mbheader                       ; header length
;  	dd -(0xe85250d6 + 0 + (.hdr_end - mbheader)) ; checksum
;  
;  
;  	; framebuffer
;  	dw 5
;  	dw 0
;  	dd 20 ; size
;  	dd CONFIG_FRAMEBUFFER_WIDTH ; width
;  	dd CONFIG_FRAMEBUFFER_HEIGHT ; height
;  	dd 32 ; depth
;  
;  	; tags end
;  	dd 0, 0
;  	dq 8
;  .hdr_end:



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
	dq (boot_p3 + PG_PRESENT + PG_WRITABLE)
	times 239 dq 0

boot_p3:
	dq (boot_p2 + PG_PRESENT + PG_WRITABLE)
	times 511 dq 0


boot_p2:
	%assign pg 0
  %rep 512
    dq (pg + PG_PRESENT + PG_BIG + PG_WRITABLE)
    %assign pg pg + PAGE_SIZE * 512
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




