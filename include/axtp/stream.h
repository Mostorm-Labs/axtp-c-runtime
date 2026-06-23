#ifndef AXTP_STREAM_H
#define AXTP_STREAM_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "axtp/model.h"

#ifdef __cplusplus
extern "C" {
#endif

#define AXTP_STREAM_REGISTRY_CAPACITY 16u
#define AXTP_STREAM_KIND_CAPACITY 32u
#define AXTP_STREAM_SOURCE_CAPACITY 128u
#define AXTP_STREAM_PROFILE_CAPACITY 64u
#define AXTP_STREAM_FORMAT_CAPACITY 64u

typedef struct {
  uint32_t stream_id;
  const char* kind;
  const char* source;
  const char* stream_profile;
  const char* cursor_unit;
  const char* payload_format;
} axtp_stream_info_t;

typedef struct {
  uint32_t stream_id;
  const char* kind;
  const char* source;
  const char* stream_profile;
} axtp_active_stream_t;

typedef struct {
  uint64_t chunks;
  uint64_t bytes;
  uint64_t unknown_chunks;
  uint64_t seq_gaps;
  uint64_t duplicate_seq;
} axtp_stream_stats_t;

typedef void (*axtp_stream_opened_fn)(const axtp_stream_info_t* info, void* user_data);
typedef void (*axtp_stream_chunk_fn)(const axtp_stream_info_t* info, const axtp_stream_payload_t* stream, void* user_data);
typedef void (*axtp_stream_closed_fn)(const axtp_stream_info_t* info, void* user_data);

typedef struct {
  axtp_stream_opened_fn on_stream_opened;
  axtp_stream_chunk_fn on_stream_chunk;
  axtp_stream_closed_fn on_stream_closed;
  void* user_data;
} axtp_stream_sink_t;

typedef struct {
  axtp_stream_info_t info;
  char kind_storage[AXTP_STREAM_KIND_CAPACITY];
  char source_storage[AXTP_STREAM_SOURCE_CAPACITY];
  char stream_profile_storage[AXTP_STREAM_PROFILE_CAPACITY];
  char cursor_unit_storage[AXTP_STREAM_PROFILE_CAPACITY];
  char payload_format_storage[AXTP_STREAM_FORMAT_CAPACITY];
  uint32_t expected_seq;
  bool has_seq;
  uint64_t chunks;
  uint64_t bytes;
  bool active;
} axtp_stream_context_t;

typedef struct {
  axtp_stream_sink_t sink;
  axtp_stream_context_t streams[AXTP_STREAM_REGISTRY_CAPACITY];
  axtp_stream_context_t last_closed;
  axtp_stream_stats_t stats;
} axtp_stream_registry_t;

void axtp_stream_info_init(axtp_stream_info_t* info);
void axtp_stream_registry_init(axtp_stream_registry_t* registry, const axtp_stream_sink_t* sink);
void axtp_stream_registry_free(axtp_stream_registry_t* registry);
bool axtp_stream_registry_has_stream(const axtp_stream_registry_t* registry, uint32_t stream_id);
bool axtp_stream_registry_has_open_stream(const axtp_stream_registry_t* registry, const char* kind, const char* source);
uint16_t axtp_stream_registry_register(axtp_stream_registry_t* registry, const axtp_stream_info_t* info, bool reject_duplicate_kind_source);
bool axtp_stream_registry_close(axtp_stream_registry_t* registry, uint32_t stream_id, axtp_stream_info_t* closed_info);
void axtp_stream_registry_handle(axtp_stream_registry_t* registry, const axtp_stream_payload_t* stream);
axtp_stream_stats_t axtp_stream_registry_stats(const axtp_stream_registry_t* registry);
size_t axtp_stream_registry_active_count(const axtp_stream_registry_t* registry);
bool axtp_stream_registry_find(const axtp_stream_registry_t* registry, uint32_t stream_id, axtp_stream_info_t* out);

#ifdef __cplusplus
}
#endif

#endif
