global _start
global start


extern pml4
extern pd
extern pdpt
extern boot_stack_end


extern kmain ;; c entry point

section .mbhdr
align 8


;; TODO(put multiboot header here for true mb support)

section .boot

[bits 16]

start:
_start:
	cli ; disable interrupts


	mov eax, gdtr32
	lgdt [eax] ; load GDT register with start address of Global Descriptor Table

	mov eax, cr0
	or al, 1       ; set PE (Protection Enable) bit in CR0 (Control Register 0)
	mov cr0, eax

	;; Perform a long-jump to the 32 bit protected mode
	jmp 0x8:gdt1_loaded

;; the 32bit protected mode gdt was loaded, now we need to go about setting up
;; paging for the 64bit long mode
bits 32
gdt1_loaded:
	mov eax, 0x10
	mov ds, ax
	mov ss, ax
	mov esp, boot_stack_end - 1

	call paging_longmode_setup

	;; now our long mode GDT
	mov eax, gdtr64
	lgdt [eax]

	jmp 0x8:.gdt2_loaded



bits 64
.gdt2_loaded:
	mov eax, 0x10
	mov ds, ax
	mov es, ax
	mov ss, ax
	mov fs, ax
	mov gs, ax

	mov rsp, boot_stack_end-1

	xor eax, eax
	call kmain
	hlt


bits 32
paging_longmode_setup:
	;; PML4[0] -> PDPT
	mov eax, pdpt
	or eax, 0x3
	mov ebx, pml4
	mov [ebx], eax

	;; PDPT[0] -> PDT
	mov eax, pd
	or eax, 0x3
	mov ebx, pdpt
	mov [ebx], eax

	;; ident map the first GB
	mov ecx, 512
	mov edx, pd
	mov eax, 0x83 ;; set PS bit also, (PDE -> 2MB page)

.write_pde:

	mov [edx], eax
	add eax, 0x200000
	add edx, 0x8
	loop .write_pde

	;; put pml4 address in cr3
	mov eax, pml4
	mov cr3, eax

	;; enable PAE
	mov eax, cr4
	or eax, 1 << 5
	mov cr4, eax

	;; enable lme bit in MSR
	mov ecx, 0xc0000080
	rdmsr
	or eax, 1 << 8
	wrmsr

	;; paging enable
	mov eax, cr0
	or eax, 1 << 31

	;; make sure we are in "normal cache mode"
	mov ebx, ~(3 << 29)
	and eax, ebx
	mov eax, cr0

	ret


align 8
gdt32:
	dq 0x0000000000000000 ;; null
	dq 0x00cf9a000000ffff ;; code
	dq 0x00cf92000000ffff ;; data

align 8
gdt64:
	dq 0x0000000000000000 ;; null
	dq 0x00af9a000000ffff ;; code (note lme bit)
	dq 0x00af92000000ffff ;; data (most entries don't matter)


align 8
gdtr32:
	dw 23
	dd gdt32


align 8
global gdtr64
gdtr64:
	dw 23
	dq gdt64



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
