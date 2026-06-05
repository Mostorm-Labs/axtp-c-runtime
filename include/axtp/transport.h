#ifndef AXTP_TRANSPORT_H
#define AXTP_TRANSPORT_H

#include <stddef.h>
#include <stdint.h>

#include "axtp/status.h"

#ifdef __cplusplus
extern "C" {
#endif

#define AXTP_TRANSPORT_KIND_MOCK 6u
#define AXTP_WIRE_MODE_FRAMED_BINARY 1u

typedef void (*axtp_byte_sink_fn)(const uint8_t* data, size_t len, void* user_data);

typedef struct {
  uint8_t kind;
  uint8_t wire_mode;
  uint8_t default_rpc_encoding;
  size_t preferred_frame_size;
} axtp_transport_profile_t;

typedef struct {
  void* impl;
  void (*bind)(void* impl, axtp_byte_sink_fn sink, void* user_data);
  axtp_status_t (*open)(void* impl);
  void (*close)(void* impl);
  axtp_status_t (*send_bytes)(void* impl, const uint8_t* data, size_t len);
  axtp_transport_profile_t (*profile)(void* impl);
} axtp_transport_t;

axtp_transport_profile_t axtp_default_transport_profile(void);

#ifdef __cplusplus
}
#endif

#endif
