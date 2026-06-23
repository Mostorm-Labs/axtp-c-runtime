#include <assert.h>
#include <string.h>

#include "axtp/axtp.h"

typedef struct {
  size_t opened;
  size_t chunks;
  size_t closed;
  uint32_t last_stream_id;
} recording_media_sink_t;

static void on_media_opened(const axtp_media_stream_info_t* info, void* user_data) {
  recording_media_sink_t* sink = (recording_media_sink_t*)user_data;
  assert(info->kind == AXTP_MEDIA_KIND_VIDEO);
  sink->opened += 1u;
}

static void on_media_chunk(axtp_media_kind_t kind, const axtp_stream_payload_t* stream, void* user_data) {
  recording_media_sink_t* sink = (recording_media_sink_t*)user_data;
  assert(kind == AXTP_MEDIA_KIND_VIDEO);
  sink->last_stream_id = stream->stream_id;
  sink->chunks += 1u;
}

static void on_media_closed(axtp_media_kind_t kind, uint32_t stream_id, void* user_data) {
  recording_media_sink_t* sink = (recording_media_sink_t*)user_data;
  assert(kind == AXTP_MEDIA_KIND_VIDEO);
  sink->last_stream_id = stream_id;
  sink->closed += 1u;
}

static void make_request(axtp_rpc_payload_t* rpc, uint32_t request_id, uint16_t method_id, const char* body) {
  axtp_rpc_payload_init(rpc);
  rpc->encoding = AXTP_RPC_ENCODING_JSON;
  rpc->op = AXTP_RPC_OP_REQUEST;
  rpc->request_id = request_id;
  rpc->method_or_event_id = method_id;
  rpc->body_encoding = AXTP_RPC_BODY_ENCODING_NONE;
  rpc->meta.source_protocol = AXTP_SOURCE_PROTOCOL_JSON_RPC;
  rpc->meta.request_id = request_id;
  assert(axtp_rpc_payload_set_body(rpc, (const uint8_t*)body, strlen(body)) == AXTP_STATUS_OK);
}

int main(void) {
  recording_media_sink_t sink;
  memset(&sink, 0, sizeof(sink));

  axtp_media_stream_sink_t callbacks;
  callbacks.on_stream_opened = on_media_opened;
  callbacks.on_stream_chunk = on_media_chunk;
  callbacks.on_stream_closed = on_media_closed;
  callbacks.user_data = &sink;

  axtp_media_host_options_t options;
  axtp_media_host_options_init(&options);
  options.open_mode = AXTP_MEDIA_OPEN_MODE_PRODUCER_OPEN;
  options.stream_sink = &callbacks;

  axtp_media_stream_registry_t registry;
  axtp_media_stream_registry_init(&registry, &options);

  axtp_broker_t* broker = axtp_broker_new();
  assert(broker != NULL);
  assert(axtp_install_media_host_handlers(broker, &registry) == AXTP_STATUS_OK);

  axtp_rpc_payload_t open_rpc;
  make_request(&open_rpc,
               77u,
               AXTP_METHOD_ID_VIDEO_OPEN_STREAM,
               "{\"source\":\"wireless_cast_video\",\"peerRole\":\"receiver\",\"codec\":\"h264\"}");
  axtp_broker_task_t open_task;
  open_task.type = AXTP_BROKER_TASK_RPC_REQUEST;
  open_task.rpc = open_rpc;
  assert(axtp_broker_submit(broker, &open_task) == AXTP_STATUS_OK);
  assert(axtp_broker_poll(broker, 8u) == AXTP_STATUS_OK);

  axtp_rpc_payload_t open_result;
  axtp_rpc_payload_init(&open_result);
  assert(axtp_broker_poll_result(broker, &open_result) == AXTP_STATUS_OK);
  assert(open_result.status_code == AXTP_ERROR_CODE_SUCCESS);
  assert(strstr((const char*)open_result.body, "\"streamId\":4097") != NULL);
  assert(strstr((const char*)open_result.body, "\"codec\":\"h264\"") != NULL);
  assert(strstr((const char*)open_result.body, "\"codecFormat\":\"annexb\"") != NULL);
  assert(sink.opened == 1u);

  axtp_stream_payload_t stream;
  axtp_stream_payload_init(&stream);
  stream.stream_id = 0x1001u;
  stream.seq_id = 0u;
  stream.cursor = 1000u;
  const uint8_t bytes[] = {0x00u, 0x00u, 0x01u, 0x67u, 0x42u};
  assert(axtp_stream_payload_set_data(&stream, bytes, sizeof(bytes)) == AXTP_STATUS_OK);
  assert(axtp_broker_submit_stream(broker, &stream) == AXTP_STATUS_OK);
  assert(axtp_broker_poll(broker, 8u) == AXTP_STATUS_OK);
  axtp_media_stream_stats_t stats = axtp_media_stream_registry_stats(&registry);
  assert(stats.video_chunks == 1u);
  assert(stats.video_bytes == 5u);
  assert(sink.chunks == 1u);
  assert(sink.last_stream_id == 0x1001u);

  axtp_rpc_payload_t close_rpc;
  make_request(&close_rpc, 78u, AXTP_METHOD_ID_VIDEO_CLOSE_STREAM, "{\"streamId\":4097,\"peerRole\":\"transmitter\"}");
  axtp_broker_task_t close_task;
  close_task.type = AXTP_BROKER_TASK_RPC_REQUEST;
  close_task.rpc = close_rpc;
  assert(axtp_broker_submit(broker, &close_task) == AXTP_STATUS_OK);
  assert(axtp_broker_poll(broker, 8u) == AXTP_STATUS_OK);

  axtp_rpc_payload_t close_result;
  axtp_rpc_payload_init(&close_result);
  assert(axtp_broker_poll_result(broker, &close_result) == AXTP_STATUS_OK);
  assert(close_result.status_code == AXTP_ERROR_CODE_SUCCESS);
  assert(sink.closed == 1u);
  assert(axtp_media_stream_registry_active_count(&registry) == 0u);

  axtp_rpc_payload_free(&open_result);
  axtp_rpc_payload_free(&close_result);
  axtp_rpc_payload_free(&open_rpc);
  axtp_rpc_payload_free(&close_rpc);
  axtp_stream_payload_free(&stream);
  axtp_broker_free(broker);
  axtp_media_stream_registry_free(&registry);
  return 0;
}
