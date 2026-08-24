#pragma once

#include "esphome/components/sensor/sensor.h"
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"

#include "../flowis_meter.h"

namespace esphome {
namespace flowis_meter {

class FlowisSensor : public sensor::Sensor,
                     public Component,
                     public Parented<FlowisMeter> {
 public:
  void setup() override;
  void dump_config() override;

  void set_field(NumericField field) { this->field_ = field; }

 protected:
  NumericField field_{NumericField::TOTAL_M3};
};

}  // namespace flowis_meter
}  // namespace esphome
