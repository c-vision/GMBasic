/* SPDX-License-Identifier: PUL-1.1 */
#ifndef GMBMQTT_H
#define GMBMQTT_H
#ifdef __cplusplus
extern "C" {
#endif
int gmbmqtt_available(void);
int gmbmqtt_connect(const char* host, long port, const char* client_id);
int gmbmqtt_publish(long h, const char* topic, const char* payload);
int gmbmqtt_subscribe(long h, const char* topic);
int gmbmqtt_receive(long h, long timeout_ms);
const char* gmbmqtt_last_topic(long h);
const char* gmbmqtt_last_payload(long h);
int gmbmqtt_disconnect(long h);
#ifdef __cplusplus
}
#endif
#endif
