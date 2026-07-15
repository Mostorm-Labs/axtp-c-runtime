#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "axtp/axtp.h"

static void bridge(axtp_mock_transport_t* left, axtp_mock_transport_t* right) {
  while (1) {
    uint8_t* bytes = NULL;
    size_t len = 0;
    if (axtp_mock_transport_try_pop_outgoing(left, &bytes, &len) != AXTP_STATUS_OK) {
      return;
    }
    assert(axtp_mock_transport_inject_incoming(right, bytes, len) == AXTP_STATUS_OK);
    axtp_free_bytes(bytes);
  }
}

static axtp_status_t audio_get_config(const axtp_rpc_context_t* context, const char* params_json, char* response_json, size_t response_capacity, void* user_data) {
  (void)params_json;
  (void)user_data;
  snprintf(response_json, response_capacity, "{\"method\":\"%s\",\"ok\":true}", context->method_name);
  return AXTP_STATUS_OK;
}

static void submit_rpc(axtp_broker_t* broker, uint32_t request_id, uint16_t method_id) {
  axtp_broker_task_t task;
  task.type = AXTP_BROKER_TASK_RPC_REQUEST;
  axtp_rpc_payload_init(&task.rpc);
  task.rpc.encoding = AXTP_RPC_ENCODING_JSON;
  task.rpc.op = AXTP_RPC_OP_REQUEST;
  task.rpc.request_id = request_id;
  task.rpc.method_or_event_id = method_id;
  task.rpc.meta.request_id = request_id;
  assert(axtp_rpc_payload_set_body(&task.rpc, (const uint8_t*)"{}", 2u) == AXTP_STATUS_OK);
  assert(axtp_broker_submit(broker, &task) == AXTP_STATUS_OK);
  axtp_rpc_payload_free(&task.rpc);
}

static void test_unbound_registered_method_and_liveness(void) {
  axtp_broker_t* broker = axtp_broker_new();
  assert(broker != NULL);
  assert(axtp_broker_register_json_method(broker, AXTP_METHOD_ID_AUDIO_GET_ALGORITHM_CONFIG,
                                          audio_get_config, NULL) == AXTP_STATUS_OK);
  submit_rpc(broker, 40u, AXTP_METHOD_ID_AUDIO_SET_ALGORITHM_CONFIG);
  submit_rpc(broker, 41u, 0x7fffu);
  submit_rpc(broker, 42u, AXTP_METHOD_ID_AUDIO_GET_ALGORITHM_CONFIG);
  assert(axtp_broker_poll(broker, 3u) == AXTP_STATUS_OK);

  axtp_rpc_payload_t response;
  axtp_rpc_payload_init(&response);
  assert(axtp_broker_poll_result(broker, &response) == AXTP_STATUS_OK);
  assert(response.request_id == 40u);
  assert(response.status_code == AXTP_ERROR_CODE_NOT_SUPPORTED);
  axtp_rpc_payload_free(&response);

  assert(axtp_broker_poll_result(broker, &response) == AXTP_STATUS_OK);
  assert(response.request_id == 41u);
  assert(response.status_code == AXTP_ERROR_CODE_RPC_METHOD_NOT_FOUND);
  axtp_rpc_payload_free(&response);

  assert(axtp_broker_poll_result(broker, &response) == AXTP_STATUS_OK);
  assert(response.request_id == 42u);
  assert(response.status_code == AXTP_ERROR_CODE_SUCCESS);
  axtp_rpc_payload_free(&response);
  axtp_broker_free(broker);
}

int main(void) {
  test_unbound_registered_method_and_liveness();
  axtp_client_t* client = axtp_client_new();
  axtp_server_t* server = axtp_server_new();
  axtp_mock_transport_t* client_mock = axtp_mock_transport_new();
  axtp_mock_transport_t* server_mock = axtp_mock_transport_new();
  assert(client != NULL);
  assert(server != NULL);
  assert(client_mock != NULL);
  assert(server_mock != NULL);

  axtp_transport_t client_transport = axtp_mock_transport_as_transport(client_mock);
  axtp_transport_t server_transport = axtp_mock_transport_as_transport(server_mock);
  assert(axtp_client_attach_transport(client, &client_transport) == AXTP_STATUS_OK);
  assert(axtp_server_attach_transport(server, &server_transport) == AXTP_STATUS_OK);
  assert(axtp_server_on_json(server, "audio.getAlgorithmConfig", audio_get_config, NULL) == AXTP_STATUS_OK);

  uint32_t request_id = 0;
  assert(axtp_client_send_json(client, "audio.getAlgorithmConfig", "{}", &request_id) == AXTP_STATUS_OK);
  bridge(client_mock, server_mock);
  assert(axtp_server_poll(server) == AXTP_STATUS_OK);
  bridge(server_mock, client_mock);
  assert(axtp_client_poll(client) == AXTP_STATUS_OK);

  char response[256];
  assert(axtp_client_try_take_json_response(client, request_id, response, sizeof(response)) == AXTP_STATUS_OK);
  assert(strcmp(response, "{\"method\":\"audio.getAlgorithmConfig\",\"ok\":true}") == 0);

  axtp_mock_transport_free(client_mock);
  axtp_mock_transport_free(server_mock);
  axtp_client_free(client);
  axtp_server_free(server);
  return 0;
}
