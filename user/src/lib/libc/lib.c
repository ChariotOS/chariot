



extern int main(int argc, char **argv, char **envp);


int foo() {
  return 0;
}

int bar() {
  return 1;
}

void _start() {
  main(0,0,0);

  while(1) {}
}
