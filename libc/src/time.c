#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/sysbind.h>
#include <sys/syscall.h>
#include <sys/time.h>
#include <time.h>

#define MS_PER_SEC (1000)
#define US_PER_SEC (MS_PER_SEC * 1000)
#define NS_PER_SEC (US_PER_SEC * 1000)

time_t time(time_t *tloc) {
  time_t val = sysbind_localtime(0);
  if (tloc) *tloc = val;
  return val;
}

time_t getlocaltime(struct tm *tloc) {
  return sysbind_localtime(tloc);
}


int clock_gettime(int id, struct timespec *s) {
  long us = sysbind_gettime_microsecond();
  s->tv_nsec = us * 1000;
  s->tv_sec = us / 1000 / 1000;
  return 0;
}


clock_t clock(void) {
  return sysbind_gettime_microsecond() / 1000;
}


// TODO: this is wrong, don't ignore t
struct tm *localtime(const time_t *t) {
  static struct tm tm;

  time_t time = getlocaltime(&tm);
  (void)time;

  // if (t != NULL) *t = time;
  return &tm;
}


int gettimeofday(struct timeval *tv, void *idklol) {
  uint64_t usec = sysbind_gettime_microsecond();
  tv->tv_sec = usec / US_PER_SEC;
  tv->tv_usec = usec % US_PER_SEC;
  return 0;
}



int is_leap_year(int year) {
  return year % 4 == 0 && (year % 100 != 0 || year % 400 == 0);
}

unsigned days_in_year(int year) {
  return 365 + is_leap_year(year);
}

static int day_of_year(int year, unsigned month, int day) {
  assert(month >= 1 && month <= 12);

  static const int seek_table[] = {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334};
  int day_of_year = seek_table[month - 1] + day - 1;

  if (is_leap_year(year) && month >= 3) day_of_year++;

  return day_of_year;
}

static int days_in_month(int year, unsigned month) {
  assert(month >= 1 && month <= 12);
  if (month == 2) return is_leap_year(year) ? 29 : 28;

  bool is_long_month = (month == 1 || month == 3 || month == 5 || month == 7 || month == 8 ||
                        month == 10 || month == 12);
  return is_long_month ? 31 : 30;
}

static unsigned day_of_week(int year, unsigned month, int day) {
  assert(month >= 1 && month <= 12);
  static const int seek_table[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
  if (month < 3) --year;

  return (year + year / 4 - year / 100 + year / 400 + seek_table[month - 1] + day) % 7;
}


static inline int years_to_days_since_epoch(int year) {
  int days = 0;
  for (int current_year = 1970; current_year < year; ++current_year)
    days += days_in_year(current_year);
  for (int current_year = year; current_year < 1970; ++current_year)
    days -= days_in_year(current_year);
  return days;
}




static const int __seconds_per_day = 60 * 60 * 24;

static void time_to_tm(struct tm *tm, time_t t) {
  int year = 1970;
  for (; t >= days_in_year(year) * __seconds_per_day; ++year)
    t -= days_in_year(year) * __seconds_per_day;
  for (; t < 0; --year)
    t += days_in_year(year - 1) * __seconds_per_day;
  tm->tm_year = year - 1900;

  assert(t >= 0);
  int days = t / __seconds_per_day;
  tm->tm_yday = days;
  int remaining = t % __seconds_per_day;
  tm->tm_sec = remaining % 60;
  remaining /= 60;
  tm->tm_min = remaining % 60;
  tm->tm_hour = remaining / 60;

  int month;
  for (month = 1; month < 12 && days >= days_in_month(year, month); ++month)
    days -= days_in_month(year, month);

  tm->tm_mday = days + 1;
  tm->tm_wday = day_of_week(year, month, tm->tm_mday);
  tm->tm_mon = month - 1;
}

static time_t tm_to_time(struct tm *tm, long timezone_adjust_seconds) {
  // "The original values of the tm_wday and tm_yday components of the structure are ignored,
  // and the original values of the other components are not restricted to the ranges described in
  // <time.h>.
  // [...]
  // Upon successful completion, the values of the tm_wday and tm_yday components of the structure
  // shall be set appropriately, and the other components are set to represent the specified time
  // since the Epoch, but with their values forced to the ranges indicated in the <time.h> entry;
  // the final value of tm_mday shall not be set until tm_mon and tm_year are determined."

  // FIXME: Handle tm_isdst eventually.

  tm->tm_year += tm->tm_mon / 12;
  tm->tm_mon %= 12;
  if (tm->tm_mon < 0) {
    tm->tm_year--;
    tm->tm_mon += 12;
  }

  tm->tm_yday = day_of_year(1900 + tm->tm_year, tm->tm_mon + 1, tm->tm_mday);
  time_t days_since_epoch = years_to_days_since_epoch(1900 + tm->tm_year) + tm->tm_yday;
  long timestamp = ((days_since_epoch * 24 + tm->tm_hour) * 60 + tm->tm_min) * 60 + tm->tm_sec +
                   timezone_adjust_seconds;
  time_to_tm(tm, timestamp);
  return timestamp;
}



size_t strftime(char *destination, size_t max_size, const char *format, const struct tm *tm) {
  const char wday_short_names[7][4] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
  const char wday_long_names[7][10] = {"Sunday",   "Monday", "Tuesday", "Wednesday",
                                       "Thursday", "Friday", "Saturday"};
  const char mon_short_names[12][4] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                       "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
  const char mon_long_names[12][10] = {"January",   "February", "March",    "April",
                                       "May",       "June",     "July",     "August",
                                       "September", "October",  "November", "December"};

  char *builder = destination;
  char temp[48];

#define FMT(args...) builder += snprintf(builder, 48, ##args)
#define FSTR(thing) FMT("%s", (thing))

  const int format_len = strlen(format);
  for (int i = 0; i < format_len; ++i) {
    if (format[i] != '%') {
      FMT("%c", format[i]);
    } else {
      if (++i >= format_len) return 0;

      switch (format[i]) {
        case 'a':
          FSTR(wday_short_names[tm->tm_wday]);
          break;
        case 'A':
          FSTR(wday_long_names[tm->tm_wday]);
          break;
        case 'b':
          FSTR(mon_short_names[tm->tm_mon]);
          break;
        case 'B':
          FSTR(mon_long_names[tm->tm_mon]);
          break;
        case 'C':
          FMT("%02d", (tm->tm_year + 1900) / 100);
          break;
        case 'd':
          FMT("%02d", tm->tm_mday);
          break;
        case 'D':
          FMT("%02d/%02d/%02d", tm->tm_mon + 1, tm->tm_mday, (tm->tm_year + 1900) % 100);
          break;
        case 'e':
          FMT("%2d", tm->tm_mday);
          break;
        case 'h':
          FMT(mon_short_names[tm->tm_mon]);
          break;
        case 'H':
          FMT("%02d", tm->tm_hour);
          break;
        case 'I':
          FMT("%02d", tm->tm_hour % 12);
          break;
        case 'j':
          FMT("%03d", tm->tm_yday + 1);
          break;
        case 'm':
          FMT("%02d", tm->tm_mon + 1);
          break;
        case 'M':
          FMT("%02d", tm->tm_min);
          break;
        case 'n':
          FMT("\n");
          break;
        case 'p':
          FSTR(tm->tm_hour < 12 ? "a.m." : "p.m.");
          break;
        case 'r':
          FMT("%02d:%02d:%02d %s", tm->tm_hour % 12, tm->tm_min, tm->tm_sec,
              tm->tm_hour < 12 ? "a.m." : "p.m.");
          break;
        case 'R':
          FMT("%02d:%02d", tm->tm_hour, tm->tm_min);
          break;
        case 'S':
          FMT("%02d", tm->tm_sec);
          break;
        case 't':
          FMT("\t");
          break;
        case 'T':
          FMT("%02d:%02d:%02d", tm->tm_hour, tm->tm_min, tm->tm_sec);
          break;
        case 'u':
          FMT("%d", tm->tm_wday ? tm->tm_wday : 7);
          break;
        case 'U': {
          const int wday_of_year_beginning = (tm->tm_wday + 6 * tm->tm_yday) % 7;
          const int week_number = (tm->tm_yday + wday_of_year_beginning) / 7;
          FMT("%02d", week_number);
          break;
        }
        case 'V': {
          const int wday_of_year_beginning = (tm->tm_wday + 6 + 6 * tm->tm_yday) % 7;
          int week_number = (tm->tm_yday + wday_of_year_beginning) / 7 + 1;
          if (wday_of_year_beginning > 3) {
            if (tm->tm_yday >= 7 - wday_of_year_beginning)
              --week_number;
            else {
              const int days_of_last_year = days_in_year(tm->tm_year + 1900 - 1);
              const int wday_of_last_year_beginning =
                  (wday_of_year_beginning + 6 * days_of_last_year) % 7;
              week_number = (days_of_last_year + wday_of_last_year_beginning) / 7 + 1;
              if (wday_of_last_year_beginning > 3) --week_number;
            }
          }
          FMT("%02d", week_number);
          break;
        }
        case 'w':
          FMT("%d", tm->tm_wday);
          break;
        case 'W': {
          const int wday_of_year_beginning = (tm->tm_wday + 6 + 6 * tm->tm_yday) % 7;
          const int week_number = (tm->tm_yday + wday_of_year_beginning) / 7;
          FMT("%02d", week_number);
          break;
        }
        case 'y':
          FMT("%02d", (tm->tm_year + 1900) % 100);
          break;
        case 'Y':
          FMT("%d", tm->tm_year + 1900);
          break;
        case '%':
          FMT("%");
          break;
        default:
          return 0;
      }
    }
  }
  return 0;
}
