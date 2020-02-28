#include <dev/driver.h>
#include <fs/ext2.h>
#include <kargs.h>
#include <module.h>
#include <pci.h>
#include <pit.h>
#include <process.h>
#include <vga.h>
#include <asm.h>
#include <phys.h>
#include <fs/vfs.h>


// HACK: not real kernel modules right now, just basic function pointers in an
// array statically.
// TODO: some initramfs or something with a TAR file and load them in as kernel
// modules or something :)
void initialize_kernel_modules(void) {
  extern struct kernel_module_info __start__kernel_modules[];
  extern struct kernel_module_info __stop__kernel_modules[];
  struct kernel_module_info *mod = __start__kernel_modules;
  int i = 0;
  while (mod != __stop__kernel_modules) {
    // KINFO("[%s] init\n", mod->name);
    mod->initfn();
    mod = &(__start__kernel_modules[++i]);
  }
}

void init_rootvfs(fs::file dev) {
  auto rootfs = make_unique<fs::ext2>(dev);
  if (!rootfs->init()) panic("failed to init fs on root disk\n");
  if (vfs::mount_root(move(rootfs)) < 0) panic("failed to mount rootfs");
}


extern struct multiboot_info *mbinfo;

struct symbol {
  off_t addr;
  char name[50];
};
/**
 * the kernel drops here in a kernel task
 *
 * Further initialization past getting the scheduler working should run here
 */
int kernel_init(void *) {

  pci::init(); /* initialize the PCI subsystem */
  KINFO("Initialized PCI\n");
  init_pit();
  KINFO("Initialized PIT\n");
  syscall_init();

  // walk the kernel modules and run their init function
  KINFO("Calling kernel module init functions\n");
  initialize_kernel_modules();
  KINFO("kernel modules initialized\n");

  // open up the disk device for the root filesystem
  const char *rootdev_path = kargs::get("root", "ata0p1");
  KINFO("rootdev=%s\n", rootdev_path);
  auto rootdev = dev::open(rootdev_path);
  assert(rootdev.ino != NULL);
  init_rootvfs(rootdev);

  // setup stdio stuff for the kernel (to be inherited by spawn)
  int fd = sys::open("/dev/console", O_RDWR);
  assert(fd == 0);

  sys::dup2(fd, 1);
  sys::dup2(fd, 2);



  string init_paths = kargs::get("init", "/bin/init");

  auto paths = init_paths.split(',');
  pid_t init_pid = sched::proc::spawn_init(paths);

  sys::waitpid(init_pid, NULL, 0);
  panic("init died!\n");

  // sched::proc::create_kthread(draw_spam);

  if (init_pid == -1) {
    KERR("failed to create init process\n");
    KERR("check the grub config and make sure that the init command line arg\n")
    KERR("is set to a comma seperated list of possible paths.\n")
  }

  // yield back to scheduler, we don't really want to run this thread anymore
  while (1) {
    arch::halt();
    sched::yield();
  }

  panic("main kernel thread reached unreachable code\n");
}
