#include "axtp/model.h"

#include <stdlib.h>
#include <string.h>

void axtp_rpc_payload_init(axtp_rpc_payload_t* payload) {
  if (payload == NULL) {
    return;
  }
  memset(payload, 0, sizeof(*payload));
  payload->encoding = AXTP_RPC_ENCODING_JSON;
  payload->op = AXTP_RPC_OP_REQUEST;
  payload->status_code = AXTP_ERROR_CODE_SUCCESS;
  payload->body_encoding = AXTP_RPC_BODY_ENCODING_NONE;
  payload->meta.source_protocol = AXTP_SOURCE_PROTOCOL_AXTP_V1;
}

void axtp_rpc_payload_free(axtp_rpc_payload_t* payload) {
  if (payload == NULL) {
    return;
  }
  free(payload->body);
  axtp_rpc_payload_init(payload);
}

axtp_status_t axtp_rpc_payload_set_body(axtp_rpc_payload_t* payload, const uint8_t* body, size_t body_len) {
  if (payload == NULL || (body == NULL && body_len > 0)) {
    return AXTP_STATUS_INVALID_ARGUMENT;
  }
  uint8_t* copy = NULL;
  if (body_len > 0) {
    copy = (uint8_t*)malloc(body_len);
    if (copy == NULL) {
      return AXTP_STATUS_NO_MEMORY;
    }
    memcpy(copy, body, body_len);
  }
  free(payload->body);
  payload->body = copy;
  payload->body_len = body_len;
  return AXTP_STATUS_OK;
}

axtp_status_t axtp_rpc_payload_copy(axtp_rpc_payload_t* dst, const axtp_rpc_payload_t* src) {
  if (dst == NULL || src == NULL) {
    return AXTP_STATUS_INVALID_ARGUMENT;
  }
  axtp_rpc_payload_init(dst);
  *dst = *src;
  dst->body = NULL;
  dst->body_len = 0;
  return axtp_rpc_payload_set_body(dst, src->body, src->body_len);
}

void axtp_stream_payload_init(axtp_stream_payload_t* payload) {
  if (payload == NULL) {
    return;
  }
  memset(payload, 0, sizeof(*payload));
  payload->meta.source_protocol = AXTP_SOURCE_PROTOCOL_AXTP_V1;
}

void axtp_stream_payload_free(axtp_stream_payload_t* payload) {
  if (payload == NULL) {
    return;
  }
  free(payload->data);
  axtp_stream_payload_init(payload);
}

axtp_status_t axtp_stream_payload_set_data(axtp_stream_payload_t* payload, const uint8_t* data, size_t data_len) {
  if (payload == NULL) {
    return AXTP_STATUS_INVALID_ARGUMENT;
  }
  uint8_t* copy = NULL;
  if (data_len > 0) {
    copy = (uint8_t*)malloc(data_len);
    if (copy == NULL) {
      return AXTP_STATUS_NO_MEMORY;
    }
    if (data != NULL) {
      memcpy(copy, data, data_len);
    } else {
      memset(copy, 0, data_len);
    }
  }
  free(payload->data);
  payload->data = copy;
  payload->data_len = data_len;
  return AXTP_STATUS_OK;
}

axtp_status_t axtp_stream_payload_copy(axtp_stream_payload_t* dst, const axtp_stream_payload_t* src) {
  if (dst == NULL || src == NULL) {
    return AXTP_STATUS_INVALID_ARGUMENT;
  }
  axtp_stream_payload_init(dst);
  *dst = *src;
  dst->data = NULL;
  dst->data_len = 0;
  return axtp_stream_payload_set_data(dst, src->data, src->data_len);
}

void axtp_frame_init(axtp_frame_t* frame) {
  if (frame == NULL) {
    return;
  }
  memset(frame, 0, sizeof(*frame));
}

void axtp_frame_free(axtp_frame_t* frame) {
  if (frame == NULL) {
    return;
  }
  free(frame->payload);
  axtp_frame_init(frame);
}
