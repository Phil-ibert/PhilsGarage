#pragma once

#include <string>
#include <vector>

#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "esphome/core/helpers.h"

#ifdef USE_SENSOR
#include "esphome/components/sensor/sensor.h"
#endif
#ifdef USE_BINARY_SENSOR
#include "esphome/components/binary_sensor/binary_sensor.h"
#endif
#ifdef USE_TEXT_SENSOR
#include "esphome/components/text_sensor/text_sensor.h"
#endif

#include "phils_codec.h"

namespace esphome::phils_lora {

/// Reçoit chaque champ "K:V" d'un message valide (utilisé par le switch).
class PhilsLoraFieldListener {
 public:
  virtual void on_field(const char *key, size_t key_len, const char *value, size_t value_len) = 0;
};

#ifdef USE_BINARY_SENSOR
/// Binary sensor "liaison OK" : vrai tant qu'un paquet valide a été reçu depuis moins de `timeout`.
class PhilsLoraLinkSensor : public binary_sensor::BinarySensor {
 public:
  void set_timeout(uint32_t timeout_ms) { this->timeout_ms_ = timeout_ms; }
  uint32_t get_timeout() const { return this->timeout_ms_; }

 protected:
  uint32_t timeout_ms_{30 * 60 * 1000};
};
#endif

/// Passerelle du protocole PhilsCryptoLoRa, indépendante du modem Semtech.
/// Le modem (sx127x / sx126x...) est déclaré dans le YAML :
///   - son `on_packet` appelle `handle_packet(x, rssi, snr)`
///   - notre `on_transmit` appelle son action `send_packet`
class PhilsLora : public Component {
 public:
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

  void set_key(const std::vector<uint8_t> &key) { this->key_ = key; }

  /// À appeler depuis `on_packet` du modem radio.
  void handle_packet(const std::vector<uint8_t> &packet, float rssi, float snr);
  /// Chiffre et envoie un message clair, ex. "#S:1$". Retourne false si impossible.
  bool send(const std::string &message);
  /// millis() du dernier paquet valide (0 = jamais).
  uint32_t get_last_rx_ms() const { return this->last_rx_ms_; }

  Trigger<std::vector<uint8_t>> *get_transmit_trigger() { return &this->transmit_trigger_; }
  void set_has_transmit_automation(bool v) { this->has_transmit_automation_ = v; }
  Trigger<std::string, float, float> *get_message_trigger() { return &this->message_trigger_; }
  void set_has_message_automation(bool v) { this->has_message_automation_ = v; }

  void add_field_listener(PhilsLoraFieldListener *listener) { this->field_listeners_.push_back(listener); }

#ifdef USE_SENSOR
  void add_field_sensor(const char *key, sensor::Sensor *s) { this->field_sensors_.push_back({key, s}); }
  void set_rssi_sensor(sensor::Sensor *s) { this->rssi_sensor_ = s; }
  void set_snr_sensor(sensor::Sensor *s) { this->snr_sensor_ = s; }
  void set_valid_packets_sensor(sensor::Sensor *s) { this->valid_packets_sensor_ = s; }
  void set_rejected_packets_sensor(sensor::Sensor *s) { this->rejected_packets_sensor_ = s; }
#endif
#ifdef USE_BINARY_SENSOR
  void add_field_binary_sensor(const char *key, binary_sensor::BinarySensor *s) {
    this->field_binary_sensors_.push_back({key, s});
  }
  void add_link_sensor(PhilsLoraLinkSensor *s) { this->link_sensors_.push_back(s); }
#endif
#ifdef USE_TEXT_SENSOR
  void set_last_message_sensor(text_sensor::TextSensor *s) { this->last_message_sensor_ = s; }
  void set_last_error_sensor(text_sensor::TextSensor *s) { this->last_error_sensor_ = s; }
#endif

 protected:
  void dispatch_field_(const char *key, size_t key_len, const char *value, size_t value_len);
  void update_link_sensors_();
  void count_rejected_(DecodeStatus status, float rssi);

  PhilsCodec codec_;
  std::vector<uint8_t> key_;
  char rx_buf_[PHILS_MAX_PACKET + 1]{};

  Trigger<std::vector<uint8_t>> transmit_trigger_;
  Trigger<std::string, float, float> message_trigger_;
  bool has_transmit_automation_{false};
  bool has_message_automation_{false};

  uint32_t last_rx_ms_{0};
  uint32_t valid_packets_{0};
  uint32_t rejected_packets_{0};

  std::vector<PhilsLoraFieldListener *> field_listeners_;

  template<typename T> struct Keyed {
    const char *key;
    T *entity;
  };
#ifdef USE_SENSOR
  std::vector<Keyed<sensor::Sensor>> field_sensors_;
  sensor::Sensor *rssi_sensor_{nullptr};
  sensor::Sensor *snr_sensor_{nullptr};
  sensor::Sensor *valid_packets_sensor_{nullptr};
  sensor::Sensor *rejected_packets_sensor_{nullptr};
#endif
#ifdef USE_BINARY_SENSOR
  std::vector<Keyed<binary_sensor::BinarySensor>> field_binary_sensors_;
  std::vector<PhilsLoraLinkSensor *> link_sensors_;
#endif
#ifdef USE_TEXT_SENSOR
  text_sensor::TextSensor *last_message_sensor_{nullptr};
  text_sensor::TextSensor *last_error_sensor_{nullptr};
#endif
};

/// Action YAML `phils_lora.send`
template<typename... Ts> class SendAction : public Action<Ts...>, public Parented<PhilsLora> {
 public:
  TEMPLATABLE_VALUE(std::string, payload)

  void play(const Ts &...x) override { this->parent_->send(this->payload_.value(x...)); }
};

}  // namespace esphome::phils_lora
