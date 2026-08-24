#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace esphome {
namespace flowis_radio {

/// Number of consumption periods carried by a type-2 (history) telegram.
static constexpr size_t FLOWIS_HISTORY_SLOTS = 7;

/// A CRC-validated Flowis telegram, decoded as far as the format is understood.
///
/// Frame layout.  Offsets are into `raw`, which keeps the length byte as its
/// first element so that every offset below matches the on-air byte order:
///
///   [0]      L  - payload length, excluding this byte and the two CRC bytes
///   [1]      Y  - telegram type: 1 = current reading, 2 = consumption history
///   [2..5]   I  - meter id, little endian
///   ...      type-specific payload
///   [L+1..]  C  - CRC-16, poly 0x8005, init 0xFFFF, big endian, over [0..L]
///
/// Type 1 (current reading):
///   [6]      flags; bit 0x40 set when the archive block is present
///   [7..10]  device clock, bit-packed little endian
///   [11..13] volume in litres, little endian
///   [14]     backflow counter
///   [15]     alarm bits
///   [18]     0xA5 end-of-block marker
///   [L-5..]  optional archive block, ending in a 32-bit litre total
///
/// Type 2 (consumption history):
///   [6..19]  seven 16-bit little-endian litre counts.  Slot 0 is the volume
///            accumulated since the archive snapshot; slots 1..6 are the six
///            preceding periods, newest first.
struct FlowisTelegram {
  std::vector<uint8_t> raw;
  int8_t rssi{0};

  uint32_t meter_id{0};
  uint8_t type{0};

  // --- type 1 -------------------------------------------------------------
  bool has_reading{false};
  uint32_t volume_l{0};
  uint8_t backflow{0};
  uint8_t alarm{0};

  bool has_clock{false};
  uint16_t clock_year{0};
  uint8_t clock_month{0}, clock_day{0};
  uint8_t clock_hour{0}, clock_minute{0}, clock_second{0};
  bool clock_dst{false};

  bool has_archive{false};
  uint32_t archive_l{0};

  // --- type 2 -------------------------------------------------------------
  bool has_history{false};
  uint16_t history_l[FLOWIS_HISTORY_SLOTS]{};

  float volume_m3() const { return this->volume_l / 1000.0f; }
  float archive_m3() const { return this->archive_l / 1000.0f; }

  /// "2026-08-14T14:25:31", or an empty string when no clock was decoded.
  std::string clock_to_string() const;
  std::string to_hex() const;
};

/// CRC-16 with polynomial 0x8005 and initial value 0xFFFF, MSB first.
uint16_t flowis_crc16(const uint8_t *data, size_t length);

/// Validate and decode a raw frame (length byte first, CRC last).
/// Returns false when the frame is truncated or the CRC does not match.
bool flowis_parse(const uint8_t *data, size_t length, FlowisTelegram *out);

}  // namespace flowis_radio
}  // namespace esphome
