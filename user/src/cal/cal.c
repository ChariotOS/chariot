#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>

const char *month_names[12] = {
    "January", "February", "March",     "April",   "May",      "June",
    "July",    "August",   "September", "October", "November", "December",
};

const char *day_names[7] = {"Monday", "Tuesday",  "Wednesday", "Thursday",
                            "Friday", "Saturday", "Sunday"};

int main(int argc, char **argv) {
  struct tm t;
  getlocaltime(&t);

  printf("%s %s %d, %d\n", day_names[t.tm_wday], month_names[t.tm_mon - 1],
         t.tm_mday, t.tm_year);
  printf("%02d:%02d:%02d\n", t.tm_hour, t.tm_min, t.tm_sec);

  return 0;
}
