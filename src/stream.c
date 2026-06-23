#include "axtp/stream.h"

#include <string.h>

static void copy_text(char* dst, size_t capacity, const char* src) {
  if (capacity == 0) {
    return;
  }
  if (src == NULL) {
    src = "";
  }
  strncpy(dst, src, capacity - 1);
  dst[capacity - 1] = '\0';
}

static void bind_context_storage(axtp_stream_context_t* context) {
  context->info.kind = context->kind_storage;
  context->info.source = context->source_storage;
  context->info.stream_profile = context->stream_profile_storage;
  context->info.cursor_unit = context->cursor_unit_storage;
  context->info.payload_format = context->payload_format_storage;
}

static axtp_stream_context_t* find_context(axtp_stream_registry_t* registry, uint32_t stream_id) {
  for (size_t i = 0; i < AXTP_STREAM_REGISTRY_CAPACITY; ++i) {
    if (registry->streams[i].active && registry->streams[i].info.stream_id == stream_id) {
      return &registry->streams[i];
    }
  }
  return NULL;
}

static const axtp_stream_context_t* find_context_const(const axtp_stream_registry_t* registry, uint32_t stream_id) {
  for (size_t i = 0; i < AXTP_STREAM_REGISTRY_CAPACITY; ++i) {
    if (registry->streams[i].active && registry->streams[i].info.stream_id == stream_id) {
      return &registry->streams[i];
    }
  }
  return NULL;
}

void axtp_stream_info_init(axtp_stream_info_t* info) {
  if (info != NULL) {
    memset(info, 0, sizeof(*info));
  }
}

void axtp_stream_registry_init(axtp_stream_registry_t* registry, const axtp_stream_sink_t* sink) {
  if (registry == NULL) {
    return;
  }
  memset(registry, 0, sizeof(*registry));
  if (sink != NULL) {
    registry->sink = *sink;
  }
}

void axtp_stream_registry_free(axtp_stream_registry_t* registry) {
  if (registry != NULL) {
    memset(registry, 0, sizeof(*registry));
  }
}

bool axtp_stream_registry_has_stream(const axtp_stream_registry_t* registry, uint32_t stream_id) {
  return registry != NULL && find_context_const(registry, stream_id) != NULL;
}

bool axtp_stream_registry_has_open_stream(const axtp_stream_registry_t* registry, const char* kind, const char* source) {
  if (registry == NULL) {
    return false;
  }
  if (kind == NULL) {
    kind = "";
  }
  if (source == NULL) {
    source = "";
  }
  for (size_t i = 0; i < AXTP_STREAM_REGISTRY_CAPACITY; ++i) {
    const axtp_stream_context_t* context = &registry->streams[i];
    if (context->active && strcmp(context->info.kind, kind) == 0 && strcmp(context->info.source, source) == 0) {
      return true;
    }
  }
  return false;
}

uint16_t axtp_stream_registry_register(axtp_stream_registry_t* registry, const axtp_stream_info_t* info, bool reject_duplicate_kind_source) {
  if (registry == NULL || info == NULL) {
    return AXTP_ERROR_CODE_INVALID_ARGUMENT;
  }
  if (info->stream_id == 0) {
    return AXTP_ERROR_CODE_STREAM_ID_INVALID;
  }
  if (info->kind == NULL || info->kind[0] == '\0') {
    return AXTP_ERROR_CODE_STREAM_PAYLOAD_INVALID;
  }
  if (axtp_stream_registry_has_stream(registry, info->stream_id)) {
    return AXTP_ERROR_CODE_STREAM_ALREADY_OPEN;
  }
  if (reject_duplicate_kind_source && axtp_stream_registry_has_open_stream(registry, info->kind, info->source)) {
    return AXTP_ERROR_CODE_STREAM_ALREADY_OPEN;
  }

  axtp_stream_context_t* slot = NULL;
  for (size_t i = 0; i < AXTP_STREAM_REGISTRY_CAPACITY; ++i) {
    if (!registry->streams[i].active) {
      slot = &registry->streams[i];
      break;
    }
  }
  if (slot == NULL) {
    return AXTP_ERROR_CODE_RESOURCE_EXHAUSTED;
  }

  memset(slot, 0, sizeof(*slot));
  copy_text(slot->kind_storage, sizeof(slot->kind_storage), info->kind);
  copy_text(slot->source_storage, sizeof(slot->source_storage), info->source);
  copy_text(slot->stream_profile_storage, sizeof(slot->stream_profile_storage), info->stream_profile);
  copy_text(slot->cursor_unit_storage, sizeof(slot->cursor_unit_storage), info->cursor_unit);
  copy_text(slot->payload_format_storage, sizeof(slot->payload_format_storage), info->payload_format);
  slot->info.stream_id = info->stream_id;
  bind_context_storage(slot);
  slot->active = true;
  if (registry->sink.on_stream_opened != NULL) {
    registry->sink.on_stream_opened(&slot->info, registry->sink.user_data);
  }
  return AXTP_ERROR_CODE_SUCCESS;
}

bool axtp_stream_registry_close(axtp_stream_registry_t* registry, uint32_t stream_id, axtp_stream_info_t* closed_info) {
  if (registry == NULL) {
    return false;
  }
  axtp_stream_context_t* context = find_context(registry, stream_id);
  if (context == NULL) {
    return false;
  }
  registry->last_closed = *context;
  bind_context_storage(&registry->last_closed);
  registry->last_closed.active = false;
  if (closed_info != NULL) {
    *closed_info = registry->last_closed.info;
  }
  if (registry->sink.on_stream_closed != NULL) {
    registry->sink.on_stream_closed(&registry->last_closed.info, registry->sink.user_data);
  }
  memset(context, 0, sizeof(*context));
  return true;
}

void axtp_stream_registry_handle(axtp_stream_registry_t* registry, const axtp_stream_payload_t* stream) {
  if (registry == NULL || stream == NULL) {
    return;
  }
  axtp_stream_context_t* context = find_context(registry, stream->stream_id);
  if (context == NULL) {
    registry->stats.unknown_chunks += 1u;
    return;
  }
  if (context->has_seq) {
    if (stream->seq_id == context->expected_seq - 1u) {
      registry->stats.duplicate_seq += 1u;
    } else if (stream->seq_id != context->expected_seq) {
      registry->stats.seq_gaps += 1u;
    }
  }
  context->has_seq = true;
  context->expected_seq = stream->seq_id + 1u;
  context->chunks += 1u;
  context->bytes += stream->data_len;
  registry->stats.chunks += 1u;
  registry->stats.bytes += stream->data_len;
  if (registry->sink.on_stream_chunk != NULL) {
    registry->sink.on_stream_chunk(&context->info, stream, registry->sink.user_data);
  }
}

axtp_stream_stats_t axtp_stream_registry_stats(const axtp_stream_registry_t* registry) {
  axtp_stream_stats_t stats;
  memset(&stats, 0, sizeof(stats));
  if (registry != NULL) {
    stats = registry->stats;
  }
  return stats;
}

size_t axtp_stream_registry_active_count(const axtp_stream_registry_t* registry) {
  size_t count = 0;
  if (registry == NULL) {
    return 0;
  }
  for (size_t i = 0; i < AXTP_STREAM_REGISTRY_CAPACITY; ++i) {
    if (registry->streams[i].active) {
      ++count;
    }
  }
  return count;
}

bool axtp_stream_registry_find(const axtp_stream_registry_t* registry, uint32_t stream_id, axtp_stream_info_t* out) {
  if (registry == NULL || out == NULL) {
    return false;
  }
  const axtp_stream_context_t* context = find_context_const(registry, stream_id);
  if (context == NULL) {
    return false;
  }
  *out = context->info;
  return true;
}
