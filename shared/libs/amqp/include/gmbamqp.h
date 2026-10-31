/* SPDX-License-Identifier: LicenseRef-PUL-1.1 */
#ifndef GMBAMQP_H
#define GMBAMQP_H
#ifdef __cplusplus
extern "C" {
#endif
int gmbamqp_available(void);
int gmbamqp_connect(const char* host, long port, const char* user, const char* pass);
int gmbamqp_declare_queue(long h, const char* queue);
int gmbamqp_publish(long h, const char* exchange, const char* routing_key, const char* body);
int gmbamqp_get(long h, const char* queue, long timeout_ms);
const char* gmbamqp_last_body(long h);
const char* gmbamqp_last_routingkey(long h);
long gmbamqp_last_size(long h);
int gmbamqp_disconnect(long h);
#ifdef __cplusplus
}
#endif
#endif
