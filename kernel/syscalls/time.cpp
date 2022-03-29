#include <cpu.h>
#ifdef CONFIG_X86
#include <dev/RTC.h>
#endif
#include <syscall.h>
#include <time.h>

#define INT_MIN (-1 - 0x7fffffff)
#define INT_MAX 0x7fffffff

/* 2000-03-01 (mod 400 year, immediately after feb29 */
#define LEAPOCH (946684800LL + 86400 * (31 + 29))

#define DAYS_PER_400Y (365 * 400 + 97)
#define DAYS_PER_100Y (365 * 100 + 24)
#define DAYS_PER_4Y (365 * 4 + 1)

int __secs_to_tm(long long t, struct tm *tm) {
  long long days, secs, years;
  int remdays, remsecs, remyears;
  int qc_cycles, c_cycles, q_cycles;
  int months;
  int wday, yday, leap;
  static const char days_in_month[] = {31, 30, 31, 30, 31, 31, 30, 31, 30, 31, 31, 29};

  /* Reject time_t values whose year would overflow int */
  if (t < INT_MIN * 31622400LL || t > INT_MAX * 31622400LL) return -1;

  secs = t - LEAPOCH;
  days = secs / 86400;
  remsecs = secs % 86400;
  if (remsecs < 0) {
    remsecs += 86400;
    days--;
  }

  wday = (3 + days) % 7;
  if (wday < 0) wday += 7;

  qc_cycles = days / DAYS_PER_400Y;
  remdays = days % DAYS_PER_400Y;
  if (remdays < 0) {
    remdays += DAYS_PER_400Y;
    qc_cycles--;
  }

  c_cycles = remdays / DAYS_PER_100Y;
  if (c_cycles == 4) c_cycles--;
  remdays -= c_cycles * DAYS_PER_100Y;

  q_cycles = remdays / DAYS_PER_4Y;
  if (q_cycles == 25) q_cycles--;
  remdays -= q_cycles * DAYS_PER_4Y;

  remyears = remdays / 365;
  if (remyears == 4) remyears--;
  remdays -= remyears * 365;

  leap = !remyears && (q_cycles || !c_cycles);
  yday = remdays + 31 + 28 + leap;
  if (yday >= 365 + leap) yday -= 365 + leap;

  years = remyears + 4 * q_cycles + 100 * c_cycles + 400LL * qc_cycles;

  for (months = 0; days_in_month[months] <= remdays; months++)
    remdays -= days_in_month[months];

  if (months >= 10) {
    months -= 12;
    years++;
  }

  if (years + 100 > INT_MAX || years + 100 < INT_MIN) return -1;

  tm->tm_year = years + 100;
  tm->tm_mon = months + 2;
  tm->tm_mday = remdays + 1;
  tm->tm_wday = wday;
  tm->tm_yday = yday;

  tm->tm_hour = remsecs / 3600;
  tm->tm_min = remsecs / 60 % 60;
  tm->tm_sec = remsecs % 60;

  return 0;
}



time_t sys::localtime(struct tm *tloc) {
  time_t t = time::now_ms() / 1000;

  if (tloc != NULL) {
    if (!curproc->mm->validate_pointer(tloc, sizeof(*tloc), VPROT_WRITE)) {
      return -1;
    }

#ifdef CONFIG_X86
    dev::RTC::localtime(*tloc);
#else
    auto sec = time::now_ms() / 1000;
    printf("sec %d\n", sec);
    __secs_to_tm(sec, tloc);
#endif
  }

  return t;
}

size_t sys::gettime_microsecond(void) { return time::now_us(); }
