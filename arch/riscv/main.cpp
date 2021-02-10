#include <asm.h>
#include <cpu.h>
#include <dev/virtio_mmio.h>
#include <devicetree.h>
#include <fs/vfs.h>
#include <module.h>
#include <phys.h>
#include <printk.h>
#include <riscv/arch.h>
#include <riscv/dis.h>
#include <riscv/paging.h>
#include <riscv/plic.h>
#include <riscv/uart.h>
#include <sleep.h>
#include <syscall.h>
#include <time.h>
#include <util.h>

#include <rbtree.h>
#include <rbtree_augmented.h>

#define PAGING_IMPL_BOOTCODE
#include "paging_impl.h"

typedef void (*func_ptr)(void);
extern "C" func_ptr __init_array_start[0], __init_array_end[0];
extern "C" char _kernel_end[];

/* lowlevel.S, calls kerneltrap() */
extern "C" void kernelvec(void);

/*
 * Data nodes in an rbtree tree are structures containing a struct rb_node member::
 */
struct mytype {
  struct rb_node node;
  const char *keystring;
  mytype(const char *k) : keystring(k) {
  }
};

// When dealing with a pointer to the embedded struct rb_node, the containing data
// structure may be accessed with the standard container_of() macro.  In addition,
// individual members may be accessed directly via rb_entry(node, type, member).
//
// At the root of each rbtree is an rb_root structure, which is initialized to be
// empty via:
struct rb_root mytree = RB_ROOT;

// Searching for a value in an rbtree
// ----------------------------------
//
// Writing a search function for your tree is fairly straightforward: start at the
// root, compare each value, and follow the left or right branch as necessary.

struct mytype *my_search(struct rb_root *root, char *string) {
  // grab a pointer to the root node
  struct rb_node *node = root->rb_node;

  while (node) {
    // get the data at the node
    struct mytype *data = rb_entry(node, struct mytype, node);
    int result;

    result = strcmp(string, data->keystring);

    /* Compare and go left, right, or return the data */
    if (result < 0)
      node = node->rb_left;
    else if (result > 0)
      node = node->rb_right;
    else
      return data;
  }
  return NULL;
}

// Inserting data into an rbtree
// -----------------------------
//
// Inserting data in the tree involves first searching for the place to insert the
// new node, then inserting the node and rebalancing ("recoloring") the tree.
//
// The search for insertion differs from the previous search by finding the
// location of the pointer on which to graft the new node.  The new node also
// needs a link to its parent node for rebalancing purposes.
//
// Example::

int my_insert(struct rb_root *root, struct mytype *data) {
  printk("insert %p\n", data);
  struct rb_node **n = &(root->rb_node), *parent = NULL;

  /* Figure out where to put new node */
  while (*n) {
    struct mytype *self = rb_entry(*n, struct mytype, node);
    int result = strcmp(data->keystring, self->keystring);

    parent = *n;
    if (result < 0)
      n = &((*n)->rb_left);
    else if (result > 0)
      n = &((*n)->rb_right);
    else
      return false;
  }

  /* Add new node and rebalance tree. */
  rb_link_node(&data->node, parent, n);
  rb_insert_color(&data->node, root);

  return true;
}

template <typename Fn>
struct rb_node *rb_search(struct rb_root *root, Fn compare) {
  struct rb_node **n = &(root->rb_node), *parent = NULL;

  while (*n) {
    auto *cur = *n;
    int result = compare(cur);

    parent = *n;
    if (result < 0)
      n = &((*n)->rb_left);
    else if (result > 0)
      n = &((*n)->rb_right);
    else
      return *n;
  }
  return NULL;
}

// To remove an existing node from a tree, call::
//
//   void rb_erase(struct rb_node *victim, struct rb_root *tree);
//
// Example::
//
//   struct mytype *data = mysearch(&mytree, "walrus");
//
//   if (data) {
//   	rb_erase(&data->node, &mytree);
//   	myfree(data);
//   }
//
// To replace an existing node in a tree with a new one with the same key, call::
//
//   void rb_replace_node(struct rb_node *old, struct rb_node *new,
//   			struct rb_root *tree);
//
// Replacing a node this way does not re-sort the tree: If the new node doesn't
// have the same key as the old node, the rbtree will probably become corrupted.
//
// Iterating through the elements stored in an rbtree (in sort order)

void print_tree_linear(struct rb_root *root) {
  for (struct rb_node *node = rb_first(&mytree); node; node = rb_next(node))
    printk("key=%s\n", rb_entry(node, struct mytype, node)->keystring);
}


void print_tree_node(struct rb_node *node, int depth) {
  if (node == NULL) {
    return;
  }

  for (int i = 0; i < depth; i++)
    printk("->");

  struct mytype *self = rb_entry(node, struct mytype, node);
  printk(" '%s' %c\n", self->keystring, node->__rb_parent_color & RB_BLACK ? 'B' : 'R');

  print_tree_node(node->rb_left, depth + 1);
  print_tree_node(node->rb_right, depth + 1);
}

void print_tree_nice(struct rb_root *root) {
  print_tree_node(root->rb_node, 0);
}


void test_rbtree(void) {
  my_insert(&mytree, new mytype("hello"));
  my_insert(&mytree, new mytype("world"));
  my_insert(&mytree, new mytype("how"));
  my_insert(&mytree, new mytype("are"));
  my_insert(&mytree, new mytype("you"));
  my_insert(&mytree, new mytype("aaaaa"));


  auto *node = rb_search(&mytree, [&](auto *node) -> int {
    struct mytype *mt = rb_entry(node, struct mytype, node);
		printk("checking %p\n", mt);
    return strcmp("hello", mt->keystring);
  });
	printk("node with hello: %p\n", node);
  print_tree_linear(&mytree);
  print_tree_nice(&mytree);
}



static unsigned long riscv_high_acc_time_func(void) {
  return (read_csr(time) * NS_PER_SEC) / CONFIG_RISCV_CLOCKS_PER_SECOND;
}

static off_t dtb_ram_start = 0;
static size_t dtb_ram_size = 0;

static int wakes = 0;
void main() {

  /*
   * Machine mode passes us the scratch structure through
   * the thread pointer register. We need to then move it
   * to our sscratch register after copying it
   */
  struct rv::hart_state *sc = (rv::hart_state *)p2v(rv::get_tp());


  /* The scratch register is a physical address. This is so the timervec doesn't have to
   * do any address translation or whatnot. We just pay the cost everywhere else! :^) */
  rv::set_tp((rv::xsize_t)p2v(sc));

  rv::get_hstate().kernel_sp = 0;

  int hartid = rv::hartid();
  /* TODO: release these somehow :) */
  if (rv::hartid() != 0)
    while (1)
      arch_halt();

  /* Initialize the platform level interrupt controller for this HART */
  rv::plic::hart_init();

  rv::uart_init();

  /* Set the supervisor trap vector location */
  write_csr(stvec, kernelvec);


  off_t boot_free_start = (off_t)v2p(_kernel_end);
  off_t boot_free_end = boot_free_start + 1 * MB;
  printk(KERN_DEBUG "Freeing bootup ram %llx:%llx\n", boot_free_start, boot_free_end);



  /* Tell the device tree to copy the device tree and parse it */
  dtb::parse((dtb::fdt_header *)p2v(rv::get_hstate().dtb));

  use_kernel_vm = 1;
  phys::free_range((void *)boot_free_start, (void *)boot_free_end);

  cpu::seginit(NULL);

  dtb::walk_devices([](dtb::node *node) -> bool {
    if (!strcmp(node->name, "memory")) {
      /* We found the ram (there might be more, but idk for now) */
      dtb_ram_size = node->reg.length;
      dtb_ram_start = node->reg.address;
      return false;
    }
    return true;
  });

  if (dtb_ram_start == 0) {
    printk(KERN_ERROR "dtb didn't contain a memory segment, guessing 128mb :)\n");
    dtb_ram_size = 128 * MB;
    dtb_ram_start = boot_free_start;
  }

  off_t dtb_ram_end = dtb_ram_start + dtb_ram_size;
  dtb_ram_start = max(dtb_ram_start, boot_free_end + 4096);
  if (dtb_ram_end - dtb_ram_start > 0) {
    printk(KERN_DEBUG "Freeing discovered ram %llx:%llx\n", dtb_ram_start, dtb_ram_end);
    phys::free_range((void *)dtb_ram_start, (void *)dtb_ram_end);
  }



  /* Now that we have a memory allocator, call global constructors */
  for (func_ptr *func = __init_array_start; func != __init_array_end; func++)
    (*func)();

  arch_enable_ints();

  time::set_cps(CONFIG_RISCV_CLOCKS_PER_SECOND);
  time::set_high_accuracy_time_fn(riscv_high_acc_time_func);
  /* set SUM bit in sstatus so kernel can access userspace pages. Also enable floating point */
  write_csr(sstatus, read_csr(sstatus) | (1 << 18) | (1 << 13));

  cpus[0].timekeeper = true;




  assert(sched::init());
  KINFO("Initialized the scheduler with %llu pages of ram (%llu bytes)\n", phys::nfree(),
        phys::bytes_free());



  sched::proc::create_kthread("main task", [](void *) -> int {
    KINFO("Calling kernel module init functions\n");
    initialize_builtin_modules();
    KINFO("kernel modules initialized\n");


    dtb::walk_devices([](dtb::node *node) -> bool {
      if (false && !strcmp(node->compatible, "cfi-flash")) {
        void *flash = p2v(node->address);
        uint32_t *ifl = (uint32_t *)flash;
        ifl[0]++;
        printk("%08x\n", ifl[0]);
        __sync_synchronize();

        printk("flash: %p\n", flash);
        hexdump(flash, 256, true);
      }
      if (!strcmp(node->compatible, "virtio,mmio")) {
        virtio::check_mmio(node->address);
      }
      return true;
    });


    // test_rbtree();


    // printk("waiting!\n");
    // do_usleep(1000 * 1000);

    int mnt_res = vfs::mount("/dev/disk0p1", "/", "ext2", 0, NULL);
    if (mnt_res != 0) {
      panic("failed to mount root. Error=%d\n", -mnt_res);
    }

    /* Mount /dev and /tmp. TODO: move this to userspace in /init? */
    vfs::mount("none", "/dev", "devfs", 0, NULL);
    vfs::mount("none", "/tmp", "tmpfs", 0, NULL);


    auto kproc = sched::proc::kproc();
    kproc->root = fs::inode::acquire(vfs::get_root());
    kproc->cwd = fs::inode::acquire(vfs::get_root());

    string init_paths = "/bin/init,/init";
    auto paths = init_paths.split(',');
    pid_t init_pid = sched::proc::spawn_init(paths);

    sys::waitpid(init_pid, NULL, 0);
    panic("INIT DIED!\n");

    return 0;
  });


  KINFO("starting scheduler\n");
  sched::run();

  while (1)
    arch_halt();
}

