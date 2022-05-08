#include <arch.h>
#include <asm.h>
#include <cpu.h>
#include <idt.h>
#include <mem.h>
#include <phys.h>
#include <printf.h>
#include <syscall.h>
#include <types.h>
#include <module.h>
#include <util.h>

int processor_count = 0;

struct list_head cpu::cores;

cpu::Core::Core(void) : local_scheduler(*this) {}


void cpu::add(cpu::Core *cpu) {
  processor_count++;
  cpu->active = true;
  cpu::cores.add_tail(&cpu->cores);
}


int cpu::nproc(void) { return processor_count; }

cpu::Core *cpu::get() { return &cpu::current(); }

cpu::Core *cpu::get(int id) {
  cpu::Core *core;
  list_for_each_entry(core, &cpu::cores, cores) {
    if (id == core->id) return core;
  }

  return nullptr;
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

static spinlock global_xcall_lock;

void cpu::run_pending_xcalls(void) {
  arch_disable_ints();
  auto &p = cpu::current();
  struct xcall_command cmd = p.xcall_command;
  memset(&p.xcall_command, 0, sizeof(p.xcall_command));
  core().xcall_lock.unlock();

  if (cmd.fn == nullptr) {
    // printf("xcall null!\n");
    return;
  }
  cmd.fn(cmd.arg);
  if (cmd.count != NULL) {
    __atomic_fetch_sub(cmd.count, 1, __ATOMIC_ACQ_REL);
  }
}

void cpu::xcall(int core, xcall_t func, void *arg) {
  int count = 0;
  if (core == -1) {
    // all the cores
    cpu::each([&](cpu::Core *core) {
      count++;
      core->prep_xcall(func, arg, &count);
    });
  } else {
    auto c = cpu::get(core);
    if (c == NULL) {
      panic("invalid xcall target %d\n", core);
    }
    count++;
    c->prep_xcall(func, arg, &count);
  }
  // printf("xcall count %d\n", count);

  arch_deliver_xcall(core);

  do {
    int val = __atomic_load_n(&count, __ATOMIC_SEQ_CST);
    if (val == 0) break;
    arch_relax();
  } while (1);
}


static void run_xcall_bench(void *arg) {
  int count = 1000;

  printf("running xcall benchmark from %d\n", core_id());

  auto *measurements = new uint64_t[count];
  cpu::each([&](cpu::Core *c) {
    int target_id = c->id;
    for (int i = 0; i < count; i++) {
      auto start = arch_read_timestamp();
      cpu::xcall(
          target_id,
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
    uint64_t ns = arch_timestamp_to_ns(max);

    printf("%d -> %d avg/min/max: %lu/%lu/%lu (max %lu nanoseconds)\n", core_id(), target_id, avg, min, max, ns);
  });
  delete[] measurements;
}

ksh_def("xcall", "deliver a bunch of xcalls, printing the average cycles") {
  run_xcall_bench(NULL);
  return 0;
  cpu::each([&](cpu::Core *c) {
    int target_id = c->id;
    printf("asking %d from %d\n", target_id, core_id());
    cpu::xcall(target_id, run_xcall_bench, NULL);
  });


  return 0;
}


ksh_def("cores", "dump cpu information") {
  cpu::each([](cpu::Core *cpu) {
    printf("core #%d:", cpu->id);

    uint64_t hz = cpu->kstat.tsc_per_tick * cpu->ticks_per_second;
    printf(" %llumhz", hz / 1000 / 1000);

    unsigned long total_ticks = cpu->kstat.user_ticks + cpu->kstat.kernel_ticks + cpu->kstat.idle_ticks;

    printf(" sched:{u:%llu,k:%llu,i:%llu,total:%llu}", cpu->kstat.user_ticks, cpu->kstat.kernel_ticks, cpu->kstat.idle_ticks, total_ticks);
    printf(" ticks:%llu", cpu->ticks_per_second);
    printf(" t:%d", cpu->timekeeper);

    printf("\n");

#ifdef CONFIG_X86
    x86::Apic &apic = cpu->apic;
    printf("        ");
    printf(" bus:%lluhz", apic.bus_freq_hz);
    printf(" %llucyc/us", apic.cycles_per_us);
    printf(" %llucyc/tick", apic.cycles_per_tick);

    printf("\n");
#endif
  });

  return 0;
}
