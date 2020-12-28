#include <arpa/inet.h>
#include <assert.h>
#include <ck/string.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/sysbind.h>
#include <sys/time.h>
#include <time.h>

typedef uint64_t ntp_ts_t;

struct [[gnu::packed]] ntp_pkt {
  uint8_t li_vn_mode;
  uint8_t stratum;
  int8_t poll;
  int8_t precision;

  uint32_t root_delay;
  uint32_t root_dispersion;
  uint32_t reference_id;

  ntp_ts_t reference_timestamp;
  ntp_ts_t origin_timestamp;
  ntp_ts_t receive_timestamp;
  ntp_ts_t transmit_timestamp;

  uint8_t leap_information() const { return li_vn_mode >> 6; }
  uint8_t version_number() const { return (li_vn_mode >> 3) & 7; }
  uint8_t mode() const { return li_vn_mode & 7; }
};
static_assert(sizeof(ntp_pkt) == 48);



// NTP measures time in seconds since 1900-01-01, POSIX in seconds since 1970-01-01.
// 1900 wasn't a leap year, so there are 70/4 leap years between 1900 and 1970.
// Overflows a 32-bit signed int, but not a 32-bit unsigned int.
const unsigned SecondsFrom1900To1970 = (70u * 365u + 70u / 4u) * 24u * 60u * 60u;


static ntp_ts_t ntp_timestamp_from_timeval(const timeval& t) {
  assert(t.tv_usec >= 0 && t.tv_usec < 1'000'000);  // Fits in 20 bits when normalized.

  // Seconds just need translation to the different origin.
  uint32_t seconds = t.tv_sec + SecondsFrom1900To1970;

  // Fractional bits are decimal fixed point (*1'000'000) in timeval, but binary fixed-point (* 2**32) in NTP
  // timestamps.
  uint32_t fractional_bits = static_cast<uint32_t>((static_cast<uint64_t>(t.tv_usec) << 32) / 1'000'000);

  return (static_cast<ntp_ts_t>(seconds) << 32) | fractional_bits;
}


static struct timeval timeval_from_ntp_timestamp(const ntp_ts_t& ntp_timestamp) {
  struct timeval t;
  t.tv_sec = static_cast<time_t>(ntp_timestamp >> 32) - SecondsFrom1900To1970;
  t.tv_usec = static_cast<suseconds_t>((static_cast<uint64_t>(ntp_timestamp & 0xFFFFFFFFu) * 1'000'000) >> 32);
  return t;
}


static ck::string format_ntp_timestamp(ntp_ts_t ntp_timestamp) {
  char buffer[28];  // YYYY-MM-DDTHH:MM:SS.UUUUUUZ is 27 characters long.
  timeval t = timeval_from_ntp_timestamp(ntp_timestamp);
  struct tm tm;
  // gmtime_r(&t.tv_sec, &tm);
  size_t written = strftime(buffer, sizeof(buffer), "%Y-%m-%dT%T.", &tm);
  assert(written == 20);
  written += snprintf(buffer + written, sizeof(buffer) - written, "%06d", (int)t.tv_usec);
  assert(written == 26);
  buffer[written++] = 'Z';
  buffer[written] = '\0';
  return buffer;
}


// #define NTP_PORT 8080
// #define NTP_HOST "fenrir.nickw.io"
#define NTP_PORT 123
#define NTP_HOST "time.google.com"

int main(int argc, char** argv) {
  bool adjust_time = false;
  bool set_time = false;
  bool verbose = false;
  verbose = true;


  int fd = socket(AF_INET, SOCK_DGRAM, 0);
  if (fd < 0) {
    perror("socket");
    return 1;
  }

  struct timeval timeout {
    5, 0
  };


  struct sockaddr_in peer_address;
  memset(&peer_address, 0, sizeof(peer_address));
  peer_address.sin_family = AF_INET;
  peer_address.sin_port = htons(NTP_PORT);

  sysbind_dnslookup(NTP_HOST, &peer_address.sin_addr.s_addr);

  ntp_pkt packet;
  memset(&packet, 0, sizeof(packet));
  packet.li_vn_mode = (4 << 3) | 3;  // Version 4, client connection.


  uint64_t random_transmit_timestamp = (long)rand() << 32 | rand();
  timeval local_transmit_time;
  gettimeofday(&local_transmit_time, nullptr);
  packet.transmit_timestamp = random_transmit_timestamp;


  ssize_t rc = sendto(fd, &packet, sizeof(packet), 0, (const struct sockaddr*)&peer_address, sizeof(peer_address));

	// const char msg[] = "Hello\n";
	// sendto(fd, msg, sizeof(msg), 0, (const struct sockaddr*)&peer_address, sizeof(peer_address));


  if (rc < 0) {
    perror("sendto");
    return 1;
  }
  if ((size_t)rc < sizeof(packet)) {
    fprintf(stderr, "incomplete packet send\n");
    return 1;
  }

  return EXIT_SUCCESS;
}
