#include <chariot.h>
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/sysbind.h>
#include <sys/syscall.h>
#include <sys/wait.h>

#define PTHREAD_STACK_SIZE 4096

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


static inline void *fs() {
  void *self;
  __asm__("mov %%fs, %0" : "=r"(self));
  return self;
}



static int __pthread_died(struct __pthread *data) {
  // we are done!
  pthread_mutex_unlock(&data->runlock);
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

int pthread_create(pthread_t *thd, const pthread_attr_t *attr, void *(*fn)(void *), void *arg) {
  // printf("fs=%p\n", fs());
  struct __pthread *data = malloc(sizeof(*data));

  data->stack_size = PTHREAD_STACK_SIZE;
  data->stack = malloc(data->stack_size);
  data->arg = arg;
  data->fn = fn;


  pthread_mutex_init(&data->runlock, NULL);
  pthread_mutex_lock(&data->runlock);

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

  pthread_mutex_lock(&t->runlock);

  if (retval != NULL) *retval = t->res;
  free(t->stack);
  free(t);
  return result;
}



/////////////////////////////////////////////////////////////////
//  Mutex
/////////////////////////////////////////////////////////////////

int pthread_mutex_lock(pthread_mutex_t *m) {
  while (__sync_lock_test_and_set((volatile int *)m, 0x01)) {
    sysbind_yield();
  }
  return 0;
}


int pthread_mutex_trylock(pthread_mutex_t *mutex) {
  if (__sync_lock_test_and_set(mutex, 0x01)) {
    return EBUSY;
  }
  return 0;
}

int pthread_mutex_unlock(pthread_mutex_t *mutex) {
  __sync_lock_release(mutex);
  return 0;
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
