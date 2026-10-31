/* SPDX-License-Identifier: LicenseRef-PUL-1.1 */
#ifndef GMBNATS_H
#define GMBNATS_H
#ifdef __cplusplus
extern "C" {
#endif
int gmbnats_available(void);
int gmbnats_connect(const char* host, long port, const char* name);
int gmbnats_subscribe(long h, const char* subject);
int gmbnats_publish(long h, const char* subject, const char* payload);
int gmbnats_receive(long h, long timeout_ms);
const char* gmbnats_last_subject(long h);
const char* gmbnats_last_payload(long h);
int gmbnats_disconnect(long h);
#ifdef __cplusplus
}
#endif
#endif
