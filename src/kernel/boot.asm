global start

[bits 16]

start:


	in eax, 0xfe
	hlt

	cli ; disable interrupts

	;; clear out the data segments
	xor ax, ax
	mov ds, ax
	mov es, ax
	mov ss, ax
	mov cs, ax

	lgdt [gdtr]    ; load GDT register with start address of Global Descriptor Table
	mov eax, cr0
	or al, 1       ; set PE (Protection Enable) bit in CR0 (Control Register 0)
	mov cr0, eax

	;; Perform a long-jump to the 32 bit protected mode
	jmp (CODE_DESC - NULL_DESC):start32

bits 32
;;  St
start32:
	;; setup the basic stack
	mov esp, 0x8000
	mov ebp, esp

	mov eax, 0x100
	hlt


NULL_DESC:
    dd 0            ; null descriptor
    dd 0

CODE_DESC:
    dw 0xFFFF       ; limit low
    dw 0            ; base low
    db 0            ; base middle
    db 10011010b    ; access
    db 11001111b    ; granularity
    db 0            ; base high

DATA_DESC:
    dw 0xFFFF       ; limit low
    dw 0            ; base low
    db 0            ; base middle
    db 10010010b    ; access
    db 11001111b    ; granularity
    db 0            ; base high

gdtr:
    LIMIT dw gdtr - NULL_DESC - 1 ; length of GDT
    BASE  dd NULL_DESC   ; base of GDT


print_regs:
	out 0x3f, eax
	ret
set_start:
	rdtsc
	out 0xfd, eax
	ret
print_time:
	rdtsc
	out 0xfe, eax
	ret
