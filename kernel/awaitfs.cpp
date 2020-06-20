#include <awaitfs.h>
#include <cpu.h>
#include <errno.h>
#include <map.h>
#include <syscall.h>
#include <wait.h>
#include <time.h>


using table_key_t = off_t;


struct await_table_entry {
	ref<fs::file> file;
  // what are we waiting on?
  short awaiting;
  short event;

  int index = 0;
};


int sys::awaitfs(struct await_target *targs, int nfds, int flags, unsigned long timeout_time) {

	if (nfds == 0) return -EINVAL;
  if (!curproc->mm->validate_pointer(targs, sizeof(*targs) * nfds,
                                     PROT_READ | PROT_WRITE))
    return -1;

	vec<await_table_entry> entries;

  // build up the entry list
  for (int i = 0; i < nfds; i++) {
    auto &targ = targs[i];
    auto file = curproc->get_fd(targ.fd);

    if (file) {
			struct await_table_entry entry;
			entry.awaiting = targ.awaiting;
			entry.event = 0;
			entry.index = i;
			entry.file = file;
			entries.push(entry);
    }
  }

  int index = -1;
	auto start = time::now_ms();
	unsigned long loops = 0;
	// this probably isn't great
	while (1) {
		if (timeout_time != 0) {
			if (timeout_time <= time::now_ms()) {
				return -ETIMEDOUT;
			}
		}
		for (auto &ent : entries) {
			if (ent.file) {
				int occurred = ent.file->ino->poll(*ent.file, ent.awaiting);
				if (occurred & ent.awaiting) {
					index = ent.index;
					targs[index].occurred = occurred;
					// printk("awaitfs took %d ms, %d loops\n", time::now_ms() - start, loops);
					return index;
				}
			}
		}
		loops++;

		asm("hlt");
		sched::yield();
	}

  return -ETIMEDOUT;
}
