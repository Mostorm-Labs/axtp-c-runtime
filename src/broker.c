#include "axtp/broker.h"

#include <stdlib.h>
#include <string.h>

#include "generated/axtp_registry_generated.h"

typedef enum {
  HANDLER_NONE = 0,
  HANDLER_RAW = 1,
  HANDLER_JSON = 2
} handler_type_t;

typedef struct {
  uint16_t method_id;
  handler_type_t type;
  axtp_raw_method_handler_fn raw_handler;
  axtp_json_method_handler_fn json_handler;
  void* user_data;
} handler_entry_t;

struct axtp_broker {
  axtp_broker_task_t tasks[AXTP_QUEUE_CAPACITY];
  size_t task_count;
  axtp_rpc_payload_t results[AXTP_QUEUE_CAPACITY];
  size_t result_count;
  handler_entry_t handlers[32];
  size_t handler_count;
};

static void task_free(axtp_broker_task_t* task) {
  if (task != NULL) {
    axtp_rpc_payload_free(&task->rpc);
  }
}

static handler_entry_t* find_handler(axtp_broker_t* broker, uint16_t method_id) {
  for (size_t i = 0; i < broker->handler_count; ++i) {
    if (broker->handlers[i].method_id == method_id) {
      return &broker->handlers[i];
    }
  }
  return NULL;
}

static axtp_status_t push_result(axtp_broker_t* broker, const axtp_rpc_payload_t* payload) {
  if (broker->result_count >= AXTP_QUEUE_CAPACITY) {
    return AXTP_STATUS_QUEUE_FULL;
  }
  return axtp_rpc_payload_copy(&broker->results[broker->result_count++], payload);
}

axtp_broker_t* axtp_broker_new(void) {
  axtp_broker_t* broker = (axtp_broker_t*)calloc(1, sizeof(axtp_broker_t));
  return broker;
}

void axtp_broker_free(axtp_broker_t* broker) {
  if (broker == NULL) {
    return;
  }
  for (size_t i = 0; i < broker->task_count; ++i) {
    task_free(&broker->tasks[i]);
  }
  for (size_t i = 0; i < broker->result_count; ++i) {
    axtp_rpc_payload_free(&broker->results[i]);
  }
  free(broker);
}

axtp_status_t axtp_broker_register_raw_method(axtp_broker_t* broker, uint16_t method_id, axtp_raw_method_handler_fn handler, void* user_data) {
  if (broker == NULL || handler == NULL) {
    return AXTP_STATUS_INVALID_ARGUMENT;
  }
  handler_entry_t* entry = find_handler(broker, method_id);
  if (entry == NULL) {
    if (broker->handler_count >= 32) {
      return AXTP_STATUS_QUEUE_FULL;
    }
    entry = &broker->handlers[broker->handler_count++];
  }
  entry->method_id = method_id;
  entry->type = HANDLER_RAW;
  entry->raw_handler = handler;
  entry->json_handler = NULL;
  entry->user_data = user_data;
  return AXTP_STATUS_OK;
}

axtp_status_t axtp_broker_register_json_method(axtp_broker_t* broker, uint16_t method_id, axtp_json_method_handler_fn handler, void* user_data) {
  if (broker == NULL || handler == NULL) {
    return AXTP_STATUS_INVALID_ARGUMENT;
  }
  handler_entry_t* entry = find_handler(broker, method_id);
  if (entry == NULL) {
    if (broker->handler_count >= 32) {
      return AXTP_STATUS_QUEUE_FULL;
    }
    entry = &broker->handlers[broker->handler_count++];
  }
  entry->method_id = method_id;
  entry->type = HANDLER_JSON;
  entry->raw_handler = NULL;
  entry->json_handler = handler;
  entry->user_data = user_data;
  return AXTP_STATUS_OK;
}

axtp_status_t axtp_broker_submit(axtp_broker_t* broker, const axtp_broker_task_t* task) {
  if (broker == NULL || task == NULL) {
    return AXTP_STATUS_INVALID_ARGUMENT;
  }
  if (broker->task_count >= AXTP_QUEUE_CAPACITY) {
    return AXTP_STATUS_QUEUE_FULL;
  }
  broker->tasks[broker->task_count].type = task->type;
  return axtp_rpc_payload_copy(&broker->tasks[broker->task_count++].rpc, &task->rpc);
}

axtp_status_t axtp_broker_poll(axtp_broker_t* broker, size_t max_tasks) {
  if (broker == NULL) {
    return AXTP_STATUS_INVALID_ARGUMENT;
  }
  size_t processed = 0;
  while (broker->task_count > 0 && processed < max_tasks) {
    axtp_broker_task_t task = broker->tasks[0];
    memmove(&broker->tasks[0], &broker->tasks[1], sizeof(broker->tasks[0]) * (broker->task_count - 1));
    broker->task_count--;
    processed++;

    axtp_rpc_payload_t response;
    axtp_rpc_payload_init(&response);
    response.encoding = task.rpc.encoding;
    response.op = AXTP_RPC_OP_REQUEST_RESPONSE;
    response.request_id = task.rpc.request_id;
    response.method_or_event_id = task.rpc.method_or_event_id;
    response.status_code = AXTP_ERROR_CODE_SUCCESS;
    response.body_encoding = task.rpc.body_encoding;
    response.meta = task.rpc.meta;

    handler_entry_t* handler = find_handler(broker, task.rpc.method_or_event_id);
    if (handler == NULL) {
      response.status_code = AXTP_ERROR_CODE_RPC_METHOD_NOT_FOUND;
    } else {
      const axtp_method_descriptor_t* descriptor = axtp_method_by_id(task.rpc.method_or_event_id);
      axtp_rpc_context_t context;
      context.session_id = task.rpc.meta.session_id;
      context.request_id = task.rpc.request_id;
      context.method_id = task.rpc.method_or_event_id;
      context.method_name = descriptor == NULL ? "" : descriptor->name;
      context.encoding = task.rpc.encoding;
      context.source_protocol = task.rpc.meta.source_protocol;
      if (handler->type == HANDLER_RAW) {
        if (handler->raw_handler(&context, &task.rpc, &response, handler->user_data) != AXTP_STATUS_OK) {
          response.status_code = AXTP_ERROR_CODE_RPC_EXECUTION_FAILED;
        }
      } else if (handler->type == HANDLER_JSON) {
        char params[4096];
        char body[4096];
        const size_t params_len = task.rpc.body_len < sizeof(params) - 1 ? task.rpc.body_len : sizeof(params) - 1;
        memcpy(params, task.rpc.body, params_len);
        params[params_len] = '\0';
        body[0] = '\0';
        if (handler->json_handler(&context, params, body, sizeof(body), handler->user_data) != AXTP_STATUS_OK) {
          response.status_code = AXTP_ERROR_CODE_RPC_EXECUTION_FAILED;
        } else {
          response.encoding = AXTP_RPC_ENCODING_JSON;
          response.body_encoding = AXTP_RPC_BODY_ENCODING_RAW_BYTES;
          axtp_rpc_payload_set_body(&response, (const uint8_t*)body, strlen(body));
        }
      }
    }
    push_result(broker, &response);
    axtp_rpc_payload_free(&response);
    task_free(&task);
  }
  return AXTP_STATUS_OK;
}

axtp_status_t axtp_broker_poll_result(axtp_broker_t* broker, axtp_rpc_payload_t* out) {
  if (broker == NULL || out == NULL) {
    return AXTP_STATUS_INVALID_ARGUMENT;
  }
  if (broker->result_count == 0) {
    return AXTP_STATUS_NOT_FOUND;
  }
  axtp_status_t status = axtp_rpc_payload_copy(out, &broker->results[0]);
  axtp_rpc_payload_free(&broker->results[0]);
  memmove(&broker->results[0], &broker->results[1], sizeof(broker->results[0]) * (broker->result_count - 1));
  broker->result_count--;
  return status;
}
