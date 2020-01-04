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
	int 0x80 ;; TODO: real syscall instruction
	ret
