#pragma once

#include <types.h>

namespace dev {

  namespace CMOS {
    u8 read(u8 index);
    void write(u8 index, u8 val);

  };
};
