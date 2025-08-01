/*
* ECIES
* (C) 2016 Philipp Weber
*     2016 Daniel Neus, Rohde & Schwarz Cybersecurity
*     2025 Jack Lloyd
*
* Botan is released under the Simplified BSD License (see license.txt)
*/

#include <botan/ecies.h>

#include <botan/cipher_mode.h>
#include <botan/ecdh.h>
#include <botan/kdf.h>
#include <botan/mac.h>
#include <botan/rng.h>
#include <botan/internal/ct_utils.h>
#include <botan/internal/pk_ops_impl.h>

namespace Botan {

namespace {

/**
* Private key type for ECIES_ECDH_KA_Operation
*
* TODO(Botan4) this can be removed once cofactor support is removed from ECDH
*/

BOTAN_DIAGNOSTIC_PUSH
BOTAN_DIAGNOSTIC_IGNORE_INHERITED_VIA_DOMINANCE

class ECIES_PrivateKey final : public EC_PrivateKey,
                               public PK_Key_Agreement_Key {
   public:
      explicit ECIES_PrivateKey(const ECDH_PrivateKey& private_key) :
            // NOLINTNEXTLINE(*-slicing)
            EC_PublicKey(private_key), EC_PrivateKey(private_key), PK_Key_Agreement_Key(), m_key(private_key) {}

      std::vector<uint8_t> public_value() const override { return m_key.public_value(); }

      std::string algo_name() const override { return "ECIES"; }

      std::unique_ptr<Public_Key> public_key() const override { return m_key.public_key(); }

      bool supports_operation(PublicKeyOperation op) const override { return (op == PublicKeyOperation::KeyAgreement); }

      std::unique_ptr<Private_Key> generate_another(RandomNumberGenerator& rng) const override {
         return m_key.generate_another(rng);
      }

      std::unique_ptr<PK_Ops::Key_Agreement> create_key_agreement_op(RandomNumberGenerator& rng,
                                                                     std::string_view params,
                                                                     std::string_view provider) const override;

   private:
      ECDH_PrivateKey m_key;
};

BOTAN_DIAGNOSTIC_POP

/**
* Implements ECDH key agreement without using the cofactor mode
*
* TODO(Botan4) this can be removed once cofactor support is removed from ECDH
*/
class ECIES_ECDH_KA_Operation final : public PK_Ops::Key_Agreement_with_KDF {
   public:
      ECIES_ECDH_KA_Operation(const ECIES_PrivateKey& private_key, RandomNumberGenerator& rng) :
            PK_Ops::Key_Agreement_with_KDF("Raw"), m_key(private_key), m_rng(rng) {}

      size_t agreed_value_size() const override { return m_key.domain().get_p_bytes(); }

      secure_vector<uint8_t> raw_agree(const uint8_t w[], size_t w_len) override {
         const EC_Group& group = m_key.domain();
         if(auto input_point = EC_AffinePoint::deserialize(group, {w, w_len})) {
            return input_point->mul(m_key._private_key(), m_rng).x_bytes();
         } else {
            throw Decoding_Error("ECIES - Invalid elliptic curve point");
         }
      }

   private:
      ECIES_PrivateKey m_key;
      RandomNumberGenerator& m_rng;
};

std::unique_ptr<PK_Ops::Key_Agreement> ECIES_PrivateKey::create_key_agreement_op(RandomNumberGenerator& rng,
                                                                                 std::string_view /*params*/,
                                                                                 std::string_view /*provider*/) const {
   return std::make_unique<ECIES_ECDH_KA_Operation>(*this, rng);
}

/**
* Creates a PK_Key_Agreement instance for the given key and ecies_params
* Returns either ECIES_ECDH_KA_Operation or the default implementation for the given key,
* depending on the key and ecies_params
* @param private_key the private key used for the key agreement
* @param ecies_params settings for ecies
* @param for_encryption disable cofactor mode if the secret will be used for encryption
* (according to ISO 18033 cofactor mode is only used during decryption)
*
* TODO(Botan4) this entire function can be removed once cofactor support is gone
*/
PK_Key_Agreement create_key_agreement(const PK_Key_Agreement_Key& private_key,
                                      const ECIES_KA_Params& ecies_params,
                                      bool for_encryption,
                                      RandomNumberGenerator& rng) {
   const ECDH_PrivateKey* ecdh_key = dynamic_cast<const ECDH_PrivateKey*>(&private_key);

   if(ecdh_key == nullptr &&
      (ecies_params.cofactor_mode() || ecies_params.old_cofactor_mode() || ecies_params.check_mode())) {
      // assume we have a private key from an external provider (e.g. pkcs#11):
      // there is no way to determine or control whether the provider uses cofactor mode or not.
      // ISO 18033 does not allow cofactor mode in combination with old cofactor mode or check mode
      // => disable cofactor mode, old cofactor mode and check mode for unknown keys/providers (as a precaution).
      throw Invalid_Argument("ECIES: cofactor, old cofactor and check mode are only supported for ECDH_PrivateKey");
   }

   if(ecdh_key != nullptr && (for_encryption || !ecies_params.cofactor_mode())) {
      // ECDH_KA_Operation uses cofactor mode: use own key agreement method if cofactor should not be used.
      return PK_Key_Agreement(ECIES_PrivateKey(*ecdh_key), rng, "Raw");
   }

   return PK_Key_Agreement(private_key, rng, "Raw");  // use default implementation
}
}  // namespace

ECIES_KA_Operation::ECIES_KA_Operation(const PK_Key_Agreement_Key& private_key,
                                       const ECIES_KA_Params& ecies_params,
                                       bool for_encryption,
                                       RandomNumberGenerator& rng) :
      m_ka(create_key_agreement(private_key, ecies_params, for_encryption, rng)), m_params(ecies_params) {}

#if defined(BOTAN_HAS_LEGACY_EC_POINT)
/**
* ECIES secret derivation according to ISO 18033-2
*/
SymmetricKey ECIES_KA_Operation::derive_secret(const std::vector<uint8_t>& eph_public_key_bin,
                                               const EC_Point& other_public_key_point) const {
   if(other_public_key_point.is_zero()) {
      throw Invalid_Argument("ECIES: other public key point is zero");
   }

   auto kdf = KDF::create_or_throw(m_params.kdf());

   EC_Point other_point = other_public_key_point;

   // ISO 18033: step b
   // TODO(Botan4) remove when cofactor support is removed
   if(m_params.old_cofactor_mode() && m_params.group().has_cofactor()) {
      other_point *= m_params.group().get_cofactor();
   }

   secure_vector<uint8_t> derivation_input;

   // ISO 18033: encryption step e / decryption step g
   if(!m_params.single_hash_mode()) {
      derivation_input += eph_public_key_bin;
   }

   // ISO 18033: encryption step f / decryption step h
   std::vector<uint8_t> other_public_key_bin = other_point.encode(m_params.point_format());
   // Note: the argument `m_params.secret_length()` passed for `key_len` will only be used by providers because
   // "Raw" is passed to the `PK_Key_Agreement` if the implementation of botan is used.
   const SymmetricKey peh =
      m_ka.derive_key(m_params.group().get_order_bytes(), other_public_key_bin.data(), other_public_key_bin.size());
   derivation_input.insert(derivation_input.end(), peh.begin(), peh.end());

   // ISO 18033: encryption step g / decryption step i
   return SymmetricKey(kdf->derive_key(m_params.secret_length(), derivation_input));
}
#endif

/**
* ECIES secret derivation according to ISO 18033-2
*/
SymmetricKey ECIES_KA_Operation::derive_secret(std::span<const uint8_t> eph_public_key_bin,
                                               const EC_AffinePoint& other_public_key_point) const {
   BOTAN_ARG_CHECK(!other_public_key_point.is_identity(), "ECIES: peer public key point is the identity element");

   auto kdf = KDF::create_or_throw(m_params.kdf());

   auto other_point = other_public_key_point;

   const auto& group = m_params.group();

   // ISO 18033: step b
   // TODO(Botan4) remove when cofactor support is removed
   if(m_params.old_cofactor_mode() && group.has_cofactor()) {
      Null_RNG null_rng;
      auto cofactor = EC_Scalar::from_bigint(group, group.get_cofactor());
      other_point = other_point.mul(cofactor, null_rng);
   }

   secure_vector<uint8_t> derivation_input;

   // ISO 18033: encryption step e / decryption step g
   if(!m_params.single_hash_mode()) {
      derivation_input.assign(eph_public_key_bin.begin(), eph_public_key_bin.end());
   }

   // ISO 18033: encryption step f / decryption step h
   std::vector<uint8_t> other_public_key_bin = other_point.serialize(m_params.point_format());
   // Note: the argument `m_params.secret_length()` passed for `key_len` will only be used by providers because
   // "Raw" is passed to the `PK_Key_Agreement` if the implementation of botan is used.
   const SymmetricKey peh =
      m_ka.derive_key(m_params.group().get_order_bytes(), other_public_key_bin.data(), other_public_key_bin.size());
   derivation_input.insert(derivation_input.end(), peh.begin(), peh.end());

   // ISO 18033: encryption step g / decryption step i
   return SymmetricKey(kdf->derive_key(m_params.secret_length(), derivation_input));
}

ECIES_KA_Params::ECIES_KA_Params(
   const EC_Group& group, std::string_view kdf, size_t length, EC_Point_Format point_format, ECIES_Flags flags) :
      m_group(group),
      m_kdf(kdf),
      m_length(length),
      m_point_format(point_format),
      m_single_hash_mode((flags & ECIES_Flags::SingleHashMode) == ECIES_Flags::SingleHashMode),
      m_check_mode((flags & ECIES_Flags::CheckMode) == ECIES_Flags::CheckMode),
      m_cofactor_mode((flags & ECIES_Flags::CofactorMode) == ECIES_Flags::CofactorMode),
      m_old_cofactor_mode((flags & ECIES_Flags::OldCofactorMode) == ECIES_Flags::OldCofactorMode) {}

ECIES_KA_Params::ECIES_KA_Params(
   const EC_Group& group, std::string_view kdf, size_t length, EC_Point_Format point_format, bool single_hash_mode) :
      m_group(group),
      m_kdf(kdf),
      m_length(length),
      m_point_format(point_format),
      m_single_hash_mode(single_hash_mode),
      m_check_mode(true),
      m_cofactor_mode(false),
      m_old_cofactor_mode(false) {}

ECIES_System_Params::ECIES_System_Params(const EC_Group& group,
                                         std::string_view kdf,
                                         std::string_view dem_algo_spec,
                                         size_t dem_key_len,
                                         std::string_view mac_spec,
                                         size_t mac_key_len,
                                         EC_Point_Format point_format,
                                         ECIES_Flags flags) :
      ECIES_KA_Params(group, kdf, dem_key_len + mac_key_len, point_format, flags),
      m_dem_spec(dem_algo_spec),
      m_dem_keylen(dem_key_len),
      m_mac_spec(mac_spec),
      m_mac_keylen(mac_key_len) {
   // ISO 18033: "At most one of CofactorMode, OldCofactorMode, and CheckMode may be 1."
   if(size_t(cofactor_mode()) + size_t(old_cofactor_mode()) + size_t(check_mode()) > 1) {
      throw Invalid_Argument("ECIES: only one of cofactor_mode, old_cofactor_mode and check_mode can be set");
   }
}

ECIES_System_Params::ECIES_System_Params(const EC_Group& group,
                                         std::string_view kdf,
                                         std::string_view dem_algo_spec,
                                         size_t dem_key_len,
                                         std::string_view mac_spec,
                                         size_t mac_key_len,
                                         EC_Point_Format point_format,
                                         bool single_hash_mode) :
      ECIES_KA_Params(group, kdf, dem_key_len + mac_key_len, point_format, single_hash_mode),
      m_dem_spec(dem_algo_spec),
      m_dem_keylen(dem_key_len),
      m_mac_spec(mac_spec),
      m_mac_keylen(mac_key_len) {}

std::unique_ptr<MessageAuthenticationCode> ECIES_System_Params::create_mac() const {
   return MessageAuthenticationCode::create_or_throw(m_mac_spec);
}

std::unique_ptr<Cipher_Mode> ECIES_System_Params::create_cipher(Cipher_Dir direction) const {
   return Cipher_Mode::create_or_throw(m_dem_spec, direction);
}

/*
* ECIES_Encryptor Constructor
*/
ECIES_Encryptor::ECIES_Encryptor(const PK_Key_Agreement_Key& private_key,
                                 const ECIES_System_Params& ecies_params,
                                 RandomNumberGenerator& rng) :
      m_ka(private_key, ecies_params, true, rng),
      m_params(ecies_params),
      m_eph_public_key_bin(private_key.public_value()) {
   if(ecies_params.point_format() != EC_Point_Format::Uncompressed) {
      // ISO 18033: step d
      // convert only if necessary; m_eph_public_key_bin has been initialized with the uncompressed format
      m_eph_public_key_bin =
         EC_AffinePoint(m_params.group(), m_eph_public_key_bin).serialize(ecies_params.point_format());
   }
   m_mac = m_params.create_mac();
   m_cipher = m_params.create_cipher(Cipher_Dir::Encryption);
}

/*
* ECIES_Encryptor Constructor
*/
ECIES_Encryptor::ECIES_Encryptor(RandomNumberGenerator& rng, const ECIES_System_Params& ecies_params) :
      ECIES_Encryptor(ECDH_PrivateKey(rng, ecies_params.group()), ecies_params, rng) {}

size_t ECIES_Encryptor::maximum_input_size() const {
   /*
   ECIES should just be used for key transport so this (arbitrary) limit
   seems sufficient
   */
   return 64;
}

size_t ECIES_Encryptor::ciphertext_length(size_t ptext_len) const {
   return m_eph_public_key_bin.size() + m_mac->output_length() + m_cipher->output_length(ptext_len);
}

/*
* ECIES Encryption according to ISO 18033-2
*/
std::vector<uint8_t> ECIES_Encryptor::enc(const uint8_t data[],
                                          size_t length,
                                          RandomNumberGenerator& /*unused*/) const {
   if(!m_other_point.has_value()) {
      throw Invalid_State("ECIES_Encryptor: peer key invalid or not set");
   }

   const SymmetricKey secret_key = m_ka.derive_secret(m_eph_public_key_bin, m_other_point.value());

   // encryption

   m_cipher->set_key(SymmetricKey(secret_key.begin(), m_params.dem_keylen()));
   if(m_iv.empty() && !m_cipher->valid_nonce_length(m_iv.size())) {
      throw Invalid_Argument("ECIES with " + m_cipher->name() + " requires an IV be set");
   }

   m_cipher->start(m_iv.bits_of());

   secure_vector<uint8_t> encrypted_data(data, data + length);
   m_cipher->finish(encrypted_data);

   // compute the MAC
   m_mac->set_key(secret_key.begin() + m_params.dem_keylen(), m_params.mac_keylen());
   m_mac->update(encrypted_data);
   if(!m_label.empty()) {
      m_mac->update(m_label);
   }
   const auto mac = m_mac->final();

   // concat elements
   return concat(m_eph_public_key_bin, encrypted_data, mac);
}

ECIES_Decryptor::ECIES_Decryptor(const PK_Key_Agreement_Key& key,
                                 const ECIES_System_Params& ecies_params,
                                 RandomNumberGenerator& rng) :
      m_ka(key, ecies_params, false, rng), m_params(ecies_params) {
   /*
   ISO 18033: "If v > 1 and CheckMode = 0, then we must have gcd(u, v) = 1." (v = index, u= order)

   We skip this check because even if CheckMode = 0 we actually do check that
   the point is valid. In addition the check from ISO 18033 is pretty odd; u is
   the _prime_ order subgroup, and v is the cofactor. For gcd(u, v) > 1 to occur
   the cofactor would have to be a multiple of the group order, implying that
   the overall group was at least the square of the group order. Such a curve
   would also break our assumption that one can check for membership in the
   prime order subgroup by multiplying by the group order and checking for the
   identity.
   */

   m_mac = m_params.create_mac();
   m_cipher = m_params.create_cipher(Cipher_Dir::Decryption);
}

namespace {

size_t compute_point_size(const EC_Group& group, EC_Point_Format format) {
   const size_t fe_bytes = group.get_p_bytes();
   if(format == EC_Point_Format::Compressed) {
      return 1 + fe_bytes;
   } else {
      return 1 + 2 * fe_bytes;
   }
}

}  // namespace

size_t ECIES_Decryptor::plaintext_length(size_t ctext_len) const {
   const size_t point_size = compute_point_size(m_params.group(), m_params.point_format());
   const size_t overhead = point_size + m_mac->output_length();

   if(ctext_len < overhead) {
      return 0;
   }

   return m_cipher->output_length(ctext_len - overhead);
}

/**
* ECIES Decryption according to ISO 18033-2
*/
secure_vector<uint8_t> ECIES_Decryptor::do_decrypt(uint8_t& valid_mask, const uint8_t in[], size_t in_len) const {
   const size_t point_size = compute_point_size(m_params.group(), m_params.point_format());

   if(in_len < point_size + m_mac->output_length()) {
      throw Decoding_Error("ECIES decryption: ciphertext is too short");
   }

   // extract data
   const std::vector<uint8_t> other_public_key_bin(in, in + point_size);  // the received (ephemeral) public key
   const std::vector<uint8_t> encrypted_data(in + point_size, in + in_len - m_mac->output_length());
   const std::vector<uint8_t> mac_data(in + in_len - m_mac->output_length(), in + in_len);

   // ISO 18033: step a
   auto other_public_key = EC_AffinePoint(m_params.group(), other_public_key_bin);

   // ISO 18033: step b would check if other_public_key is on the curve iff check_mode is on
   // but we ignore this and always check if the point is on the curve

   // ISO 18033: step e (and step f because get_affine_x (called by ECDH_KA_Operation::raw_agree)
   // throws Illegal_Transformation if the point is zero)
   const SymmetricKey secret_key = m_ka.derive_secret(other_public_key_bin, other_public_key);

   // validate mac
   m_mac->set_key(secret_key.begin() + m_params.dem_keylen(), m_params.mac_keylen());
   m_mac->update(encrypted_data);
   if(!m_label.empty()) {
      m_mac->update(m_label);
   }
   const secure_vector<uint8_t> calculated_mac = m_mac->final();
   valid_mask = CT::is_equal(mac_data.data(), calculated_mac.data(), mac_data.size()).value();

   if(valid_mask == 0xFF) {
      // decrypt data

      m_cipher->set_key(SymmetricKey(secret_key.begin(), m_params.dem_keylen()));
      if(m_iv.empty() && !m_cipher->valid_nonce_length(m_iv.size())) {
         throw Invalid_Argument("ECIES with " + m_cipher->name() + " requires an IV be set");
      }
      m_cipher->start(m_iv.bits_of());

      try {
         // the decryption can fail:
         // e.g. Invalid_Authentication_Tag is thrown if GCM is used and the message does not have a valid tag
         secure_vector<uint8_t> decrypted_data(encrypted_data.begin(), encrypted_data.end());
         m_cipher->finish(decrypted_data);
         return decrypted_data;
      } catch(...) {
         valid_mask = 0;
      }
   }
   return secure_vector<uint8_t>();
}

}  // namespace Botan
