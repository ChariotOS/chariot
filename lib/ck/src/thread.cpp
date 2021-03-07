#include <ck/thread.h>
#include <stdio.h>

ck::thread::~thread() {
  join();
}

void ck::thread::join(void) {
  if (joinable) {
    joinable = false;
    pthread_join(thd, NULL);
  }
}

ck::thread &ck::thread::operator=(ck::thread &&rhs) noexcept {
  thd = rhs.thd;
  joinable = rhs.joinable;
  rhs.joinable = false;
  return *this;
}
