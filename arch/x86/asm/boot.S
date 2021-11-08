
.code32
.section multiboot
.align 8

.global mbheader
.global gdt
.global gdt32

/**
 * This header makes us multi-boot compliant
 */
mbheader:
    .long 0xe85250d6
    .long 0
    .long .hdr_end - mbheader
    .long -(0xe85250d6 + 0 + (.hdr_end - mbheader))

    /* BEGIN TAGS */

    /* framebufer */
		framebuffer_tag_start:  
			.word 5
			.word 0
			.long 20 # framebuffer_tag_end - framebuffer_tag_start
			.long CONFIG_FRAMEBUFFER_WIDTH
			.long CONFIG_FRAMEBUFFER_HEIGHT
			.long 32
		framebuffer_tag_end:


    /* tags end */
    .long 0, 0
    .long 8
.hdr_end:



.code32
.section .init.data

boot_stack:
	.space 512
.boot_stack_end:



.code32
.section .init
.global _start
_start:


	/* get GRUB multiboot info and stash it */
	movl %ebx, %edi
  movl %eax, %esi
	
	/* Setup the boot stack */
	movl $.boot_stack_end, %esp
	movl %esp, %ebp

	/* Enable PAE and PSE */
	movl %cr4, %eax
	orl $(1 << 5), %eax /* PAE */
	orl $(1 << 4), %eax /* PSE */
	movl %eax, %cr4

	/* Enable long mode and the NX bit */
	movl $0xC0000080, %ecx
	rdmsr
	orl  $(0x100 | 0x800 | 0x001), %eax
	wrmsr


	/* Set cr3 to p4 of the boot page table */
	movl $boot_p4, %eax
	movl %eax, %cr3
	

	/* Enable paging */
	mov %cr0, %eax
	orl $0x80000000, %eax
	mov %eax, %cr0


	/* Load the gdt */
	movl $gdtr, %eax
	lgdt (%eax)

	movl $0x10, %eax
	movw %ax, %ds
	movw %ax, %ss
	
  ljmp $0x8, $.trampoline

.code64
.trampoline:
	movq $.next_ptr, %rax
	movq (%rax), %rax
	jmp *%rax

.next_ptr:
	.quad .next

.code64
.align 8
.section .init_high

.next:
	/* now that we are in long mode, re-load the gdtr
	 * with a virtual base address
	 */
	movq (gdtr + 2), %rax
	movabs $0xffff880000000000, %rbx
	addq %rbx, %rax
	movq %rax, (gdtr + 2)
	movq $gdtr, %rax
	addq %rbx, %rax
	lgdt (%rax)
	

	movq $0, %rbp
	movabs $.stack_end, %rsp

	pushq $0
	popf

	call kmain

	# jmp .

.section .bss
stack:
	.space 4096
.stack_end:
