#ifndef AXTP_CODEC_H
#define AXTP_CODEC_H

#include <stddef.h>
#include <stdint.h>

#include "axtp/model.h"

#ifdef __cplusplus
extern "C" {
#endif

axtp_status_t axtp_encode_rpc_payload(const axtp_rpc_payload_t* payload, uint8_t** out, size_t* out_len);
axtp_status_t axtp_decode_rpc_payload(const uint8_t* data, size_t len, axtp_rpc_payload_t* out);
axtp_status_t axtp_encode_frame(uint8_t payload_type, const uint8_t* payload, size_t payload_len, uint16_t message_id, uint8_t** out, size_t* out_len);
axtp_status_t axtp_decode_frame(const uint8_t* data, size_t len, axtp_frame_t* out);
void axtp_free_bytes(uint8_t* data);

#ifdef __cplusplus
}
#endif

#endif
