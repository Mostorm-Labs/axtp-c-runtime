#include "axtp/media_profile.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char* kind_name(axtp_media_kind_t kind) {
  return kind == AXTP_MEDIA_KIND_AUDIO ? "audio" : "video";
}

static axtp_media_kind_t kind_from_stream_info(const axtp_stream_info_t* info) {
  return (info != NULL && info->kind != NULL && strcmp(info->kind, "audio") == 0) ? AXTP_MEDIA_KIND_AUDIO : AXTP_MEDIA_KIND_VIDEO;
}

static bool producer_open_enabled(axtp_media_open_mode_t mode) {
  return mode == AXTP_MEDIA_OPEN_MODE_PRODUCER_OPEN || mode == AXTP_MEDIA_OPEN_MODE_BOTH;
}

static bool media_enabled(const axtp_media_stream_registry_t* registry, axtp_media_kind_t kind) {
  return kind == AXTP_MEDIA_KIND_VIDEO ? registry->options.accept_video : registry->options.accept_audio;
}

static const char* source_for(const axtp_media_stream_registry_t* registry, axtp_media_kind_t kind) {
  const char* source = registry->options.source;
  if (source == NULL || source[0] == '\0' || strcmp(source, "wireless_cast") == 0) {
    return kind == AXTP_MEDIA_KIND_VIDEO ? "wireless_cast_video" : "wireless_cast_audio";
  }
  return source;
}

static void json_string_or(const char* text, const char* key, const char* fallback, char* out, size_t out_capacity) {
  if (out_capacity == 0) {
    return;
  }
  const char* value = NULL;
  char pattern[64];
  snprintf(pattern, sizeof(pattern), "\"%s\"", key);
  const char* found = text == NULL ? NULL : strstr(text, pattern);
  if (found != NULL) {
    const char* colon = strchr(found + strlen(pattern), ':');
    if (colon != NULL) {
      const char* quote = strchr(colon, '"');
      if (quote != NULL) {
        value = quote + 1;
      }
    }
  }
  if (value == NULL) {
    value = fallback == NULL ? "" : fallback;
  }
  size_t i = 0;
  while (i + 1 < out_capacity && value[i] != '\0' && value[i] != '"') {
    out[i] = value[i];
    ++i;
  }
  out[i] = '\0';
}

static uint32_t json_u32_or(const char* text, const char* key, uint32_t fallback) {
  char pattern[64];
  snprintf(pattern, sizeof(pattern), "\"%s\"", key);
  const char* found = text == NULL ? NULL : strstr(text, pattern);
  if (found == NULL) {
    return fallback;
  }
  const char* colon = strchr(found + strlen(pattern), ':');
  if (colon == NULL) {
    return fallback;
  }
  char* end = NULL;
  unsigned long value = strtoul(colon + 1, &end, 10);
  if (end == colon + 1 || value > 0xFFFFFFFFul) {
    return fallback;
  }
  return (uint32_t)value;
}

static void make_media_info(const axtp_stream_info_t* info, axtp_media_stream_info_t* media) {
  memset(media, 0, sizeof(*media));
  media->kind = kind_from_stream_info(info);
  media->stream_id = info->stream_id;
  media->source = info->source;
  media->codec = info->payload_format;
  media->stream_profile = info->stream_profile;
  media->cursor_unit = info->cursor_unit;
}

static void stream_opened(const axtp_stream_info_t* info, void* user_data) {
  axtp_media_stream_registry_t* registry = (axtp_media_stream_registry_t*)user_data;
  if (registry->options.stream_sink != NULL && registry->options.stream_sink->on_stream_opened != NULL) {
    axtp_media_stream_info_t media;
    make_media_info(info, &media);
    registry->options.stream_sink->on_stream_opened(&media, registry->options.stream_sink->user_data);
  }
}

static void stream_chunk(const axtp_stream_info_t* info, const axtp_stream_payload_t* stream, void* user_data) {
  axtp_media_stream_registry_t* registry = (axtp_media_stream_registry_t*)user_data;
  const axtp_media_kind_t kind = kind_from_stream_info(info);
  if (kind == AXTP_MEDIA_KIND_VIDEO) {
    registry->stats.video_chunks += 1u;
    registry->stats.video_bytes += stream->data_len;
  } else {
    registry->stats.audio_chunks += 1u;
    registry->stats.audio_bytes += stream->data_len;
  }
  if (registry->options.stream_sink != NULL && registry->options.stream_sink->on_stream_chunk != NULL) {
    registry->options.stream_sink->on_stream_chunk(kind, stream, registry->options.stream_sink->user_data);
  }
}

static void stream_closed(const axtp_stream_info_t* info, void* user_data) {
  axtp_media_stream_registry_t* registry = (axtp_media_stream_registry_t*)user_data;
  if (registry->options.stream_sink != NULL && registry->options.stream_sink->on_stream_closed != NULL) {
    registry->options.stream_sink->on_stream_closed(kind_from_stream_info(info), info->stream_id, registry->options.stream_sink->user_data);
  }
}

void axtp_media_host_options_init(axtp_media_host_options_t* options) {
  if (options == NULL) {
    return;
  }
  memset(options, 0, sizeof(*options));
  options->accept_video = true;
  options->accept_audio = true;
  options->open_mode = AXTP_MEDIA_OPEN_MODE_RECEIVER_PULL;
  options->source = "wireless_cast";
  options->audio_format = "adts";
  options->audio_sample_rate = 48000u;
  options->audio_channels = 1u;
}

void axtp_media_stream_registry_init(axtp_media_stream_registry_t* registry, const axtp_media_host_options_t* options) {
  if (registry == NULL) {
    return;
  }
  memset(registry, 0, sizeof(*registry));
  if (options != NULL) {
    registry->options = *options;
  } else {
    axtp_media_host_options_init(&registry->options);
  }
  registry->next_video_stream_id = 0x1001u;
  registry->next_audio_stream_id = 0x2001u;
  axtp_stream_sink_t sink;
  memset(&sink, 0, sizeof(sink));
  sink.on_stream_opened = stream_opened;
  sink.on_stream_chunk = stream_chunk;
  sink.on_stream_closed = stream_closed;
  sink.user_data = registry;
  axtp_stream_registry_init(&registry->streams, &sink);
}

void axtp_media_stream_registry_free(axtp_media_stream_registry_t* registry) {
  if (registry != NULL) {
    axtp_stream_registry_free(&registry->streams);
    memset(registry, 0, sizeof(*registry));
  }
}

static uint16_t open_accepted(axtp_media_stream_registry_t* registry,
                              axtp_media_kind_t kind,
                              uint32_t stream_id,
                              const char* source,
                              const char* peer_role,
                              const char* codec,
                              const char* stream_profile,
                              const char* cursor_unit,
                              const char* extra_json,
                              char* body,
                              size_t body_capacity) {
  if (extra_json == NULL) {
    extra_json = "";
  }
  snprintf(body,
           body_capacity,
           "{\"streamId\":%u,\"state\":\"streaming\",\"source\":\"%s\",\"peerRole\":\"%s\",\"codec\":\"%s\",\"streamProfile\":\"%s\",\"cursorUnit\":\"%s\"%s%s}",
           stream_id,
           source,
           peer_role,
           codec,
           stream_profile,
           cursor_unit,
           extra_json[0] == '\0' ? "" : ",",
           extra_json);

  axtp_stream_info_t info;
  axtp_stream_info_init(&info);
  info.stream_id = stream_id;
  info.kind = kind_name(kind);
  info.source = source;
  info.payload_format = codec;
  info.stream_profile = stream_profile;
  info.cursor_unit = cursor_unit;
  return axtp_stream_registry_register(&registry->streams, &info, true);
}

static uint16_t accept_producer_open(axtp_media_stream_registry_t* registry, axtp_media_kind_t kind, const char* params, char* body, size_t body_capacity) {
  if (!producer_open_enabled(registry->options.open_mode)) {
    return AXTP_ERROR_CODE_RPC_PARAM_INVALID;
  }
  if (!media_enabled(registry, kind)) {
    return AXTP_ERROR_CODE_NOT_SUPPORTED;
  }

  char source[128];
  char peer_role[32];
  char codec[32];
  json_string_or(params, "source", source_for(registry, kind), source, sizeof(source));
  json_string_or(params, "peerRole", "receiver", peer_role, sizeof(peer_role));

  if (kind == AXTP_MEDIA_KIND_VIDEO) {
    json_string_or(params, "codec", "h264", codec, sizeof(codec));
    if (strcmp(codec, "h264") != 0) {
      return AXTP_ERROR_CODE_MEDIA_CODEC_UNSUPPORTED;
    }
    return open_accepted(registry,
                         kind,
                         registry->next_video_stream_id++,
                         source,
                         peer_role,
                         "h264",
                         "media.video",
                         "timestampUs",
                         "\"codecFormat\":\"annexb\",\"parameterSetsInKeyFrame\":true",
                         body,
                         body_capacity);
  }

  char transport_format[32];
  json_string_or(params, "codec", "aac", codec, sizeof(codec));
  json_string_or(params, "transportFormat", registry->options.audio_format == NULL ? "adts" : registry->options.audio_format, transport_format, sizeof(transport_format));
  if (strcmp(codec, "aac") != 0 || strcmp(transport_format, "adts") != 0) {
    return AXTP_ERROR_CODE_MEDIA_CODEC_UNSUPPORTED;
  }
  char extra[128];
  snprintf(extra,
           sizeof(extra),
           "\"transportFormat\":\"%s\",\"sampleRate\":%u,\"channels\":%u",
           transport_format,
           json_u32_or(params, "sampleRate", registry->options.audio_sample_rate == 0 ? 48000u : registry->options.audio_sample_rate),
           json_u32_or(params, "channels", registry->options.audio_channels == 0 ? 1u : registry->options.audio_channels));
  return open_accepted(registry,
                       kind,
                       registry->next_audio_stream_id++,
                       source,
                       peer_role,
                       "aac",
                       "media.audio",
                       "timestampUs",
                       extra,
                       body,
                       body_capacity);
}

static uint16_t close_stream(axtp_media_stream_registry_t* registry, axtp_media_kind_t kind, const char* params, char* body, size_t body_capacity) {
  const uint32_t stream_id = json_u32_or(params, "streamId", 0);
  if (stream_id == 0) {
    return AXTP_ERROR_CODE_RPC_PARAM_MISSING;
  }
  bool already_closed = true;
  axtp_stream_info_t info;
  if (axtp_stream_registry_find(&registry->streams, stream_id, &info)) {
    already_closed = false;
    if (kind_from_stream_info(&info) != kind) {
      return AXTP_ERROR_CODE_STREAM_NOT_FOUND;
    }
    (void)axtp_stream_registry_close(&registry->streams, stream_id, NULL);
  }
  snprintf(body,
           body_capacity,
           "{\"streamId\":%u,\"state\":\"closed\",\"alreadyClosed\":%s}",
           stream_id,
           already_closed ? "true" : "false");
  return AXTP_ERROR_CODE_SUCCESS;
}

void axtp_media_stream_registry_handle(axtp_media_stream_registry_t* registry, const axtp_stream_payload_t* stream) {
  if (registry == NULL) {
    return;
  }
  axtp_stream_registry_handle(&registry->streams, stream);
  axtp_stream_stats_t stats = axtp_stream_registry_stats(&registry->streams);
  registry->stats.unknown_chunks = stats.unknown_chunks;
  registry->stats.seq_gaps = stats.seq_gaps;
  registry->stats.duplicate_seq = stats.duplicate_seq;
}

axtp_media_stream_stats_t axtp_media_stream_registry_stats(const axtp_media_stream_registry_t* registry) {
  axtp_media_stream_stats_t stats;
  memset(&stats, 0, sizeof(stats));
  if (registry != NULL) {
    stats = registry->stats;
    axtp_stream_stats_t stream_stats = axtp_stream_registry_stats(&registry->streams);
    stats.unknown_chunks = stream_stats.unknown_chunks;
    stats.seq_gaps = stream_stats.seq_gaps;
    stats.duplicate_seq = stream_stats.duplicate_seq;
  }
  return stats;
}

size_t axtp_media_stream_registry_active_count(const axtp_media_stream_registry_t* registry) {
  return registry == NULL ? 0u : axtp_stream_registry_active_count(&registry->streams);
}

static axtp_status_t media_raw_handler(const axtp_rpc_context_t* context, const axtp_rpc_payload_t* request, axtp_rpc_payload_t* response, void* user_data) {
  (void)context;
  axtp_media_stream_registry_t* registry = (axtp_media_stream_registry_t*)user_data;
  char params[4096];
  size_t params_len = request->body_len < sizeof(params) - 1 ? request->body_len : sizeof(params) - 1;
  memcpy(params, request->body, params_len);
  params[params_len] = '\0';

  char body[1024];
  body[0] = '\0';
  uint16_t status = AXTP_ERROR_CODE_RPC_METHOD_NOT_FOUND;
  switch (request->method_or_event_id) {
    case AXTP_METHOD_ID_VIDEO_OPEN_STREAM:
      status = accept_producer_open(registry, AXTP_MEDIA_KIND_VIDEO, params, body, sizeof(body));
      break;
    case AXTP_METHOD_ID_AUDIO_OPEN_STREAM:
      status = accept_producer_open(registry, AXTP_MEDIA_KIND_AUDIO, params, body, sizeof(body));
      break;
    case AXTP_METHOD_ID_VIDEO_CLOSE_STREAM:
      status = close_stream(registry, AXTP_MEDIA_KIND_VIDEO, params, body, sizeof(body));
      break;
    case AXTP_METHOD_ID_AUDIO_CLOSE_STREAM:
      status = close_stream(registry, AXTP_MEDIA_KIND_AUDIO, params, body, sizeof(body));
      break;
  }

  response->encoding = AXTP_RPC_ENCODING_JSON;
  response->body_encoding = AXTP_RPC_BODY_ENCODING_NONE;
  response->status_code = status;
  if (status == AXTP_ERROR_CODE_SUCCESS) {
    return axtp_rpc_payload_set_body(response, (const uint8_t*)body, strlen(body));
  }
  return AXTP_STATUS_OK;
}

static axtp_status_t media_stream_handler(const axtp_rpc_context_t* context, const axtp_stream_payload_t* stream, void* user_data) {
  (void)context;
  axtp_media_stream_registry_handle((axtp_media_stream_registry_t*)user_data, stream);
  return AXTP_STATUS_OK;
}

axtp_status_t axtp_install_media_host_handlers(axtp_broker_t* broker, axtp_media_stream_registry_t* registry) {
  if (broker == NULL || registry == NULL) {
    return AXTP_STATUS_INVALID_ARGUMENT;
  }
  axtp_status_t status = axtp_broker_register_raw_method(broker, AXTP_METHOD_ID_VIDEO_OPEN_STREAM, media_raw_handler, registry);
  if (status != AXTP_STATUS_OK) return status;
  status = axtp_broker_register_raw_method(broker, AXTP_METHOD_ID_AUDIO_OPEN_STREAM, media_raw_handler, registry);
  if (status != AXTP_STATUS_OK) return status;
  status = axtp_broker_register_raw_method(broker, AXTP_METHOD_ID_VIDEO_CLOSE_STREAM, media_raw_handler, registry);
  if (status != AXTP_STATUS_OK) return status;
  status = axtp_broker_register_raw_method(broker, AXTP_METHOD_ID_AUDIO_CLOSE_STREAM, media_raw_handler, registry);
  if (status != AXTP_STATUS_OK) return status;
  return axtp_broker_register_stream_handler(broker, media_stream_handler, registry);
}
