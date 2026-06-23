#ifndef AXTP_MEDIA_PROFILE_H
#define AXTP_MEDIA_PROFILE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "axtp/broker.h"
#include "axtp/stream.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
  AXTP_MEDIA_KIND_VIDEO = 1,
  AXTP_MEDIA_KIND_AUDIO = 2
} axtp_media_kind_t;

typedef enum {
  AXTP_MEDIA_OPEN_MODE_RECEIVER_PULL = 1,
  AXTP_MEDIA_OPEN_MODE_PRODUCER_OPEN = 2,
  AXTP_MEDIA_OPEN_MODE_BOTH = 3
} axtp_media_open_mode_t;

typedef struct {
  axtp_media_kind_t kind;
  uint32_t stream_id;
  const char* source;
  const char* codec;
  const char* stream_profile;
  const char* cursor_unit;
  uint32_t width;
  uint32_t height;
  uint32_t sample_rate;
  uint32_t channels;
} axtp_media_stream_info_t;

typedef struct {
  uint64_t video_chunks;
  uint64_t audio_chunks;
  uint64_t video_bytes;
  uint64_t audio_bytes;
  uint64_t unknown_chunks;
  uint64_t seq_gaps;
  uint64_t duplicate_seq;
} axtp_media_stream_stats_t;

typedef void (*axtp_media_stream_opened_fn)(const axtp_media_stream_info_t* info, void* user_data);
typedef void (*axtp_media_stream_chunk_fn)(axtp_media_kind_t kind, const axtp_stream_payload_t* stream, void* user_data);
typedef void (*axtp_media_stream_closed_fn)(axtp_media_kind_t kind, uint32_t stream_id, void* user_data);

typedef struct {
  axtp_media_stream_opened_fn on_stream_opened;
  axtp_media_stream_chunk_fn on_stream_chunk;
  axtp_media_stream_closed_fn on_stream_closed;
  void* user_data;
} axtp_media_stream_sink_t;

typedef struct {
  bool accept_video;
  bool accept_audio;
  axtp_media_open_mode_t open_mode;
  const char* source;
  const char* audio_format;
  uint32_t audio_sample_rate;
  uint32_t audio_channels;
  axtp_media_stream_sink_t* stream_sink;
} axtp_media_host_options_t;

typedef struct {
  axtp_media_host_options_t options;
  axtp_stream_registry_t streams;
  axtp_media_stream_stats_t stats;
  uint32_t next_video_stream_id;
  uint32_t next_audio_stream_id;
} axtp_media_stream_registry_t;

void axtp_media_host_options_init(axtp_media_host_options_t* options);
void axtp_media_stream_registry_init(axtp_media_stream_registry_t* registry, const axtp_media_host_options_t* options);
void axtp_media_stream_registry_free(axtp_media_stream_registry_t* registry);
void axtp_media_stream_registry_handle(axtp_media_stream_registry_t* registry, const axtp_stream_payload_t* stream);
axtp_media_stream_stats_t axtp_media_stream_registry_stats(const axtp_media_stream_registry_t* registry);
size_t axtp_media_stream_registry_active_count(const axtp_media_stream_registry_t* registry);
axtp_status_t axtp_install_media_host_handlers(axtp_broker_t* broker, axtp_media_stream_registry_t* registry);

#ifdef __cplusplus
}
#endif

#endif
