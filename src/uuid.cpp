#include <asm.h>
#include <dev/RTC.h>
#include <uuid.h>

static u64 counter = 0;

uuid_t uuid::next(void) {
  uuid_t id = 0;

  u64 time = dev::RTC::now();
  id |= time << 32;
  id |= (rdtsc() & 0xFFFF) << 16;
  id |= (counter++ & 0xFFFF);

  return id;
}

string uuid::format(uuid_t id) {
  return string::format("%08llx-%04lx-%04lx", (id >> 32) & 0xFFFFFFFF,
                        (id >> 16) & 0xFFFF, id & 0xFFFF);
}
