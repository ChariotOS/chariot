#include <arch/cc.h>
#include <arch/sys_arch.h>
#include <lwip/sys.h>
#include <syscall.h>
#include <time.h>


// static u64_t start_time;  // in ms

#define NS_PER_MS 1000000ULL
#define FLOOR_DIV(x, y) (((x) / (y)))
#define CEIL_DIV(x, y) (((x) / (y)) + (!!((x) % (y))))

void sys_init(void) {}

err_t sys_sem_new(sys_sem_t *sem, u8_t count) {
  sem->sem = new semaphore(count);
  return ERR_OK;
}

void sys_sem_free(sys_sem_t *sem) { delete sem->sem; }

void sys_sem_signal(sys_sem_t *sem) { sem->sem->post(); }

u32_t sys_arch_sem_wait(sys_sem_t *sem, u32_t timeout) {
  // printk("sys_arch_sem_wait timeout: %u\n", timeout);

  auto start = time::now_ms();
  /*
while (time::now_ms() - start < timeout) {
if (mb->ch->avail()) {
*msg = mb->ch->recv();
break;
}
sched::yield();
}

return time::now_ms() - start;
  */
  while (sem->sem->wait().interrupted()) {
    printk("lwip sem_wait interrupted\n");
  }
  return time::now_ms() - start;
}

int sys_sem_valid(sys_sem_t *sem) { return sem->sem != NULL; }

void sys_sem_set_invalid(sys_sem_t *sem) {
  delete sem->sem;
  sem->sem = NULL;
}


u32_t sys_now() { return time::now_ms(); }


err_t sys_mutex_new(sys_mutex_t *mutex) {
  mutex->locked = 0;
  mutex->valid = 1;
  return ERR_OK;
}

void sys_mutex_lock(sys_mutex_t *mutex) { spinlock::lock(mutex->locked); }

void sys_mutex_unlock(sys_mutex_t *mutex) { spinlock::unlock(mutex->locked); }

void sys_mutex_free(sys_mutex_t *mutex) {
  //
}
int sys_mutex_valid(sys_mutex_t *mutex) { return mutex->valid == 1; }


void sys_mutex_set_invalid(sys_mutex_t *mutex) { mutex->valid = false; }




int sys_mbox_valid(sys_mbox_t *mbox) { return mbox->ch != NULL; }

err_t sys_mbox_new(sys_mbox_t *mb, int size) {
  mb->ch = new chan<void *>();
  return ERR_OK;
}

void sys_mbox_free(sys_mbox_t *mb) {
  delete mb->ch;
  mb->ch = NULL;
  return;
}

void sys_mbox_post(sys_mbox_t *mb, void *msg) {
  // printk(KERN_DEBUG "mbox post %p\n", msg);
  mb->ch->send(move(msg), false);
}


err_t sys_mbox_trypost(sys_mbox_t *mb, void *msg) {
  sys_mbox_post(mb, msg);
  return ERR_OK;
}


u32_t sys_arch_mbox_fetch(sys_mbox_t *mb, void **msg, u32_t timeout) {
  // TODO: we actually need to timeout...

  auto start = time::now_ms();
  if (msg) *msg = NULL;
  if (timeout) {
    while (time::now_ms() - start < timeout) {
      if (mb->ch->avail()) {
        *msg = mb->ch->recv();
        break;
      }
      sched::yield();
    }
  } else {
    *msg = mb->ch->recv();
  }

  if (timeout == 0) return 1;

  // printk(KERN_DEBUG "mbox wait %ums -> %p\n", timeout, *msg);
  return time::now_ms() - start;
}

u32_t sys_arch_mbox_tryfetch(sys_mbox_t *mb, void **msg) {
  if (mb->ch->avail()) {
    *msg = mb->ch->recv();
    return 0;
  }
  return SYS_MBOX_EMPTY;
}

void sys_mbox_set_invalid(sys_mbox_t *mb) {
  delete mb->ch;
  mb->ch = NULL;
}




sys_thread_t sys_thread_new(char const *name, void (*func)(void *), void *arg, int stacksize, int prio) {
  printk("[lwip] thread named '%s'\n", name);
  return sched::proc::create_kthread(name, (int (*)(void *))func, arg);
}



static spinlock big_giant_lock;

sys_prot_t sys_arch_protect(void) {
  // printk(KERN_DEBUG "Protect\n");
  big_giant_lock.lock_cli();
  return 0;
}

void sys_arch_unprotect(sys_prot_t pval) {
  // printk(KERN_DEBUG "Unprotect\n");
  big_giant_lock.unlock_cli();
}



u32_t lwip_arch_rand(void) {
  static u32_t x = 0;
  printk("LWIP RAND!\n");
  return x++;
}

