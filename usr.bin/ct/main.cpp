#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <stdlib.h>
#include <unistd.h>
#include <lumen.h>
#include <gfx/rect.h>
#include <gui/application.h>
#include <gui/window.h>

#include <ck/eventloop.h>

int main(int argc, char **argv) {

	// connect to the window server
	gui::application app;

	char buf[50];
	sprintf(buf, "window on pid %d", getpid());

	auto window = app.new_window(buf, 300, 200);

	// auto input = ck::file::unowned(0);
	// input->on_read([] { ck::eventloop::exit(); });

	// start the application!
	app.start();

	return 0;
}
