#include "phils_lora.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>

#include "esphome/core/hal.h"
#include "esphome/core/log.h"

namespace esphome::phils_lora {

static const char *const TAG = "phils_lora";

static bool key_equals(const char *key, size_t key_len, const char *expected) {
  return strlen(expected) == key_len && strncmp(key, expected, key_len) == 0;
}

void PhilsLora::setup() {
  if (!this->codec_.set_key(this->key_.data(), this->key_.size())) {
    ESP_LOGE(TAG, "Clé AES invalide (32 octets attendus, %u reçus)", (unsigned) this->key_.size());
    this->mark_failed();
    return;
  }
  // On efface la copie de la clé : le codec garde ses propres tables AES.
  std::fill(this->key_.begin(), this->key_.end(), 0);
  this->key_.clear();
  this->key_.shrink_to_fit();

#ifdef USE_BINARY_SENSOR
  if (!this->link_sensors_.empty()) {
    this->set_interval("link", 1000, [this]() { this->update_link_sensors_(); });
  }
#endif
#ifdef USE_SENSOR
  if (this->valid_packets_sensor_ != nullptr)
    this->valid_packets_sensor_->publish_state(0);
  if (this->rejected_packets_sensor_ != nullptr)
    this->rejected_packets_sensor_->publish_state(0);
#endif
}

void PhilsLora::dump_config() {
  ESP_LOGCONFIG(TAG,
                "PhilsCryptoLoRa (AES-256-ECB + CRC32):\n"
                "  Clé configurée: %s\n"
                "  on_transmit configuré: %s",
                YESNO(this->codec_.has_key()), YESNO(this->has_transmit_automation_));
#ifdef USE_SENSOR
  for (auto &fs : this->field_sensors_)
    ESP_LOGCONFIG(TAG, "  Champ '%s' -> capteur '%s'", fs.key, fs.entity->get_name().c_str());
#endif
#ifdef USE_BINARY_SENSOR
  for (auto &fs : this->field_binary_sensors_)
    ESP_LOGCONFIG(TAG, "  Champ '%s' -> binary_sensor '%s'", fs.key, fs.entity->get_name().c_str());
#endif
}

void PhilsLora::count_rejected_(DecodeStatus status, float rssi) {
  this->rejected_packets_++;
  ESP_LOGW(TAG, "Paquet rejeté (%s), RSSI %.0f dBm", decode_status_to_str(status), rssi);
#ifdef USE_SENSOR
  if (this->rejected_packets_sensor_ != nullptr)
    this->rejected_packets_sensor_->publish_state(this->rejected_packets_);
#endif
#ifdef USE_TEXT_SENSOR
  if (this->last_error_sensor_ != nullptr)
    this->last_error_sensor_->publish_state(decode_status_to_str(status));
#endif
}

void PhilsLora::handle_packet(const std::vector<uint8_t> &packet, float rssi, float snr) {
  size_t msg_len = 0;
  const DecodeStatus status =
      this->codec_.decode(packet.data(), packet.size(), this->rx_buf_, sizeof(this->rx_buf_), &msg_len);
  if (status != DecodeStatus::OK) {
    ESP_LOGV(TAG, "%u octets reçus", (unsigned) packet.size());
    this->count_rejected_(status, rssi);
    return;
  }

  this->last_rx_ms_ = millis();
  if (this->last_rx_ms_ == 0)
    this->last_rx_ms_ = 1;  // 0 est réservé à "jamais reçu"
  this->valid_packets_++;
  ESP_LOGD(TAG, "Reçu '%s' (RSSI %.0f dBm, SNR %.1f dB)", this->rx_buf_, rssi, snr);

#ifdef USE_SENSOR
  if (this->rssi_sensor_ != nullptr)
    this->rssi_sensor_->publish_state(rssi);
  if (this->snr_sensor_ != nullptr)
    this->snr_sensor_->publish_state(snr);
  if (this->valid_packets_sensor_ != nullptr)
    this->valid_packets_sensor_->publish_state(this->valid_packets_);
#endif
#ifdef USE_TEXT_SENSOR
  if (this->last_message_sensor_ != nullptr)
    this->last_message_sensor_->publish_state(this->rx_buf_, msg_len);
#endif

  const bool framed = for_each_field(this->rx_buf_, msg_len, [this](const char *k, size_t kl, const char *v, size_t vl) {
    this->dispatch_field_(k, kl, v, vl);
  });
  if (!framed)
    ESP_LOGW(TAG, "Message sans '#...$' : '%s'", this->rx_buf_);

#ifdef USE_BINARY_SENSOR
  this->update_link_sensors_();
#endif

  if (this->has_message_automation_)
    this->message_trigger_.trigger(std::string(this->rx_buf_, msg_len), rssi, snr);
}

void PhilsLora::dispatch_field_(const char *key, size_t key_len, const char *value, size_t value_len) {
  // Copie bornée et terminée par '\0' pour strtof / atoi.
  char val[32];
  const size_t n = value_len < sizeof(val) - 1 ? value_len : sizeof(val) - 1;
  memcpy(val, value, n);
  val[n] = '\0';

#ifdef USE_SENSOR
  for (auto &fs : this->field_sensors_) {
    if (!key_equals(key, key_len, fs.key))
      continue;
    char *end = nullptr;
    const float f = strtof(val, &end);
    if (end == val || !std::isfinite(f)) {
      ESP_LOGW(TAG, "Valeur non numérique pour '%s': '%s'", fs.key, val);
      continue;
    }
    fs.entity->publish_state(f);
  }
#endif
#ifdef USE_BINARY_SENSOR
  for (auto &fs : this->field_binary_sensors_) {
    if (key_equals(key, key_len, fs.key))
      fs.entity->publish_state(atoi(val) == 1);  // même règle que toInt() == 1 côté Arduino
  }
#endif
  for (auto *listener : this->field_listeners_)
    listener->on_field(key, key_len, val, n);
}

void PhilsLora::update_link_sensors_() {
#ifdef USE_BINARY_SENSOR
  const uint32_t now = millis();
  for (auto *ls : this->link_sensors_) {
    const bool ok = this->last_rx_ms_ != 0 && (now - this->last_rx_ms_) < ls->get_timeout();
    // Au démarrage on attend un timeout complet avant de déclarer la liaison perdue.
    if (!ok && this->last_rx_ms_ == 0 && now < ls->get_timeout())
      continue;
    ls->publish_state(ok);
  }
#endif
}

bool PhilsLora::send(const std::string &message) {
  if (!this->has_transmit_automation_) {
    ESP_LOGE(TAG, "Aucune action 'on_transmit' configurée : impossible d'envoyer '%s'", message.c_str());
    return false;
  }
  std::vector<uint8_t> packet(PHILS_MAX_PACKET);
  const size_t len = this->codec_.encode(message.data(), message.size(), packet.data(), packet.size());
  if (len == 0) {
    ESP_LOGE(TAG, "Impossible d'encoder '%s'", message.c_str());
    return false;
  }
  packet.resize(len);
  ESP_LOGD(TAG, "Envoi '%s' (%u octets)", message.c_str(), (unsigned) len);
  this->transmit_trigger_.trigger(packet);
  return true;
}

}  // namespace esphome::phils_lora
