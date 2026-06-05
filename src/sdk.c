#include "axtp/sdk.h"

#include <stdlib.h>
#include <string.h>

#include "generated/axtp_registry_generated.h"

struct axtp_client {
  axtp_broker_t* broker;
  axtp_endpoint_t* endpoint;
  uint32_t next_request_id;
};

struct axtp_server {
  axtp_broker_t* broker;
  axtp_endpoint_t* endpoint;
};

axtp_client_t* axtp_client_new(void) {
  axtp_client_t* client = (axtp_client_t*)calloc(1, sizeof(axtp_client_t));
  if (client == NULL) {
    return NULL;
  }
  client->broker = axtp_broker_new();
  client->endpoint = axtp_endpoint_new(client->broker);
  client->next_request_id = 1;
  if (client->broker == NULL || client->endpoint == NULL) {
    axtp_client_free(client);
    return NULL;
  }
  return client;
}

void axtp_client_free(axtp_client_t* client) {
  if (client == NULL) {
    return;
  }
  axtp_endpoint_free(client->endpoint);
  axtp_broker_free(client->broker);
  free(client);
}

axtp_status_t axtp_client_attach_transport(axtp_client_t* client, const axtp_transport_t* transport) {
  if (client == NULL) {
    return AXTP_STATUS_INVALID_ARGUMENT;
  }
  return axtp_endpoint_attach_transport(client->endpoint, transport);
}

axtp_status_t axtp_client_poll(axtp_client_t* client) {
  if (client == NULL) {
    return AXTP_STATUS_INVALID_ARGUMENT;
  }
  return axtp_endpoint_poll(client->endpoint, 8);
}

axtp_status_t axtp_client_send_json(axtp_client_t* client, const char* method_name, const char* params_json, uint32_t* request_id) {
  if (client == NULL || method_name == NULL || request_id == NULL) {
    return AXTP_STATUS_INVALID_ARGUMENT;
  }
  const axtp_method_descriptor_t* descriptor = axtp_method_by_name(method_name);
  if (descriptor == NULL) {
    return AXTP_STATUS_NOT_FOUND;
  }
  axtp_rpc_payload_t payload;
  axtp_rpc_payload_init(&payload);
  payload.encoding = AXTP_RPC_ENCODING_JSON;
  payload.op = AXTP_RPC_OP_REQUEST;
  payload.request_id = client->next_request_id++;
  if (client->next_request_id == 0) {
    client->next_request_id = 1;
  }
  payload.method_or_event_id = descriptor->id;
  payload.body_encoding = AXTP_RPC_BODY_ENCODING_RAW_BYTES;
  payload.meta.request_id = payload.request_id;
  const char* body = params_json == NULL ? "" : params_json;
  axtp_rpc_payload_set_body(&payload, (const uint8_t*)body, strlen(body));
  axtp_status_t status = axtp_endpoint_send_rpc_request(client->endpoint, &payload);
  *request_id = payload.request_id;
  axtp_rpc_payload_free(&payload);
  return status;
}

axtp_status_t axtp_client_try_take_json_response(axtp_client_t* client, uint32_t request_id, char* out, size_t out_capacity) {
  if (client == NULL || out == NULL || out_capacity == 0) {
    return AXTP_STATUS_INVALID_ARGUMENT;
  }
  axtp_rpc_payload_t response;
  axtp_status_t status = axtp_endpoint_try_take_rpc_response(client->endpoint, request_id, &response);
  if (status != AXTP_STATUS_OK) {
    return status;
  }
  const size_t copy_len = response.body_len < out_capacity - 1 ? response.body_len : out_capacity - 1;
  memcpy(out, response.body, copy_len);
  out[copy_len] = '\0';
  axtp_rpc_payload_free(&response);
  return AXTP_STATUS_OK;
}

axtp_server_t* axtp_server_new(void) {
  axtp_server_t* server = (axtp_server_t*)calloc(1, sizeof(axtp_server_t));
  if (server == NULL) {
    return NULL;
  }
  server->broker = axtp_broker_new();
  server->endpoint = axtp_endpoint_new(server->broker);
  if (server->broker == NULL || server->endpoint == NULL) {
    axtp_server_free(server);
    return NULL;
  }
  return server;
}

void axtp_server_free(axtp_server_t* server) {
  if (server == NULL) {
    return;
  }
  axtp_endpoint_free(server->endpoint);
  axtp_broker_free(server->broker);
  free(server);
}

axtp_status_t axtp_server_attach_transport(axtp_server_t* server, const axtp_transport_t* transport) {
  if (server == NULL) {
    return AXTP_STATUS_INVALID_ARGUMENT;
  }
  return axtp_endpoint_attach_transport(server->endpoint, transport);
}

axtp_status_t axtp_server_poll(axtp_server_t* server) {
  if (server == NULL) {
    return AXTP_STATUS_INVALID_ARGUMENT;
  }
  return axtp_endpoint_poll(server->endpoint, 8);
}

axtp_status_t axtp_server_on_json(axtp_server_t* server, const char* method_name, axtp_json_method_handler_fn handler, void* user_data) {
  if (server == NULL || method_name == NULL || handler == NULL) {
    return AXTP_STATUS_INVALID_ARGUMENT;
  }
  const axtp_method_descriptor_t* descriptor = axtp_method_by_name(method_name);
  if (descriptor == NULL) {
    return AXTP_STATUS_NOT_FOUND;
  }
  return axtp_broker_register_json_method(server->broker, descriptor->id, handler, user_data);
}
