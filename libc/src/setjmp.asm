

global setjmp
global longjmp

__setjmp:
_setjmp:
setjmp:
	mov [rdi], rbx ; rdi is jmp_buf, move registers onto it
	mov [rdi + 8], rbp
	mov [rdi + 16], r12
	mov [rdi + 24], r13
	mov [rdi + 32], r14
	mov [rdi + 40], r15
	lea rdx, [rsp + 8] ; this is our rsp WITHOUT current ret addr
	mov [rdi + 48], rdx
	mov rdx, [rsp]    ; save return addr ptr for new rip
	mov [rdi + 56], rdx
	xor rax, rax      ; always return 0
	ret



_longjmp:
longjmp:
	mov rax, rsi    ; val will be longjmp return
	test rax, rax
	jnz .continue
	inc rax         ; if val==0, val=1 per longjmp semantics
.continue:
	mov rbx, [rdi]  ; rdi is the jmp_buf, restore regs from it
	mov rbp, [rdi + 8]
	mov r12, [rdi + 16]
	mov r13, [rdi + 24]
	mov r14, [rdi + 32]
	mov r15, [rdi + 40]
	mov rdx, [rdi + 48] ; this ends up being the stack pointer
	mov rsp, rdx
	mov rdx, [rdi + 56] ; this is the instruction pointer
	jmp rdx
