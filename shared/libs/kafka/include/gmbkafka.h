/* SPDX-License-Identifier: LicenseRef-PUL-1.1 */
#ifndef GMBKAFKA_H
#define GMBKAFKA_H
#ifdef __cplusplus
extern "C" {
#endif
int gmbkafka_available(void);
int gmbkafka_connect(const char* host, long port, const char* client_id);
int gmbkafka_produce(long h, const char* topic, long partition, const char* value);
int gmbkafka_fetch(long h, const char* topic, long partition, long offset, long timeout_ms);
const char* gmbkafka_last_value(long h);
long gmbkafka_next_offset(long h);
int gmbkafka_disconnect(long h);
#ifdef __cplusplus
}
#endif
#endif
