bits 64

global __get_cpu_struct
__get_cpu_struct:
	mov rax, [gs:0]
	ret

global __set_cpu_struct
__set_cpu_struct:
	mov [gs:0], rdi
	ret
