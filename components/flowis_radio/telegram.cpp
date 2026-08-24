#include "telegram.h"
#include "esphome/core/log.h"
#include <cstdio>

namespace esphome {
namespace flowis_radio {

static const char *const TAG = "flowis_radio.telegram";

uint16_t flowis_crc16(const uint8_t *data, size_t length) {
  uint16_t crc = 0xFFFF;
  for (size_t i = 0; i < length; i++) {
    crc ^= static_cast<uint16_t>(data[i]) << 8;
    for (uint8_t bit = 0; bit < 8; bit++)
      crc = (crc & 0x8000) ? static_cast<uint16_t>((crc << 1) ^ 0x8005)
                           : static_cast<uint16_t>(crc << 1);
  }
  return crc;
}

static inline uint32_t le32(const uint8_t *p) {
  return static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8) |
         (static_cast<uint32_t>(p[2]) << 16) |
         (static_cast<uint32_t>(p[3]) << 24);
}

static inline uint32_t le24(const uint8_t *p) {
  return static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8) |
         (static_cast<uint32_t>(p[2]) << 16);
}

static inline uint16_t le16(const uint8_t *p) {
  return static_cast<uint16_t>(p[0] | (p[1] << 8));
}

/// Unpack the 32-bit device clock.
///
/// Bit layout, counting from the LSB of the little-endian word:
///   [0:5]   second      [6:11]  minute     [12:16] hour
///   [17:21] day         [22:25] month      [26:30] year (offset 2000)
///   [31]    summer-time flag
///
/// Note the split at the top of the word: bit 31 is a summer-time flag, not
/// the high bit of the year.  Reading the year as six bits appears to work on
/// winter captures and then puts summer frames exactly 32 years in the future,
/// which is an easy trap to fall into when only cold-season samples are at
/// hand.
static void decode_clock(uint32_t word, FlowisTelegram *t) {
  t->clock_second = word & 0x3F;
  t->clock_minute = (word >> 6) & 0x3F;
  t->clock_hour = (word >> 12) & 0x1F;
  t->clock_day = (word >> 17) & 0x1F;
  t->clock_month = (word >> 22) & 0x0F;
  t->clock_year = 2000 + ((word >> 26) & 0x1F);
  t->clock_dst = (word & 0x80000000UL) != 0;

  t->has_clock = t->clock_month >= 1 && t->clock_month <= 12 &&
                 t->clock_day >= 1 && t->clock_day <= 31 &&
                 t->clock_hour <= 23 && t->clock_minute <= 59 &&
                 t->clock_second <= 59;
}

std::string FlowisTelegram::clock_to_string() const {
  if (!this->has_clock)
    return {};
  char buffer[32];
  snprintf(buffer, sizeof(buffer), "%04u-%02u-%02uT%02u:%02u:%02u",
           this->clock_year, this->clock_month, this->clock_day,
           this->clock_hour, this->clock_minute, this->clock_second);
  return std::string(buffer);
}

std::string FlowisTelegram::to_hex() const {
  std::string out;
  out.reserve(this->raw.size() * 2);
  char buffer[3];
  for (uint8_t byte : this->raw) {
    snprintf(buffer, sizeof(buffer), "%02X", byte);
    out += buffer;
  }
  return out;
}

bool flowis_parse(const uint8_t *data, size_t length, FlowisTelegram *out) {
  if (length < 4)
    return false;

  const uint8_t l_field = data[0];
  const size_t total = static_cast<size_t>(l_field) + 3;  // L + payload + CRC
  if (total > length)
    return false;

  const uint16_t expected = flowis_crc16(data, l_field + 1);
  const uint16_t actual =
      static_cast<uint16_t>((data[l_field + 1] << 8) | data[l_field + 2]);
  if (expected != actual) {
    ESP_LOGV(TAG, "CRC mismatch: got %04X, expected %04X", actual, expected);
    return false;
  }

  out->raw.assign(data, data + total);
  out->type = data[1];
  out->meter_id = le32(&data[2]);

  if (out->type == 0x01 && l_field >= 18) {
    decode_clock(le32(&data[7]), out);
    out->volume_l = le24(&data[11]);
    out->backflow = data[14];
    out->alarm = data[15];
    out->has_reading = true;

    // The archive block, when present, always ends the payload: the last four
    // bytes are a little-endian litre total.  Its presence is flagged by bit
    // 0x40 of the byte at offset 6, which is only ever set on frames long
    // enough to carry it.
    if ((data[6] & 0x40) != 0 && l_field >= 25) {
      out->archive_l = le32(&data[l_field - 3]);
      out->has_archive = true;
    }
  } else if (out->type == 0x02 && l_field >= 19) {
    for (size_t i = 0; i < FLOWIS_HISTORY_SLOTS; i++)
      out->history_l[i] = le16(&data[6 + i * 2]);
    out->has_history = true;
  }

  return true;
}

}  // namespace flowis_radio
}  // namespace esphome
