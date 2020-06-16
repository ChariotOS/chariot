#include <ck/io.h>

#include <chariot/mshare.h>
#include <sys/syscall.h>
#include <sys/mman.h>

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

#define N 255

int main(int argc, char **argv) {
  volatile auto region1 = (int *)mshare_create("my_region", 4096);

  for (int i = 0; i < N; i++) {
    auto other = (volatile uint32_t *)mshare_acquire("my_region", NULL);
		uint32_t c = i;
		other[i] = (c << 24) | (c << 16) | (c << 8) | (c);
		munmap((void*)other, 4096);
  }

	ck::hexdump(region1, sizeof(int) * N);

  return 0;
}
