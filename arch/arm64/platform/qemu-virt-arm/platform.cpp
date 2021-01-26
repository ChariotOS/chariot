#include <platform/qemu-virt-arm.h>
#include <types.h>

void arm64_platform_init(uint64_t dtb, uint64_t x1, uint64_t x2, uint64_t x3) {
}



void serial_install();
int serial_rcvd(int device) {
	return 0;
}
char serial_recv(int device) {
	return 0;
}
char serial_recv_async(int device) {
	return 0;
}
int serial_transmit_empty(int device) {
	return 0;
}
void serial_send(int device, char out) {
}
void serial_string(int device, char *out) {
}
