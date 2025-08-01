/*
 * ISO-9796-2 - Digital signature schemes giving message recovery schemes 2 and 3
 * (C) 2016 Tobias Niemann, Hackmanit GmbH
 *
 * Botan is released under the Simplified BSD License (see license.txt)
 */

#ifndef BOTAN_ISO9796_H_
#define BOTAN_ISO9796_H_

#include <botan/internal/sig_padding.h>
#include <memory>
#include <string>
#include <vector>

namespace Botan {

class HashFunction;

/**
* ISO-9796-2 - Digital signature scheme 2 (probabilistic)
*/
class ISO_9796_DS2 final : public SignaturePaddingScheme {
   public:
      /**
       * @param hash function to use
       * @param implicit whether or not the trailer is implicit
       * @param salt_size size of the salt to use in bytes
       */
      ISO_9796_DS2(std::unique_ptr<HashFunction> hash, bool implicit, size_t salt_size) :
            m_hash(std::move(hash)), m_implicit(implicit), m_salt_len(salt_size) {}

      std::string hash_function() const override;

      std::string name() const override;

   private:
      void update(const uint8_t input[], size_t length) override;

      std::vector<uint8_t> raw_data() override;

      std::vector<uint8_t> encoding_of(std::span<const uint8_t> msg,
                                       size_t output_bits,
                                       RandomNumberGenerator& rng) override;

      bool verify(std::span<const uint8_t> coded, std::span<const uint8_t> raw, size_t key_bits) override;

      std::unique_ptr<HashFunction> m_hash;
      bool m_implicit;
      size_t m_salt_len;
      std::vector<uint8_t> m_msg_buffer;
};

/**
* ISO-9796-2 - Digital signature scheme 3 (deterministic)
*/
class ISO_9796_DS3 final : public SignaturePaddingScheme {
   public:
      /**
       * @param hash function to use
       * @param implicit whether or not the trailer is implicit
       */
      explicit ISO_9796_DS3(std::unique_ptr<HashFunction> hash, bool implicit = false) :
            m_hash(std::move(hash)), m_implicit(implicit) {}

      std::string name() const override;

      std::string hash_function() const override;

   private:
      void update(const uint8_t input[], size_t length) override;

      std::vector<uint8_t> raw_data() override;

      std::vector<uint8_t> encoding_of(std::span<const uint8_t> msg,
                                       size_t output_bits,
                                       RandomNumberGenerator& rng) override;

      bool verify(std::span<const uint8_t> coded, std::span<const uint8_t> raw, size_t key_bits) override;

      std::unique_ptr<HashFunction> m_hash;
      bool m_implicit;
      std::vector<uint8_t> m_msg_buffer;
};

}  // namespace Botan

#endif
