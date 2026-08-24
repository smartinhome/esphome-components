#include "flowis_sensor.h"
#include "esphome/core/log.h"

namespace esphome {
namespace flowis_meter {

static const char *const TAG = "flowis_meter.sensor";

void FlowisSensor::setup() {
  this->parent_->add_listener([this](const FlowisTelegram &telegram) {
    float value;
    // A telegram that does not carry this field leaves the sensor alone: the
    // meter alternates between reading and history telegrams, and republishing
    // a placeholder on every other frame would wreck long-term statistics.
    if (extract_numeric(telegram, this->field_, &value))
      this->publish_state(value);
  });
}

void FlowisSensor::dump_config() {
  LOG_SENSOR("", "Flowis sensor", this);
  ESP_LOGCONFIG(TAG, "  Meter: 0x%08X",
                static_cast<unsigned>(this->parent_->get_meter_id()));
}

}  // namespace flowis_meter
}  // namespace esphome
