#ifndef AXTP_ENDPOINT_H
#define AXTP_ENDPOINT_H

#include "axtp/broker.h"
#include "axtp/core.h"
#include "axtp/transport.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct axtp_endpoint axtp_endpoint_t;

axtp_endpoint_t* axtp_endpoint_new(axtp_broker_t* broker);
void axtp_endpoint_free(axtp_endpoint_t* endpoint);
axtp_status_t axtp_endpoint_attach_transport(axtp_endpoint_t* endpoint, const axtp_transport_t* transport);
axtp_status_t axtp_endpoint_poll(axtp_endpoint_t* endpoint, size_t max_tasks);
axtp_status_t axtp_endpoint_send_rpc_request(axtp_endpoint_t* endpoint, const axtp_rpc_payload_t* payload);
axtp_status_t axtp_endpoint_try_take_rpc_response(axtp_endpoint_t* endpoint, uint32_t request_id, axtp_rpc_payload_t* out);
axtp_core_t* axtp_endpoint_core(axtp_endpoint_t* endpoint);
axtp_broker_t* axtp_endpoint_broker(axtp_endpoint_t* endpoint);

#ifdef __cplusplus
}
#endif

#endif
