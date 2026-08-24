#include "flowis_radio.h"
#include "rf_settings.h"

#include "esphome/core/hal.h"
#include "esphome/core/log.h"

namespace esphome {
namespace flowis_radio {

static const char *const TAG = "flowis_radio";

/// If no complete packet turns up this long after entering RX, assume the state
/// machine is wedged (missed sync, aborted transmission, RX overflow) and kick
/// it.  Meters transmit every couple of seconds, so this is generous.
static constexpr uint32_t RX_WATCHDOG_MS = 10000;

float FlowisRadio::get_setup_priority() const {
  return setup_priority::BUS;
}

void FlowisRadio::setup() {
  this->spi_setup();

  if (this->gdo0_pin_ != nullptr) {
    this->gdo0_pin_->setup();
    this->gdo0_pin_->pin_mode(gpio::FLAG_INPUT);
  }
  if (this->gdo2_pin_ != nullptr) {
    this->gdo2_pin_->setup();
    this->gdo2_pin_->pin_mode(gpio::FLAG_INPUT);
  }

  this->radio_ = make_unique<CC1101>(this);

  if (!this->radio_->reset()) {
    this->mark_failed();
    return;
  }

  apply_flowis_rf_settings(*this->radio_);
  this->radio_->set_frequency(this->frequency_mhz_);

  this->radio_ok_ = true;
  this->start_rx_();
}

void FlowisRadio::start_rx_() {
  this->radio_->send_strobe(Strobe::SIDLE);
  this->radio_->send_strobe(Strobe::SFRX);
  this->radio_->send_strobe(Strobe::SRX);
  this->last_rx_start_ = millis();
}

void FlowisRadio::retune(float mhz) {
  this->frequency_mhz_ = mhz;
  if (!this->radio_ok_)
    return;
  this->radio_->send_strobe(Strobe::SIDLE);
  this->radio_->set_frequency(mhz);
  // MCSM0 is configured to auto-calibrate on the IDLE -> RX transition, so the
  // synthesiser recalibrates for the new carrier on the next SRX.
  this->start_rx_();
}

void FlowisRadio::loop() {
  if (!this->radio_ok_)
    return;

  // Fast path: GDO0 is wired to "RX FIFO at threshold", and the threshold is
  // set to a full FIFO, so the pin going high means a complete packet is
  // waiting.  Without the pin we fall back to polling RXBYTES over SPI.
  if (this->gdo0_pin_ != nullptr) {
    if (this->gdo0_pin_->digital_read()) {
      this->handle_packet_();
      return;
    }
  } else {
    bool overflow = false;
    if (this->radio_->rx_bytes(&overflow) >= FLOWIS_FIFO_BYTES && !overflow) {
      this->handle_packet_();
      return;
    }
  }

  if (millis() - this->last_rx_start_ > RX_WATCHDOG_MS) {
    uint8_t marc = this->radio_->read_status(Status::MARCSTATE) & 0x1F;
    if (marc != static_cast<uint8_t>(MarcState::RX)) {
      ESP_LOGW(TAG, "Receiver stuck in MARCSTATE 0x%02X, restarting", marc);
    }
    this->start_rx_();
  }
}

void FlowisRadio::handle_packet_() {
  uint8_t buffer[FLOWIS_FIFO_BYTES];
  this->radio_->read_rx_fifo(buffer, FLOWIS_FIFO_BYTES);
  this->start_rx_();

  // Status bytes appended by the CC1101 (PKTCTRL1.APPEND_STATUS).
  const uint8_t rssi_raw = buffer[FLOWIS_PACKET_BYTES];
  const int8_t rssi = static_cast<int8_t>(
      (rssi_raw >= 128 ? (static_cast<int>(rssi_raw) - 256) : rssi_raw) / 2 -
      74);

  FlowisTelegram telegram;
  if (!flowis_parse(buffer, FLOWIS_PACKET_BYTES, &telegram)) {
    this->crc_errors_++;
    ESP_LOGV(TAG, "Discarded frame (RSSI %d dBm), L=0x%02X", rssi, buffer[0]);
    return;
  }
  telegram.rssi = rssi;
  this->frames_received_++;

  if (telegram.has_reading) {
    ESP_LOGD(TAG, "Meter 0x%08X: %.3f m3, backflow=%u, alarm=%u, RSSI %d dBm",
             static_cast<unsigned>(telegram.meter_id), telegram.volume_m3(),
             telegram.backflow, telegram.alarm, rssi);
    // Braces are load-bearing: ESP_LOGV expands to nothing once the log level
    // is compiled out, which would otherwise leave a bare `if (...) ;`.
    if (telegram.has_clock) {
      ESP_LOGV(TAG, "  device clock %s%s", telegram.clock_to_string().c_str(),
               telegram.clock_dst ? " (DST)" : "");
    }
    if (telegram.has_archive) {
      ESP_LOGV(TAG, "  archive %.3f m3", telegram.archive_m3());
    }
  } else if (telegram.has_history) {
    ESP_LOGD(TAG,
             "Meter 0x%08X history: %u | %u %u %u %u %u %u l, RSSI %d dBm",
             static_cast<unsigned>(telegram.meter_id), telegram.history_l[0],
             telegram.history_l[1], telegram.history_l[2],
             telegram.history_l[3], telegram.history_l[4],
             telegram.history_l[5], telegram.history_l[6], rssi);
  } else {
    ESP_LOGD(TAG, "Meter 0x%08X: unhandled telegram type %u (%s)",
             static_cast<unsigned>(telegram.meter_id), telegram.type,
             telegram.to_hex().c_str());
  }

  for (auto &handler : this->handlers_)
    handler(telegram);
}

void FlowisRadio::dump_config() {
  ESP_LOGCONFIG(TAG, "Flowis radio:");
  ESP_LOGCONFIG(TAG, "  Frequency: %.3f MHz", this->frequency_mhz_);
  LOG_PIN("  GDO0 pin: ", this->gdo0_pin_);
  LOG_PIN("  GDO2 pin: ", this->gdo2_pin_);
  if (this->is_failed())
    ESP_LOGE(TAG, "  CC1101 initialisation failed");
}

}  // namespace flowis_radio
}  // namespace esphome
