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

	// auto start = time::now_ms();

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
	unsigned long loops = 0;
	// this probably isn't great
	while (1) {
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
			asm("pause");
		}
		loops++;

		// do we timeout?
		auto now = time::now_ms();
		if (timeout_time > 0) {
			// printk("to: %-12d now: %-12d delta: %d cpu: %-3d\n", timeout_time, now, now - timeout_time, cpu::current().cpunum);
			if (now > timeout_time) {
				// printk("timed out in %dms\n", now - start);
				return -ETIMEDOUT;
			}
		}
		asm("hlt");
		sched::yield();
	}

  return -ETIMEDOUT;
}
