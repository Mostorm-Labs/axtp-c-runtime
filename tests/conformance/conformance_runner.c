#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "axtp/axtp.h"
#include "generated/axtp_generated_version.h"

typedef enum {
  CASE_REQUIRED,
  CASE_OPTIONAL,
  CASE_NOT_SELECTED,
  CASE_UNSUPPORTED
} case_requirement_t;

typedef enum {
  CASE_PENDING,
  CASE_PASSED,
  CASE_FAILED,
  CASE_SKIPPED,
  CASE_STATUS_UNSUPPORTED
} case_status_t;

typedef struct {
  const char* id;
  const char* level;
  case_requirement_t requirement;
  case_status_t status;
  double duration_ms;
  char message[192];
} conformance_case_t;

static conformance_case_t cases[] = {
  {"handshake.open_accept", "framed-binary", CASE_UNSUPPORTED, CASE_STATUS_UNSUPPORTED, 0.0, "framed binary control/session behavior is not exposed by the C runtime"},
  {"handshake.open_reject", "framed-binary", CASE_UNSUPPORTED, CASE_STATUS_UNSUPPORTED, 0.0, "framed binary control/session behavior is not exposed by the C runtime"},
  {"handshake.close", "framed-binary", CASE_UNSUPPORTED, CASE_STATUS_UNSUPPORTED, 0.0, "framed binary control/session behavior is not exposed by the C runtime"},
  {"handshake.ping_pong", "framed-binary", CASE_UNSUPPORTED, CASE_STATUS_UNSUPPORTED, 0.0, "framed binary control/session behavior is not exposed by the C runtime"},
  {"session.hello_identify_identified", "websocket-jsonrpc", CASE_UNSUPPORTED, CASE_STATUS_UNSUPPORTED, 0.0, "WebSocket JSON-RPC session gating is not supported by the C runtime"},
  {"session.request_before_identified", "websocket-jsonrpc", CASE_UNSUPPORTED, CASE_STATUS_UNSUPPORTED, 0.0, "WebSocket JSON-RPC session gating is not supported by the C runtime"},
  {"rpc.request_response_json", "core", CASE_REQUIRED, CASE_PENDING, 0.0, ""},
  {"rpc.method_not_found", "core", CASE_REQUIRED, CASE_PENDING, 0.0, ""},
  {"rpc.invalid_params", "core", CASE_NOT_SELECTED, CASE_SKIPPED, 0.0, "schema-aware parameter validation is outside the required C core profile"},
  {"rpc.request_id_match", "core", CASE_REQUIRED, CASE_PENDING, 0.0, ""},
  {"event.subscribe_event", "event", CASE_UNSUPPORTED, CASE_STATUS_UNSUPPORTED, 0.0, "event subscription dispatch is not exposed by the C runtime"},
  {"event.unsubscribe_event", "event", CASE_UNSUPPORTED, CASE_STATUS_UNSUPPORTED, 0.0, "event subscription dispatch is not exposed by the C runtime"},
  {"event.emit_event", "event", CASE_UNSUPPORTED, CASE_STATUS_UNSUPPORTED, 0.0, "event subscription dispatch is not exposed by the C runtime"},
  {"capability.get_all", "capability", CASE_OPTIONAL, CASE_PENDING, 0.0, ""},
  {"capability.method_binding", "capability", CASE_OPTIONAL, CASE_PENDING, 0.0, ""},
  {"capability.unsupported_method", "capability", CASE_OPTIONAL, CASE_PENDING, 0.0, ""},
  {"error.standard_error_shape", "core", CASE_REQUIRED, CASE_PENDING, 0.0, ""},
  {"error.unauthorized", "core", CASE_NOT_SELECTED, CASE_SKIPPED, 0.0, "auth policy hooks are outside the required C core profile"},
  {"error.server_busy", "core", CASE_NOT_SELECTED, CASE_SKIPPED, 0.0, "busy-state policy hooks are outside the required C core profile"},
  {"stream.stream_open", "stream", CASE_UNSUPPORTED, CASE_STATUS_UNSUPPORTED, 0.0, "STREAM payload behavior is not exposed by the C runtime"},
  {"stream.stream_data", "stream", CASE_UNSUPPORTED, CASE_STATUS_UNSUPPORTED, 0.0, "STREAM payload behavior is not exposed by the C runtime"},
  {"stream.stream_close", "stream", CASE_UNSUPPORTED, CASE_STATUS_UNSUPPORTED, 0.0, "STREAM payload behavior is not exposed by the C runtime"},
};

static const size_t case_count = sizeof(cases) / sizeof(cases[0]);

static double now_ms(void) {
  return (double)clock() * 1000.0 / (double)CLOCKS_PER_SEC;
}

static conformance_case_t* find_case(const char* id) {
  for (size_t i = 0; i < case_count; ++i) {
    if (strcmp(cases[i].id, id) == 0) {
      return &cases[i];
    }
  }
  return NULL;
}

static void set_result(const char* id, case_status_t status, double duration_ms, const char* message) {
  conformance_case_t* item = find_case(id);
  if (item == NULL) {
    return;
  }
  item->status = status;
  item->duration_ms = duration_ms;
  if (message != NULL) {
    snprintf(item->message, sizeof(item->message), "%s", message);
  }
}

static int file_exists(const char* path) {
  FILE* fp = fopen(path, "rb");
  if (fp == NULL) {
    return 0;
  }
  fclose(fp);
  return 1;
}

static void bridge(axtp_mock_transport_t* left, axtp_mock_transport_t* right) {
  while (1) {
    uint8_t* bytes = NULL;
    size_t len = 0;
    if (axtp_mock_transport_try_pop_outgoing(left, &bytes, &len) != AXTP_STATUS_OK) {
      return;
    }
    (void)axtp_mock_transport_inject_incoming(right, bytes, len);
    axtp_free_bytes(bytes);
  }
}

static axtp_status_t audio_get_config(const axtp_rpc_context_t* context, const char* params_json, char* response_json, size_t response_capacity, void* user_data) {
  (void)params_json;
  (void)user_data;
  snprintf(response_json, response_capacity, "{\"method\":\"%s\",\"ok\":true}", context->method_name);
  return AXTP_STATUS_OK;
}

static int test_request_response_json(char* message, size_t message_len) {
  int ok = 0;
  axtp_client_t* client = axtp_client_new();
  axtp_server_t* server = axtp_server_new();
  axtp_mock_transport_t* client_mock = axtp_mock_transport_new();
  axtp_mock_transport_t* server_mock = axtp_mock_transport_new();
  if (client == NULL || server == NULL || client_mock == NULL || server_mock == NULL) {
    snprintf(message, message_len, "failed to allocate client/server test objects");
    goto cleanup;
  }

  axtp_transport_t client_transport = axtp_mock_transport_as_transport(client_mock);
  axtp_transport_t server_transport = axtp_mock_transport_as_transport(server_mock);
  if (axtp_client_attach_transport(client, &client_transport) != AXTP_STATUS_OK ||
      axtp_server_attach_transport(server, &server_transport) != AXTP_STATUS_OK ||
      axtp_server_on_json(server, "audio.getAlgorithmConfig", audio_get_config, NULL) != AXTP_STATUS_OK) {
    snprintf(message, message_len, "failed to attach transports or register JSON handler");
    goto cleanup;
  }

  uint32_t request_id = 0;
  if (axtp_client_send_json(client, "audio.getAlgorithmConfig", "{}", &request_id) != AXTP_STATUS_OK || request_id != 1) {
    snprintf(message, message_len, "client did not send audio.getAlgorithmConfig with request id 1");
    goto cleanup;
  }
  bridge(client_mock, server_mock);
  if (axtp_server_poll(server) != AXTP_STATUS_OK) {
    snprintf(message, message_len, "server poll failed");
    goto cleanup;
  }
  bridge(server_mock, client_mock);
  if (axtp_client_poll(client) != AXTP_STATUS_OK) {
    snprintf(message, message_len, "client poll failed");
    goto cleanup;
  }

  char response[256];
  if (axtp_client_try_take_json_response(client, request_id, response, sizeof(response)) != AXTP_STATUS_OK) {
    snprintf(message, message_len, "client did not receive response for request id %u", request_id);
    goto cleanup;
  }
  if (strcmp(response, "{\"method\":\"audio.getAlgorithmConfig\",\"ok\":true}") != 0) {
    snprintf(message, message_len, "unexpected JSON response: %s", response);
    goto cleanup;
  }
  ok = 1;

cleanup:
  axtp_mock_transport_free(client_mock);
  axtp_mock_transport_free(server_mock);
  axtp_client_free(client);
  axtp_server_free(server);
  return ok;
}

static int broker_unknown_method_response(uint32_t request_id, uint16_t* status_code, uint32_t* response_request_id) {
  int ok = 0;
  axtp_broker_t* broker = axtp_broker_new();
  if (broker == NULL) {
    return 0;
  }
  axtp_broker_task_t task;
  task.type = AXTP_BROKER_TASK_RPC_REQUEST;
  axtp_rpc_payload_init(&task.rpc);
  task.rpc.encoding = AXTP_RPC_ENCODING_JSON;
  task.rpc.op = AXTP_RPC_OP_REQUEST;
  task.rpc.request_id = request_id;
  task.rpc.method_or_event_id = 0x7FFFu;
  task.rpc.body_encoding = AXTP_RPC_BODY_ENCODING_RAW_BYTES;
  task.rpc.meta.request_id = request_id;
  if (axtp_rpc_payload_set_body(&task.rpc, (const uint8_t*)"{}", 2) != AXTP_STATUS_OK) {
    goto cleanup_task;
  }
  if (axtp_broker_submit(broker, &task) != AXTP_STATUS_OK || axtp_broker_poll(broker, 1) != AXTP_STATUS_OK) {
    goto cleanup_task;
  }

  axtp_rpc_payload_t response;
  if (axtp_broker_poll_result(broker, &response) == AXTP_STATUS_OK) {
    *status_code = response.status_code;
    *response_request_id = response.request_id;
    axtp_rpc_payload_free(&response);
    ok = 1;
  }

cleanup_task:
  axtp_rpc_payload_free(&task.rpc);
  axtp_broker_free(broker);
  return ok;
}

static int test_method_not_found(uint32_t request_id, char* message, size_t message_len) {
  uint16_t status_code = AXTP_ERROR_CODE_SUCCESS;
  uint32_t response_request_id = 0;
  if (!broker_unknown_method_response(request_id, &status_code, &response_request_id)) {
    snprintf(message, message_len, "broker did not produce an unknown-method response");
    return 0;
  }
  if (response_request_id != request_id) {
    snprintf(message, message_len, "response request id %u did not match %u", response_request_id, request_id);
    return 0;
  }
  if (status_code != AXTP_ERROR_CODE_RPC_METHOD_NOT_FOUND) {
    snprintf(message, message_len, "status code 0x%04X was not RPC_METHOD_NOT_FOUND", status_code);
    return 0;
  }
  return 1;
}

static int test_request_id_match(char* message, size_t message_len) {
  axtp_rpc_payload_t payload;
  axtp_rpc_payload_init(&payload);
  payload.encoding = AXTP_RPC_ENCODING_JSON;
  payload.op = AXTP_RPC_OP_REQUEST_RESPONSE;
  payload.request_id = 55;
  payload.method_or_event_id = AXTP_METHOD_ID_AUDIO_GET_ALGORITHM_CONFIG;
  payload.status_code = AXTP_ERROR_CODE_SUCCESS;
  payload.body_encoding = AXTP_RPC_BODY_ENCODING_RAW_BYTES;
  if (axtp_rpc_payload_set_body(&payload, (const uint8_t*)"{}", 2) != AXTP_STATUS_OK) {
    snprintf(message, message_len, "failed to set response body");
    axtp_rpc_payload_free(&payload);
    return 0;
  }
  uint8_t* bytes = NULL;
  size_t len = 0;
  if (axtp_encode_rpc_payload(&payload, &bytes, &len) != AXTP_STATUS_OK) {
    snprintf(message, message_len, "failed to encode RPC response");
    axtp_rpc_payload_free(&payload);
    return 0;
  }
  axtp_rpc_payload_t decoded;
  if (axtp_decode_rpc_payload(bytes, len, &decoded) != AXTP_STATUS_OK) {
    snprintf(message, message_len, "failed to decode RPC response");
    axtp_free_bytes(bytes);
    axtp_rpc_payload_free(&payload);
    return 0;
  }
  const int ok = decoded.request_id == 55 && decoded.method_or_event_id == AXTP_METHOD_ID_AUDIO_GET_ALGORITHM_CONFIG;
  if (!ok) {
    snprintf(message, message_len, "decoded request id or method id did not match");
  }
  axtp_rpc_payload_free(&decoded);
  axtp_free_bytes(bytes);
  axtp_rpc_payload_free(&payload);
  return ok;
}

static int test_capability_get_all(char* message, size_t message_len) {
  const axtp_method_descriptor_t* get_config = axtp_method_by_name("audio.getAlgorithmConfig");
  const axtp_method_descriptor_t* get_caps = axtp_method_by_name("audio.getAlgorithmCapabilities");
  const axtp_method_descriptor_t* set_config = axtp_method_by_name("audio.setAlgorithmConfig");
  const axtp_method_descriptor_t* reset_config = axtp_method_by_name("audio.resetAlgorithmConfig");
  if (AXTP_METHOD_REGISTRY_COUNT < 4 || get_config == NULL || get_caps == NULL || set_config == NULL || reset_config == NULL) {
    snprintf(message, message_len, "generated registry does not expose the adopted audio methods");
    return 0;
  }
  if (get_config->id != AXTP_METHOD_ID_AUDIO_GET_ALGORITHM_CONFIG || get_caps->id != AXTP_METHOD_ID_AUDIO_GET_ALGORITHM_CAPABILITIES) {
    snprintf(message, message_len, "generated audio method ids do not match spec/v0.0.2");
    return 0;
  }
  return 1;
}

static int test_capability_method_binding(char* message, size_t message_len) {
  const axtp_method_descriptor_t* get_config = axtp_method_by_name("audio.getAlgorithmConfig");
  const axtp_event_descriptor_t* changed = axtp_event_by_name("audio.algorithmConfigChanged");
  if (AXTP_CAPABILITY_ID_AUDIO_ALGORITHM != 0x0901u || get_config == NULL || changed == NULL) {
    snprintf(message, message_len, "audio.algorithm capability id, method, or event binding is missing");
    return 0;
  }
  if (strcmp(get_config->domain, "audio") != 0 || strcmp(changed->domain, "audio") != 0) {
    snprintf(message, message_len, "audio method/event registry entries are not bound to the audio domain");
    return 0;
  }
  return 1;
}

typedef int (*case_fn_t)(char*, size_t);

static void run_case_fn(const char* id, case_fn_t fn) {
  char message[192] = "";
  const double start = now_ms();
  const int ok = fn(message, sizeof(message));
  const double duration = now_ms() - start;
  set_result(id, ok ? CASE_PASSED : CASE_FAILED, duration, ok ? "" : message);
}

static int request_response_wrapper(char* message, size_t message_len) {
  return test_request_response_json(message, message_len);
}

static int method_not_found_wrapper(char* message, size_t message_len) {
  return test_method_not_found(2, message, message_len);
}

static int request_id_match_wrapper(char* message, size_t message_len) {
  return test_request_id_match(message, message_len);
}

static int standard_error_shape_wrapper(char* message, size_t message_len) {
  return test_method_not_found(99, message, message_len);
}

static int capability_get_all_wrapper(char* message, size_t message_len) {
  return test_capability_get_all(message, message_len);
}

static int capability_method_binding_wrapper(char* message, size_t message_len) {
  return test_capability_method_binding(message, message_len);
}

static int capability_unsupported_method_wrapper(char* message, size_t message_len) {
  uint16_t status_code = AXTP_ERROR_CODE_SUCCESS;
  uint32_t response_request_id = 0;
  if (!broker_unknown_method_response(4, &status_code, &response_request_id)) {
    snprintf(message, message_len, "broker did not produce an unsupported-method response");
    return 0;
  }
  if (response_request_id != 4) {
    snprintf(message, message_len, "response request id %u did not match 4", response_request_id);
    return 0;
  }
  if (status_code != AXTP_ERROR_CODE_RPC_METHOD_NOT_FOUND && status_code != AXTP_ERROR_CODE_CAPABILITY_METHOD_UNSUPPORTED) {
    snprintf(message, message_len, "status code 0x%04X was not an accepted unsupported-method code", status_code);
    return 0;
  }
  return 1;
}

static const char* status_name(case_status_t status) {
  switch (status) {
    case CASE_PASSED:
      return "passed";
    case CASE_FAILED:
      return "failed";
    case CASE_SKIPPED:
      return "skipped";
    case CASE_STATUS_UNSUPPORTED:
      return "unsupported";
    case CASE_PENDING:
    default:
      return "failed";
  }
}

static const char* requirement_name(case_requirement_t requirement) {
  switch (requirement) {
    case CASE_REQUIRED:
      return "required";
    case CASE_OPTIONAL:
      return "optional";
    case CASE_UNSUPPORTED:
      return "unsupported";
    case CASE_NOT_SELECTED:
    default:
      return "not-selected";
  }
}

static void json_string(FILE* fp, const char* value) {
  fputc('"', fp);
  for (const char* p = value == NULL ? "" : value; *p != '\0'; ++p) {
    if (*p == '"' || *p == '\\') {
      fputc('\\', fp);
      fputc(*p, fp);
    } else if (*p == '\n') {
      fputs("\\n", fp);
    } else if (*p == '\r') {
      fputs("\\r", fp);
    } else if (*p == '\t') {
      fputs("\\t", fp);
    } else {
      fputc(*p, fp);
    }
  }
  fputc('"', fp);
}

static int write_result(const char* output_path, const char* profile_path) {
  FILE* fp = fopen(output_path, "wb");
  if (fp == NULL) {
    fprintf(stderr, "failed to open conformance result for writing: %s\n", output_path);
    return 0;
  }

  size_t passed = 0;
  size_t failed = 0;
  size_t skipped = 0;
  size_t unsupported = 0;
  for (size_t i = 0; i < case_count; ++i) {
    switch (cases[i].status) {
      case CASE_PASSED:
        passed++;
        break;
      case CASE_FAILED:
      case CASE_PENDING:
        failed++;
        break;
      case CASE_SKIPPED:
        skipped++;
        break;
      case CASE_STATUS_UNSUPPORTED:
        unsupported++;
        break;
    }
  }

  fprintf(fp, "{\n");
  fprintf(fp, "  \"runtime\": \"axtp-c-runtime\",\n");
  fprintf(fp, "  \"runtimeVersion\": ");
  json_string(fp, AXTP_RUNTIME_VERSION);
  fprintf(fp, ",\n");
  fprintf(fp, "  \"specTag\": ");
  json_string(fp, AXTP_SPEC_TAG);
  fprintf(fp, ",\n");
  fprintf(fp, "  \"profile\": ");
  json_string(fp, profile_path);
  fprintf(fp, ",\n");
  fprintf(fp, "  \"requiredLevels\": [\"core\"],\n");
  fprintf(fp, "  \"optionalLevels\": [\"capability\"],\n");
  fprintf(fp, "  \"unsupportedLevels\": [\"framed-binary\", \"websocket-jsonrpc\", \"event\", \"stream\"],\n");
  fprintf(fp, "  \"summary\": {\"total\": %zu, \"passed\": %zu, \"failed\": %zu, \"skipped\": %zu, \"unsupported\": %zu},\n", case_count, passed, failed, skipped, unsupported);
  fprintf(fp, "  \"cases\": [\n");
  for (size_t i = 0; i < case_count; ++i) {
    fprintf(fp, "    {\"id\": ");
    json_string(fp, cases[i].id);
    fprintf(fp, ", \"level\": ");
    json_string(fp, cases[i].level);
    fprintf(fp, ", \"requirement\": ");
    json_string(fp, requirement_name(cases[i].requirement));
    fprintf(fp, ", \"status\": ");
    json_string(fp, status_name(cases[i].status));
    fprintf(fp, ", \"durationMs\": %.3f, \"message\": ", cases[i].duration_ms);
    json_string(fp, cases[i].message);
    fprintf(fp, "}%s\n", i + 1 == case_count ? "" : ",");
  }
  fprintf(fp, "  ]\n");
  fprintf(fp, "}\n");
  fclose(fp);
  return 1;
}

int main(int argc, char** argv) {
  if (argc < 4) {
    fprintf(stderr, "usage: %s <axtp-spec-path> <result-json-path> <runtime-profile-path>\n", argv[0]);
    return 2;
  }

  char manifest_path[1024];
  snprintf(manifest_path, sizeof(manifest_path), "%s/docs/conformance/manifest.yaml", argv[1]);
  if (!file_exists(manifest_path)) {
    snprintf(manifest_path, sizeof(manifest_path), "%s/conformance/manifest.yaml", argv[1]);
  }
  if (!file_exists(manifest_path)) {
    fprintf(stderr, "missing conformance manifest: %s\n", manifest_path);
    return 2;
  }
  if (!file_exists(argv[3])) {
    fprintf(stderr, "missing runtime profile: %s\n", argv[3]);
    return 2;
  }

  run_case_fn("rpc.request_response_json", request_response_wrapper);
  run_case_fn("rpc.method_not_found", method_not_found_wrapper);
  run_case_fn("rpc.request_id_match", request_id_match_wrapper);
  run_case_fn("error.standard_error_shape", standard_error_shape_wrapper);
  run_case_fn("capability.get_all", capability_get_all_wrapper);
  run_case_fn("capability.method_binding", capability_method_binding_wrapper);
  run_case_fn("capability.unsupported_method", capability_unsupported_method_wrapper);

  int required_issue = 0;
  int optional_issue = 0;
  for (size_t i = 0; i < case_count; ++i) {
    if (cases[i].requirement == CASE_REQUIRED && cases[i].status != CASE_PASSED) {
      required_issue = 1;
    }
    if (cases[i].requirement == CASE_OPTIONAL && cases[i].status != CASE_PASSED) {
      optional_issue = 1;
    }
  }

  if (!write_result(argv[2], argv[3])) {
    return 1;
  }

  const int allow_incomplete = getenv("CONFORMANCE_ALLOW_INCOMPLETE") != NULL && strcmp(getenv("CONFORMANCE_ALLOW_INCOMPLETE"), "true") == 0;
  const int strict_optional = getenv("CONFORMANCE_STRICT_OPTIONAL") != NULL && strcmp(getenv("CONFORMANCE_STRICT_OPTIONAL"), "true") == 0;
  if ((required_issue && !allow_incomplete) || (optional_issue && strict_optional)) {
    return 1;
  }
  return 0;
}
