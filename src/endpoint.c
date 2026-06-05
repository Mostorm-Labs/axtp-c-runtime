#include "axtp/endpoint.h"

#include <stdlib.h>

#include "axtp/codec.h"

struct axtp_endpoint {
  axtp_broker_t* broker;
  axtp_core_t* core;
  axtp_transport_t transport;
  int has_transport;
};

static void endpoint_on_bytes(const uint8_t* data, size_t len, void* user_data) {
  axtp_endpoint_t* endpoint = (axtp_endpoint_t*)user_data;
  (void)axtp_core_on_bytes(endpoint->core, data, len);
}

static axtp_status_t flush_outbound(axtp_endpoint_t* endpoint) {
  if (!endpoint->has_transport) {
    return AXTP_STATUS_OK;
  }
  while (1) {
    uint8_t* data = NULL;
    size_t len = 0;
    axtp_status_t status = axtp_core_try_pop_outbound_bytes(endpoint->core, &data, &len);
    if (status == AXTP_STATUS_NOT_FOUND) {
      return AXTP_STATUS_OK;
    }
    if (status != AXTP_STATUS_OK) {
      return status;
    }
    status = endpoint->transport.send_bytes(endpoint->transport.impl, data, len);
    axtp_free_bytes(data);
    if (status != AXTP_STATUS_OK) {
      return status;
    }
  }
}

axtp_endpoint_t* axtp_endpoint_new(axtp_broker_t* broker) {
  if (broker == NULL) {
    return NULL;
  }
  axtp_endpoint_t* endpoint = (axtp_endpoint_t*)calloc(1, sizeof(axtp_endpoint_t));
  if (endpoint == NULL) {
    return NULL;
  }
  endpoint->broker = broker;
  endpoint->core = axtp_core_new();
  if (endpoint->core == NULL) {
    free(endpoint);
    return NULL;
  }
  return endpoint;
}

void axtp_endpoint_free(axtp_endpoint_t* endpoint) {
  if (endpoint == NULL) {
    return;
  }
  axtp_core_free(endpoint->core);
  free(endpoint);
}

axtp_status_t axtp_endpoint_attach_transport(axtp_endpoint_t* endpoint, const axtp_transport_t* transport) {
  if (endpoint == NULL || transport == NULL || transport->bind == NULL) {
    return AXTP_STATUS_INVALID_ARGUMENT;
  }
  endpoint->transport = *transport;
  endpoint->has_transport = 1;
  axtp_core_configure(endpoint->core, transport->profile(transport->impl));
  endpoint->transport.bind(endpoint->transport.impl, endpoint_on_bytes, endpoint);
  if (endpoint->transport.open != NULL) {
    return endpoint->transport.open(endpoint->transport.impl);
  }
  return AXTP_STATUS_OK;
}

axtp_status_t axtp_endpoint_poll(axtp_endpoint_t* endpoint, size_t max_tasks) {
  if (endpoint == NULL) {
    return AXTP_STATUS_INVALID_ARGUMENT;
  }
  while (1) {
    axtp_core_event_t event;
    axtp_status_t status = axtp_core_poll_event(endpoint->core, &event);
    if (status == AXTP_STATUS_NOT_FOUND) {
      break;
    }
    if (status != AXTP_STATUS_OK) {
      return status;
    }
    axtp_broker_task_t task;
    task.type = event.type == AXTP_CORE_EVENT_RPC_REQUEST ? AXTP_BROKER_TASK_RPC_REQUEST : AXTP_BROKER_TASK_RPC_EVENT;
    task.rpc = event.rpc;
    status = axtp_broker_submit(endpoint->broker, &task);
    axtp_rpc_payload_free(&event.rpc);
    if (status != AXTP_STATUS_OK) {
      return status;
    }
  }
  axtp_status_t status = axtp_broker_poll(endpoint->broker, max_tasks);
  if (status != AXTP_STATUS_OK) {
    return status;
  }
  while (1) {
    axtp_rpc_payload_t result;
    status = axtp_broker_poll_result(endpoint->broker, &result);
    if (status == AXTP_STATUS_NOT_FOUND) {
      break;
    }
    if (status != AXTP_STATUS_OK) {
      return status;
    }
    status = axtp_core_send_rpc_response(endpoint->core, &result);
    axtp_rpc_payload_free(&result);
    if (status != AXTP_STATUS_OK) {
      return status;
    }
  }
  return flush_outbound(endpoint);
}

axtp_status_t axtp_endpoint_send_rpc_request(axtp_endpoint_t* endpoint, const axtp_rpc_payload_t* payload) {
  if (endpoint == NULL || payload == NULL) {
    return AXTP_STATUS_INVALID_ARGUMENT;
  }
  axtp_status_t status = axtp_core_expect_rpc_response(endpoint->core, payload->request_id);
  if (status != AXTP_STATUS_OK) {
    return status;
  }
  status = axtp_core_send_rpc_request(endpoint->core, payload);
  if (status != AXTP_STATUS_OK) {
    return status;
  }
  return flush_outbound(endpoint);
}

axtp_status_t axtp_endpoint_try_take_rpc_response(axtp_endpoint_t* endpoint, uint32_t request_id, axtp_rpc_payload_t* out) {
  if (endpoint == NULL) {
    return AXTP_STATUS_INVALID_ARGUMENT;
  }
  return axtp_core_try_take_rpc_response(endpoint->core, request_id, out);
}

axtp_core_t* axtp_endpoint_core(axtp_endpoint_t* endpoint) {
  return endpoint == NULL ? NULL : endpoint->core;
}

axtp_broker_t* axtp_endpoint_broker(axtp_endpoint_t* endpoint) {
  return endpoint == NULL ? NULL : endpoint->broker;
}
