#include "flowis_meter.h"
#include "esphome/core/log.h"

namespace esphome {
namespace flowis_meter {

static const char *const TAG = "flowis_meter";

float FlowisMeter::get_setup_priority() const {
  return setup_priority::DATA;
}

void FlowisMeter::setup() {
  if (this->radio_ == nullptr) {
    ESP_LOGE(TAG, "No radio configured for meter 0x%08X",
             static_cast<unsigned>(this->meter_id_));
    this->mark_failed();
    return;
  }

  this->radio_->add_telegram_handler([this](const FlowisTelegram &telegram) {
    if (telegram.meter_id != this->meter_id_)
      return;
    for (auto &listener : this->listeners_)
      listener(telegram);
  });
}

void FlowisMeter::dump_config() {
  ESP_LOGCONFIG(TAG, "Flowis meter 0x%08X",
                static_cast<unsigned>(this->meter_id_));
}

bool extract_numeric(const FlowisTelegram &telegram, NumericField field,
                     float *out) {
  switch (field) {
    case NumericField::RSSI_DBM:
      *out = telegram.rssi;
      return true;

    case NumericField::TOTAL_M3:
      if (!telegram.has_reading)
        return false;
      *out = telegram.volume_m3();
      return true;

    case NumericField::ALARM:
      if (!telegram.has_reading)
        return false;
      *out = telegram.alarm;
      return true;

    case NumericField::BACKFLOW:
      if (!telegram.has_reading)
        return false;
      *out = telegram.backflow;
      return true;

    case NumericField::ARCHIVE_M3:
      if (!telegram.has_archive)
        return false;
      *out = telegram.archive_m3();
      return true;

    default:
      break;
  }

  if (!telegram.has_history)
    return false;

  const auto slot = static_cast<uint8_t>(field) -
                    static_cast<uint8_t>(NumericField::PERIOD_0_M3);
  if (slot >= flowis_radio::FLOWIS_HISTORY_SLOTS)
    return false;

  *out = telegram.history_l[slot] / 1000.0f;
  return true;
}

bool extract_text(const FlowisTelegram &telegram, TextField field,
                  std::string *out) {
  switch (field) {
    case TextField::DEVICE_TIME:
      if (!telegram.has_clock)
        return false;
      *out = telegram.clock_to_string();
      return true;

    case TextField::TELEGRAM_TYPE:
      *out = telegram.has_reading   ? "reading"
             : telegram.has_history ? "history"
                                    : "unknown";
      return true;

    case TextField::TELEGRAM_HEX:
      *out = telegram.to_hex();
      return true;
  }
  return false;
}

}  // namespace flowis_meter
}  // namespace esphome
