#include <task.h>
#include <mem.h>
#include <phys.h>
#include <cpu.h>


extern "C" void trapret(void);






task::task(string name, pid_t pid, gid_t gid, int ring) : m_ring(ring),m_name(name), m_pid(pid), m_gid(gid) {
  int stksize = 4096 * 4;
  kernel_stack = kmalloc(stksize);


  auto sp = (u64)kernel_stack + stksize;


  // leave room for a trap frame
  sp -= sizeof(regs_t);
  tf = (regs_t*)sp;


  // Set up new context to start executing at forkret,
  // which returns to trapret.
  sp -= sizeof(u64);
  *(u64*)sp = (u64)trapret;

  // setup initial context
  sp -= sizeof(regs_t);
  context = (context_t*)sp;

  memset(context, 0, sizeof(regs_t));

  state = state::EMBRYO;
}


task::~task(void) {
  kfree(kernel_stack);
}

regs_t &task::regs(void) {
  return *tf;
}

const string &task::name(void) {
  return m_name;
}
