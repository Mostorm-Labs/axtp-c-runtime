#ifndef AXTP_BROKER_H
#define AXTP_BROKER_H

#include <stddef.h>
#include <stdint.h>

#include "axtp/model.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct axtp_broker axtp_broker_t;

typedef struct {
  uint32_t session_id;
  uint32_t request_id;
  uint16_t method_id;
  const char* method_name;
  uint8_t encoding;
  uint8_t source_protocol;
} axtp_rpc_context_t;

typedef axtp_status_t (*axtp_raw_method_handler_fn)(const axtp_rpc_context_t* context, const axtp_rpc_payload_t* request, axtp_rpc_payload_t* response, void* user_data);
typedef axtp_status_t (*axtp_json_method_handler_fn)(const axtp_rpc_context_t* context, const char* params_json, char* response_json, size_t response_capacity, void* user_data);

typedef enum {
  AXTP_BROKER_TASK_RPC_REQUEST = 1,
  AXTP_BROKER_TASK_RPC_EVENT = 2
} axtp_broker_task_type_t;

typedef struct {
  axtp_broker_task_type_t type;
  axtp_rpc_payload_t rpc;
} axtp_broker_task_t;

axtp_broker_t* axtp_broker_new(void);
void axtp_broker_free(axtp_broker_t* broker);
axtp_status_t axtp_broker_register_raw_method(axtp_broker_t* broker, uint16_t method_id, axtp_raw_method_handler_fn handler, void* user_data);
axtp_status_t axtp_broker_register_json_method(axtp_broker_t* broker, uint16_t method_id, axtp_json_method_handler_fn handler, void* user_data);
axtp_status_t axtp_broker_submit(axtp_broker_t* broker, const axtp_broker_task_t* task);
axtp_status_t axtp_broker_poll(axtp_broker_t* broker, size_t max_tasks);
axtp_status_t axtp_broker_poll_result(axtp_broker_t* broker, axtp_rpc_payload_t* out);

#ifdef __cplusplus
}
#endif

#endif
