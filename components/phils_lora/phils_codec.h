#pragma once

// Codec du protocole LoRa "PhilsCryptoLoRa" (émetteur Arduino du garage).
//
// Format sur l'air (inchangé par rapport au code Arduino) :
//   clair   = "<message>~<crc32 décimal>"   ex: "#T:21.50;H:55.2;L:1;E:12.345;P:345$~3783492489"
//   paquet  = AES-256-ECB(clair complété par des 0x00 jusqu'à un multiple de 16)
//   crc32   = CRC-32 standard (zlib / ISO-HDLC) calculé sur <message> uniquement
//
// Ce fichier ne dépend pas d'ESPHome (seulement de mbedTLS) pour pouvoir être
// testé sur PC. Aucune allocation dynamique : tout passe par des buffers fournis.

#include <cstddef>
#include <cstdint>

#include "mbedtls/aes.h"

namespace esphome::phils_lora {

static constexpr size_t PHILS_BLOCK_SIZE = 16;
// Taille max d'un paquet LoRa (255) arrondie au bloc AES inférieur.
static constexpr size_t PHILS_MAX_PACKET = 240;
// Longueur max d'un message clair (sans "~crc") qu'on accepte / émet.
static constexpr size_t PHILS_MAX_MESSAGE = PHILS_MAX_PACKET - 12;

enum class DecodeStatus : uint8_t {
  OK = 0,
  BAD_LENGTH,      // vide, trop long ou pas un multiple de 16 octets
  NO_KEY,          // clé AES non configurée
  NO_SEPARATOR,    // pas de '~' dans le texte déchiffré
  EMPTY_MESSAGE,   // rien avant le '~'
  BAD_CRC_FORMAT,  // ce qui suit '~' n'est pas un entier 32 bits décimal
  CRC_MISMATCH,    // CRC calculé différent du CRC reçu (mauvaise clé, paquet corrompu, autre émetteur...)
};

const char *decode_status_to_str(DecodeStatus status);

/// CRC-32 standard (polynôme 0x04C11DB7 réfléchi, init 0xFFFFFFFF, xorout 0xFFFFFFFF).
/// Identique à RobTillaart CRC32 avec ses paramètres par défaut et à zlib.crc32().
uint32_t phils_crc32(const uint8_t *data, size_t len);

class PhilsCodec {
 public:
  PhilsCodec();
  ~PhilsCodec();
  PhilsCodec(const PhilsCodec &) = delete;
  PhilsCodec &operator=(const PhilsCodec &) = delete;

  /// key: 32 octets (AES-256)
  bool set_key(const uint8_t *key, size_t len);
  bool has_key() const { return this->has_key_; }

  /// Déchiffre et vérifie un paquet. En cas de succès, `out` contient le message
  /// (sans "~crc") terminé par '\0' et `out_len` sa longueur.
  /// `out_size` doit être >= PHILS_MAX_PACKET + 1.
  DecodeStatus decode(const uint8_t *data, size_t len, char *out, size_t out_size, size_t *out_len);

  /// Chiffre un message. `out` doit faire au moins PHILS_MAX_PACKET octets.
  /// Retourne la taille du paquet (multiple de 16) ou 0 en cas d'erreur.
  /// Le texte clair contient TOUJOURS au moins un 0x00 final : l'émetteur Arduino
  /// d'origine lit le texte déchiffré comme une chaîne C sans borne, et sans ce 0x00
  /// il déborde de son buffer (c'était le cas des commandes "#S:1$" / "#S:0$",
  /// qui font pile 16 octets avec leur CRC).
  size_t encode(const char *message, size_t len, uint8_t *out, size_t out_size);

 protected:
  mbedtls_aes_context enc_;
  mbedtls_aes_context dec_;
  bool has_key_{false};
};

/// Parcourt les champs d'un message "#K:V;K:V;...$".
/// Appelle cb(key, key_len, value, value_len) pour chaque champ non vide.
/// Retourne false si le message n'est pas encadré par '#' ... '$'.
template<typename F> bool for_each_field(const char *msg, size_t len, F &&cb) {
  if (len < 2 || msg[0] != '#' || msg[len - 1] != '$')
    return false;
  const char *p = msg + 1;
  const char *end = msg + len - 1;
  while (p < end) {
    const char *seg_end = p;
    while (seg_end < end && *seg_end != ';')
      seg_end++;
    // segment [p, seg_end) = "K:V"
    const char *colon = p;
    while (colon < seg_end && *colon != ':')
      colon++;
    if (colon > p && colon < seg_end) {
      const char *val = colon + 1;
      const char *val_end = val;
      // Comme getValue(':', 1) côté Arduino : la valeur s'arrête au ':' suivant.
      while (val_end < seg_end && *val_end != ':')
        val_end++;
      cb(p, static_cast<size_t>(colon - p), val, static_cast<size_t>(val_end - val));
    }
    p = seg_end + 1;
  }
  return true;
}

}  // namespace esphome::phils_lora
