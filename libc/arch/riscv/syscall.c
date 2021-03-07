

unsigned long __syscall(int number, unsigned long a0, unsigned long a1, unsigned long a2,
                        unsigned long a3, unsigned long a4, unsigned long a5

) {
  long int res = 0;

  register long int __num asm("a0") = number;
  register long int __a1 asm("a1") = (long int)(a0);
  register long int __a2 asm("a2") = (long int)(a1);
  register long int __a3 asm("a3") = (long int)(a2);
  register long int __a4 asm("a4") = (long int)(a3);
  register long int __a5 asm("a5") = (long int)(a4);
  register long int __a6 asm("a6") = (long int)(a5);
  __asm__ volatile("scall\n\t"
                   : "+r"(__num)
                   : "r"(__num), "r"(__a1), "r"(__a2), "r"(__a3), "r"(__a4), "r"(__a5)
                   : "memory");
  res = __num;
  return res;
}
