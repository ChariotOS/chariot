#include <ck/lock.h>
#include <stdlib.h>
#include <stdio.h>
#include <chariot/futex.h>
#include <sys/sysbind.h>
#include <errno.h>
#include <sys/syscall.h>
#include <stdint.h>

static inline int cmpxchg(int *ptr, int old, int newval) {
  int ret;
  asm volatile("lock\n" "cmpxchgl %2,%1\n"
              : "=a" (ret), "+m" (*(int *)ptr)
              : "r" (newval), "0" (old)
              : "memory");
   
  return ret;
}

static inline int xchg(int *ptr, int x) {
  asm volatile("lock\n" "xchgl %0,%1\n"
              :"=r" (x), "+m" (*(int*)ptr)
              :"0" (x)
              :"memory");
  return x;
}


inline static int _futex(int *uaddr, int futex_op, int val, uint32_t timeout, int *uaddr2, int val3) {
  int r = 0;
  do {
    r = errno_wrap(sysbind_futex(uaddr, futex_op, val, 0, uaddr2, val3));
  } while (errno == EINTR);
  return r;
}

#define cpu_relax() \
  __asm__ __volatile__ ( "pause\n" : : : "memory")


ck::mutex::mutex(void) {
  // pthread_mutex_init(&m_mutex, NULL);
}
ck::mutex::~mutex(void) {
  // pthread_mutex_destroy(&m_mutex);
}

void ck::mutex::lock(void) {
  int c, i;
      
  for (i=0; true; i++){
    c = cmpxchg(&m_mutex, 0, 1);
    if (!c)
      return;
    cpu_relax();
  }
  
  if (c == 1)
    c = xchg(&m_mutex, 2);

  while (c) {
    _futex(&m_mutex, FUTEX_WAIT, 2, NULL, NULL, 0);
    c = xchg(&m_mutex, 2);
  }
}


void ck::mutex::unlock(void) {
  int i;

  if ((m_mutex) == 2){                 
    (m_mutex) = 0;                    
  } else if (xchg(&m_mutex, 0) == 1){ 
    return;
  }
  
  for (i=0; true; i++){
    if ((m_mutex)){
      if (cmpxchg(&m_mutex, 1, 2)){
        return;
      }
    }
    cpu_relax();
  }
    
  _futex(&m_mutex, FUTEX_WAKE, 1, NULL, NULL, 0);
}
