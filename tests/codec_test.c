#include <assert.h>
#include <string.h>

#include "axtp/axtp.h"

int main(void) {
  assert(axtp_crc16_ccitt_false((const uint8_t*)"123456789", 9) == 0x29B1);

  axtp_rpc_payload_t payload;
  axtp_rpc_payload_init(&payload);
  payload.encoding = AXTP_RPC_ENCODING_JSON;
  payload.op = AXTP_RPC_OP_REQUEST;
  payload.request_id = 7;
  payload.method_or_event_id = AXTP_METHOD_ID_AUDIO_GET_ALGORITHM_CONFIG;
  payload.body_encoding = AXTP_RPC_BODY_ENCODING_NONE;
  assert(axtp_rpc_payload_set_body(&payload, (const uint8_t*)"{}", 2) == AXTP_STATUS_OK);

  uint8_t* rpc_bytes = NULL;
  size_t rpc_len = 0;
  assert(axtp_encode_rpc_payload(&payload, &rpc_bytes, &rpc_len) == AXTP_STATUS_OK);
  assert(rpc_len == AXTP_BINARY_RPC_HEADER_SIZE + 2);
  assert(rpc_bytes[2] == 0x00);
  assert(rpc_bytes[3] == 0x00);
  assert(rpc_bytes[4] == 0x00);
  assert(rpc_bytes[5] == 0x07);
  assert(rpc_bytes[6] == 0x09);
  assert(rpc_bytes[7] == 0x01);

  axtp_rpc_payload_t decoded_rpc;
  assert(axtp_decode_rpc_payload(rpc_bytes, rpc_len, &decoded_rpc) == AXTP_STATUS_OK);
  assert(decoded_rpc.request_id == 7);
  assert(decoded_rpc.method_or_event_id == AXTP_METHOD_ID_AUDIO_GET_ALGORITHM_CONFIG);
  assert(decoded_rpc.body_len == 2);
  assert(memcmp(decoded_rpc.body, "{}", 2) == 0);

  uint8_t* frame_bytes = NULL;
  size_t frame_len = 0;
  assert(axtp_encode_frame(AXTP_PAYLOAD_TYPE_RPC, rpc_bytes, rpc_len, 1, &frame_bytes, &frame_len) == AXTP_STATUS_OK);
  assert(frame_len == AXTP_STANDARD_FRAME_HEADER_SIZE + rpc_len + AXTP_STANDARD_FRAME_CRC_SIZE);
  assert(frame_bytes[4] == 0x00);
  assert(frame_bytes[5] == (uint8_t)rpc_len);
  assert(frame_bytes[8] == 0x00);
  assert(frame_bytes[9] == 0x01);

  axtp_frame_t frame;
  assert(axtp_decode_frame(frame_bytes, frame_len, &frame) == AXTP_STATUS_OK);
  assert(frame.header.payload_type == AXTP_PAYLOAD_TYPE_RPC);
  assert(frame.payload_len == rpc_len);

  frame_bytes[frame_len - 1] ^= 0xFFu;
  axtp_frame_t invalid;
  assert(axtp_decode_frame(frame_bytes, frame_len, &invalid) == AXTP_STATUS_DECODE_ERROR);

  axtp_frame_free(&frame);
  axtp_rpc_payload_free(&decoded_rpc);
  axtp_free_bytes(frame_bytes);
  axtp_free_bytes(rpc_bytes);
  axtp_rpc_payload_free(&payload);
  return 0;
}
