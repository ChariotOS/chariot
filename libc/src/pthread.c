#include <chariot.h>
#include <chariot/pctl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

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
struct pthread_data {
  // this struct contains the args used to create, so the thread can get it's
  // tid and whatnot
  struct pctl_create_thread_args pctl_args;
  void *(*fn)(void *);
  void *arg;

  long stack_size;
  void *stack;
  /* filled in by __pthread_trampoline */
  void *res;

  /* used to allow the thread to run in the trampoline */
  int runnable;

  /* ... */
};

static int __pthread_died(struct pthread_data *data) {
  // here, we are still living within the thread, so we can't delete the data
  // yet...
  // TODO: clean up better.
  free(data);

  printf("thread died!\n");
  while (1) {
    // TODO: exit_thread() to avoid segfault
    yield();
  }

  return 0;
}

static int __pthread_trampoline(void *arg) {
  struct pthread_data *data = arg;


  // wait till the thread can actually run (sync with the creator)
  while (data->runnable == 0) {
    __asm__ ("pause");
    yield();
  }
  // printf("fn = %p\n", data->fn);


  // run the thread's function
  data->res = data->fn(data->arg);


  __pthread_died(data);
  return 0;
}

int pthread_create(pthread_t *thd, const pthread_attr_t *attr,
                   void *(*fn)(void *), void *arg) {
  struct pthread_data *data = malloc(sizeof(*data));

  data->stack_size = PTHREAD_STACK_SIZE;
  data->stack = malloc(data->stack_size);
  data->arg = arg;
  data->fn = fn;
  data->runnable = 0;

  // fill in the pctl args
  data->pctl_args.arg = data;
  data->pctl_args.fn = __pthread_trampoline;
  data->pctl_args.stack = data->stack;
  data->pctl_args.stack_size = data->stack_size;
  data->pctl_args.flags = 0; /*TODO*/

  int res = pctl(0, PCTL_CREATE_THREAD, &data->pctl_args);


  if (res != 0) {
    free(data->stack);
    free(data);
    printf("oops!\n");
    return -1;
  }

  // fill in the thread identifier in the pthread_t* passed
  thd->tid = data->pctl_args.tid;

  // at this point, we know the thread is created, so we start it :)
  // TODO: use a semaphore, lock, or something smarter...
  data->runnable = 1;

  return 0;
}
