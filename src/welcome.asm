align 4096
global chariot_welcome_start
chariot_welcome_start:

incbin "src/welcome.txt"
db 0 ;; null terminator

