#include <stdio.h>
#include <ctype.h>

/* C#: ReplicationSchemaMetadata — chiavi gen/isch/gms, JSON piatto. */

void ReplicationSchemaMetadata_ForIsam(int64_t generation, const char* ischB64, char* out, int cap)
{
    snprintf(out, (size_t)cap, "{\"gen\":%lld,\"isch\":\"%s\"}", (long long)generation, ischB64 ? ischB64 : "");
}

void ReplicationSchemaMetadata_ForDbfGms(int64_t generation, const char* gmsB64, char* out, int cap)
{
    snprintf(out, (size_t)cap, "{\"gen\":%lld,\"gms\":\"%s\"}", (long long)generation, gmsB64 ? gmsB64 : "");
}

int64_t ReplicationSchemaMetadata_RequireGeneration(const char* json)
{
    const char* p;
    if (!json) return -1;
    p = strstr(json, "\"gen\"");
    if (!p) return -1;
    p = strchr(p, ':');
    if (!p) return -1;
    return (int64_t)atoll(p + 1);
}

int ReplicationSchemaMetadata_RequireBase64(const char* json, const char* key, char* out, int cap)
{
    char pat[32];
    const char* p;
    int n = 0;
    if (!json || !key || !out || cap <= 0) return GMB_DB_ARG;
    snprintf(pat, sizeof(pat), "\"%s\"", key);
    p = strstr(json, pat);
    if (!p) return GMB_DB_ERR;
    p = strchr(p, ':');
    if (!p) return GMB_DB_ERR;
    p++;
    while (*p == ' ') p++;
    if (*p != '"') return GMB_DB_ERR;
    p++;
    while (*p && *p != '"' && n < cap - 1) out[n++] = *p++;
    out[n] = 0;
    return GMB_DB_OK;
}

static const char b64tab[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static int b64_val(int c)
{
    const char* p = strchr(b64tab, c);
    if (p) return (int)(p - b64tab);
    return -1;
}

int ReplicationSchemaMetadata_DecodeBase64(const char* text, uint8_t* out, int cap, int* nOut)
{
    int i = 0, n = 0, v, acc = 0, bits = 0;
    if (nOut) *nOut = 0;
    if (!text || !out) return GMB_DB_ARG;
    while (text[i]) {
        unsigned char c = (unsigned char)text[i++];
        if (c == '=' || isspace(c)) continue;
        v = b64_val(c);
        if (v < 0) return GMB_DB_ERR;
        acc = (acc << 6) | v;
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            if (n >= cap) return GMB_DB_ERR;
            out[n++] = (uint8_t)((acc >> bits) & 0xFF);
        }
    }
    if (nOut) *nOut = n;
    return GMB_DB_OK;
}

static const char b64enc[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

void ReplicationSchemaMetadata_EncodeBase64(const uint8_t* data, int len, char* out, int cap)
{
    int i = 0, n = 0;
    if (!out || cap <= 0) return;
    out[0] = 0;
    if (!data || len < 0) return;
    while (i < len && n + 5 < cap) {
        int a = data[i++];
        int b = i < len ? data[i++] : 0;
        int c = i < len ? data[i++] : 0;
        int triple = (a << 16) | (b << 8) | c;
        out[n++] = b64enc[(triple >> 18) & 63];
        out[n++] = b64enc[(triple >> 12) & 63];
        out[n++] = (i - 2 < len) ? b64enc[(triple >> 6) & 63] : '=';
        out[n++] = (i - 1 < len) ? b64enc[triple & 63] : '=';
    }
    out[n] = 0;
}
