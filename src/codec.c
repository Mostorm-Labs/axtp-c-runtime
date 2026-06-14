#include "axtp/codec.h"

#include <stdlib.h>
#include <string.h>

#include "axtp/io.h"

void axtp_free_bytes(uint8_t* data) {
  free(data);
}

axtp_status_t axtp_encode_rpc_payload(const axtp_rpc_payload_t* payload, uint8_t** out, size_t* out_len) {
  if (payload == NULL || out == NULL || out_len == NULL) {
    return AXTP_STATUS_INVALID_ARGUMENT;
  }
  const size_t len = AXTP_BINARY_RPC_HEADER_SIZE + payload->body_len;
  uint8_t* bytes = (uint8_t*)malloc(len == 0 ? 1 : len);
  if (bytes == NULL) {
    return AXTP_STATUS_NO_MEMORY;
  }
  bytes[0] = payload->encoding;
  bytes[1] = payload->op;
  axtp_write_u32_be(bytes + 2, payload->request_id);
  axtp_write_u16_be(bytes + 6, payload->method_or_event_id);
  axtp_write_u16_be(bytes + 8, payload->status_code);
  bytes[10] = payload->body_encoding;
  if (payload->body_len > 0) {
    memcpy(bytes + AXTP_BINARY_RPC_HEADER_SIZE, payload->body, payload->body_len);
  }
  *out = bytes;
  *out_len = len;
  return AXTP_STATUS_OK;
}

axtp_status_t axtp_decode_rpc_payload(const uint8_t* data, size_t len, axtp_rpc_payload_t* out) {
  if (data == NULL || out == NULL || len < AXTP_BINARY_RPC_HEADER_SIZE) {
    return AXTP_STATUS_INVALID_ARGUMENT;
  }
  axtp_rpc_payload_init(out);
  out->encoding = data[0];
  out->op = data[1];
  out->request_id = axtp_read_u32_be(data + 2);
  out->method_or_event_id = axtp_read_u16_be(data + 6);
  out->status_code = axtp_read_u16_be(data + 8);
  out->body_encoding = data[10];
  out->meta.source_protocol = AXTP_SOURCE_PROTOCOL_AXTP_V1;
  out->meta.request_id = out->request_id;
  return axtp_rpc_payload_set_body(out, data + AXTP_BINARY_RPC_HEADER_SIZE, len - AXTP_BINARY_RPC_HEADER_SIZE);
}

axtp_status_t axtp_encode_frame(uint8_t payload_type, const uint8_t* payload, size_t payload_len, uint16_t message_id, uint8_t** out, size_t* out_len) {
  if ((payload == NULL && payload_len > 0) || out == NULL || out_len == NULL || payload_len > 0xFFFFu) {
    return AXTP_STATUS_INVALID_ARGUMENT;
  }
  const size_t len = AXTP_STANDARD_FRAME_HEADER_SIZE + payload_len + AXTP_STANDARD_FRAME_CRC_SIZE;
  uint8_t* bytes = (uint8_t*)malloc(len);
  if (bytes == NULL) {
    return AXTP_STATUS_NO_MEMORY;
  }
  bytes[0] = AXTP_STANDARD_MAGIC0;
  bytes[1] = AXTP_STANDARD_MAGIC1;
  bytes[2] = AXTP_VERSION_1;
  bytes[3] = payload_type;
  axtp_write_u16_be(bytes + 4, (uint16_t)payload_len);
  bytes[6] = 0;
  bytes[7] = 0;
  axtp_write_u16_be(bytes + 8, message_id == 0 ? 1u : message_id);
  bytes[10] = 0;
  bytes[11] = 1;
  if (payload_len > 0) {
    memcpy(bytes + AXTP_STANDARD_FRAME_HEADER_SIZE, payload, payload_len);
  }
  axtp_write_u16_be(bytes + AXTP_STANDARD_FRAME_HEADER_SIZE + payload_len, axtp_crc16_ccitt_false(bytes, AXTP_STANDARD_FRAME_HEADER_SIZE + payload_len));
  *out = bytes;
  *out_len = len;
  return AXTP_STATUS_OK;
}

axtp_status_t axtp_decode_frame(const uint8_t* data, size_t len, axtp_frame_t* out) {
  if (data == NULL || out == NULL || len < AXTP_STANDARD_FRAME_HEADER_SIZE + AXTP_STANDARD_FRAME_CRC_SIZE) {
    return AXTP_STATUS_INVALID_ARGUMENT;
  }
  if (data[0] != AXTP_STANDARD_MAGIC0 || data[1] != AXTP_STANDARD_MAGIC1 || data[2] != AXTP_VERSION_1) {
    return AXTP_STATUS_DECODE_ERROR;
  }
  const uint16_t payload_len = axtp_read_u16_be(data + 4);
  const size_t total = AXTP_STANDARD_FRAME_HEADER_SIZE + (size_t)payload_len + AXTP_STANDARD_FRAME_CRC_SIZE;
  if (len < total) {
    return AXTP_STATUS_DECODE_ERROR;
  }
  const uint16_t expected_crc = axtp_read_u16_be(data + total - AXTP_STANDARD_FRAME_CRC_SIZE);
  const uint16_t actual_crc = axtp_crc16_ccitt_false(data, total - AXTP_STANDARD_FRAME_CRC_SIZE);
  if (expected_crc != actual_crc) {
    return AXTP_STATUS_DECODE_ERROR;
  }
  axtp_frame_init(out);
  out->payload = NULL;
  if (payload_len > 0) {
    out->payload = (uint8_t*)malloc(payload_len);
    if (out->payload == NULL) {
      return AXTP_STATUS_NO_MEMORY;
    }
    memcpy(out->payload, data + AXTP_STANDARD_FRAME_HEADER_SIZE, payload_len);
  }
  out->payload_len = payload_len;
  out->crc16 = expected_crc;
  out->header.version = data[2];
  out->header.payload_type = data[3];
  out->header.payload_length = payload_len;
  out->header.source_id = data[6];
  out->header.destination_id = data[7];
  out->header.message_id = axtp_read_u16_be(data + 8);
  out->header.frame_index = data[10];
  out->header.frame_count = data[11];
  return AXTP_STATUS_OK;
}
