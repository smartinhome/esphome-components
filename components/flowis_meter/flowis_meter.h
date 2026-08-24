#pragma once

#include "esphome/components/flowis_radio/flowis_radio.h"
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"

#include <functional>
#include <vector>

namespace esphome {
namespace flowis_meter {

using flowis_radio::FlowisRadio;
using flowis_radio::FlowisTelegram;

enum class NumericField : uint8_t {
  TOTAL_M3,
  ARCHIVE_M3,
  ALARM,
  BACKFLOW,
  RSSI_DBM,
  PERIOD_0_M3,
  PERIOD_1_M3,
  PERIOD_2_M3,
  PERIOD_3_M3,
  PERIOD_4_M3,
  PERIOD_5_M3,
  PERIOD_6_M3,
};

enum class TextField : uint8_t {
  DEVICE_TIME,
  TELEGRAM_TYPE,
  TELEGRAM_HEX,
};

class FlowisMeter : public Component {
 public:
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override;

  void set_radio(FlowisRadio *radio) { this->radio_ = radio; }
  void set_meter_id(uint32_t meter_id) { this->meter_id_ = meter_id; }
  uint32_t get_meter_id() const { return this->meter_id_; }

  void add_listener(std::function<void(const FlowisTelegram &)> &&callback) {
    this->listeners_.push_back(std::move(callback));
  }

 protected:
  FlowisRadio *radio_{nullptr};
  uint32_t meter_id_{0};
  std::vector<std::function<void(const FlowisTelegram &)>> listeners_;
};

/// Pull one numeric field out of a telegram.
///
/// Returns false when the telegram does not carry that field - a history
/// telegram has no volume, a reading telegram has no period counters - so the
/// sensor keeps its previous state instead of publishing a bogus zero.
bool extract_numeric(const FlowisTelegram &telegram, NumericField field,
                     float *out);

/// Same contract as extract_numeric(), for string-valued fields.
bool extract_text(const FlowisTelegram &telegram, TextField field,
                  std::string *out);

}  // namespace flowis_meter
}  // namespace esphome
