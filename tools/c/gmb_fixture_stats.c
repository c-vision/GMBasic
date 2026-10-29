/* gmb_fixture_stats.c — expected-stats.json per fixture codec (da gmb_fixture_stats.py).
 * Uso: gmb_fixture_stats <fixture-dir> [expected_f32.bin] [decoded.bin]
 * C99, nessuna dipendenza Python. SHA256: OpenSSL (-lcrypto). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>
#include <sys/stat.h>
#include <openssl/sha.h>

static int path_join(char *out, size_t n, const char *d, const char *f) {
    return snprintf(out, n, "%s/%s", d, f) < (int)n;
}
static int read_all(const char *p, unsigned char **buf, size_t *len) {
    FILE *f = fopen(p, "rb"); if (!f) return 0;
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return 0; }
    long n = ftell(f); if (n < 0) { fclose(f); return 0; }
    rewind(f);
    unsigned char *b = (unsigned char*)malloc((size_t)n ? (size_t)n : 1);
    if (!b) { fclose(f); return 0; }
    if (n && fread(b, 1, (size_t)n, f) != (size_t)n) { free(b); fclose(f); return 0; }
    fclose(f); *buf = b; *len = (size_t)n; return 1;
}
static void sha_hex(const unsigned char *b, size_t n, char out[65]) {
    unsigned char h[SHA256_DIGEST_LENGTH]; SHA256(b, n, h);
    for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) sprintf(out + i * 2, "%02x", h[i]);
    out[64] = 0;
}
static void num(double v, char *out, size_t n) {
    snprintf(out, n, "%.17g", v);
    if (!strchr(out, '.') && !strchr(out, 'e') && !strchr(out, 'E')) {
        size_t l = strlen(out); if (l + 2 < n) { out[l] = '.'; out[l+1] = '0'; out[l+2] = 0; }
    }
}
int main(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "uso: %s <fixture-dir> [expected_f32.bin] [decoded.bin]\n", argv[0]); return 2; }
    const char *dir = argv[1];
    const char *expn = argc > 2 ? argv[2] : "expected_f32.bin";
    const char *decn = argc > 3 ? argv[3] : NULL;
    char p[4096]; unsigned char *src = NULL; size_t srclen = 0; const char *srcname = NULL;
    const char *cands[] = {"source.bin", "weight.bin", "weight2d.bin"};
    for (int i = 0; i < 3; i++) {
        if (path_join(p, sizeof p, dir, cands[i]) && read_all(p, &src, &srclen)) { srcname = cands[i]; break; }
    }
    char expath[4096], decpath[4096];
    if (!path_join(expath, sizeof expath, dir, expn)) return 2;
    unsigned char *raw = NULL; size_t rawlen = 0;
    if (!read_all(expath, &raw, &rawlen)) { fprintf(stderr, "manca %s\n", expath); free(src); return 2; }
    size_t n = rawlen / 4;
    const float *y = (const float*)raw;
    float fmin = n ? y[0] : 0.0f, fmax = fmin;
    float fsum = 0.0f, fsq = 0.0f;
    for (size_t i = 0; i < n; i++) {
        float v = y[i];
        if (v < fmin) fmin = v; if (v > fmax) fmax = v;
        fsum += v; fsq += v * v;
    }
    char sh_src[65] = "", sh_exp[65] = "";
    if (src) sha_hex(src, srclen, sh_src);
    sha_hex(raw, rawlen, sh_exp);
    unsigned long long ck = 0; for (size_t i = 0; i < srclen; i++) ck = (ck + src[i]) & 0xffffffffULL;
    char outpath[4096]; path_join(outpath, sizeof outpath, dir, "expected-stats.json");
    FILE *o = fopen(outpath, "w"); if (!o) { fprintf(stderr, "scrivo %s: errore\n", outpath); free(src); free(raw); return 2; }
    fprintf(o, "{\n \"dir\": \"%s\",\n", dir);
    if (srcname) {
        fprintf(o, " \"source_file\": \"%s\",\n \"bytes\": %zu,\n \"sha256\": \"%s\",\n \"checksum\": %llu,\n",
                srcname, srclen, sh_src, ck);
    }
    fprintf(o, " \"expected\": {\n  \"numel\": %zu,\n", n);
    char a[64], b[64], c[64], d[64];
    num((double)fmin, a, sizeof a); num((double)fmax, b, sizeof b);
    num(n ? (double)(fsum / (float)n) : 0.0, c, sizeof c);
    num(n ? sqrt((double)(fsq / (float)n)) : 0.0, d, sizeof d);
    fprintf(o, "  \"min\": %s,\n  \"max\": %s,\n  \"mean\": %s,\n  \"rms\": %s,\n  \"sha256\": \"%s\"\n }",
            a, b, c, d, sh_exp);
    if (decn && path_join(decpath, sizeof decpath, dir, decn)) {
        unsigned char *dec = NULL; size_t declen = 0;
        if (read_all(decpath, &dec, &declen) && declen == rawlen) {
            const float *z = (const float*)dec; double ma = 0.0, sa = 0.0, mr = 0.0;
            for (size_t i = 0; i < n; i++) {
                double e = fabs((double)z[i] - (double)y[i]); if (e > ma) ma = e; sa += e;
                double rr = e / (fabs((double)y[i]) + 1e-12); if (rr > mr) mr = rr;
            }
            char e1[64], e2[64], e3[64];
            num(ma, e1, sizeof e1); num(n ? sa / (double)n : 0.0, e2, sizeof e2); num(mr, e3, sizeof e3);
            fprintf(o, ",\n \"error_vs_decode\": {\n  \"max_abs_error\": %s,\n  \"mean_abs_error\": %s,\n  \"max_rel_error\": %s\n }", e1, e2, e3);
        }
        free(dec);
    }
    fprintf(o, "\n}"); fclose(o);
    printf("stats -> %s\n", outpath);
    free(src); free(raw); return 0;
}
