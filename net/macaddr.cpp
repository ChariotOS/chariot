#include <net/net.h>


net::macaddr::macaddr(void) { clear(); }

net::macaddr::macaddr(const u8 data[6]) { memcpy(raw, data, 6); }
net::macaddr::macaddr(u8 a, u8 b, u8 c, u8 d, u8 e, u8 f) {
  raw[0] = a;
  raw[1] = b;
  raw[2] = c;
  raw[3] = d;
  raw[4] = e;
  raw[5] = f;
}
net::macaddr::~macaddr() {}

bool net::macaddr::operator==(const net::macaddr &other) const {
  for (int i = 0; i < 6; i++) {
    if (raw[i] != other.raw[i]) return false;
  }
  return true;
}

bool net::macaddr::is_zero() const {
  return raw[0] == 0 && raw[1] == 0 && raw[2] == 0 && raw[3] == 0 &&
	 raw[4] == 0 && raw[5] == 0;
}

void net::macaddr::clear(void) {
  for (int i = 0; i < 6; i++) raw[i] = 0;
}
