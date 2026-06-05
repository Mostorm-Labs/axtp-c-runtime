#ifndef AXTP_SDK_H
#define AXTP_SDK_H

#include "axtp/endpoint.h"
#include "axtp/testing/mock_transport.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct axtp_client axtp_client_t;
typedef struct axtp_server axtp_server_t;

axtp_client_t* axtp_client_new(void);
void axtp_client_free(axtp_client_t* client);
axtp_status_t axtp_client_attach_transport(axtp_client_t* client, const axtp_transport_t* transport);
axtp_status_t axtp_client_poll(axtp_client_t* client);
axtp_status_t axtp_client_send_json(axtp_client_t* client, const char* method_name, const char* params_json, uint32_t* request_id);
axtp_status_t axtp_client_try_take_json_response(axtp_client_t* client, uint32_t request_id, char* out, size_t out_capacity);

axtp_server_t* axtp_server_new(void);
void axtp_server_free(axtp_server_t* server);
axtp_status_t axtp_server_attach_transport(axtp_server_t* server, const axtp_transport_t* transport);
axtp_status_t axtp_server_poll(axtp_server_t* server);
axtp_status_t axtp_server_on_json(axtp_server_t* server, const char* method_name, axtp_json_method_handler_fn handler, void* user_data);

#ifdef __cplusplus
}
#endif

#endif
