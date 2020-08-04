
; Save a list of registers to the stack
%macro SAVE 0-*
	sub rsp, (%0)*8
%assign POS 0
%rep %0
	mov [rsp+POS], %1
%assign POS POS+8
%rotate 1
%endrep
%endmacro
; Restore a list of registers
%macro RESTORE 0-*
%assign POS 0
%rep %0
	mov %1, [rsp+POS]
%assign POS POS+8
%rotate 1
%endrep
	add rsp, (%0)*8
%endmacro

extern trap

global syscall_entry
syscall_entry:

	; jmp syscall_entry


	db 0x48
	sysret
	;; db 0x48
	o64 sysret

	mov rsp, [fs:0]


	;; build a trap frame
	SAVE r15, r14, r13, r12, r11, rbp, rcx, rbx, r9, r8, r10, rdx, rsi, rdi, rax

  mov rdi, rsp ; frame in arg1
  call trap

	RESTORE r15, r14, r13, r12, r11, rbp, rcx, rbx, r9, r8, r10, rdx, rsi, rdi, rax

	db 0x48
	sysret



global ignore_sysret
ignore_sysret:
	mov rax, -1
	db 0x48
	sysret

