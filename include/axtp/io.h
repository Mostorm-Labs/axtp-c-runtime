#ifndef AXTP_IO_H
#define AXTP_IO_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

uint16_t axtp_crc16_ccitt_false(const uint8_t* data, size_t len);
void axtp_write_u16_be(uint8_t* out, uint16_t value);
void axtp_write_u32_be(uint8_t* out, uint32_t value);
uint16_t axtp_read_u16_be(const uint8_t* data);
uint32_t axtp_read_u32_be(const uint8_t* data);

#ifdef __cplusplus
}
#endif

#endif
