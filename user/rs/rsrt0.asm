extern rs_libstd_start

global _start
_start:
	mov rbp, rsp
	push rbp



	push rdx ; envp
	push rsi ; argv
	push rdi ; argc

	mov rdi, [rs_main]
	pop rsi ; argc
	pop rdx ; argv
	pop rcx ; envp


	jmp rs_libstd_start

	ud2


extern main
rs_main: dq main
