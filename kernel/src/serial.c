#include <asm.h>
#include <serial.h>




void serial_enable(int device) {
	outb(0x00, device + 1);
	outb(0x80, device + 3); /* Enable divisor mode */
	outb(0x03, device + 0); /* Div Low:  03 Set the port to 38400 bps */
	outb(0x00, device + 1); /* Div High: 00 */
	outb(0x03, device + 3);
	outb(0xC7, device + 2);
	outb(0x0B, device + 4);
}

void
serial_install() {
	serial_enable(SERIAL_PORT_A);
	// serial_enable(SERIAL_PORT_B);
}

int serial_rcvd(int device) {
	return inb(device + 5) & 1;
}

char serial_recv(int device) {
	while (serial_rcvd(device) == 0) ;
	return inb(device);
}

char serial_recv_async(int device) {
	return inb(device);
}

int serial_transmit_empty(int device) {
	return inb(device + 5) & 0x20;
}

void serial_send(int device, char out) {
	while (serial_transmit_empty(device) == 0);
	outb(out, device);
}

void serial_string(int device, char * out) {
	for (uint32_t i = 0; out[i] != '\0'; i++) {
		serial_send(device, out[i]);
	}
}
