/*
* Buffered Computation
* (C) 1999-2007 Jack Lloyd
*
* Botan is released under the Simplified BSD License (see license.txt)
*/

#ifndef BOTAN_BUFFERED_COMPUTATION_H_
#define BOTAN_BUFFERED_COMPUTATION_H_

#include <botan/concepts.h>
#include <botan/secmem.h>
#include <span>
#include <string_view>

namespace Botan {

/**
* This class represents any kind of computation which uses an internal
* state, such as hash functions or MACs
*/
class BOTAN_PUBLIC_API(2, 0) Buffered_Computation /* NOLINT(*special-member-functions) */ {
   public:
      /**
      * @return length of the output of this function in bytes
      */
      virtual size_t output_length() const = 0;

      /**
      * Add new input to process.
      * @param in the input to process as a byte array
      * @param length of param in in bytes
      */
      void update(const uint8_t in[], size_t length) { add_data({in, length}); }

      /**
      * Add new input to process.
      * @param in the input to process as a contiguous data range
      */
      void update(std::span<const uint8_t> in) { add_data(in); }

      void update_be(uint16_t val);
      void update_be(uint32_t val);
      void update_be(uint64_t val);

      void update_le(uint16_t val);
      void update_le(uint32_t val);
      void update_le(uint64_t val);

      /**
      * Add new input to process.
      * @param str the input to process as a std::string_view. Will be interpreted
      * as a byte array based on the strings encoding.
      */
      void update(std::string_view str);

      /**
      * Process a single byte.
      * @param in the byte to process
      */
      void update(uint8_t in) { add_data({&in, 1}); }

      /**
      * Complete the computation and retrieve the
      * final result.
      * @param out The byte array to be filled with the result.
      * Must be of length output_length()
      */
      void final(uint8_t out[]) { final_result({out, output_length()}); }

      /**
      * Complete the computation and retrieve the
      * final result as a container of your choice.
      * @return a contiguous container holding the result
      */
      template <concepts::resizable_byte_buffer T = secure_vector<uint8_t>>
      T final() {
         T output(output_length());
         final_result(output);
         return output;
      }

      std::vector<uint8_t> final_stdvec() { return final<std::vector<uint8_t>>(); }

      void final(std::span<uint8_t> out);

      template <concepts::resizable_byte_buffer T>
      void final(T& out) {
         out.resize(output_length());
         final_result(out);
      }

      /**
      * Update and finalize computation. Does the same as calling update()
      * and final() consecutively.
      * @param in the input to process as a byte array
      * @param length the length of the byte array
      * @result the result of the call to final()
      */
      template <concepts::resizable_byte_buffer T = secure_vector<uint8_t>>
      T process(const uint8_t in[], size_t length) {
         update(in, length);
         return final<T>();
      }

      /**
      * Update and finalize computation. Does the same as calling update()
      * and final() consecutively.
      * @param in the input to process as a string
      * @result the result of the call to final()
      */
      template <concepts::resizable_byte_buffer T = secure_vector<uint8_t>>
      T process(std::string_view in) {
         update(in);
         return final<T>();
      }

      /**
      * Update and finalize computation. Does the same as calling update()
      * and final() consecutively.
      * @param in the input to process as a contiguous container
      * @result the result of the call to final()
      */
      template <concepts::resizable_byte_buffer T = secure_vector<uint8_t>>
      T process(std::span<const uint8_t> in) {
         update(in);
         return final<T>();
      }

      virtual ~Buffered_Computation() = default;

   private:
      /**
      * Add more data to the computation
      * @param input is an input buffer
      */
      virtual void add_data(std::span<const uint8_t> input) = 0;

      /**
      * Write the final output to out
      * @param out is an output buffer of output_length()
      */
      virtual void final_result(std::span<uint8_t> out) = 0;
};

}  // namespace Botan

#endif
