#include <dev/CMOS.h>
#include <dev/RTC.h>
#include <module.h>
#include <util.h>
#include <string.h>

static time_t s_boot_time;

inline bool is_leap_year(unsigned year) {
  return ((year % 4 == 0) && ((year % 100 != 0) || (year % 400) == 0));
}

static unsigned days_in_months_since_start_of_year(unsigned month,
						   unsigned year) {
  assert(month <= 11);
  unsigned days = 0;
  switch (month) {
    case 11:
      days += 30;
      [[fallthrough]];
    case 10:
      days += 31;
      [[fallthrough]];
    case 9:
      days += 30;
      [[fallthrough]];
    case 8:
      days += 31;
      [[fallthrough]];
    case 7:
      days += 31;
      [[fallthrough]];
    case 6:
      days += 30;
      [[fallthrough]];
    case 5:
      days += 31;
      [[fallthrough]];
    case 4:
      days += 30;
      [[fallthrough]];
    case 3:
      days += 31;
      [[fallthrough]];
    case 2:
      if (is_leap_year(year))
	days += 29;
      else
	days += 28;
      [[fallthrough]];
    case 1:
      days += 31;
  }
  return days;
}

static unsigned days_in_years_since_epoch(unsigned year) {
  unsigned days = 0;
  while (year > 1969) {
    days += 365;
    if (is_leap_year(year)) ++days;
    --year;
  }
  return days;
}

static bool update_in_progress() { return dev::CMOS::read(0x0a) & 0x80; }

void dev::RTC::read_registers(int& year, int& month, int& day,
			      int& hour, int& minute,
			      int& second) {
  while (update_in_progress())
    ;

  year = (CMOS::read(0x32) * 100) + CMOS::read(0x09);
  month = CMOS::read(0x08);
  day = CMOS::read(0x07);
  hour = CMOS::read(0x04);
  minute = CMOS::read(0x02);
  second = CMOS::read(0x00);
}

time_t dev::RTC::now() {
  while (update_in_progress())
    ;

  int year, month, day, hour, minute, second;
  read_registers(year, month, day, hour, minute, second);

  // printk("%d:%d:%d\n", hour, minute, second);

  // printk("year: %d, month: %d, day: %d\n", year, month, day);

  assert(year >= 2019);

  return days_in_years_since_epoch(year - 1) * 86400 +
	 days_in_months_since_start_of_year(month - 1, year) * 86400 +
	 (day - 1) * 86400 + hour * 3600 + minute * 60 + second;
}


time_t dev::RTC::boot_time() {
  return s_boot_time;
}



void dev::RTC::localtime(struct tm& t) {
  read_registers(t.tm_year, t.tm_mon, t.tm_mday, t.tm_hour, t.tm_min, t.tm_sec);
}


void rtc_init(void) {
  u8 cmos_mode = dev::CMOS::read(0x0b);
  cmos_mode |= 2;  // 24 hour mode
  cmos_mode |= 4;  // No BCD mode
  dev::CMOS::write(0x0b, cmos_mode);

  s_boot_time = dev::RTC::now();


}

module_init("RTC", rtc_init);
