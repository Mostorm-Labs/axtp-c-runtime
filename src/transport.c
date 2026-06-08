#include "axtp/transport.h"

#include "generated/axtp_ids_generated.h"

axtp_transport_profile_t axtp_default_transport_profile(void) {
  axtp_transport_profile_t profile;
  profile.kind = AXTP_TRANSPORT_KIND_MOCK;
  profile.wire_mode = AXTP_WIRE_MODE_FRAMED_BINARY;
  profile.default_rpc_encoding = AXTP_RPC_ENCODING_JSON;
  profile.preferred_frame_size = 4096;
  return profile;
}
