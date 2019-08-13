bits 64


global pic_send_eoi
pic_send_eoi:
	mov al, 0x20
	out 0x20, al ; Send the EOI to the PIC
	ret

global pic_enable
pic_enable:

	;; restart both of the PICs
	mov al, 0x11
	out 0x20, al     ;restart PIC1
	out 0xA0, al     ;restart PIC2


	;; have both PICs send their interrupts to different locations
	mov al, 0x20
	out 0x21, al     ;PIC1 now starts at 32
	mov al, 0x28
	out 0xA1, al     ;PIC2 now starts at 40
	
	mov al, 0x04
	out 0x21, al     ;setup cascading
	mov al, 0x02
	out 0xA1, al

	mov al, 0x01
	out 0x21, al
	out 0xA1, al     ;done!

	mov al, 0x36
	out 0x43, al    ;tell the PIT which channel we're setting

	mov ax, 11931
	out 0x40, al    ;send low byte
	mov al, ah
	out 0x40, al    ;send high byte
	ret




;; new stack in rdi, func in rsi
global call_with_new_stack
call_with_new_stack:
	mov rsp, rdi
	call rsi
