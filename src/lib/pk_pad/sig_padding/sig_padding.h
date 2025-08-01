/*
* (C) 1999-2007 Jack Lloyd
*
* Botan is released under the Simplified BSD License (see license.txt)
*/

#ifndef BOTAN_SIGNATURE_PADDING_SCHEME_H_
#define BOTAN_SIGNATURE_PADDING_SCHEME_H_

#include <botan/types.h>
#include <memory>
#include <span>
#include <string>
#include <vector>

namespace Botan {

class RandomNumberGenerator;

/**
* RSA Signature Padding Scheme
*
* Previously called 'EMSA' from IEEE 1363's "Encoding Method for Signatures, Appendix"
*/
class BOTAN_TEST_API SignaturePaddingScheme /* NOLINT(*-special-member-functions) */ {
   public:
      virtual ~SignaturePaddingScheme() = default;

      /**
      * Factory method for SignaturePaddingScheme (message-encoding methods for signatures
      * with appendix) objects
      * @param algo_spec the name of the SignaturePaddingScheme to create
      * @return pointer to newly allocated object of that type, or nullptr
      */
      static std::unique_ptr<SignaturePaddingScheme> create(std::string_view algo_spec);

      /**
      * Factory method for SignaturePaddingScheme (message-encoding methods for signatures
      * with appendix) objects
      * @param algo_spec the name of the SignaturePaddingScheme to create
      * @return pointer to newly allocated object of that type, or throws
      */
      static std::unique_ptr<SignaturePaddingScheme> create_or_throw(std::string_view algo_spec);

      /**
      * Add more data to the signature computation
      * @param input some data
      * @param length length of input in bytes
      */
      virtual void update(const uint8_t input[], size_t length) = 0;

      /**
      * @return raw hash
      */
      virtual std::vector<uint8_t> raw_data() = 0;

      /**
      * Return the encoding of a message
      * @param msg the result of raw_data()
      * @param output_bits the desired output bit size
      * @param rng a random number generator
      * @return encoded signature
      */
      virtual std::vector<uint8_t> encoding_of(std::span<const uint8_t> msg,
                                               size_t output_bits,
                                               RandomNumberGenerator& rng) = 0;

      /**
      * Verify the encoding
      * @param encoding the received (coded) message representative
      * @param raw_hash the computed (local, uncoded) message representative
      * @param key_bits the size of the key in bits
      * @return true if coded is a valid encoding of raw, otherwise false
      */
      virtual bool verify(std::span<const uint8_t> encoding, std::span<const uint8_t> raw_hash, size_t key_bits) = 0;

      /**
      * Return the hash function being used by this padding scheme
      */
      virtual std::string hash_function() const = 0;

      /**
      * @return the SCAN name of the encoding/padding scheme
      */
      virtual std::string name() const = 0;
};

}  // namespace Botan

#endif
