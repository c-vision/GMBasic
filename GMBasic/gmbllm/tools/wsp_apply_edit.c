/* wsp_apply_edit.c — applica in C99 una edit "sparse" del repo lm_understand-logs (edit.npz + manifest.json) a un
 * checkpoint safetensors BF16 e materializza il risultato (W3). Sostituisce lo script Python degli autori
 * (apply_edit.py) per la forma "sparse" (indici piatti + valori bf16); la forma "lowrank" non è supportata.
 * Verifica per ogni tensore lo sha256 della base E quello del tensore ricostruito contro il manifest.
 * Usa il comando di sistema `unzip -p` per estrarre i membri .npy dallo zip (nessuna libreria, nessun Python).
 * Uso: wsp_apply_edit <dir_base> <dir_edit> <dir_out> [prefisso_manifest prefisso_store]
 * (il rinomina serve per checkpoint MLX: "model.language_model." -> "language_model.model.")
 * Compilazione: cc -std=c99 -Iinclude tools/wsp_apply_edit.c src/llmwsp_store.c src/llmwsp_mat.c */
#define _POSIX_C_SOURCE 200809L
#include "llmwsp_mat.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned char *unzip_member(const char *zip, const char *member, size_t *n) {
    char cmd[2048]; snprintf(cmd, sizeof cmd, "unzip -p '%s' '%s' 2>/dev/null", zip, member);
    FILE *p = popen(cmd, "r");
    if (!p) return NULL;
    size_t cap = 1 << 16, len = 0; unsigned char *b = (unsigned char *)malloc(cap);
    size_t r;
    while (b && (r = fread(b + len, 1, cap - len, p)) > 0) { len += r; if (len == cap) { cap *= 2; b = (unsigned char *)realloc(b, cap); } }
    pclose(p);
    *n = len;
    return b;
}

/* restituisce puntatore ai dati dopo l'header .npy; *esz = dimensione elemento; *count = numero elementi */
static const unsigned char *npy_data(const unsigned char *b, size_t n, size_t *esz, size_t *count, int *is_signed) {
    if (n < 10 || memcmp(b, "\x93NUMPY", 6) != 0) return NULL;
    size_t hl = b[8] | ((size_t)b[9] << 8), off = 10;
    if (b[6] >= 2) { hl = b[8] | ((size_t)b[9] << 8) | ((size_t)b[10] << 16) | ((size_t)b[11] << 24); off = 12; }
    if (off + hl > n) return NULL;
    char hdr[512]; size_t k = hl < 511 ? hl : 511; memcpy(hdr, b + off, k); hdr[k] = 0;
    const char *d = strstr(hdr, "'descr'");
    if (!d || !strstr(hdr, "'fortran_order': False")) return NULL;
    d = strchr(d + 7, '<'); if (!d) return NULL;
    *is_signed = d[1] == 'i';
    *esz = (size_t)(d[2] - '0');
    if (*esz != 1 && *esz != 2 && *esz != 4 && *esz != 8) return NULL;
    *count = (n - off - hl) / *esz;
    return b + off + hl;
}

static char *slurp(const char *path) {
    FILE *f = fopen(path, "rb"); if (!f) return NULL;
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    char *b = (char *)malloc((size_t)n + 1);
    if (!b || fread(b, 1, (size_t)n, f) != (size_t)n) { free(b); fclose(f); return NULL; }
    fclose(f); b[n] = 0; return b;
}

static int field(const char *from, const char *key, char *out, size_t cap) {   /* "key": "valore" dopo from */
    char pat[64]; snprintf(pat, sizeof pat, "\"%s\"", key);
    const char *p = strstr(from, pat); if (!p) return 0;
    p = strchr(p + strlen(pat), '"'); if (!p) return 0;
    const char *e = strchr(p + 1, '"'); if (!e) return 0;
    size_t n = (size_t)(e - p - 1); if (n >= cap) n = cap - 1;
    memcpy(out, p + 1, n); out[n] = 0; return 1;
}

int main(int argc, char **argv) {
    if (argc < 4) { fprintf(stderr, "uso: %s <dir_base> <dir_edit> <dir_out> [prefisso_manifest prefisso_store]\n", argv[0]); return 2; }
    char err[400], p[1100];
    llmwsp_store *s;
    if (llmwsp_store_open(argv[1], &s, err, sizeof err) < 0) { fprintf(stderr, "store: %s\n", err); return 1; }
    snprintf(p, sizeof p, "%s/manifest.json", argv[2]);
    char *man = slurp(p);
    if (!man) { fprintf(stderr, "manifest illeggibile\n"); return 1; }
    snprintf(p, sizeof p, "%s/edit.npz", argv[2]);
    char zip[1100]; snprintf(zip, sizeof zip, "%s", p);
    llmwsp_edit ed[16]; unsigned char *bufs[16]; int ne = 0; int fail = 0;
    const char *cur = man;
    for (;;) {
        const char *nm = strstr(cur, "\"name\""); if (!nm) break;
        char name[300], form[32], bsha[80], esha[80], c1[32];
        const char *seg = nm; cur = nm + 6;
        if (!field(seg, "name", name, sizeof name) || !field(seg, "base_sha256", bsha, sizeof bsha) || !field(seg, "edited_sha256", esha, sizeof esha) || !field(seg, "form", form, sizeof form)) break;
        (void)c1;
        if (strcmp(form, "sparse") != 0) { fprintf(stderr, "forma '%s' non supportata (%s)\n", form, name); return 1; }
        char sname[400]; snprintf(sname, sizeof sname, "%s", name);
        if (argc >= 6 && strncmp(name, argv[4], strlen(argv[4])) == 0) snprintf(sname, sizeof sname, "%s%s", argv[5], name + strlen(argv[4]));   /* rinomina prefisso (HF -> MLX) */
        const llmwsp_tensor *t = llmwsp_store_find(s, sname);
        if (!t || strcmp(t->dtype, "BF16") != 0) { fprintf(stderr, "tensore assente o non BF16: %s\n", name); return 1; }
        char h[65]; llmwsp_tensor_sha256(s, t, h);
        if (strcmp(h, bsha) != 0) { fprintf(stderr, "FAIL: la base differisce dal tensore su cui è stata fatta l'edit: %s\n", name); fail++; }
        unsigned char *raw = (unsigned char *)malloc((size_t)t->nbytes);
        llmwsp_read_bytes(s, t, 0, t->nbytes, raw);
        char m1[64], m2[64]; snprintf(m1, sizeof m1, "t%d_idx.npy", ne); snprintf(m2, sizeof m2, "t%d_val.npy", ne);
        size_t n1, n2, e1, e2, c1n, c2n; int s1, s2;
        unsigned char *z1 = unzip_member(zip, m1, &n1), *z2 = unzip_member(zip, m2, &n2);
        const unsigned char *di = z1 ? npy_data(z1, n1, &e1, &c1n, &s1) : NULL, *dv = z2 ? npy_data(z2, n2, &e2, &c2n, &s2) : NULL;
        if (!di || !dv || c1n != c2n || e2 != 2) { fprintf(stderr, "npy non valido per %s\n", name); return 1; }
        uint64_t total = (uint64_t)t->nbytes / 2;
        for (size_t k = 0; k < c1n; k++) {
            uint64_t idx = 0; memcpy(&idx, di + k * e1, e1);
            if (idx >= total) { fprintf(stderr, "indice fuori limiti (%llu >= %llu)\n", (unsigned long long)idx, (unsigned long long)total); return 1; }
            memcpy(raw + idx * 2, dv + k * 2, 2);
        }
        llmwsp_sha256_buf(raw, (size_t)t->nbytes, h);
        int ok = strcmp(h, esha) == 0;
        printf("%s: %zu voci applicate, sha256 ricostruito %s il manifest\n", name, c1n, ok ? "CORRISPONDE a" : "DIFFERISCE da");
        if (!ok) fail++;
        free(z1); free(z2);
        ed[ne].name = t->name; ed[ne].data = raw; ed[ne].nbytes = t->nbytes; bufs[ne] = raw; ne++;
        if (ne >= 16) break;
    }
    if (fail || ne == 0) { fprintf(stderr, "verifica fallita: nessun checkpoint scritto\n"); return 1; }
    llmwsp_mat_stats st;
    if (llmwsp_materialize(s, argv[1], ed, (size_t)ne, argv[3], &st, err, sizeof err) < 0) { fprintf(stderr, "materialize: %s\n", err); return 1; }
    printf("checkpoint scritto in %s (shard riscritti %d, clonati %d, tensori modificati %d)\n", argv[3], st.shards_rewritten, st.shards_cloned, st.edited_tensors);
    for (int i = 0; i < ne; i++) free(bufs[i]);
    llmwsp_store_close(s); free(man);
    return 0;
}
