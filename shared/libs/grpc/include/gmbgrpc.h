/* SPDX-License-Identifier: LicenseRef-PUL-1.1 */
#ifndef GMBGRPC_H
#define GMBGRPC_H
#ifdef __cplusplus
extern "C" {
#endif
int gmbgrpc_available(void);
int gmbgrpc_call(const char* host, long port, const char* path, const char* req_hex);
const char* gmbgrpc_last_response(void);
long gmbgrpc_last_status(void);
#ifdef __cplusplus
}
#endif
#endif
