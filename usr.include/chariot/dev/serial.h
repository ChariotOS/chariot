#ifndef __SERIAL__
#define __SERIAL__

/* Serial */
#define COM1 0x3f8
#define SERIAL_PORT_A 0x3F8
#define SERIAL_PORT_B 0x2F8
#define SERIAL_PORT_C 0x3E8
#define SERIAL_PORT_D 0x2E8

#define SERIAL_IRQ 4

void serial_install();
int serial_rcvd(int device);
char serial_recv(int device);
char serial_recv_async(int device);
int serial_transmit_empty(int device);
void serial_send(int device, char out);
void serial_string(int device, char* out);

#endif
