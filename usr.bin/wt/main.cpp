#include <ck/io.h>

#include <chariot/mshare.h>
#include <sys/syscall.h>

void *mshare_create(const char *name, size_t size) {
  struct mshare_create arg;
  arg.size = size;
  strncpy(arg.name, name, MSHARE_NAMESZ - 1);
  return (void *)syscall(SYS_mshare, MSHARE_CREATE, &arg);
}


void *mshare_acquire(const char *name, size_t *size) {
  struct mshare_acquire arg;
  arg.size = 0;
  strncpy(arg.name, name, MSHARE_NAMESZ - 1);

  if (size != NULL) *size = arg.size;
  return (void *)syscall(SYS_mshare, MSHARE_ACQUIRE, &arg);
}

int main(int argc, char **argv) {
  volatile auto region1 = (int *)mshare_create("my_region", 4096);

  for (int i = 0; i < 10; i++) {
    auto other = (volatile int *)mshare_acquire("my_region", NULL);
		*other = i;
    printf("other: %p\n", other);
  }
  printf("val: %d\n", *region1);

  return 0;
}
