#include <botan/auto_rng.h>
#include <botan/certstor.h>
#include <botan/ecdh.h>
#include <botan/tls.h>

/**
 * @brief Callbacks invoked by TLS::Channel.
 *
 * Botan::TLS::Callbacks is an abstract class.
 * For improved readability, only the functions that are mandatory
 * to implement are listed here. See src/lib/tls/tls_callbacks.h.
 */
class Callbacks : public Botan::TLS::Callbacks {
   public:
      void tls_emit_data([[maybe_unused]] std::span<const uint8_t> data) override {
         // send data to tls server, e.g., using BSD sockets or boost asio
      }

      void tls_record_received([[maybe_unused]] uint64_t seq_no,
                               [[maybe_unused]] std::span<const uint8_t> data) override {
         // process full TLS record received by tls server, e.g.,
         // by passing it to the application
      }

      void tls_alert([[maybe_unused]] Botan::TLS::Alert alert) override {
         // handle a tls alert received from the tls server
      }

      std::unique_ptr<Botan::PK_Key_Agreement_Key> tls_generate_ephemeral_key(
         const std::variant<Botan::TLS::Group_Params, Botan::DL_Group>& group,
         Botan::RandomNumberGenerator& rng) override {
         if(std::holds_alternative<Botan::TLS::Group_Params>(group) &&
            std::get<Botan::TLS::Group_Params>(group) == Botan::TLS::Group_Params(0xFE00)) {
            // generate a private key of my custom curve
            const auto ec_group = Botan::EC_Group::from_name("numsp256d1");
            return std::make_unique<Botan::ECDH_PrivateKey>(rng, ec_group);
         } else {
            // no custom curve used: up-call the default implementation
            return Botan::TLS::Callbacks::tls_generate_ephemeral_key(group, rng);
         }
      }

      std::unique_ptr<Botan::Public_Key> tls_deserialize_peer_public_key(
         const std::variant<Botan::TLS::Group_Params, Botan::DL_Group>& group,
         std::span<const uint8_t> public_value) override {
         if(std::holds_alternative<Botan::TLS::Group_Params>(group) &&
            std::get<Botan::TLS::Group_Params>(group) == Botan::TLS::Group_Params(0xFE00)) {
            // load the peer's public key of my custom curve
            const auto ec_group = Botan::EC_Group::from_name("numsp256d1");
            return std::make_unique<Botan::ECDH_PublicKey>(ec_group, Botan::EC_AffinePoint(ec_group, public_value));
         } else {
            // no custom curve used: up-call the default implementation
            return Botan::TLS::Callbacks::tls_deserialize_peer_public_key(group, public_value);
         }
      }
};

/**
 * @brief Credentials storage for the tls client.
 *
 * It returns a list of trusted CA certificates from a local directory.
 * TLS client authentication is disabled. See src/lib/tls/credentials_manager.h.
 */
class Client_Credentials : public Botan::Credentials_Manager {
   public:
      std::vector<Botan::Certificate_Store*> trusted_certificate_authorities(
         [[maybe_unused]] const std::string& type, [[maybe_unused]] const std::string& context) override {
         // return a list of certificates of CAs we trust for tls server certificates,
         // e.g., all the certificates in the local directory "cas"
         return {&m_cert_store};
      }

   private:
      Botan::Certificate_Store_In_Memory m_cert_store{"cas"};
};

class Client_Policy : public Botan::TLS::Strict_Policy {
   public:
      std::vector<Botan::TLS::Group_Params> key_exchange_groups() const override {
         // modified strict policy to allow our custom curves

         // NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange)
         return {static_cast<Botan::TLS::Group_Params>(0xFE00)};
      }
};

int main() {
   if(!Botan::EC_Group::supports_application_specific_group()) {
      // This build configuration does not support application specific EC groups
      return 1;
   }

   // prepare rng
   auto rng = std::make_shared<Botan::AutoSeeded_RNG>();

   // In this case we will use numsp256d1 from https://datatracker.ietf.org/doc/html/draft-black-numscurves-02

   const Botan::BigInt p("0xFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF43");
   const Botan::BigInt a("0xFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF40");
   const Botan::BigInt b("0x25581");
   const Botan::BigInt n("0xFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFE43C8275EA265C6020AB20294751A825");

   const Botan::BigInt g_x("0x01");
   const Botan::BigInt g_y("0x696F1853C1E466D7FC82C96CCEEEDD6BD02C2F9375894EC10BF46306C2B56C77");

   // This is an OID reserved in Botan's private arc for numsp256d1
   // If you use some other curve you should create your own OID
   const Botan::OID oid("1.3.6.1.4.1.25258.4.1");

   // create EC_Group object to register the curve
   Botan::EC_Group numsp256d1(oid, p, a, b, g_x, g_y, n);

   if(!numsp256d1.verify_group(*rng)) {
      return 1;
      // Warning: if verify_group returns false the curve parameters are insecure
   }

   // register name to specified oid
   Botan::OID::register_oid(oid, "numsp256d1");

   // prepare all the parameters
   auto callbacks = std::make_shared<Callbacks>();
   auto session_mgr = std::make_shared<Botan::TLS::Session_Manager_In_Memory>(rng);
   auto creds = std::make_shared<Client_Credentials>();
   auto policy = std::make_shared<Botan::TLS::Strict_Policy>();

   // open the tls connection
   Botan::TLS::Client client(callbacks,
                             session_mgr,
                             creds,
                             policy,
                             rng,
                             Botan::TLS::Server_Information("botan.randombit.net", 443),
                             Botan::TLS::Protocol_Version::TLS_V12);

   while(!client.is_closed()) {
      // read data received from the tls server, e.g., using BSD sockets or boost asio
      // ...

      // send data to the tls server using client.send()
   }

   return 0;
}
