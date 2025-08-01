/*
* (C) 2016 Jack Lloyd
*
* Botan is released under the Simplified BSD License (see license.txt)
*/

#include "tests.h"

#if defined(BOTAN_HAS_PK_PADDING)
   #include <botan/internal/eme.h>
#endif

#if defined(BOTAN_HAS_RSA_SIGNATURE_PADDING)
   #include <botan/internal/fmt.h>
   #include <botan/internal/sig_padding.h>
#endif

namespace Botan_Tests {

#if defined(BOTAN_HAS_EME_PKCS1)
class EME_PKCS1v15_Decoding_Tests final : public Text_Based_Test {
   public:
      EME_PKCS1v15_Decoding_Tests() : Text_Based_Test("pk_pad_eme/pkcs1.vec", "RawCiphertext", "Plaintext") {}

      Test::Result run_one_test(const std::string& hdr, const VarMap& vars) override {
         const bool is_valid = (hdr == "valid");

         Test::Result result("PKCSv15 Decoding");

         auto pkcs = Botan::EME::create("PKCS1v15");
         if(!pkcs) {
            return result;
         }

         const std::vector<uint8_t> ciphertext = vars.get_req_bin("RawCiphertext");
         const std::vector<uint8_t> plaintext = vars.get_opt_bin("Plaintext");

         if(!is_valid) {
            result.test_eq("Plaintext value should be empty for invalid EME inputs", plaintext.size(), 0);
         }

         std::vector<uint8_t> decoded(ciphertext.size());
         auto len = pkcs->unpad(decoded, ciphertext);

         result.test_eq("EME decoding valid/invalid matches", len.has_value().as_bool(), is_valid);

         if(len.has_value().as_bool()) {
            decoded.resize(len.value_or(0));
            result.test_eq("EME decoded plaintext correct", decoded, plaintext);
         } else {
            bool all_zeros = true;
            for(uint8_t b : decoded) {
               if(b != 0) {
                  all_zeros = false;
               }
            }

            result.confirm("On invalid padding output is all zero", all_zeros);
         }

         // TODO: also test that encoding is accepted

         return result;
      }
};

BOTAN_REGISTER_TEST("pubkey", "eme_pkcs1v15", EME_PKCS1v15_Decoding_Tests);
#endif

#if defined(BOTAN_HAS_RSA_SIGNATURE_PADDING)
class SignaturePaddingSchemeNameTests final : public Test {
   public:
      std::vector<Test::Result> run() override {
         Test::Result result("SignaturePaddingScheme::name");

         std::vector<std::string> pads_need_hash = {
   #if BOTAN_HAS_EMSA_X931
            "X9.31",
   #endif
   #if BOTAN_HAS_EMSA_PKCS1
            "PKCS1v15",
   #endif
   #if BOTAN_HAS_EMSA_PSSR
            "PSS",
            "PSS_Raw",
   #endif
   #if BOTAN_HAS_ISO_9796
            "ISO_9796_DS2",
            "ISO_9796_DS3",
   #endif
         };

         std::vector<std::string> pads_no_hash = {
   #if BOTAN_HAS_EMSA_RAW
            "Raw",
   #endif
   #if BOTAN_HAS_EMSA_PKCS1
            "PKCS1v15(Raw)",
            "PKCS1v15(Raw,SHA-512)",
   #endif
         };

         for(const auto& pad : pads_need_hash) {
            try {
               const std::string hash_to_use = "SHA-256";
               auto padding = Botan::SignaturePaddingScheme::create_or_throw(Botan::fmt("{}({})", pad, hash_to_use));
               auto padding_copy = Botan::SignaturePaddingScheme::create(padding->name());
               result.test_eq("SignaturePaddingScheme::name for " + pad, padding->name(), padding_copy->name());
            } catch(Botan::Lookup_Error&) {
               result.test_note("Skipping test due to missing hash");
            } catch(const std::exception& e) {
               result.test_failure("SignaturePaddingScheme::name for " + pad + ": " + e.what());
            }
         }

         for(const auto& pad : pads_need_hash) {
            std::string algo_name = pad + "(YYZ)";
            try {
               auto padding = Botan::SignaturePaddingScheme::create_or_throw(algo_name);
               result.test_failure("SignaturePaddingScheme::name for " + pad + ": " +
                                   "Could create SignaturePaddingScheme with fantasy hash YYZ");
            } catch(Botan::Lookup_Error&) {
               result.test_note("Skipping test due to missing hash");
            } catch(const std::exception& e) {
               result.test_eq("SignaturePaddingScheme::name for " + pad,
                              e.what(),
                              "Could not find any algorithm named \"" + algo_name + "\"");
            }
         }

         for(const auto& pad : pads_no_hash) {
            try {
               auto padding = Botan::SignaturePaddingScheme::create(pad);
               auto padding_copy = Botan::SignaturePaddingScheme::create(padding->name());
               result.test_eq("SignaturePaddingScheme::name for " + pad, padding->name(), padding_copy->name());
            } catch(Botan::Lookup_Error&) {
               result.test_note("Skipping test due to missing hash");
            } catch(const std::exception& e) {
               result.test_failure("SignaturePaddingScheme::name for " + pad + ": " + e.what());
            }
         }

         return {result};
      }
};

BOTAN_REGISTER_TEST("pubkey", "sig_padding_name", SignaturePaddingSchemeNameTests);

#endif

}  // namespace Botan_Tests
