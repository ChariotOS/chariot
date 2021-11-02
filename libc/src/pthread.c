#include <chariot.h>
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/sysbind.h>
#include <string.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <chariot/futex.h>



#define ATOMIC_SET(thing) __atomic_test_and_set((thing), __ATOMIC_ACQUIRE)
#define ATOMIC_CLEAR(thing) __atomic_clear((thing), __ATOMIC_RELEASE)
#define ATOMIC_LOAD(thing) __atomic_load_n((thing), __ATOMIC_RELAXED)



/**
 * posix thread bindings to the pctl create_thread and related functions
 *
 * TODO: store threads in a global list somehow.
 * TODO: pthread_exit
 * TODO: pthread_join
 * TODO: stack (size) management
 */

/**
 * pthreads are required to return void *, which is given back to the user on
 * pthread_join(). So this funciton is a trampoline function that allows
 * maintaining state, destructing, etc..
 */
struct __pthread {
  // this struct contains the args used to create, so the thread can get it's
  // tid and whatnot
  void *(*fn)(void *);
  void *arg;

  int tid;  // the thread id of the thread

  long stack_size;
  void *stack;
  /* filled in by __pthread_trampoline */
  void *res;

  /* held while running. Joining means taking the lock until the runner gives it up. */
  pthread_mutex_t runlock;

  /* ... */
};


static int __pthread_died(struct __pthread *data) {
  // we are done!
  // pthread_mutex_unlock(&data->runlock);
  while (1) {
    sysbind_exit_thread(0);
  }

  return 0;
}

static int __pthread_trampoline(void *arg) {
  struct __pthread *data = arg;


  // wait till the thread can actually run (sync with the creator)
  // pthread_mutex_lock(&data->runlock);

  // run the thread's function
  data->res = data->fn(data->arg);


  __pthread_died(data);
  return 0;
}

pthread_t pthread_self(void) { return 0; }

// #define MMAP_PTHREAD_STACK
// #define PTHREAD_STACK_SIZE (1 * 1024L * 1024L)
#define PTHREAD_STACK_SIZE (4 * 4096L)


// a buffer of pthread stacks that are of size PTHREAD_STACK_SIZE
static void *pthread_stacks[64] = {0};
static pthread_mutex_t pthread_stacks_lock = PTHREAD_MUTEX_INITIALIZER;



static void *get_stack(void) {
#ifdef MMAP_PTHREAD_STACK
  return mmap(NULL, PTHREAD_STACK_SIZE, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0);
#else
  return malloc(PTHREAD_STACK_SIZE);
#endif
}

static void release_stack(void *stk) {
#ifdef MMAP_PTHREAD_STACK
  munmap(stk, PTHREAD_STACK_SIZE);
#else
  free(stk);
#endif
}



int pthread_create(pthread_t *thd, const pthread_attr_t *attr, void *(*fn)(void *), void *arg) {
  struct __pthread *data = malloc(sizeof(*data));

  data->stack_size = PTHREAD_STACK_SIZE;

  data->stack = get_stack();
  memset(data->stack, 0, PTHREAD_STACK_SIZE);

  data->arg = arg;
  data->fn = fn;



  // pthread_mutex_init(&data->runlock, NULL);
  // pthread_mutex_lock(&data->runlock);

  // int res = pctl(0, PCTL_CREATE_THREAD, &data->pctl_args);

  int tid = sysbind_spawnthread(data->stack + data->stack_size, __pthread_trampoline, data, 0);

  if (tid < 0) {
    free(data->stack);
    free(data);
    return errno_wrap(tid);
  }

  data->tid = tid;
  *thd = data;

  // at this point, we know the thread is created, so we start it :)
  // TODO: use a semaphore, lock, or something smarter...
  // pthread_mutex_unlock(&data->runlock);

  return 0;
}

int pthread_join(pthread_t t, void **retval) {
  int status;

  int result = sysbind_jointhread(t->tid);
  if (result != 0) {
    errno = -result;
    return -1;
  }

  // pthread_mutex_lock(&t->runlock);

  if (retval != NULL) *retval = t->res;

  release_stack(t->stack);
  free(t);
  return result;
}



/////////////////////////////////////////////////////////////////
//  Mutex
/////////////////////////////////////////////////////////////////


inline static int _futex(int *uaddr, int futex_op, int val, uint32_t timeout, int *uaddr2, int val3) {
  int r = 0;
  do {
    r = errno_wrap(sysbind_futex(uaddr, futex_op, val, 0, uaddr2, val3));
  } while (errno == EINTR);
  return r;
}


#ifdef CONFIG_X86
static inline int cmpxchg(int *ptr, int old, int newval) {
  // __atomic_compare_exchange_n(ptr, &old, newval, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);

  int ret;
  asm volatile(
      "lock\n"
      "cmpxchgl %2,%1\n"
      : "=a"(ret), "+m"(*(int *)ptr)
      : "r"(newval), "0"(old)
      : "memory");

  return ret;
}

static inline int xchg(int *ptr, int x) {
  asm volatile(
      "lock\n"
      "xchgl %0,%1\n"
      : "=r"(x), "+m"(*(int *)ptr)
      : "0"(x)
      : "memory");
  return x;
}
#endif




int pthread_mutex_lock(pthread_mutex_t *m) {
#ifdef CONFIG_X86
  int c, i;

  for (i = 0; 1; i++) {
    c = cmpxchg(m, 0, 1);
    if (!c) return 0;
    // cpu_relax();
  }

  if (c == 1) c = xchg(m, 2);

  while (c) {
    _futex(m, FUTEX_WAIT, 2, 0, NULL, 0);
    c = xchg(m, 2);
  }

#else
  // this is a hack to get other archs working w/o their own inline asm
  while (ATOMIC_SET(m)) {  // MESI protocol optimization
    while (__atomic_load_n(m, __ATOMIC_RELAXED) == 1) {
    }
  }
#endif
  return 0;
}


int pthread_mutex_trylock(pthread_mutex_t *mutex) {
  if (__sync_lock_test_and_set(mutex, 0x01)) {
    return EBUSY;
  }
  return 0;
}

int pthread_mutex_unlock(pthread_mutex_t *m) {
#ifdef CONFIG_X86
  int i;

  if ((*m) == 2) {
    (*m) = 0;
  } else if (xchg(m, 0) == 1) {
    return 0;
  }

  for (i = 0; 1; i++) {
    if ((*m)) {
      if (cmpxchg(m, 1, 2)) {
        return 0;
      }
    }
  }

  _futex(m, FUTEX_WAKE, 1, 0, NULL, 0);
#else
  // this is a hack to get other archs working w/o their own inline asm
  ATOMIC_CLEAR(m);
#endif
}


int pthread_mutex_init(pthread_mutex_t *mutex, const pthread_mutexattr_t *attr) {
  *mutex = 0;
  return 0;
}

int pthread_mutex_destroy(pthread_mutex_t *mutex) { return 0; }

/////////////////////////////////////////////////////////////////




static void *threadDataTable[64];
static int freeEntry = 0;
int pthread_key_create(pthread_key_t *key, void (*a)(void *)) {
  assert(freeEntry < 64);

  *key = freeEntry;
  freeEntry++;
  return 0;
}

int pthread_once(pthread_once_t *control, void (*init)(void)) {
  if (*control == 0) {
    (*init)();
    *control = 1;
  }
  return 0;
}

void *pthread_getspecific(pthread_key_t key) { return threadDataTable[key]; }

int pthread_setspecific(pthread_key_t key, const void *data) {
  threadDataTable[key] = (void *)data;
  return 0;
}




int pthread_cond_init(pthread_cond_t *cond, const pthread_condattr_t *attr) { return -1; }
int pthread_cond_destroy(pthread_cond_t *cond) { return 0; }


static int cond_wait(pthread_cond_t *cond, pthread_mutex_t *mutex, const struct timespec *abstime) { return -1; }

int pthread_cond_wait(pthread_cond_t *cond, pthread_mutex_t *mutex) { return 0; }


int pthread_cond_timedwait(pthread_cond_t *c, pthread_mutex_t *m, const struct timespec *ts) { return -1; }
int pthread_cond_broadcast(pthread_cond_t *c) { return -1; }
int pthread_cond_signal(pthread_cond_t *c) { return -1; }
