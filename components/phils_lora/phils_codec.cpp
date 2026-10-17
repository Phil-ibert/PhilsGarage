#include "phils_codec.h"

#include <cstdio>
#include <cstring>

namespace esphome::phils_lora {

const char *decode_status_to_str(DecodeStatus status) {
  switch (status) {
    case DecodeStatus::OK:
      return "OK";
    case DecodeStatus::BAD_LENGTH:
      return "longueur invalide";
    case DecodeStatus::NO_KEY:
      return "pas de clé";
    case DecodeStatus::NO_SEPARATOR:
      return "pas de séparateur '~'";
    case DecodeStatus::EMPTY_MESSAGE:
      return "message vide";
    case DecodeStatus::BAD_CRC_FORMAT:
      return "CRC illisible";
    case DecodeStatus::CRC_MISMATCH:
      return "CRC incorrect";
  }
  return "inconnu";
}

uint32_t phils_crc32(const uint8_t *data, size_t len) {
  uint32_t crc = 0xFFFFFFFFu;
  for (size_t i = 0; i < len; i++) {
    crc ^= data[i];
    for (int b = 0; b < 8; b++)
      crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
  }
  return crc ^ 0xFFFFFFFFu;
}

PhilsCodec::PhilsCodec() {
  mbedtls_aes_init(&this->enc_);
  mbedtls_aes_init(&this->dec_);
}

PhilsCodec::~PhilsCodec() {
  mbedtls_aes_free(&this->enc_);
  mbedtls_aes_free(&this->dec_);
}

bool PhilsCodec::set_key(const uint8_t *key, size_t len) {
  this->has_key_ = false;
  if (key == nullptr || len != 32)
    return false;
  if (mbedtls_aes_setkey_enc(&this->enc_, key, 256) != 0)
    return false;
  if (mbedtls_aes_setkey_dec(&this->dec_, key, 256) != 0)
    return false;
  this->has_key_ = true;
  return true;
}

DecodeStatus PhilsCodec::decode(const uint8_t *data, size_t len, char *out, size_t out_size, size_t *out_len) {
  if (out_len != nullptr)
    *out_len = 0;
  if (!this->has_key_)
    return DecodeStatus::NO_KEY;
  if (data == nullptr || out == nullptr || len == 0 || len > PHILS_MAX_PACKET || (len % PHILS_BLOCK_SIZE) != 0 ||
      out_size < len + 1)
    return DecodeStatus::BAD_LENGTH;

  for (size_t i = 0; i < len; i += PHILS_BLOCK_SIZE) {
    mbedtls_aes_crypt_ecb(&this->dec_, MBEDTLS_AES_DECRYPT, data + i, reinterpret_cast<uint8_t *>(out) + i);
  }
  out[len] = '\0';  // borne explicite : on ne lit jamais au-delà du paquet
  const size_t clear_len = strnlen(out, len);

  const char *tilde = static_cast<const char *>(memchr(out, '~', clear_len));
  if (tilde == nullptr)
    return DecodeStatus::NO_SEPARATOR;
  const size_t msg_len = static_cast<size_t>(tilde - out);
  if (msg_len == 0)
    return DecodeStatus::EMPTY_MESSAGE;

  // CRC décimal : 1 à 10 chiffres, jusqu'au '~' suivant ou la fin.
  const char *p = tilde + 1;
  const char *end = out + clear_len;
  uint64_t received = 0;
  size_t digits = 0;
  while (p < end && *p != '~') {
    if (*p < '0' || *p > '9')
      return DecodeStatus::BAD_CRC_FORMAT;
    received = received * 10u + static_cast<uint64_t>(*p - '0');
    if (++digits > 10 || received > 0xFFFFFFFFull)
      return DecodeStatus::BAD_CRC_FORMAT;
    p++;
  }
  if (digits == 0)
    return DecodeStatus::BAD_CRC_FORMAT;

  const uint32_t computed = phils_crc32(reinterpret_cast<const uint8_t *>(out), msg_len);
  if (computed != static_cast<uint32_t>(received))
    return DecodeStatus::CRC_MISMATCH;

  out[msg_len] = '\0';
  if (out_len != nullptr)
    *out_len = msg_len;
  return DecodeStatus::OK;
}

size_t PhilsCodec::encode(const char *message, size_t len, uint8_t *out, size_t out_size) {
  if (!this->has_key_ || message == nullptr || out == nullptr || len == 0 || len > PHILS_MAX_MESSAGE)
    return 0;
  if (memchr(message, '~', len) != nullptr || memchr(message, '\0', len) != nullptr)
    return 0;

  uint8_t clear[PHILS_MAX_PACKET];
  memset(clear, 0, sizeof(clear));
  memcpy(clear, message, len);
  const uint32_t crc = phils_crc32(reinterpret_cast<const uint8_t *>(message), len);
  const int n = snprintf(reinterpret_cast<char *>(clear) + len, sizeof(clear) - len, "~%lu", (unsigned long) crc);
  if (n <= 0 || static_cast<size_t>(n) >= sizeof(clear) - len)
    return 0;
  const size_t clear_len = len + static_cast<size_t>(n);
  // +1 : au moins un 0x00 de terminaison dans le texte chiffré (voir phils_codec.h)
  const size_t packet_len = ((clear_len + 1 + PHILS_BLOCK_SIZE - 1) / PHILS_BLOCK_SIZE) * PHILS_BLOCK_SIZE;
  if (packet_len > PHILS_MAX_PACKET || packet_len > out_size)
    return 0;

  for (size_t i = 0; i < packet_len; i += PHILS_BLOCK_SIZE) {
    mbedtls_aes_crypt_ecb(&this->enc_, MBEDTLS_AES_ENCRYPT, clear + i, out + i);
  }
  return packet_len;
}

}  // namespace esphome::phils_lora
