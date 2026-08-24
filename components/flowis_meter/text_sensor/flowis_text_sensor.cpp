#include "flowis_text_sensor.h"
#include "esphome/core/log.h"

namespace esphome {
namespace flowis_meter {

static const char *const TAG = "flowis_meter.text_sensor";

void FlowisTextSensor::setup() {
  this->parent_->add_listener([this](const FlowisTelegram &telegram) {
    std::string value;
    if (extract_text(telegram, this->field_, &value))
      this->publish_state(value);
  });
}

void FlowisTextSensor::dump_config() {
  LOG_TEXT_SENSOR("", "Flowis text sensor", this);
  ESP_LOGCONFIG(TAG, "  Meter: 0x%08X",
                static_cast<unsigned>(this->parent_->get_meter_id()));
}

}  // namespace flowis_meter
}  // namespace esphome
