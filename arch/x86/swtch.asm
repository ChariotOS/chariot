global swtch
swtch:
	;; the return address is implicitly at the top of the stack
	;; right now, so it is accessable within the context_t struct

	;; save old registers
  push rbp
  push rbx
  push r11
  push r12
  push r13
  push r14
  push r15

	;; switch stacks
	;; store the old context in the address at the first argument
	mov [rdi], rsp
	;; load the old context
	mov rsp, rsi


	;; load new registers
	pop r15
	pop r14
	pop r13
	pop r12
	pop r11
	pop rbx
	pop rbp

	ret
