#include <desktop.h>
#include <dev/driver.h>
#include "../majors.h"
#include <module.h>
#include <sched.h>
#include <sem.h>
#include <kshell.h>
#include <errno.h>
#include <vga.h>
#include <fb.h>


static semaphore compositor_sem(0);
static struct {
	unsigned long long frames = 0;
	// default resolution
	size_t resx = 1024, resy = 768;
} globals;


static int compositor_thread(void *) {
	while (1) {
		compositor_sem.wait();
		printk("work to be done on the compositor!\n");

		globals.frames++;
	}
}


// TODO: dump to the process's stdout or whatever
static void desktop_debug_dump() {
	printk("frames: %llu\n", globals.frames);
}


static void start(void) {
	struct ck_fb_info info;
	info.active = true;
	info.width = globals.resx;
	info.height = globals.resy;

	vga::configure(info);
}

static void stop(void) {
	struct ck_fb_info info;
	info.active = false;
	vga::configure(info);
}

static unsigned long desktop_shell(vec<string> &args, void *data, int dlen) {

	if (args.size() > 0) {

		if (args[0] == "composite" || args[0] == "c") {
			compositor_sem.post();
			return 0;
		}


		if (args[0] == "debug" || args[0] == "d") {
			desktop_debug_dump();
			return 0;
		}

		if (args[0] == "n") {
			printk("create window\n");
			return 0;
		}


		if (args[0] == "start") {
			start();
			return 0;
		}


		if (args[0] == "stop") {
			stop();
			return 0;
		}



		return -EINVAL;
	}

	return 0;
}


static struct fs::file_operations desktop_ops = {
};


static struct dev::driver_info desktop_driver {
  .name = "desktop", .type = DRIVER_CHAR, .major = MAJOR_MEM,

  .char_ops = &desktop_ops,
};

static void desktop_init(void) {
	dev::register_driver(desktop_driver);


	// the main desktop file exists at minor = 0
	dev::register_name(desktop_driver, "desk", 0);


	kshell::add("desk", desktop_shell);
  sched::proc::create_kthread("[compositor]", compositor_thread);
}

module_init("desktop", desktop_init);
