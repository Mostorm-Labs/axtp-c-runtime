#ifndef AXTP_MODEL_H
#define AXTP_MODEL_H

#include <stddef.h>
#include <stdint.h>

#include "axtp/status.h"
#include "generated/axtp_ids_generated.h"

#ifdef __cplusplus
extern "C" {
#endif

#define AXTP_STANDARD_MAGIC0 0x41u
#define AXTP_STANDARD_MAGIC1 0x58u
#define AXTP_VERSION_1 0x01u
#define AXTP_STANDARD_FRAME_HEADER_SIZE 12u
#define AXTP_STANDARD_FRAME_CRC_SIZE 2u
#define AXTP_BINARY_RPC_HEADER_SIZE 11u
#define AXTP_QUEUE_CAPACITY 16u

#define AXTP_SOURCE_PROTOCOL_AXTP_V1 0x01u
#define AXTP_SOURCE_PROTOCOL_JSON_RPC 0x02u

typedef struct {
  uint8_t source_protocol;
  uint32_t session_id;
  uint32_t request_id;
} axtp_payload_meta_t;

typedef struct {
  uint8_t encoding;
  uint8_t op;
  uint32_t request_id;
  uint16_t method_or_event_id;
  uint16_t status_code;
  uint8_t body_encoding;
  axtp_payload_meta_t meta;
  uint8_t* body;
  size_t body_len;
} axtp_rpc_payload_t;

typedef struct {
  uint8_t version;
  uint8_t payload_type;
  uint16_t payload_length;
  uint8_t source_id;
  uint8_t destination_id;
  uint16_t message_id;
  uint8_t frame_index;
  uint8_t frame_count;
} axtp_frame_header_t;

typedef struct {
  axtp_frame_header_t header;
  uint8_t* payload;
  size_t payload_len;
  uint16_t crc16;
} axtp_frame_t;

void axtp_rpc_payload_init(axtp_rpc_payload_t* payload);
void axtp_rpc_payload_free(axtp_rpc_payload_t* payload);
axtp_status_t axtp_rpc_payload_set_body(axtp_rpc_payload_t* payload, const uint8_t* body, size_t body_len);
axtp_status_t axtp_rpc_payload_copy(axtp_rpc_payload_t* dst, const axtp_rpc_payload_t* src);
void axtp_frame_init(axtp_frame_t* frame);
void axtp_frame_free(axtp_frame_t* frame);

#ifdef __cplusplus
}
#endif

#endif
