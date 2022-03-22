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


  namespace aes {
    enum {
      AES_STATECOLS = 4,  /* columns in the state & expanded key */
      AES128_KEYCOLS = 4, /* columns in a key for aes128 */
      AES192_KEYCOLS = 6, /* columns in a key for aes128 */
      AES256_KEYCOLS = 8, /* columns in a key for aes128 */
      AES128_ROUNDS = 10, /* rounds in encryption for aes128 */
      AES192_ROUNDS = 12, /* rounds in encryption for aes192 */
      AES256_ROUNDS = 14, /* rounds in encryption for aes256 */
      AES128_KEY_LENGTH = 128 / 8,
      AES192_KEY_LENGTH = 192 / 8,
      AES256_KEY_LENGTH = 256 / 8,
      AES128_EXPAND_KEY_LENGTH = 4 * AES_STATECOLS * (AES128_ROUNDS + 1),
      AES192_EXPAND_KEY_LENGTH = 4 * AES_STATECOLS * (AES192_ROUNDS + 1),
      AES256_EXPAND_KEY_LENGTH = 4 * AES_STATECOLS * (AES256_ROUNDS + 1),
      AES_BLOCK_LENGTH = 128 / 8,
    };
    /**
     * expand_key() - Expand the AES key
     *
     * Expand a key into a key schedule, which is then used for the other
     * operations.
     *
     * @key		Key
     * @key_size	Size of the key (in bits)
     * @expkey	Buffer to place expanded key, EXPAND_KEY_LENGTH
     */
    void expand_key(u8 *key, u32 key_size, u8 *expkey);

    /**
     * encrypt() - Encrypt single block of data with AES 128
     *
     * @key_size	Size of the aes key (in bits)
     * @in		Input data
     * @expkey	Expanded key to use for encryption (from expand_key())
     * @out		Output data
     */
    void encrypt(u32 key_size, u8 *in, u8 *expkey, u8 *out);

    /**
     * decrypt() - Decrypt single block of data with AES 128
     *
     * @key_size	Size of the aes key (in bits)
     * @in		Input data
     * @expkey	Expanded key to use for decryption (from expand_key())
     * @out		Output data
     */
    void decrypt(u32 key_size, u8 *in, u8 *expkey, u8 *out);

    /**
     * Apply chain data to the destination using EOR
     *
     * Each array is of length BLOCK_LENGTH.
     *
     * @cbc_chain_data	Chain data
     * @src			Source data
     * @dst			Destination data, which is modified here
     */
    void apply_cbc_chain_data(u8 *cbc_chain_data, u8 *src, u8 *dst);

    /**
     * cbc_encrypt_blocks() - Encrypt multiple blocks of data with AES CBC.
     *
     * @key_size		Size of the aes key (in bits)
     * @key_exp		Expanded key to use
     * @iv			Initialization vector
     * @src			Source data to encrypt
     * @dst			Destination buffer
     * @num_blocks	Number of AES blocks to encrypt
     */
    void cbc_encrypt_blocks(u32 key_size, u8 *key_exp, u8 *iv, u8 *src, u8 *dst, u32 num_blocks);

    /**
     * Decrypt multiple blocks of data with AES CBC.
     *
     * @key_size		Size of the aes key (in bits)
     * @key_exp		Expanded key to use
     * @iv			Initialization vector
     * @src			Source data to decrypt
     * @dst			Destination buffer
     * @num_blocks	Number of AES blocks to decrypt
     */
    void cbc_decrypt_blocks(u32 key_size, u8 *key_exp, u8 *iv, u8 *src, u8 *dst, u32 num_blocks);
  };  // namespace aes


}  // namespace crypto
