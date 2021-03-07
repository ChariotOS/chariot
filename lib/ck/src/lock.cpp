#include <ck/lock.h>
#include <stdlib.h>
#include <stdio.h>

ck::mutex::mutex(void) {
  pthread_mutex_init(&m_mutex, NULL);
}
ck::mutex::~mutex(void) {
  pthread_mutex_destroy(&m_mutex);
}

void ck::mutex::lock(void) {
  pthread_mutex_lock(&m_mutex);
}
void ck::mutex::unlock(void) {
  pthread_mutex_unlock(&m_mutex);
}
