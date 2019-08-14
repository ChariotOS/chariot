#pragma once

#include <types.h>

namespace dev {
namespace RTC {
time_t now();
time_t boot_time();

void read_registers(unsigned& year, unsigned& month, unsigned& day,
                    unsigned& hour, unsigned& minute, unsigned& second);
};  // namespace RTC
}  // namespace dev
