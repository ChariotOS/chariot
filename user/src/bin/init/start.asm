

extern _main
global _start
_start:
	call _main
	;; TODO: exit!
.loop:
	jmp .loop
