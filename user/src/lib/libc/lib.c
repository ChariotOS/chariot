
// user provided main (linked when library is used)
extern int main(int argc, char **argv, char **envp);


typedef unsigned long long u64;
extern int __syscall(int num, u64 a, u64 b, u64 c, u64 d, u64 e);



int write(int fd, void *data, unsigned long len) {
  return __syscall(6, fd, (unsigned long long)data, len, 0, 0);
}
void libc_start() {
  main(0,0,0);
  // exit!
  while(1) {}
}
