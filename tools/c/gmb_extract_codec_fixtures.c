/* gmb_extract_codec_fixtures.c — slice fixture codec da un modello locale
 * (da gmb_extract_codec_fixtures.py). Uso:
 *   gmb_extract_codec_fixtures <model-dir> <codec> <out-dir> [n-elem]
 * C99, json-c + OpenSSL. Non scarica nulla; legge model.safetensors. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <json-c/json.h>
#include <openssl/sha.h>

static int mkdir_p(const char *p) {
    char b[4096]; snprintf(b, sizeof b, "%s", p);
    for (char *q = b + 1; *q; q++) if (*q == '/') { *q = 0; mkdir(b, 0755); *q = '/'; }
    return mkdir(b, 0755) == 0 || 1;
}
static unsigned char *read_all(const char *p, size_t *n) {
    FILE *f = fopen(p, "rb"); if (!f) return NULL;
    fseek(f, 0, SEEK_END); long sz = ftell(f); rewind(f);
    if (sz < 0) { fclose(f); return NULL; }
    unsigned char *b = malloc((size_t)sz ? (size_t)sz : 1);
    if (!b) { fclose(f); return NULL; }
    if (sz && fread(b, 1, (size_t)sz, f) != (size_t)sz) { free(b); fclose(f); return NULL; }
    fclose(f); *n = (size_t)sz; return b;
}
static void sha_hex(const unsigned char *b, size_t n, char out[65]) {
    unsigned char h[SHA256_DIGEST_LENGTH]; SHA256(b, n, h);
    for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) sprintf(out + i*2, "%02x", h[i]);
    out[64] = 0;
}
static int is_weight(const char *k) {
    size_t n = strlen(k);
    return n >= 7 && strcmp(k + n - 7, ".weight") == 0 && strstr(k, "embed") == NULL;
}
static int st_size(const char *d) {
    if (!strcmp(d,"F16")||!strcmp(d,"BF16")) return 2;
    if (!strcmp(d,"F32")||!strcmp(d,"I32")||!strcmp(d,"U32")) return 4;
    if (!strcmp(d,"U8")||!strcmp(d,"I8")) return 1;
    return 0;
}
int main(int argc, char **argv) {
    if (argc < 4) { fprintf(stderr, "uso: %s <model-dir> <codec> <out-dir> [n-elem]\n", argv[0]); return 2; }
    const char *md = argv[1], *codec = argv[2], *out = argv[3];
    mkdir_p(out);
    DIR *d = opendir(md); if (!d) { fprintf(stderr, "dir %s\n", md); return 2; }
    char stname[1024] = ""; struct dirent *de;
    while ((de = readdir(d))) {
        size_t l = strlen(de->d_name);
        if (l > 12 && strcmp(de->d_name + l - 12, ".safetensors") == 0) {
            if (!stname[0] || strcmp(de->d_name, stname) < 0) snprintf(stname, sizeof stname, "%s", de->d_name);
        }
    }
    closedir(d);
    if (!stname[0]) { fprintf(stderr, "nessun .safetensors\n"); return 1; }
    char stpath[4096]; snprintf(stpath, sizeof stpath, "%s/%s", md, stname);
    size_t total = 0; unsigned char *file = read_all(stpath, &total);
    if (!file || total < 8) { fprintf(stderr, "header %s\n", stpath); free(file); return 1; }
    uint64_t hn = 0; for (int i = 0; i < 8; i++) hn |= ((uint64_t)file[i]) << (8*i);
    if (hn > total - 8) { free(file); return 1; }
    char *hdr = malloc((size_t)hn + 1); memcpy(hdr, file + 8, (size_t)hn); hdr[hn] = 0;
    json_object *root = json_tokener_parse(hdr); free(hdr);
    if (!root) { free(file); return 1; }
    const char *best = NULL;
    json_object_object_foreach(root, key, val) {
        (void)val;
        if (is_weight(key) && (!best || strcmp(key, best) < 0)) best = key;
    }
    if (!best) { json_object_put(root); free(file); fprintf(stderr, "nessun .weight\n"); return 1; }
    json_object *v = NULL; json_object_object_get_ex(root, best, &v);
    json_object *jdtype = NULL, *jshape = NULL, *joff = NULL;
    json_object_object_get_ex(v, "dtype", &jdtype);
    json_object_object_get_ex(v, "shape", &jshape);
    json_object_object_get_ex(v, "data_offsets", &joff);
    const char *dtype = jdtype ? json_object_get_string(jdtype) : "";
    int sz = st_size(dtype);
    size_t row = 1;
    if (jshape && json_object_array_length(jshape) > 0)
        row = (size_t)json_object_get_int64(json_object_array_get_idx(jshape, json_object_array_length(jshape)-1));
    uint64_t o0 = 0, o1 = 0;
    if (joff && json_object_array_length(joff) >= 2) {
        o0 = (uint64_t)json_object_get_int64(json_object_array_get_idx(joff,0));
        o1 = (uint64_t)json_object_get_int64(json_object_array_get_idx(joff,1));
    }
    size_t n = 0;
    if (sz > 0 && o1 > o0) {
        n = (size_t)((o1 - o0) / (uint64_t)sz);
        if (n > 2 * row) n = 2 * row;
    }
    if (n == 0) n = 1;
    size_t bytes = n * (size_t)sz;
    if (o0 + bytes > total - 8) { json_object_put(root); free(file); return 1; }
    unsigned char *slice = file + 8 + o0;
    char outsrc[4096]; snprintf(outsrc, sizeof outsrc, "%s/source.bin", out);
    FILE *fo = fopen(outsrc, "wb"); if (!fo) { json_object_put(root); free(file); return 1; }
    fwrite(slice, 1, bytes, fo); fclose(fo);
    char sha[65]; sha_hex(slice, bytes, sha);
    char metap[4096]; snprintf(metap, sizeof metap, "%s/meta.json", out);
    FILE *fm = fopen(metap, "w"); if (!fm) { json_object_put(root); free(file); return 1; }
    const char *model = strrchr(md, '/'); model = model ? model + 1 : md;
    fprintf(fm, "{\n \"model\": \"%s\",\n \"codec\": \"%s\",\n \"tensor\": \"%s\",\n \"dtype\": \"%s\",\n \"shape0\": [",
            model, codec, best, dtype);
    if (jshape) for (size_t i = 0; i < json_object_array_length(jshape); i++)
        fprintf(fm, "%s%lld", i ? ", " : "", (long long)json_object_get_int64(json_object_array_get_idx(jshape, i)));
    fprintf(fm, "],\n \"sha256\": \"%s\",\n \"bytes\": %zu\n}\n", sha, bytes);
    fclose(fm);
    printf("OK %s %s %s -> %s %zu byte\n", codec, best, dtype, out, bytes);
    json_object_put(root); free(file); return 0;
}
