#ifndef AXTP_CORE_H
#define AXTP_CORE_H

#include "axtp/broker.h"
#include "axtp/model.h"
#include "axtp/transport.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct axtp_core axtp_core_t;

typedef enum {
  AXTP_CORE_EVENT_RPC_REQUEST = 1,
  AXTP_CORE_EVENT_RPC_EVENT = 2
} axtp_core_event_type_t;

typedef struct {
  axtp_core_event_type_t type;
  axtp_rpc_payload_t rpc;
} axtp_core_event_t;

axtp_core_t* axtp_core_new(void);
void axtp_core_free(axtp_core_t* core);
void axtp_core_configure(axtp_core_t* core, axtp_transport_profile_t profile);
axtp_status_t axtp_core_on_bytes(axtp_core_t* core, const uint8_t* data, size_t len);
axtp_status_t axtp_core_poll_event(axtp_core_t* core, axtp_core_event_t* out);
axtp_status_t axtp_core_expect_rpc_response(axtp_core_t* core, uint32_t request_id);
axtp_status_t axtp_core_send_rpc_request(axtp_core_t* core, const axtp_rpc_payload_t* payload);
axtp_status_t axtp_core_send_rpc_response(axtp_core_t* core, const axtp_rpc_payload_t* payload);
axtp_status_t axtp_core_try_take_rpc_response(axtp_core_t* core, uint32_t request_id, axtp_rpc_payload_t* out);
axtp_status_t axtp_core_try_pop_outbound_bytes(axtp_core_t* core, uint8_t** out, size_t* out_len);

#ifdef __cplusplus
}
#endif

#endif
