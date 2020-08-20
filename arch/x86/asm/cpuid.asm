global cpuid_busfreq

bits 64
cpuid_busfreq:

	push rbp
	mov rbp, rsp

	push rbx

	mov rax, 0x16
	mov rcx, 0
	mov rbx, 0

	cpuid

	and eax, 0xFF
	and ebx, 0xFF
	and ecx, 0xFF

	mov [rdi],  eax
	mov [rdi + 4], ebx
	mov [rdi + 8], ecx

	pop rbx


	pop rbp
	ret
