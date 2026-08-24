#pragma once

#include "esphome/core/component.h"
#include "esphome/core/gpio.h"

#include "cc1101.h"
#include "telegram.h"

#include <functional>
#include <memory>
#include <vector>

namespace esphome {
namespace flowis_radio {

/// The radio always captures a full 64-byte FIFO: 62 payload bytes plus the
/// two status bytes appended by the CC1101.  Flowis telegrams top out around
/// 31 bytes, so the tail is noise that the length byte and CRC discard.
static constexpr size_t FLOWIS_PACKET_BYTES = 62;
static constexpr size_t FLOWIS_FIFO_BYTES = 64;

class FlowisRadio : public Component,
                    public spi::SPIDevice<spi::BIT_ORDER_MSB_FIRST,
                                          spi::CLOCK_POLARITY_LOW,
                                          spi::CLOCK_PHASE_LEADING,
                                          FLOWIS_RADIO_SPI_DATA_RATE> {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override;

  void set_gdo0_pin(InternalGPIOPin *pin) { this->gdo0_pin_ = pin; }
  void set_gdo2_pin(InternalGPIOPin *pin) { this->gdo2_pin_ = pin; }
  void set_frequency(float mhz) { this->frequency_mhz_ = mhz; }

  /// Retune at runtime and restart reception.  The Flowis carrier is not
  /// documented anywhere, so this exists to let a YAML interval sweep the band
  /// until telegrams start arriving.
  void retune(float mhz);

  void add_telegram_handler(std::function<void(const FlowisTelegram &)> &&cb) {
    this->handlers_.push_back(std::move(cb));
  }

  uint32_t get_frames_received() const { return this->frames_received_; }
  uint32_t get_crc_errors() const { return this->crc_errors_; }

 protected:
  void start_rx_();
  void handle_packet_();

  std::unique_ptr<CC1101> radio_;
  InternalGPIOPin *gdo0_pin_{nullptr};
  InternalGPIOPin *gdo2_pin_{nullptr};

  float frequency_mhz_{868.0f};

  std::vector<std::function<void(const FlowisTelegram &)>> handlers_;

  uint32_t last_rx_start_{0};
  uint32_t frames_received_{0};
  uint32_t crc_errors_{0};
  bool radio_ok_{false};
};

}  // namespace flowis_radio
}  // namespace esphome
