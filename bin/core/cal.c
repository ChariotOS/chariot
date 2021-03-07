#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>

#define DAY_HEADINGS_S "Su Mo Tu We Th Fr Sa"
#define DAY_HEADINGS_M "Mo Tu We Th Fr Sa Su"
#define DAY_HEADINGS_JS " Su  Mo  Tu  We  Th  Fr  Sa"
#define DAY_HEADINGS_JM " Mo  Tu  We  Th  Fr  Sa  Su"

int use_color = 1;

void set_color(int c) {
  if (use_color) printf("\x1b[%dm", c);
}

const char *month_names[12] = {
    "January", "February", "March",     "April",   "May",      "June",
    "July",    "August",   "September", "October", "November", "December",
};

const char *day_names[7] = {"Monday", "Tuesday",  "Wednesday", "Thursday",
                            "Friday", "Saturday", "Sunday"};

#define THURSDAY 4 /* for reformation */
#define SATURDAY 6 /* 1 Jan 1 was a Saturday */

#define FIRST_MISSING_DAY 639799 /* 3 Sep 1752 */
#define NUMBER_MISSING_DAYS 11   /* 11 day correction */

#define MAXDAYS 42 /* max slots in a month array */
#define SPACE -1   /* used in day array */

static const int days_in_month[2][13] = {
    {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31},  // non leap-year
    {0, 31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31},  // leap-year
};

/* leap year -- account for gregorian reformation in 1752 */
#define leap_year(yr) ((yr) <= 1752 ? !((yr) % 4) : (!((yr) % 4) && ((yr) % 100)) || !((yr) % 400))

/* number of centuries since 1700, not inclusive */
#define centuries_since_1700(yr) ((yr) > 1700 ? (yr) / 100 - 17 : 0)

/* number of centuries since 1700 whose modulo of 400 is 0 */
#define quad_centuries_since_1700(yr) ((yr) > 1600 ? ((yr)-1600) / 400 : 0)

/* number of leap years between year 1 and this year, not inclusive */
#define leap_years_since_year_1(yr) \
  ((yr) / 4 - centuries_since_1700(yr) + quad_centuries_since_1700(yr))

#define C_RESET 0

#define C_REVERSE 7
#define C_UNINV 27
#define C_GRAY 90

void center(const char *, int, int);
int day_in_week(int, int, int);
int day_in_year(int, int, int);

void print_month(int month, int year, struct tm *now) {
  char header[20];
  snprintf(header, 20, "%s %d", month_names[month - 1], year);
  center(header, 20, 0);
  printf("\n");

  if (now->tm_mon == month && now->tm_year == year) {
    int hour = now->tm_hour;
    int min = now->tm_min;
    int sec = now->tm_sec;

    char ap = hour < 12 ? 'A' : 'P';
    if (ap == 'P' && hour != 12) hour -= 12;

    snprintf(header, 12, "%d:%02d:%02d %cM", hour, min, sec, ap);
    center(header, 20, 0);
    printf("\n");
  }

  printf("%s\n", DAY_HEADINGS_S);

  int start = day_in_week(1, month, year);

  int days_in_mon = days_in_month[leap_year(year)][month];

  int d = 0;
  for (; d < start; d++) {
    printf("   ");
  }
  for (int i = 1; i <= days_in_mon; i++) {
    if (d > 6) {
      printf("\n");
      d = 0;
    }

    if (i == now->tm_mday && month == now->tm_mon) {
      set_color(C_REVERSE);
      printf("%2d", i);
      set_color(C_UNINV);
      printf("%c", use_color ? ' ' : '*');
    } else if (d == 0 || d == 6) {
      set_color(C_GRAY);
      printf("%2d ", i);
      set_color(C_RESET);
    } else {
      printf("%2d ", i);
    }
    d++;
  }
  printf("\n");
}

int main(int argc, char **argv) {
  struct tm local_time;
  getlocaltime(&local_time);

  print_month(local_time.tm_mon, local_time.tm_year, &local_time);
  return 0;
}

void center(const char *str, int len, int separate) {
  len -= strlen(str);
  (void)printf("%*s%s%*s", len / 2, "", str, len / 2 + len % 2 + separate, "");
}

int day_in_week(int day, int month, int year) {
  long temp;

  temp = (long)(year - 1) * 365 + leap_years_since_year_1(year - 1) + day_in_year(day, month, year);
  if (temp < FIRST_MISSING_DAY) return ((temp - 1 + SATURDAY) % 7);
  if (temp >= (FIRST_MISSING_DAY + NUMBER_MISSING_DAYS))
    return (((temp - 1 + SATURDAY) - NUMBER_MISSING_DAYS) % 7);
  return (THURSDAY);
}

int day_in_year(int day, int month, int year) {
  int i, leap;

  leap = leap_year(year);
  for (i = 1; i < month; i++)
    day += days_in_month[leap][i];
  return (day);
}
