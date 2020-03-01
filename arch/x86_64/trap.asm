;; #include "mmu.h"

; bits 32
extern trap
global alltraps

alltraps:
	; Build trap frame.
  push r15
  push r14
  push r13
  push r12
  push r11
  push rbp
  push rcx
  push rbx

  push r9
  push r8
  push r10
  push rdx
  push rsi
  push rdi
  push rax


  mov rdi, rsp ; frame in arg1
  call trap

	;; Return falls through to trapret...
global trapret
trapret:
  pop rax
  pop rdi
  pop rsi
  pop rdx
  pop r10
  pop r8
  pop r9

  pop rbx
  pop rcx
  pop rbp
  pop r11
  pop r12
  pop r13
  pop r14
  pop r15

  ; discard trapnum and errorcode
  add rsp, 16
  iretq

