
// user provided main (linked when library is used)
extern int main(int argc, char **argv, char **envp);

void _start() {
  main(0,0,0);
  while(1) {}
}
