#include "axtp/core.h"

#include <stdlib.h>
#include <string.h>

#include "axtp/codec.h"

struct axtp_core {
  axtp_core_event_t events[AXTP_QUEUE_CAPACITY];
  size_t event_count;
  axtp_rpc_payload_t responses[AXTP_QUEUE_CAPACITY];
  size_t response_count;
  uint32_t pending[AXTP_QUEUE_CAPACITY];
  size_t pending_count;
  uint8_t* outbound[AXTP_QUEUE_CAPACITY];
  size_t outbound_len[AXTP_QUEUE_CAPACITY];
  size_t outbound_count;
  uint16_t next_message_id;
  axtp_transport_profile_t profile;
};

static axtp_status_t push_outbound(axtp_core_t* core, uint8_t* data, size_t len) {
  if (core->outbound_count >= AXTP_QUEUE_CAPACITY) {
    free(data);
    return AXTP_STATUS_QUEUE_FULL;
  }
  core->outbound[core->outbound_count] = data;
  core->outbound_len[core->outbound_count] = len;
  core->outbound_count++;
  return AXTP_STATUS_OK;
}

static uint16_t next_message_id(axtp_core_t* core) {
  uint16_t value = core->next_message_id;
  core->next_message_id++;
  if (core->next_message_id == 0) {
    core->next_message_id = 1;
  }
  return value;
}

static int is_pending(axtp_core_t* core, uint32_t request_id) {
  for (size_t i = 0; i < core->pending_count; ++i) {
    if (core->pending[i] == request_id) {
      return 1;
    }
  }
  return 0;
}

static void remove_pending(axtp_core_t* core, uint32_t request_id) {
  for (size_t i = 0; i < core->pending_count; ++i) {
    if (core->pending[i] == request_id) {
      memmove(&core->pending[i], &core->pending[i + 1], sizeof(core->pending[0]) * (core->pending_count - i - 1));
      core->pending_count--;
      return;
    }
  }
}

axtp_core_t* axtp_core_new(void) {
  axtp_core_t* core = (axtp_core_t*)calloc(1, sizeof(axtp_core_t));
  if (core != NULL) {
    core->next_message_id = 1;
    core->profile = axtp_default_transport_profile();
  }
  return core;
}

void axtp_core_free(axtp_core_t* core) {
  if (core == NULL) {
    return;
  }
  for (size_t i = 0; i < core->event_count; ++i) {
    axtp_rpc_payload_free(&core->events[i].rpc);
  }
  for (size_t i = 0; i < core->response_count; ++i) {
    axtp_rpc_payload_free(&core->responses[i]);
  }
  for (size_t i = 0; i < core->outbound_count; ++i) {
    free(core->outbound[i]);
  }
  free(core);
}

void axtp_core_configure(axtp_core_t* core, axtp_transport_profile_t profile) {
  if (core != NULL) {
    core->profile = profile;
  }
}

axtp_status_t axtp_core_on_bytes(axtp_core_t* core, const uint8_t* data, size_t len) {
  if (core == NULL || data == NULL) {
    return AXTP_STATUS_INVALID_ARGUMENT;
  }
  axtp_frame_t frame;
  if (axtp_decode_frame(data, len, &frame) != AXTP_STATUS_OK) {
    return AXTP_STATUS_DECODE_ERROR;
  }
  if (frame.header.payload_type != AXTP_PAYLOAD_TYPE_RPC) {
    axtp_frame_free(&frame);
    return AXTP_STATUS_OK;
  }
  axtp_rpc_payload_t rpc;
  axtp_status_t status = axtp_decode_rpc_payload(frame.payload, frame.payload_len, &rpc);
  axtp_frame_free(&frame);
  if (status != AXTP_STATUS_OK) {
    return status;
  }
  if (rpc.op == AXTP_RPC_OP_REQUEST) {
    if (core->event_count >= AXTP_QUEUE_CAPACITY) {
      axtp_rpc_payload_free(&rpc);
      return AXTP_STATUS_QUEUE_FULL;
    }
    core->events[core->event_count].type = AXTP_CORE_EVENT_RPC_REQUEST;
    core->events[core->event_count].rpc = rpc;
    core->event_count++;
    return AXTP_STATUS_OK;
  }
  if (rpc.op == AXTP_RPC_OP_REQUEST_RESPONSE) {
    if (core->response_count >= AXTP_QUEUE_CAPACITY) {
      axtp_rpc_payload_free(&rpc);
      return AXTP_STATUS_QUEUE_FULL;
    }
    if (is_pending(core, rpc.request_id)) {
      remove_pending(core, rpc.request_id);
    }
    core->responses[core->response_count++] = rpc;
    return AXTP_STATUS_OK;
  }
  axtp_rpc_payload_free(&rpc);
  return AXTP_STATUS_OK;
}

axtp_status_t axtp_core_poll_event(axtp_core_t* core, axtp_core_event_t* out) {
  if (core == NULL || out == NULL) {
    return AXTP_STATUS_INVALID_ARGUMENT;
  }
  if (core->event_count == 0) {
    return AXTP_STATUS_NOT_FOUND;
  }
  *out = core->events[0];
  memmove(&core->events[0], &core->events[1], sizeof(core->events[0]) * (core->event_count - 1));
  core->event_count--;
  return AXTP_STATUS_OK;
}

axtp_status_t axtp_core_expect_rpc_response(axtp_core_t* core, uint32_t request_id) {
  if (core == NULL) {
    return AXTP_STATUS_INVALID_ARGUMENT;
  }
  if (core->pending_count >= AXTP_QUEUE_CAPACITY) {
    return AXTP_STATUS_QUEUE_FULL;
  }
  core->pending[core->pending_count++] = request_id;
  return AXTP_STATUS_OK;
}

static axtp_status_t send_rpc(axtp_core_t* core, const axtp_rpc_payload_t* payload) {
  uint8_t* rpc = NULL;
  size_t rpc_len = 0;
  axtp_status_t status = axtp_encode_rpc_payload(payload, &rpc, &rpc_len);
  if (status != AXTP_STATUS_OK) {
    return status;
  }
  uint8_t* frame = NULL;
  size_t frame_len = 0;
  status = axtp_encode_frame(AXTP_PAYLOAD_TYPE_RPC, rpc, rpc_len, next_message_id(core), &frame, &frame_len);
  free(rpc);
  if (status != AXTP_STATUS_OK) {
    return status;
  }
  return push_outbound(core, frame, frame_len);
}

axtp_status_t axtp_core_send_rpc_request(axtp_core_t* core, const axtp_rpc_payload_t* payload) {
  if (core == NULL || payload == NULL) {
    return AXTP_STATUS_INVALID_ARGUMENT;
  }
  axtp_rpc_payload_t copy;
  axtp_rpc_payload_copy(&copy, payload);
  copy.op = AXTP_RPC_OP_REQUEST;
  axtp_status_t status = send_rpc(core, &copy);
  axtp_rpc_payload_free(&copy);
  return status;
}

axtp_status_t axtp_core_send_rpc_response(axtp_core_t* core, const axtp_rpc_payload_t* payload) {
  if (core == NULL || payload == NULL) {
    return AXTP_STATUS_INVALID_ARGUMENT;
  }
  axtp_rpc_payload_t copy;
  axtp_rpc_payload_copy(&copy, payload);
  copy.op = AXTP_RPC_OP_REQUEST_RESPONSE;
  axtp_status_t status = send_rpc(core, &copy);
  axtp_rpc_payload_free(&copy);
  return status;
}

axtp_status_t axtp_core_try_take_rpc_response(axtp_core_t* core, uint32_t request_id, axtp_rpc_payload_t* out) {
  if (core == NULL || out == NULL) {
    return AXTP_STATUS_INVALID_ARGUMENT;
  }
  for (size_t i = 0; i < core->response_count; ++i) {
    if (core->responses[i].request_id == request_id) {
      axtp_status_t status = axtp_rpc_payload_copy(out, &core->responses[i]);
      axtp_rpc_payload_free(&core->responses[i]);
      memmove(&core->responses[i], &core->responses[i + 1], sizeof(core->responses[0]) * (core->response_count - i - 1));
      core->response_count--;
      return status;
    }
  }
  return AXTP_STATUS_NOT_FOUND;
}

axtp_status_t axtp_core_try_pop_outbound_bytes(axtp_core_t* core, uint8_t** out, size_t* out_len) {
  if (core == NULL || out == NULL || out_len == NULL) {
    return AXTP_STATUS_INVALID_ARGUMENT;
  }
  if (core->outbound_count == 0) {
    return AXTP_STATUS_NOT_FOUND;
  }
  *out = core->outbound[0];
  *out_len = core->outbound_len[0];
  memmove(&core->outbound[0], &core->outbound[1], sizeof(core->outbound[0]) * (core->outbound_count - 1));
  memmove(&core->outbound_len[0], &core->outbound_len[1], sizeof(core->outbound_len[0]) * (core->outbound_count - 1));
  core->outbound_count--;
  return AXTP_STATUS_OK;
}
