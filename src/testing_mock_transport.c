#include "axtp/testing/mock_transport.h"

#include <stdlib.h>
#include <string.h>

#include "axtp/codec.h"

struct axtp_mock_transport {
  axtp_byte_sink_fn sink;
  void* sink_user_data;
  uint8_t* outgoing[AXTP_QUEUE_CAPACITY];
  size_t outgoing_len[AXTP_QUEUE_CAPACITY];
  size_t outgoing_count;
  int open;
  axtp_transport_profile_t profile;
};

static void mock_bind(void* impl, axtp_byte_sink_fn sink, void* user_data) {
  axtp_mock_transport_t* transport = (axtp_mock_transport_t*)impl;
  transport->sink = sink;
  transport->sink_user_data = user_data;
}

static axtp_status_t mock_open(void* impl) {
  ((axtp_mock_transport_t*)impl)->open = 1;
  return AXTP_STATUS_OK;
}

static void mock_close(void* impl) {
  ((axtp_mock_transport_t*)impl)->open = 0;
}

static axtp_status_t mock_send_bytes(void* impl, const uint8_t* data, size_t len) {
  axtp_mock_transport_t* transport = (axtp_mock_transport_t*)impl;
  if (transport == NULL || (data == NULL && len > 0)) {
    return AXTP_STATUS_INVALID_ARGUMENT;
  }
  if (transport->outgoing_count >= AXTP_QUEUE_CAPACITY) {
    return AXTP_STATUS_QUEUE_FULL;
  }
  uint8_t* copy = NULL;
  if (len > 0) {
    copy = (uint8_t*)malloc(len);
    if (copy == NULL) {
      return AXTP_STATUS_NO_MEMORY;
    }
    memcpy(copy, data, len);
  }
  transport->outgoing[transport->outgoing_count] = copy;
  transport->outgoing_len[transport->outgoing_count] = len;
  transport->outgoing_count++;
  return AXTP_STATUS_OK;
}

static axtp_transport_profile_t mock_profile(void* impl) {
  return ((axtp_mock_transport_t*)impl)->profile;
}

axtp_mock_transport_t* axtp_mock_transport_new(void) {
  axtp_mock_transport_t* transport = (axtp_mock_transport_t*)calloc(1, sizeof(axtp_mock_transport_t));
  if (transport != NULL) {
    transport->profile = axtp_default_transport_profile();
  }
  return transport;
}

void axtp_mock_transport_free(axtp_mock_transport_t* transport) {
  if (transport == NULL) {
    return;
  }
  for (size_t i = 0; i < transport->outgoing_count; ++i) {
    free(transport->outgoing[i]);
  }
  free(transport);
}

axtp_transport_t axtp_mock_transport_as_transport(axtp_mock_transport_t* transport) {
  axtp_transport_t value;
  value.impl = transport;
  value.bind = mock_bind;
  value.open = mock_open;
  value.close = mock_close;
  value.send_bytes = mock_send_bytes;
  value.profile = mock_profile;
  return value;
}

axtp_status_t axtp_mock_transport_inject_incoming(axtp_mock_transport_t* transport, const uint8_t* data, size_t len) {
  if (transport == NULL || transport->sink == NULL || (data == NULL && len > 0)) {
    return AXTP_STATUS_INVALID_ARGUMENT;
  }
  transport->sink(data, len, transport->sink_user_data);
  return AXTP_STATUS_OK;
}

axtp_status_t axtp_mock_transport_try_pop_outgoing(axtp_mock_transport_t* transport, uint8_t** out, size_t* out_len) {
  if (transport == NULL || out == NULL || out_len == NULL) {
    return AXTP_STATUS_INVALID_ARGUMENT;
  }
  if (transport->outgoing_count == 0) {
    return AXTP_STATUS_NOT_FOUND;
  }
  *out = transport->outgoing[0];
  *out_len = transport->outgoing_len[0];
  memmove(&transport->outgoing[0], &transport->outgoing[1], sizeof(transport->outgoing[0]) * (transport->outgoing_count - 1));
  memmove(&transport->outgoing_len[0], &transport->outgoing_len[1], sizeof(transport->outgoing_len[0]) * (transport->outgoing_count - 1));
  transport->outgoing_count--;
  return AXTP_STATUS_OK;
}

size_t axtp_mock_transport_queued_outgoing_count(const axtp_mock_transport_t* transport) {
  return transport == NULL ? 0 : transport->outgoing_count;
}

int axtp_mock_transport_is_open(const axtp_mock_transport_t* transport) {
  return transport != NULL && transport->open;
}
