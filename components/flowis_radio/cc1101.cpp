#include "cc1101.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"

namespace esphome {
namespace flowis_radio {

static const char *const TAG = "flowis_radio.cc1101";

uint8_t CC1101::read_register(Reg reg) {
  this->spi_->enable();
  this->spi_->transfer_byte(static_cast<uint8_t>(reg) | READ_SINGLE);
  uint8_t value = this->spi_->transfer_byte(0x00);
  this->spi_->disable();
  return value;
}

void CC1101::write_register(Reg reg, uint8_t value) {
  this->spi_->enable();
  this->spi_->transfer_byte(static_cast<uint8_t>(reg));
  this->spi_->transfer_byte(value);
  this->spi_->disable();
}

uint8_t CC1101::read_status(Status status) {
  this->spi_->enable();
  this->spi_->transfer_byte(static_cast<uint8_t>(status) | READ_BURST);
  uint8_t value = this->spi_->transfer_byte(0x00);
  this->spi_->disable();
  return value;
}

void CC1101::read_burst(uint8_t address, uint8_t *buffer, size_t length) {
  this->spi_->enable();
  this->spi_->transfer_byte(address | READ_BURST);
  for (size_t i = 0; i < length; i++)
    buffer[i] = this->spi_->transfer_byte(0x00);
  this->spi_->disable();
}

uint8_t CC1101::send_strobe(Strobe strobe) {
  this->spi_->enable();
  uint8_t status = this->spi_->transfer_byte(static_cast<uint8_t>(strobe));
  this->spi_->disable();
  return status;
}

void CC1101::read_rx_fifo(uint8_t *buffer, size_t length) {
  this->read_burst(CC1101_FIFO, buffer, length);
}

uint8_t CC1101::rx_bytes(bool *overflow) {
  uint8_t first = this->read_status(Status::RXBYTES);
  uint8_t second = this->read_status(Status::RXBYTES);
  // Errata: keep sampling until two consecutive reads agree.
  for (int i = 0; i < 8 && first != second; i++) {
    first = second;
    second = this->read_status(Status::RXBYTES);
  }
  if (overflow != nullptr)
    *overflow = (second & 0x80) != 0;
  return second & 0x7F;
}

bool CC1101::reset() {
  this->send_strobe(Strobe::SRES);
  delay(5);

  uint8_t partnum = this->read_status(Status::PARTNUM);
  uint8_t version = this->read_status(Status::VERSION);
  ESP_LOGD(TAG, "PARTNUM=0x%02X VERSION=0x%02X", partnum, version);

  // A missing / mis-wired CC1101 reads back as all-zeros or all-ones.
  if (version == 0x00 || version == 0xFF) {
    ESP_LOGE(TAG, "CC1101 not responding - check SPI wiring and CS pin");
    return false;
  }
  return true;
}

void CC1101::set_frequency(float freq_mhz) {
  // FREQ = f_carrier * 2^16 / f_xosc, with a 26 MHz crystal.
  uint32_t word = static_cast<uint32_t>((freq_mhz * 65536.0f) / 26.0f + 0.5f);
  this->write_register(Reg::FREQ2, (word >> 16) & 0xFF);
  this->write_register(Reg::FREQ1, (word >> 8) & 0xFF);
  this->write_register(Reg::FREQ0, word & 0xFF);
  ESP_LOGD(TAG, "Carrier set to %.3f MHz (FREQ=0x%06X)", freq_mhz,
           static_cast<unsigned>(word));
}

}  // namespace flowis_radio
}  // namespace esphome
