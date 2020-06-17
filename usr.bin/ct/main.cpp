#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <stdlib.h>
#include <unistd.h>
#include <lumen.h>
#include <ck/io.h>
#include <ck/socket.h>

#include <gui/application.h>
#include <gui/window.h>

#include <ck/eventloop.h>

int main(int argc, char **argv) {

	// connect to the window server
	gui::application app;

	auto window = app.new_window("Hello, World", 300, 200);

	// start the application!
	app.start();

	return 0;
}
