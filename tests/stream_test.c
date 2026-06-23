#include <assert.h>
#include <string.h>

#include "axtp/axtp.h"

typedef struct {
  size_t opened;
  size_t chunks;
  size_t closed;
  axtp_stream_info_t last_closed;
} recording_sink_t;

static void on_stream_opened(const axtp_stream_info_t* info, void* user_data) {
  recording_sink_t* sink = (recording_sink_t*)user_data;
  assert(info->stream_id == 0x10u);
  sink->opened += 1u;
}

static void on_stream_chunk(const axtp_stream_info_t* info, const axtp_stream_payload_t* stream, void* user_data) {
  recording_sink_t* sink = (recording_sink_t*)user_data;
  assert(info->stream_id == stream->stream_id);
  sink->chunks += 1u;
}

static void on_stream_closed(const axtp_stream_info_t* info, void* user_data) {
  recording_sink_t* sink = (recording_sink_t*)user_data;
  sink->last_closed = *info;
  sink->closed += 1u;
}

static axtp_stream_payload_t chunk(uint32_t stream_id, uint32_t seq_id, uint64_t cursor, size_t bytes) {
  axtp_stream_payload_t payload;
  axtp_stream_payload_init(&payload);
  payload.stream_id = stream_id;
  payload.seq_id = seq_id;
  payload.cursor = cursor;
  assert(axtp_stream_payload_set_data(&payload, NULL, bytes) == AXTP_STATUS_OK);
  memset(payload.data, (int)(seq_id + 1u), bytes);
  return payload;
}

int main(void) {
  recording_sink_t sink;
  memset(&sink, 0, sizeof(sink));

  axtp_stream_registry_t registry;
  axtp_stream_sink_t callbacks;
  callbacks.on_stream_opened = on_stream_opened;
  callbacks.on_stream_chunk = on_stream_chunk;
  callbacks.on_stream_closed = on_stream_closed;
  callbacks.user_data = &sink;
  axtp_stream_registry_init(&registry, &callbacks);

  axtp_stream_info_t info;
  axtp_stream_info_init(&info);
  info.stream_id = 0x10u;
  info.kind = "file";
  info.source = "firmware.bin";
  info.stream_profile = "file.transfer";
  info.cursor_unit = "offsetBytes";
  info.payload_format = "binary";

  assert(axtp_stream_registry_register(&registry, &info, true) == AXTP_ERROR_CODE_SUCCESS);
  assert(axtp_stream_registry_has_stream(&registry, 0x10u));
  assert(axtp_stream_registry_has_open_stream(&registry, "file", "firmware.bin"));
  assert(axtp_stream_registry_active_count(&registry) == 1u);
  assert(sink.opened == 1u);

  assert(axtp_stream_registry_register(&registry, &info, true) == AXTP_ERROR_CODE_STREAM_ALREADY_OPEN);

  axtp_stream_payload_t first = chunk(0x10u, 0u, 0u, 3u);
  axtp_stream_payload_t gap = chunk(0x10u, 2u, 3u, 5u);
  axtp_stream_payload_t duplicate = chunk(0x10u, 2u, 3u, 7u);
  axtp_stream_payload_t unknown = chunk(0x99u, 0u, 0u, 11u);
  axtp_stream_registry_handle(&registry, &first);
  axtp_stream_registry_handle(&registry, &gap);
  axtp_stream_registry_handle(&registry, &duplicate);
  axtp_stream_registry_handle(&registry, &unknown);

  axtp_stream_stats_t stats = axtp_stream_registry_stats(&registry);
  assert(stats.chunks == 3u);
  assert(stats.bytes == 15u);
  assert(stats.seq_gaps == 1u);
  assert(stats.duplicate_seq == 1u);
  assert(stats.unknown_chunks == 1u);
  assert(sink.chunks == 3u);

  axtp_stream_info_t closed;
  assert(axtp_stream_registry_close(&registry, 0x10u, &closed) == true);
  assert(closed.stream_id == 0x10u);
  assert(strcmp(closed.kind, "file") == 0);
  assert(strcmp(closed.source, "firmware.bin") == 0);
  assert(sink.closed == 1u);
  assert(strcmp(sink.last_closed.kind, "file") == 0);
  assert(strcmp(sink.last_closed.source, "firmware.bin") == 0);
  assert(axtp_stream_registry_active_count(&registry) == 0u);
  assert(axtp_stream_registry_close(&registry, 0x10u, NULL) == false);

  axtp_stream_payload_free(&first);
  axtp_stream_payload_free(&gap);
  axtp_stream_payload_free(&duplicate);
  axtp_stream_payload_free(&unknown);
  axtp_stream_registry_free(&registry);
  return 0;
}
