#include <emx.h>

#include <cpu.h>
#include <dev/char_dev.h>
#include <dev/driver.h>
#include <errno.h>
#include <module.h>
#include <sched.h>
#include "../drivers/majors.h"


/**
 * blocker to allow the emx system to block a thread for a timeout
 */
struct emx_blocker final : public thread_blocker {
  long long timeout = 0;

 public:
  emx_blocker(long long timeout) : timeout(timeout) {}
  virtual ~emx_blocker(void) {}
  virtual bool should_unblock(struct thread &t, unsigned long now_us) override {
    if (timeout != -1) {
      return (long long)now_us >= timeout;
    }
    return false;
  }
};


struct emx_state {
	int foo;
	emx_state() {}
};

static int emx_instance_ioctl(fs::file &fd, unsigned int cmd, off_t arg) {

	fs::inode *ino = fd.ino;
	printk("%p %p ioctl\n", ino, ino->priv<struct emx_state>());
	// auto *state = ino->priv<struct emx_state>();


  if (cmd == EMX_WAIT) {
    auto *args = (struct emx_wait_args *)arg;
    if (!VALIDATE_RDWR(arg, sizeof(*args))) {
      return -EINVAL;
    }

    // run the wait routine.
		auto blk = emx_blocker(args->timeout);
		return 0;
  }

  // printk("instance!\n");
  return -ENOTIMPL;
}


static int emx_instance_open(fs::file &fd) {
	fs::inode *ino = fd.ino;
	printk("%p %p open\n", ino, ino->priv<struct emx_state>());
	// ino->priv<struct emx_state>() = new emx_state();
	return 0;
}

static void emx_instance_destroy(fs::inode &ino) {
	auto *state = ino.priv<struct emx_state>();

	printk("%p %p destroy\n", &ino, ino.priv<struct emx_state>());
	if (state != NULL) {
		delete state;
	}
}

static struct fs::file_operations emx_instance_ops = {
    .ioctl = emx_instance_ioctl,
		.open = emx_instance_open,
		.destroy = emx_instance_destroy,
};

/**
 * This defines the methods on the event multiplexer pseudo-device
 */

static int emx_ioctl(fs::file &, unsigned int cmd, off_t arg) {
  static int next_inode_nr = 0;
  if (cmd == EMX_NEW) {
    auto ino = new fs::inode(T_CHAR, fs::DUMMY_SB);
    ino->ino = next_inode_nr++;
    ino->uid = 0;
    ino->gid = 0;
    ino->size = 0;
    ino->mode = 07755;
    ino->link_count = 3;
    ino->fops = &emx_instance_ops;
		printk("%d\n", sizeof(struct emx_state));

		auto *s = (struct emx_state*)kmalloc(sizeof(struct emx_state));
		new (s) emx_state();
		s->foo = 0;
		ino->priv<struct emx_state>() = s;

    auto file = fs::file::create(ino, string::format("/emx/$%d", ino->ino));
    int fd = curproc->add_fd(file);

    return fd;
  }

  return -EINVAL;
}

static struct fs::file_operations emx_ops = {
    .ioctl = emx_ioctl,
};

static struct dev::driver_info emx_driver_info {
  .name = "emx", .type = DRIVER_CHAR, .major = MAJOR_EMX,

  .char_ops = &emx_ops,
};

static void dev_init(void) {
  dev::register_driver(emx_driver_info);

  dev::register_name(emx_driver_info, "emx", 0);
}

module_init("char", dev_init);
