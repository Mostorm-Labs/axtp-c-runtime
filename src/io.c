#include "axtp/io.h"

uint16_t axtp_crc16_ccitt_false(const uint8_t* data, size_t len) {
  uint16_t crc = 0xFFFFu;
  if (data == 0 && len > 0) {
    return crc;
  }
  for (size_t i = 0; i < len; ++i) {
    crc ^= (uint16_t)data[i] << 8u;
    for (int bit = 0; bit < 8; ++bit) {
      if ((crc & 0x8000u) != 0u) {
        crc = (uint16_t)((crc << 1u) ^ 0x1021u);
      } else {
        crc = (uint16_t)(crc << 1u);
      }
    }
  }
  return crc;
}

void axtp_write_u16_be(uint8_t* out, uint16_t value) {
  out[0] = (uint8_t)((value >> 8u) & 0xFFu);
  out[1] = (uint8_t)(value & 0xFFu);
}

void axtp_write_u32_be(uint8_t* out, uint32_t value) {
  out[0] = (uint8_t)((value >> 24u) & 0xFFu);
  out[1] = (uint8_t)((value >> 16u) & 0xFFu);
  out[2] = (uint8_t)((value >> 8u) & 0xFFu);
  out[3] = (uint8_t)(value & 0xFFu);
}

uint16_t axtp_read_u16_be(const uint8_t* data) {
  return ((uint16_t)data[0] << 8u) | (uint16_t)data[1];
}

uint32_t axtp_read_u32_be(const uint8_t* data) {
  return ((uint32_t)data[0] << 24u) | ((uint32_t)data[1] << 16u) | ((uint32_t)data[2] << 8u) | (uint32_t)data[3];
}
