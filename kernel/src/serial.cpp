#include <asm.h>
#include <serial.h>




void serial_enable(int device) {
	outb(device + 1, 0x00);
	outb(device + 3, 0x80); /* Enable divisor mode */
	outb(device + 0, 0x03); /* Div Low:  03 Set the port to 38400 bps */
	outb(device + 1, 0x00); /* Div High: 00 */
	outb(device + 3, 0x03);
	outb(device + 2, 0xC7);
	outb(device + 4, 0x0B);
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
	outb(device, out);
}

void serial_string(int device, char * out) {
	for (uint32_t i = 0; out[i] != '\0'; i++) {
		serial_send(device, out[i]);
	}
}
