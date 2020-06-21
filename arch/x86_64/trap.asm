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


;; jmp_to_userspace(ip, sp, ...)
;; rdi: user ip
;; rsi: user sp
;; rdx: arg 1
;; rcx: arg 2
;; r8 : arg 3
global jmp_to_userspace
jmp_to_userspace:
    push (5 << 3) + 3   ;; SS
    push rsi            ;; RSP
    push 0x200          ;; RFLAGS (IF)
    push (4 << 3) + 3   ;; CS
    push rdi            ;; RIP
    mov rdi, rdx        ;; user arg 1
    mov rsi, rcx        ;; user arg 2
    mov rdx, r8         ;; user arg 3
    mov rbp, rdi        ;; set user_rbp == user_rsp
    iretq

