#include <sys/sysbind.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <chariot/cpu_usage.h>
#include <string.h>


struct core {
  float usage;  // 0.0 to 1.0
  struct chariot_core_usage prev;
};

int main(void) {
  int ncores = sysbind_get_nproc();


  struct core* cores = new core[ncores];
  memset(cores, 0, sizeof(core) * ncores);

  while (1) {
    for (int core = 0; core < ncores; core++) {
      struct chariot_core_usage curr;
      auto& prev = cores[core].prev;
      sysbind_get_core_usage(core, &curr);

      struct chariot_core_usage delta;
      delta.idle_ticks = curr.idle_ticks - prev.idle_ticks;
      delta.user_ticks = curr.user_ticks - prev.user_ticks;
      delta.kernel_ticks = curr.kernel_ticks - prev.kernel_ticks;

      auto total = delta.idle_ticks + delta.user_ticks + delta.kernel_ticks;
      float usage = (float)(delta.user_ticks + delta.kernel_ticks) / (float)total;
      cores[core].usage = usage;
      cores[core].prev = curr;
    }


    float total_usage = 0;
    for (int i = 0; i < ncores; i++) {
      printf("%6.2f%%  ", cores[i].usage * 100);
      total_usage += cores[i].usage;
    }

    printf(" - %6.2f%%\n", total_usage * 100);
    // printf("\n");
    usleep(500 * 1000);
  }

  delete[] cores;


  return 0;
}