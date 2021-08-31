global __syscall:function
__syscall:

	;; rotate arguments
	mov rax, rdi
	mov rdi, rsi
	mov rsi, rdx
	mov rdx, rcx
	mov r10, r8
	mov r8, r9
	mov r9, [rsp + 8]
	;; syscall
	int 0x80 ;; TODO: real syscall instruction
	ret




global __get_gs
__get_gs:
	mov gs:0, rax
	ret