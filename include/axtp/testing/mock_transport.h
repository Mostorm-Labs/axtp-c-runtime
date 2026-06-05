#ifndef AXTP_TESTING_MOCK_TRANSPORT_H
#define AXTP_TESTING_MOCK_TRANSPORT_H

#include <stddef.h>
#include <stdint.h>

#include "axtp/transport.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct axtp_mock_transport axtp_mock_transport_t;

axtp_mock_transport_t* axtp_mock_transport_new(void);
void axtp_mock_transport_free(axtp_mock_transport_t* transport);
axtp_transport_t axtp_mock_transport_as_transport(axtp_mock_transport_t* transport);
axtp_status_t axtp_mock_transport_inject_incoming(axtp_mock_transport_t* transport, const uint8_t* data, size_t len);
axtp_status_t axtp_mock_transport_try_pop_outgoing(axtp_mock_transport_t* transport, uint8_t** out, size_t* out_len);
size_t axtp_mock_transport_queued_outgoing_count(const axtp_mock_transport_t* transport);
int axtp_mock_transport_is_open(const axtp_mock_transport_t* transport);

#ifdef __cplusplus
}
#endif

#endif
