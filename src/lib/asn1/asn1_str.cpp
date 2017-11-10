/*
* Simple ASN.1 String Types
* (C) 1999-2007 Jack Lloyd
*
* Botan is released under the Simplified BSD License (see license.txt)
*/

#include <botan/asn1_str.h>
#include <botan/der_enc.h>
#include <botan/ber_dec.h>
#include <botan/charset.h>

namespace Botan {

namespace {

/*
* Choose an encoding for the string
*/
ASN1_Tag choose_encoding(const std::string& str)
   {
   static const uint8_t IS_PRINTABLE[256] = {
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x01, 0x01, 0x01,
      0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x00,
      0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
      0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
      0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
      0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
      0x01, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00 };

   for(size_t i = 0; i != str.size(); ++i)
      {
      if(!IS_PRINTABLE[static_cast<uint8_t>(str[i])])
         {
         return UTF8_STRING;
         }
      }
   return PRINTABLE_STRING;
   }

void assert_is_string_type(ASN1_Tag tag)
   {
   if(tag != NUMERIC_STRING &&
      tag != PRINTABLE_STRING &&
      tag != VISIBLE_STRING &&
      tag != T61_STRING &&
      tag != IA5_STRING &&
      tag != UTF8_STRING &&
      tag != BMP_STRING &&
      tag != UNIVERSAL_STRING)
      {
      throw Invalid_Argument("ASN1_String: Unknown string type " +
                             std::to_string(tag));
      }
   }

}

/*
* Create an ASN1_String
*/
ASN1_String::ASN1_String(const std::string& str, ASN1_Tag t) : m_utf8_str(str), m_tag(t)
   {
   if(m_tag == DIRECTORY_STRING)
      {
      m_tag = choose_encoding(m_utf8_str);
      }

   assert_is_string_type(m_tag);
   }

/*
* Create an ASN1_String
*/
ASN1_String::ASN1_String(const std::string& str) :
   m_utf8_str(str),
   m_tag(choose_encoding(m_utf8_str))
   {}

/*
* Return this string in ISO 8859-1 encoding
*/
std::string ASN1_String::iso_8859() const
   {
   return utf8_to_latin1(m_utf8_str);
   }

/*
* DER encode an ASN1_String
*/
void ASN1_String::encode_into(DER_Encoder& encoder) const
   {
   if(m_data.empty())
      {
      encoder.add_object(tagging(), UNIVERSAL, m_utf8_str);
      }
   else
      {
      // If this string was decoded, reserialize using original encoding
      encoder.add_object(tagging(), UNIVERSAL, m_data.data(), m_data.size());
      }
   }

/*
* Decode a BER encoded ASN1_String
*/
void ASN1_String::decode_from(BER_Decoder& source)
   {
   BER_Object obj = source.get_next_object();

   assert_is_string_type(obj.type_tag);

   m_tag = obj.type_tag;
   m_data.assign(obj.value.begin(), obj.value.end());

   if(m_tag == BMP_STRING)
      {
      m_utf8_str = ucs2_to_utf8(m_data.data(), m_data.size());
      }
   else if(m_tag == UNIVERSAL_STRING)
      {
      m_utf8_str = ucs4_to_utf8(m_data.data(), m_data.size());
      }
   else
      {
      // All other supported string types are UTF-8 or some subset thereof
      m_utf8_str = ASN1::to_string(obj);
      }
   }

}
