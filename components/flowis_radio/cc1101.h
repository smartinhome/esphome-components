#pragma once

#include "esphome/components/spi/spi.h"
#include <cstdint>

#ifndef FLOWIS_RADIO_SPI_DATA_RATE
#define FLOWIS_RADIO_SPI_DATA_RATE spi::DATA_RATE_2MHZ
#endif

namespace esphome {
namespace flowis_radio {

enum class Reg : uint8_t {
  IOCFG2 = 0x00,
  IOCFG1 = 0x01,
  IOCFG0 = 0x02,
  FIFOTHR = 0x03,
  SYNC1 = 0x04,
  SYNC0 = 0x05,
  PKTLEN = 0x06,
  PKTCTRL1 = 0x07,
  PKTCTRL0 = 0x08,
  ADDR = 0x09,
  CHANNR = 0x0A,
  FSCTRL1 = 0x0B,
  FSCTRL0 = 0x0C,
  FREQ2 = 0x0D,
  FREQ1 = 0x0E,
  FREQ0 = 0x0F,
  MDMCFG4 = 0x10,
  MDMCFG3 = 0x11,
  MDMCFG2 = 0x12,
  MDMCFG1 = 0x13,
  MDMCFG0 = 0x14,
  DEVIATN = 0x15,
  MCSM2 = 0x16,
  MCSM1 = 0x17,
  MCSM0 = 0x18,
  FOCCFG = 0x19,
  BSCFG = 0x1A,
  AGCCTRL2 = 0x1B,
  AGCCTRL1 = 0x1C,
  AGCCTRL0 = 0x1D,
  WOREVT1 = 0x1E,
  WOREVT0 = 0x1F,
  WORCTRL = 0x20,
  FREND1 = 0x21,
  FREND0 = 0x22,
  FSCAL3 = 0x23,
  FSCAL2 = 0x24,
  FSCAL1 = 0x25,
  FSCAL0 = 0x26,
  RCCTRL1 = 0x27,
  RCCTRL0 = 0x28,
  FSTEST = 0x29,
  PTEST = 0x2A,
  AGCTEST = 0x2B,
  TEST2 = 0x2C,
  TEST1 = 0x2D,
  TEST0 = 0x2E,
};

enum class Status : uint8_t {
  PARTNUM = 0x30,
  VERSION = 0x31,
  FREQEST = 0x32,
  LQI = 0x33,
  RSSI = 0x34,
  MARCSTATE = 0x35,
  PKTSTATUS = 0x38,
  RXBYTES = 0x3B,
};

enum class Strobe : uint8_t {
  SRES = 0x30,
  SCAL = 0x33,
  SRX = 0x34,
  SIDLE = 0x36,
  SFRX = 0x3A,
  SNOP = 0x3D,
};

enum class MarcState : uint8_t {
  IDLE = 0x01,
  RX = 0x0D,
  RX_OVERFLOW = 0x11,
};

constexpr uint8_t CC1101_FIFO = 0x3F;
constexpr uint8_t READ_SINGLE = 0x80;
constexpr uint8_t READ_BURST = 0xC0;
constexpr uint8_t WRITE_BURST = 0x40;

using FlowisSPIDevice =
    spi::SPIDevice<spi::BIT_ORDER_MSB_FIRST, spi::CLOCK_POLARITY_LOW,
                   spi::CLOCK_PHASE_LEADING, FLOWIS_RADIO_SPI_DATA_RATE>;

/// Thin register-level wrapper around the CC1101 over SPI.
///
/// Deliberately self-contained: the Flowis link layer has nothing in common
/// with wM-Bus, so this component must build without pulling in wmbus_common
/// and its ~100 meter drivers.
class CC1101 {
 public:
  explicit CC1101(FlowisSPIDevice *spi) : spi_(spi) {}

  uint8_t read_register(Reg reg);
  void write_register(Reg reg, uint8_t value);
  uint8_t read_status(Status status);
  void read_burst(uint8_t address, uint8_t *buffer, size_t length);
  uint8_t send_strobe(Strobe strobe);
  void read_rx_fifo(uint8_t *buffer, size_t length);

  /// Number of bytes waiting in the RX FIFO.  Applies the CC1101 errata
  /// workaround: RXBYTES must be read twice with the same value before it can
  /// be trusted, because the counter can be sampled mid-update.
  uint8_t rx_bytes(bool *overflow = nullptr);

  bool reset();
  void set_frequency(float freq_mhz);

 private:
  FlowisSPIDevice *spi_;
};

}  // namespace flowis_radio
}  // namespace esphome
