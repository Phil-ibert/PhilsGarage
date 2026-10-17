#pragma once

#include <string>

#include "esphome/components/switch/switch.h"
#include "esphome/core/component.h"
#include "phils_lora.h"

namespace esphome::phils_lora {

/// Switch piloté à distance via LoRa.
///  - commande : envoie "#<command_field>:1$" ou "#<command_field>:0$" (répété `repeat` fois,
///    car la radio est half-duplex et une commande peut entrer en collision avec un
///    paquet de l'émetteur ; la commande est idempotente, la répéter est sans risque)
///  - état : suit le champ <state_field> des messages reçus, MAIS ignore les rapports
///    reçus moins de `confirm_delay` après une commande (ils ont pu partir avant
///    que l'émetteur n'ait reçu la commande -> évite le "clignotement" ON/OFF/ON).
class PhilsLoraSwitch : public switch_::Switch, public Component, public PhilsLoraFieldListener, public Parented<PhilsLora> {
 public:
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA - 1.0f; }

  void set_command_field(const char *field) { this->command_field_ = field; }
  void set_state_field(const char *field) { this->state_field_ = field; }
  void set_repeat(uint8_t repeat) { this->repeat_ = repeat; }
  void set_repeat_interval(uint32_t ms) { this->repeat_interval_ms_ = ms; }
  void set_confirm_delay(uint32_t ms) { this->confirm_delay_ms_ = ms; }

  void on_field(const char *key, size_t key_len, const char *value, size_t value_len) override;

 protected:
  void write_state(bool state) override;
  void send_command_();
  void schedule_repeat_();

  const char *command_field_{"S"};
  const char *state_field_{"L"};
  uint8_t repeat_{3};
  uint32_t repeat_interval_ms_{600};
  uint32_t confirm_delay_ms_{2000};

  bool desired_{false};
  bool pending_{false};
  uint32_t command_ms_{0};
  uint8_t remaining_{0};
};

}  // namespace esphome::phils_lora
