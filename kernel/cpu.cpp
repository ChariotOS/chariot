#include <arch.h>
#include <asm.h>
#include <cpu.h>
#include <idt.h>
#include <mem.h>
#include <phys.h>
#include <printk.h>
#include <syscall.h>
#include <types.h>
#include <module.h>

// struct processor_state cpus[CONFIG_MAX_CPUS];
int processor_count = 0;

struct list_head cpu::cores;


void cpu::add(struct processor_state *cpu) {
  processor_count++;
  cpu->active = true;
  printk("======= Found cpu %d\n", cpu->id);
  cpu::cores.add(&cpu->cores);
}


int cpu::nproc(void) { return processor_count; }

struct processor_state *cpu::get() {
  return &cpu::current();
}

struct processor_state *cpu::get(int core) {
  struct processor_state *proc = NULL;

  return proc;
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


  auto *c = cpu::get(core);
  if (c == NULL) return -EEXIST;

  if (!VALIDATE_WR(usage, sizeof(struct chariot_core_usage))) {
    return -EINVAL;
  }


  usage->user_ticks = c->kstat.user_ticks;
  usage->kernel_ticks = c->kstat.kernel_ticks;
  usage->idle_ticks = c->kstat.idle_ticks;
  usage->ticks_per_second = c->ticks_per_second;

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
      __atomic_fetch_sub(call.count, 1, __ATOMIC_ACQ_REL);
    }
  }
}


void cpu::xcall(int core, xcall_t func, void *arg, bool wait) {
  int count = 0;
  if (core == -1) {
    // all the cores
    cpu::each([&](auto *core) {
      count++;
      core->prep_xcall(func, arg, wait ? &count : NULL);
    });
  } else {
    auto c = cpu::get(core);
    if (c == NULL) {
      KERR("invalid xcall target %d\n", core);
      // hmm
      return;
    }
    count++;
    c->prep_xcall(func, arg, wait ? &count : NULL);
  }

  arch_deliver_xcall(core);

  if (wait) {
    int iters = 0;
    do {
      int val = __atomic_load_n(&count, __ATOMIC_SEQ_CST);
      if (val == 0) break;
      arch_relax();
      iters++;
    } while (1);
  }
}


ksh_def("xcall", "deliver a bunch of xcalls, printing the average cycles") {
  int count = 10000;
  auto *measurements = new uint64_t[count];

  printk("from %d\n", cpu::current().id);

  for (int i = 0; i < count; i++) {
    auto start = arch_read_timestamp();
    cpu::xcall(
        1,  // send to all cores.
        [](void *arg) {
        },
        NULL);

    auto end = arch_read_timestamp();
    measurements[i] = end - start;
  }

  uint64_t sum = 0;
  uint64_t min = ~0;
  uint64_t max = 0;

  for (int i = 0; i < count; i++) {
    auto m = measurements[i];
    sum += m;
    if (m < min) min = m;
    if (m > max) max = m;
  }

  uint64_t avg = sum / count;

  printk("avg: %lu\n", avg);
  printk("min: %lu\n", min);
  printk("max: %lu\n", max);

  return 0;
}


ksh_def("cores", "dump cpu information") {
  cpu::each([](struct processor_state *cpu) {
    printk("[core%d]:", cpu->id);


    uint64_t hz = cpu->kstat.tsc_per_tick * cpu->ticks_per_second;
    printk(" %llumhz", hz / 1000 / 1000);


    printk(" sched:{u:%llu,k:%llu,i:%llu}", cpu->kstat.user_ticks, cpu->kstat.kernel_ticks, cpu->kstat.idle_ticks);
    printk(" ticks:%llu", cpu->ticks_per_second);


    printk(" t:%d", cpu->timekeeper);

    printk("\n");
  });

  return 0;
}
