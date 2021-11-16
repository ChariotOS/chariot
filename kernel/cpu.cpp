#include <arch.h>
#include <asm.h>
#include <cpu.h>
#include <idt.h>
#include <mem.h>
#include <phys.h>
#include <printk.h>
#include <syscall.h>
#include <types.h>



struct processor_state cpus[CONFIG_MAX_CPUS];
int cpunum = 0;

int cpu::nproc(void) { return cpunum; }

struct processor_state *cpu::get() {
  return &cpu::current();
}

Process *cpu::proc(void) {
  if (curthd == NULL) return NULL;
  return &curthd->proc;
}

bool cpu::in_thread(void) { return (bool)thread(); }

Thread *cpu::thread() { return current().current_thread; }

extern "C" u64 get_sp(void);



int sys::get_core_usage(unsigned int core, struct chariot_core_usage *usage) {
  if (core > CONFIG_MAX_CPUS) return -1;
  if (core > cpu::nproc()) return -1;

  auto &c = cpus[core];

  if (!VALIDATE_WR(usage, sizeof(struct chariot_core_usage))) {
    return -EINVAL;
  }


  usage->user_ticks = c.kstat.user_ticks;
  usage->kernel_ticks = c.kstat.kernel_ticks;
  usage->idle_ticks = c.kstat.idle_ticks;
  usage->ticks_per_second = c.ticks_per_second;

  return 0;
}

int sys::get_nproc(void) { return cpu::nproc(); }

extern "C" int get_errno(void) { return curthd->kerrno; }


void cpu::run_pending_xcalls(void) {
  auto &p = cpu::current();
  auto f = p.xcall_lock.lock_irqsave();
  auto todo = p.xcall_commands;
  p.xcall_commands.clear();
  p.xcall_lock.unlock_irqrestore(f);
  for (auto call : todo) {
    call.fn(call.arg);

		if (call.count != NULL) {
			printk("yep\n");
			__atomic_fetch_sub(call.count, 1, __ATOMIC_ACQ_REL);
		}
  }
}


void cpu::xcall(int core, xcall_t func, void *arg, bool wait) {
	printk("make xcall, waiting\n");
	int count = 1;
  if (core == -1) {
		count = cpunum;
    // all the cores
    for (int i = 0; i < cpunum; i++) {
      cpus[i].prep_xcall(func, arg, wait ? &count : NULL);
    }
  } else {
    if (core < 0 || core > cpunum) {
      KERR("invalid xcall target %d\n", core);
      // hmm
      return;
    }
    cpus[core].prep_xcall(func, arg, wait ? &count : NULL);
	}

  arch_deliver_xcall(core);

	if (wait) {
		int iters = 0;
		do {
			int val = __atomic_load_n(&count, __ATOMIC_SEQ_CST);
			if (val == 0) break;
			// arch_relax();
			iters++;
		} while(1);

		printk("took %d iters\n", iters);
	}
}
