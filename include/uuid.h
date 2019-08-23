#pragma once

#include <types.h>
#include <string.h>

using uuid_t = u64;

namespace uuid {

  uuid_t next(void);

  string format(uuid_t);
};
