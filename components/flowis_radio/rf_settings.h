#pragma once

#include "cc1101.h"
#include <array>
#include <utility>

namespace esphome {
namespace flowis_radio {

/// CC1101 register set for Flowis.
///
/// Flowis is *not* wM-Bus.  The physical layer is plain 2-FSK PCM at 100 kbit/s
/// with no line coding, and the packet is introduced by the 32-bit sync word
/// D391 D391 - i.e. the CC110x "sync word repeated twice" arrangement, which
/// means the meters themselves are built around a CC110x-class transceiver.
///
/// Differences from a wM-Bus T/C configuration:
///   * SYNC1/SYNC0 = D3/91 instead of 54/3D
///   * MDMCFG2 = 0x03 -> SYNC_MODE 30/32 (both halves of D391D391 checked)
///   * exact 100.0 kbit/s data rate (10 us nominal bit period)
///   * fixed-length packet mode with PKTLEN = 62.  Flowis puts its own length
///     byte first and excludes the CRC from it, so the CC1101's variable-length
///     mode would truncate the frame two bytes early.  Instead the radio always
///     captures 62 bytes plus the 2 appended status bytes - exactly one full
///     64-byte FIFO - and the driver finds the frame boundary itself.  Real
///     telegrams are under 32 bytes, so the tail is simply noise, and because
///     the radio drops to IDLE when the FIFO is full the receive path can never
///     overflow no matter how long the main loop is blocked.
constexpr std::array<std::pair<Reg, uint8_t>, 40> FLOWIS_RF_SETTINGS = {{
    {Reg::IOCFG2, 0x06},    // GDO2: asserts on sync word, de-asserts at EOP
    {Reg::IOCFG1, 0x2E},    // GDO1: high impedance (shared with MISO)
    {Reg::IOCFG0, 0x00},    // GDO0: RX FIFO threshold reached
    {Reg::FIFOTHR, 0x0F},   // threshold = 64 bytes, i.e. "whole packet ready"
    {Reg::SYNC1, 0xD3},     // Flowis sync word, high byte
    {Reg::SYNC0, 0x91},     // Flowis sync word, low byte
    {Reg::PKTLEN, 0x3E},    // 62 payload bytes + 2 status bytes = one full FIFO
    {Reg::PKTCTRL1, 0x04},  // append RSSI/LQI status bytes, no address check
    {Reg::PKTCTRL0, 0x00},  // no whitening, no CRC, fixed length
    {Reg::ADDR, 0x00},
    {Reg::CHANNR, 0x00},
    {Reg::FSCTRL1, 0x08},
    {Reg::FSCTRL0, 0x00},
    {Reg::FREQ2, 0x21},  // overwritten by set_frequency()
    {Reg::FREQ1, 0x62},
    {Reg::FREQ0, 0x76},
    {Reg::MDMCFG4, 0x5B},  // RX filter 325 kHz, DRATE_E = 11
    {Reg::MDMCFG3, 0xF8},  // DRATE_M = 248  -> 99.98 kbit/s
    {Reg::MDMCFG2, 0x03},  // 2-FSK, no Manchester, 30/32 sync bits
    {Reg::MDMCFG1, 0x22},
    {Reg::MDMCFG0, 0xF8},
    {Reg::DEVIATN, 0x44},  // ~38 kHz
    {Reg::MCSM2, 0x07},
    {Reg::MCSM1, 0x00},  // return to IDLE after a packet
    {Reg::MCSM0, 0x18},  // auto-calibrate on IDLE -> RX
    {Reg::FOCCFG, 0x2E},
    {Reg::BSCFG, 0xBF},
    {Reg::AGCCTRL2, 0x43},
    {Reg::AGCCTRL1, 0x09},
    {Reg::AGCCTRL0, 0xB5},
    {Reg::FREND1, 0xB6},
    {Reg::FREND0, 0x10},
    {Reg::FSCAL3, 0xEA},
    {Reg::FSCAL2, 0x2A},
    {Reg::FSCAL1, 0x00},
    {Reg::FSCAL0, 0x1F},
    {Reg::TEST2, 0x81},
    {Reg::TEST1, 0x35},
    {Reg::TEST0, 0x09},
    {Reg::PTEST, 0x7F},
}};

inline void apply_flowis_rf_settings(CC1101 &radio) {
  for (const auto &[reg, value] : FLOWIS_RF_SETTINGS)
    radio.write_register(reg, value);
}

}  // namespace flowis_radio
}  // namespace esphome
