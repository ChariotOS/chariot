extern void do_call_thing(void);

extern int __syscall(int num, unsigned long a, unsigned long b, unsigned long c,
                     unsigned long d, unsigned long e);


extern int write(int fd, void *data, unsigned long len);

unsigned long strlen(const char *s) {
  int i = 0;
  for (; s[i] != 0; i++)
    ;
  return i;
}

int main(int argc, char **argv) {
  char *msg = "Hello, World\n";
    write(0, msg, strlen(msg));
  while (1) {
  }
}
