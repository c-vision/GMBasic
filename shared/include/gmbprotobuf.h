/* SPDX-License-Identifier: LicenseRef-PUL-1.1 */
#ifndef GMBPROTOBUF_H
#define GMBPROTOBUF_H
#ifdef __cplusplus
extern "C" {
#endif
int gmbprotobuf_available(void);
int gmbprotobuf_new(void);
int gmbprotobuf_addint(long h, long field, long value);
int gmbprotobuf_addstring(long h, long field, const char* s);
const char* gmbprotobuf_serialize(long h);
int gmbprotobuf_parse(const char* hex);
int gmbprotobuf_hasfield(long h, long field);
long gmbprotobuf_getint(long h, long field);
const char* gmbprotobuf_getstring(long h, long field);
int gmbprotobuf_free(long h);
#ifdef __cplusplus
}
#endif
#endif
