[org 0x1000]
[bits 64]


main:
  push rbp
  xor ebp, ebp
  push rbx
  sub rsp, 8


  mov edx, 0x000000
.L2:
  xor ebx, ebx
.L3:
  mov esi, ebx
  mov edi, ebp
  add ebx, 1

	inc edx
  call set_pixel
  cmp ebx, 480
  jne .L3
  add ebp, 1
  cmp ebp, 640
  jne .L2
  xor ebp, ebp
  jmp .L2


set_pixel:

	enter 0, 0

	mov rdx, [color]

	mov eax, 8
	int 0x80

	inc QWORD [color]

	leave
	ret


color dq 0x000000;

; msg   db    'Hello, Friend!',0xa
; len   equ   $-msg

