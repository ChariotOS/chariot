#pragma once

#include <types.h>
#include <ck/string.h>

#define SHA_DIGEST_WORDS 5
#define SHA_WORKSPACE_WORDS 80

namespace crypto {
  // a sha1 state
  class Sha1 {
   public:
    Sha1(void);


    void update(const void *buffer, size_t len);

    // finalize into a 20 byte buffer
    void finalize(unsigned char *digest);
    ck::string finalize(void);

   private:
    uint32_t state[5];
    uint32_t count[2];
    unsigned char buffer[64];
  };


  class MD5 {
   public:
    MD5(void);


    void update(const void *buffer, size_t len);

    // finalize into a 20 byte buffer
    void finalize(unsigned char *digest);
    ck::string finalize(void);

   private:

		void transform(uint8_t *data);
    unsigned char data[64];
    unsigned int datalen;
    unsigned long long bitlen;
    unsigned int state[4];
  };


}  // namespace crypto
