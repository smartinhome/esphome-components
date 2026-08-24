#pragma once

#include "esphome/core/automation.h"
#include "flowis_radio.h"

namespace esphome {
namespace flowis_radio {

class TelegramTrigger : public Trigger<FlowisTelegram> {
 public:
  explicit TelegramTrigger(FlowisRadio *radio) {
    radio->add_telegram_handler(
        [this](const FlowisTelegram &telegram) { this->trigger(telegram); });
  }
};

}  // namespace flowis_radio
}  // namespace esphome
