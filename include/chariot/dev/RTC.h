#pragma once

#include <types.h>

namespace dev {
  namespace RTC {
    time_t now();
    time_t boot_time();


    void localtime(struct tm&);


    int read_seconds(void);
    void read_registers(int& year, int& month, int& day, int& hour, int& minute, int& second);
  };  // namespace RTC
}  // namespace dev
