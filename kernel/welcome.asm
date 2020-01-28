global chariot_welcome_start
chariot_welcome_start:

incbin "kernel/welcome.txt"
db 0 ;; null terminator

