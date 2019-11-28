extern syscall_handle

global syscall_enter
syscall_enter:

	; Build trap frame.
  push r15
  push r14
  push r13
  push r12
  push r11
  push r10
  push r9
  push r8
  push rdi
  push rsi
  push rbp
  push rdx
  push rcx
  push rbx
  push rax

  mov rdi, rsp ; frame in arg1
  call syscall_handle

  pop rax
  pop rbx
  pop rcx
  pop rdx
  pop rbp
  pop rsi
  pop rdi
  pop r8
  pop r9
  pop r10
  pop r11
  pop r12
  pop r13
  pop r14
  pop r15

	sysret
