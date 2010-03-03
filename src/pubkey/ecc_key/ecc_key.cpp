/*
* ECC Key implemenation
* (C) 2007 Manuel Hartl, FlexSecure GmbH
*          Falko Strenzke, FlexSecure GmbH
*     2008-2010 Jack Lloyd
*
* Distributed under the terms of the Botan license
*/

#include <botan/ecc_key.h>
#include <botan/x509_key.h>
#include <botan/numthry.h>
#include <botan/der_enc.h>
#include <botan/ber_dec.h>
#include <botan/secmem.h>
#include <botan/point_gfp.h>

namespace Botan {

EC_PublicKey::EC_PublicKey(const EC_Domain_Params& dom_par,
                           const PointGFp& pub_point) :
   domain_params(dom_par), public_key(pub_point),
   domain_encoding(EC_DOMPAR_ENC_EXPLICIT)
   {
   if(domain().get_curve() != public_point().get_curve())
      throw Invalid_Argument("EC_PublicKey: curve mismatch in constructor");

   try
      {
      public_key.check_invariants();
      }
   catch(Illegal_Point)
      {
      throw Invalid_State("Public key failed invariant check");
      }
   }

void EC_PublicKey::X509_load_hook()
   {
   try
      {
      public_point().check_invariants();
      }
   catch(Illegal_Point)
      {
      throw Decoding_Error("Invalid public point; not on curve");
      }
   }

X509_Encoder* EC_PublicKey::x509_encoder() const
   {
   class EC_Key_Encoder : public X509_Encoder
      {
      public:
         AlgorithmIdentifier alg_id() const
            {
            return AlgorithmIdentifier(key->get_oid(),
                                       key->DER_domain());
            }

         MemoryVector<byte> key_bits() const
            {
            return EC2OSP(key->public_point(), PointGFp::COMPRESSED);
            }

         EC_Key_Encoder(const EC_PublicKey* k): key(k) {}
      private:
         const EC_PublicKey* key;
      };

   return new EC_Key_Encoder(this);
   }

X509_Decoder* EC_PublicKey::x509_decoder()
   {
   class EC_Key_Decoder : public X509_Decoder
      {
      public:
         void alg_id(const AlgorithmIdentifier& alg_id)
            {
            key->domain_params = EC_Domain_Params(alg_id.parameters);
            }

         void key_bits(const MemoryRegion<byte>& bits)
            {
            key->public_key = PointGFp(
               OS2ECP(bits, key->domain().get_curve()));

            key->X509_load_hook();
            }

         EC_Key_Decoder(EC_PublicKey* k): key(k) {}
      private:
         EC_PublicKey* key;
      };

   return new EC_Key_Decoder(this);
   }

void EC_PublicKey::set_parameter_encoding(EC_Domain_Params_Encoding form)
   {
   if(form != EC_DOMPAR_ENC_EXPLICIT &&
      form != EC_DOMPAR_ENC_IMPLICITCA &&
      form != EC_DOMPAR_ENC_OID)
      throw Invalid_Argument("Invalid encoding form for EC-key object specified");

   if((form == EC_DOMPAR_ENC_OID) && (domain_params.get_oid() == ""))
      throw Invalid_Argument("Invalid encoding form OID specified for "
                             "EC-key object whose corresponding domain "
                             "parameters are without oid");

   domain_encoding = form;
   }

const BigInt& EC_PrivateKey::private_value() const
   {
   if(private_key == 0)
      throw Invalid_State("EC_PrivateKey::private_value - uninitialized");

   return private_key;
   }

/**
* EC_PrivateKey generator
**/
EC_PrivateKey::EC_PrivateKey(const EC_Domain_Params& dom_par,
                             const BigInt& priv_key) :
   EC_PublicKey(dom_par, dom_par.get_base_point() * private_key),
   private_key(priv_key)
   {
   }

/**
* EC_PrivateKey generator
**/
EC_PrivateKey::EC_PrivateKey(RandomNumberGenerator& rng,
                             const EC_Domain_Params& dom_par)
   {
   domain_params = dom_par;

   private_key = BigInt::random_integer(rng, 1, domain().get_order());
   public_key = domain().get_base_point() * private_key;

   try
      {
      public_key.check_invariants();
      }
   catch(Illegal_Point)
      {
      throw Internal_Error("ECC private key generation failed");
      }
   }

/**
* Return the PKCS #8 public key encoder
**/
PKCS8_Encoder* EC_PrivateKey::pkcs8_encoder() const
   {
   class EC_Key_Encoder : public PKCS8_Encoder
      {
      public:
         AlgorithmIdentifier alg_id() const
            {
            return AlgorithmIdentifier(key->get_oid(),
                                       key->domain().DER_encode(EC_DOMPAR_ENC_EXPLICIT));
            }

         MemoryVector<byte> key_bits() const
            {
            return DER_Encoder()
               .start_cons(SEQUENCE)
               .encode(BigInt(1))
               .encode(BigInt::encode_1363(key->private_key, key->private_key.bytes()),
                       OCTET_STRING)
               .end_cons()
               .get_contents();
            }

         EC_Key_Encoder(const EC_PrivateKey* k): key(k) {}
      private:
         const EC_PrivateKey* key;
      };

   return new EC_Key_Encoder(this);
   }

/**
* Return the PKCS #8 public key decoder
*/
PKCS8_Decoder* EC_PrivateKey::pkcs8_decoder(RandomNumberGenerator&)
   {
   class EC_Key_Decoder : public PKCS8_Decoder
      {
      public:
         void alg_id(const AlgorithmIdentifier& alg_id)
            {
            key->domain_params = EC_Domain_Params(alg_id.parameters);
            }

         void key_bits(const MemoryRegion<byte>& bits)
            {
            u32bit version;
            SecureVector<byte> octstr_secret;

            BER_Decoder(bits)
               .start_cons(SEQUENCE)
               .decode(version)
               .decode(octstr_secret, OCTET_STRING)
               .verify_end()
               .end_cons();

            key->private_key = BigInt::decode(octstr_secret, octstr_secret.size());

            if(version != 1)
               throw Decoding_Error("Wrong PKCS #1 key format version for EC key");

            key->PKCS8_load_hook();
            }

         EC_Key_Decoder(EC_PrivateKey* k): key(k) {}
      private:
         EC_PrivateKey* key;
      };

   return new EC_Key_Decoder(this);
   }

void EC_PrivateKey::PKCS8_load_hook(bool)
   {
   public_key = domain().get_base_point() * private_key;
   }

}
