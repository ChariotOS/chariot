;; smp trampoline information
[bits 16]
[default abs]


section .init
global smp_trampoline_start
global smp_trampoline_end


smp_trampoline_start:
foo:
	mov eax, smp_trampoline_start
	ret
smp_trampoline_end:

