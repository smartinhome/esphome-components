#pragma once

#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"

#include "../flowis_meter.h"

namespace esphome {
namespace flowis_meter {

class FlowisTextSensor : public text_sensor::TextSensor,
                         public Component,
                         public Parented<FlowisMeter> {
 public:
  void setup() override;
  void dump_config() override;

  void set_field(TextField field) { this->field_ = field; }

 protected:
  TextField field_{TextField::DEVICE_TIME};
};

}  // namespace flowis_meter
}  // namespace esphome
