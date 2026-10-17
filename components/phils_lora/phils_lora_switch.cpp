#include "phils_lora_switch.h"

#include <cinttypes>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "esphome/core/hal.h"
#include "esphome/core/log.h"

namespace esphome::phils_lora {

static const char *const TAG = "phils_lora.switch";

void PhilsLoraSwitch::setup() {
  // Volontairement : AUCUNE restauration d'état au démarrage. Un redémarrage de la
  // passerelle ne doit jamais envoyer de commande (allumer/éteindre le garage).
  // L'état réel arrive avec le prochain message de l'émetteur.
  this->parent_->add_field_listener(this);
}

void PhilsLoraSwitch::dump_config() {
  LOG_SWITCH("", "PhilsLoRa Switch", this);
  ESP_LOGCONFIG(TAG,
                "  Champ commande: '%s', champ état: '%s'\n"
                "  Répétitions: %u toutes les %" PRIu32 " ms, délai de confirmation: %" PRIu32 " ms",
                this->command_field_, this->state_field_, this->repeat_, this->repeat_interval_ms_,
                this->confirm_delay_ms_);
}

void PhilsLoraSwitch::write_state(bool state) {
  this->desired_ = state;
  this->pending_ = true;
  this->command_ms_ = millis();
  this->remaining_ = this->repeat_ > 0 ? this->repeat_ - 1 : 0;
  this->send_command_();
  this->schedule_repeat_();
  // Optimiste : l'émetteur d'origine n'acquitte pas, la confirmation viendra
  // avec son prochain message (champ d'état).
  this->publish_state(state);
}

void PhilsLoraSwitch::send_command_() {
  char msg[24];
  snprintf(msg, sizeof(msg), "#%s:%d$", this->command_field_, this->desired_ ? 1 : 0);
  this->parent_->send(msg);
}

void PhilsLoraSwitch::schedule_repeat_() {
  if (this->remaining_ == 0)
    return;
  this->set_timeout("repeat", this->repeat_interval_ms_, [this]() {
    this->send_command_();
    if (this->remaining_ > 0)
      this->remaining_--;
    this->schedule_repeat_();
  });
}

void PhilsLoraSwitch::on_field(const char *key, size_t key_len, const char *value, size_t value_len) {
  if (strlen(this->state_field_) != key_len || strncmp(key, this->state_field_, key_len) != 0)
    return;
  const bool reported = atoi(value) == 1;
  if (this->pending_) {
    const uint32_t age = millis() - this->command_ms_;
    if (age < this->confirm_delay_ms_) {
      ESP_LOGV(TAG, "Rapport '%d' ignoré (%" PRIu32 " ms après la commande)", reported, age);
      return;
    }
    this->pending_ = false;
    if (reported != this->desired_)
      ESP_LOGW(TAG, "L'émetteur rapporte %s alors que %s a été demandé (commande perdue ou interrupteur mural)",
               ONOFF(reported), ONOFF(this->desired_));
  }
  this->publish_state(reported);
}

}  // namespace esphome::phils_lora
