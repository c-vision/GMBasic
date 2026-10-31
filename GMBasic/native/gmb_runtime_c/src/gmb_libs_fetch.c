/* gmb_libs_fetch.c — download ON-DEMAND di UNA libreria nativa (modello "un asset per file").
 *
 * PERCHÉ. Le librerie native non stanno nel repo: stanno su c-vision/GMBasicModules, pubblicate
 * come **un asset per file** (`libgmb_gfx-linux-arm64.so`). Il runtime C deve poterle prendere
 * da solo quando gli servono: `gmb_ffi_dl` prova a caricare `libgmb_gfx.dylib` e, se non c'è,
 * la scarica nella cache (`~/.gmbasic/libs/<rid>/`) e riprova. Stessa cosa per l'eseguibile
 * compilato (che porta accanto `<exe>.libs.txt`, emesso dal build).
 *
 * INDICE USATO (TSV, una riga per file — niente parser JSON nel runtime):
 *
 *     <nome-file>\t<sha256>\t<size>\t<url>
 *
 * es.  libgmb_gfx.dylib	3f9a…  1897512  https://github.com/.../libgmb_gfx-osx-arm64.dylib
 *
 * Il file si cerca in: <dir dell'<exe>/<exe>.libs.txt, poi <exe>/libs.txt, poi CWD/<exe>.libs.txt,
 * poi CWD/libs.txt. Se non c'è, la funzione non fa nulla (comportamento di prima).
 *
 * SICUREZZA. Il download è verificato con sha256 (gmb_sha256_hex) e scritto in modo atomico
 * (`<nome>.part` → rename). Se lo sha non combacia il file viene rimosso e la funzione fallisce:
 * meglio un errore chiaro che una libreria corrotta caricata come buona.
 *
 * DIPENDENZE. libcurl (già linkata da gmb_network.c) e lo sha256 di gmb_crypto.c. Nessuna nuova
 * dipendenza.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>

#include "gmb_runtime.h"

#if __has_include(<curl/curl.h>)
#include <curl/curl.h>
#define GMB_LIBS_HAVE_CURL 1
#endif

/* ── RID della macchina (stessa mappatura di native/cmake/GmbRid.cmake e LibsResolver.Rid) ── */
const char* gmb_libs_rid(void)
{
#if defined(_WIN32)
    return "win-x64";
#elif defined(__APPLE__)
#  if defined(__aarch64__) || defined(__arm64__)
    return "osx-arm64";
#  else
    return "osx-x64";
#  endif
#elif defined(__aarch64__) || defined(__arm64__)
    return "linux-arm64";
#else
    return "linux-x64";
#endif
}

/* ── sha256 di un file (streaming a blocchi: le lib arrivano a ~130 MB) ───────────────────── */
/* gmb_crypto.c espone solo gmb_sha256_hex(buffer,len); per non caricare 130 MB in RAM si legge
 * il file a blocchi e si accumula con una copia locale dell'algoritmo (stessa implementazione). */
typedef struct { unsigned int h[8]; unsigned long long len; unsigned char buf[64]; size_t n; } lf_sha_ctx;

static unsigned int lf_ror(unsigned int x, int n) { return (x >> n) | (x << (32 - n)); }
static const unsigned int lf_k[64] = {
    0x428a2f98u,0x71374491u,0xb5c0fbcfu,0xe9b5dba5u,0x3956c25bu,0x59f111f1u,0x923f82a4u,0xab1c5ed5u,
    0xd807aa98u,0x12835b01u,0x243185beu,0x550c7dc3u,0x72be5d74u,0x80deb1feu,0x9bdc06a7u,0xc19bf174u,
    0xe49b69c1u,0xefbe4786u,0x0fc19dc6u,0x240ca1ccu,0x2de92c6fu,0x4a7484aau,0x5cb0a9dcu,0x76f988dau,
    0x983e5152u,0xa831c66du,0xb00327c8u,0xbf597fc7u,0xc6e00bf3u,0xd5a79147u,0x06ca6351u,0x14292967u,
    0x27b70a85u,0x2e1b2138u,0x4d2c6dfcu,0x53380d13u,0x650a7354u,0x766a0abbu,0x81c2c92eu,0x92722c85u,
    0xa2bfe8a1u,0xa81a664bu,0xc24b8b70u,0xc76c51a3u,0xd192e819u,0xd6990624u,0xf40e3585u,0x106aa070u,
    0x19a4c116u,0x1e376c08u,0x2748774cu,0x34b0bcb5u,0x391c0cb3u,0x4ed8aa4au,0x5b9cca4fu,0x682e6ff3u,
    0x748f82eeu,0x78a5636fu,0x84c87814u,0x8cc70208u,0x90befffau,0xa4506cebu,0xbef9a3f7u,0xc67178f2u };

static void lf_sha_block(lf_sha_ctx* c, const unsigned char* p)
{
    unsigned int w[64], a,b,cc,d,e,f,g,h,t1,t2; int i;
    for (i = 0; i < 16; i++) w[i] = ((unsigned int)p[i*4] << 24) | ((unsigned int)p[i*4+1] << 16) | ((unsigned int)p[i*4+2] << 8) | p[i*4+3];
    for (i = 16; i < 64; i++) {
        unsigned int s0 = lf_ror(w[i-15],7) ^ lf_ror(w[i-15],18) ^ (w[i-15] >> 3);
        unsigned int s1 = lf_ror(w[i-2],17) ^ lf_ror(w[i-2],19) ^ (w[i-2] >> 10);
        w[i] = w[i-16] + s0 + w[i-7] + s1;
    }
    a=c->h[0]; b=c->h[1]; cc=c->h[2]; d=c->h[3]; e=c->h[4]; f=c->h[5]; g=c->h[6]; h=c->h[7];
    for (i = 0; i < 64; i++) {
        t1 = h + (lf_ror(e,6) ^ lf_ror(e,11) ^ lf_ror(e,25)) + ((e & f) ^ (~e & g)) + lf_k[i] + w[i];
        t2 = (lf_ror(a,2) ^ lf_ror(a,13) ^ lf_ror(a,22)) + ((a & b) ^ (a & cc) ^ (b & cc));
        h=g; g=f; f=e; e=d+t1; d=cc; cc=b; b=a; a=t1+t2;
    }
    c->h[0]+=a; c->h[1]+=b; c->h[2]+=cc; c->h[3]+=d; c->h[4]+=e; c->h[5]+=f; c->h[6]+=g; c->h[7]+=h;
}

static void lf_sha_init(lf_sha_ctx* c)
{
    c->h[0]=0x6a09e667u; c->h[1]=0xbb67ae85u; c->h[2]=0x3c6ef372u; c->h[3]=0xa54ff53au;
    c->h[4]=0x510e527fu; c->h[5]=0x9b05688cu; c->h[6]=0x1f83d9abu; c->h[7]=0x5be0cd19u;
    c->len = 0; c->n = 0;
}

static void lf_sha_update(lf_sha_ctx* c, const unsigned char* d, size_t n)
{
    while (n > 0) {
        size_t take = 64 - c->n; if (take > n) take = n;
        memcpy(c->buf + c->n, d, take); c->n += take; d += take; n -= take; c->len += take;
        if (c->n == 64) { lf_sha_block(c, c->buf); c->n = 0; }
    }
}

static void lf_sha_final(lf_sha_ctx* c, unsigned char out[32])
{
    unsigned long long bits = c->len * 8;
    unsigned char pad = 0x80; lf_sha_update(c, &pad, 1);
    unsigned char z = 0; while (c->n != 56) lf_sha_update(c, &z, 1);
    unsigned char lb[8]; for (int i = 0; i < 8; i++) lb[i] = (unsigned char)(bits >> (56 - 8*i));
    lf_sha_update(c, lb, 8);
    for (int i = 0; i < 8; i++) { out[i*4] = (unsigned char)(c->h[i] >> 24); out[i*4+1] = (unsigned char)(c->h[i] >> 16);
                                  out[i*4+2] = (unsigned char)(c->h[i] >> 8); out[i*4+3] = (unsigned char)c->h[i]; }
}

static int lf_file_sha256_hex(const char* path, char out_hex[65])
{
    FILE* f = fopen(path, "rb");
    if (!f) return 0;
    lf_sha_ctx c; lf_sha_init(&c);
    unsigned char buf[64 * 1024]; size_t n;
    while ((n = fread(buf, 1, sizeof buf, f)) > 0) lf_sha_update(&c, buf, n);
    int err = ferror(f); fclose(f);
    if (err) return 0;
    unsigned char d[32]; lf_sha_final(&c, d);
    static const char* hx = "0123456789abcdef";
    for (int i = 0; i < 32; i++) { out_hex[i*2] = hx[d[i] >> 4]; out_hex[i*2+1] = hx[d[i] & 15]; }
    out_hex[64] = '\0';
    return 1;
}

/* mkdir -p (la cache può non esistere: ~/.gmbasic/libs/<rid>) */
static void lf_mkdir_p(const char* path)
{
    char tmp[2600];
    snprintf(tmp, sizeof tmp, "%s", path);
    for (char* p = tmp + 1; *p; p++) {
        if (*p != '/') continue;
        *p = '\0';
#if defined(_WIN32)
        _mkdir(tmp);
#else
        mkdir(tmp, 0755);
#endif
        *p = '/';
    }
#if defined(_WIN32)
    _mkdir(tmp);
#else
    mkdir(tmp, 0755);
#endif
}

static int lf_sha_matches(const char* path, const char* want_hex)
{
    if (!want_hex || strlen(want_hex) != 64) return 0;
    char got[65];
    if (!lf_file_sha256_hex(path, got)) return 0;
    for (int i = 0; i < 64; i++) {
        char a = got[i], b = want_hex[i];
        if (a >= 'A' && a <= 'F') a = (char)(a - 'A' + 'a');
        if (b >= 'A' && b <= 'F') b = (char)(b - 'A' + 'a');
        if (a != b) return 0;
    }
    return 1;
}

/* ── curl ──────────────────────────────────────────────────────────────────────────────── */
struct lf_sink { FILE* f; long written; };

static size_t lf_write_cb(void* ptr, size_t size, size_t nmemb, void* userdata)
{
    struct lf_sink* s = (struct lf_sink*)userdata;
    size_t n = size * nmemb;
    if (fwrite(ptr, 1, n, s->f) != n) return 0;
    s->written += (long)n;
    return n;
}

/* scarica `url` in `dest_tmp`; 1 = ok */
static int lf_download(const char* url, const char* dest_tmp)
{
#if !defined(GMB_LIBS_HAVE_CURL)
    (void)url; (void)dest_tmp;
    fprintf(stderr, "[libs] runtime compilato SENZA libcurl: download automatico non disponibile\n");
    return 0;
#else
    CURL* c = curl_easy_init();
    if (!c) { fprintf(stderr, "[libs] curl non inizializzabile\n"); return 0; }
    struct lf_sink s = { NULL, 0 };
    s.f = fopen(dest_tmp, "wb");
    if (!s.f) {
        fprintf(stderr, "[libs] impossibile scrivere %s\n", dest_tmp);
        curl_easy_cleanup(c);
        return 0;
    }
    curl_easy_setopt(c, CURLOPT_URL, url);
    curl_easy_setopt(c, CURLOPT_FOLLOWLOCATION, 1L);      /* le release GitHub ridirigono */
    curl_easy_setopt(c, CURLOPT_MAXREDIRS, 10L);
    curl_easy_setopt(c, CURLOPT_FAILONERROR, 1L);         /* 404/403 = errore, non HTML salvato */
    curl_easy_setopt(c, CURLOPT_USERAGENT, "gmbasic-libs/1");
    curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, lf_write_cb);
    curl_easy_setopt(c, CURLOPT_WRITEDATA, &s);
    curl_easy_setopt(c, CURLOPT_CONNECTTIMEOUT, 20L);
    curl_easy_setopt(c, CURLOPT_LOW_SPEED_LIMIT, 1024L);  /* rete che muore: non restare appesi */
    curl_easy_setopt(c, CURLOPT_LOW_SPEED_TIME, 60L);
    CURLcode rc = curl_easy_perform(c);
    long http = 0; curl_easy_getinfo(c, CURLINFO_RESPONSE_CODE, &http);
    curl_easy_cleanup(c);
    fclose(s.f);
    if (rc != CURLE_OK || http >= 400) {
        fprintf(stderr, "[libs] download fallito (%s, http=%ld)\n", curl_easy_strerror(rc), http);
        remove(dest_tmp);
        return 0;
    }
    return 1;
#endif
}

/* ── ricerca dell'elenco libs accanto all'eseguibile ───────────────────────────────────────
 * Il build emette `<exe>.libs.txt` nella cartella dell'eseguibile (una sola per cartella, in
 * pratica): è l'elenco delle librerie del manifest da cui QUEL binario dipende. Fallback:
 * <exe_dir>/libs.txt e ./libs.txt (casi manuali). */
static int lf_find_index(char* buf, size_t cap, int* beside_exe)
{
    if (beside_exe) *beside_exe = 0;
    char dir[2048]; dir[0] = '\0';
    (void)gmb_paths_exe_dir(dir, sizeof dir);
    if (dir[0]) {
        DIR* d = opendir(dir);
        if (d) {
            struct dirent* e;
            char best[512] = "";
            while ((e = readdir(d)) != NULL) {
                size_t L = strlen(e->d_name);
                if (L > 9 && strcmp(e->d_name + L - 9, ".libs.txt") == 0) {
                    if (!best[0] || strcmp(e->d_name, best) < 0)
                        snprintf(best, sizeof best, "%s", e->d_name);
                }
            }
            closedir(d);
            if (best[0]) {
                snprintf(buf, cap, "%s/%s", dir, best);
                if (beside_exe) *beside_exe = 1;   /* le lib si scaricano ACCANTO all'eseguibile */
                return 1;
            }
        }
        snprintf(buf, cap, "%s/libs.txt", dir);
        if (access(buf, R_OK) == 0) return 1;
    }
    snprintf(buf, cap, "libs.txt");
    if (access(buf, R_OK) == 0) return 1;
    return 0;
}

/* ── API ───────────────────────────────────────────────────────────────────────────────── */
/* `dir` = dove mettere il file (cache del RID, oppure la cartella dell'eseguibile). */
static int lf_ensure_in_dir(const char* index_path, const char* fileName, const char* dir,
                            char* out_path, size_t cap)
{
    if (out_path && cap) out_path[0] = '\0';
    if (!index_path || !fileName || !dir || !dir[0]) return 0;

    FILE* f = fopen(index_path, "rb");
    if (!f) return 0;
    char line[4096];
    char want_sha[65] = "";
    char want_url[2048] = "";
    while (fgets(line, sizeof line, f)) {
        char* nl = strchr(line, '\n'); if (nl) *nl = '\0';
        if (line[0] == '#') continue;                 /* "# requires runtime-v1 …" (intestazione) */
        char* t1 = strchr(line, '\t'); if (!t1) continue;
        *t1 = '\0';
        if (strcmp(line, fileName) != 0) continue;
        char* sha = t1 + 1;
        char* t2 = strchr(sha, '\t'); if (!t2) break; *t2 = '\0';
        char* t3 = strchr(t2 + 1, '\t'); if (!t3) break;
        *t3 = '\0';
        snprintf(want_sha, sizeof want_sha, "%s", sha);
        /* l'URL è la 4ª colonna: se c'è una 5ª (scope: runtime|dev|component:<nome>) va tagliata,
         * altrimenti finirebbe dentro l'URL (bug visto: "...dylib<TAB>runtime" → 404). */
        char* t4 = strchr(t3 + 1, '\t');
        if (t4) *t4 = '\0';
        snprintf(want_url, sizeof want_url, "%s", t3 + 1);
        break;
    }
    fclose(f);
    if (!want_url[0] || !want_sha[0]) return 0;

    char dest[2600], tmp[2640];
    snprintf(dest, sizeof dest, "%s/%s", dir, fileName);
    snprintf(tmp, sizeof tmp, "%s.part", dest);

    if (lf_sha_matches(dest, want_sha)) {          /* già presente e integro */
        if (out_path && cap) snprintf(out_path, cap, "%s", dest);
        return 1;
    }
    /* il file può stare in una SOTTOCARTELLA della cache (dev: include/SDL2/SDL.h,
     * lib/libllmcodec.a): va creata la catena di directory del file, non solo <cache>/<rid>. */
    {
        char parent[2600];
        snprintf(parent, sizeof parent, "%s", dest);
        char* sl = strrchr(parent, '/');
        if (sl) { *sl = '\0'; lf_mkdir_p(parent); }
    }
    lf_mkdir_p(dir);
    fprintf(stderr, "[libs] manca %s → scarico da %s\n", fileName, want_url);
    if (!lf_download(want_url, tmp)) return 0;
    if (!lf_sha_matches(tmp, want_sha)) {
        fprintf(stderr, "[libs] sha256 NON valido per %s — file scartato\n", fileName);
        remove(tmp);
        return 0;
    }
    remove(dest);
    if (rename(tmp, dest) != 0) { remove(tmp); return 0; }
    if (out_path && cap) snprintf(out_path, cap, "%s", dest);
    return 1;
}

int gmb_libs_ensure_file_from(const char* index_path, const char* fileName,
                              char* out_path, size_t cap)
{
    char cache[2048];
    gmb_paths_libs_cache_dir(cache, sizeof cache);
    if (!cache[0]) return 0;
    char dir[2304];
    snprintf(dir, sizeof dir, "%s/%s", cache, gmb_libs_rid());
    return lf_ensure_in_dir(index_path, fileName, dir, out_path, cap);
}

/* Procura `fileName` ACCANTO a un eseguibile indicato esplicitamente (launcher `--exec /path/prog`,
 * che può vivere in un'altra cartella): il `.libs.txt` e la destinazione sono quelli di `exe`, non
 * della cartella del processo corrente. Senza questo, il launcher scaricava nella cache (o accanto
 * a SÉ), e al `exec` l'rpath `@loader_path` del programma non trovava nulla. */
int gmb_libs_ensure_next_to(const char* exe, const char* fileName, char* out_path, size_t cap)
{
    if (!exe || !*exe || !fileName || !*fileName) return 0;
    char idx[2600], dir[2048];
    snprintf(idx, sizeof idx, "%s.libs.txt", exe);
    snprintf(dir, sizeof dir, "%s", exe);
    char* sl = strrchr(dir, '/');
    if (sl) *sl = '\0';
    else snprintf(dir, sizeof dir, ".");
    if (access(idx, R_OK) != 0) return gmb_libs_autoensure(fileName, out_path, cap);
    return lf_ensure_in_dir(idx, fileName, dir, out_path, cap);
}

int gmb_libs_autoensure(const char* fileName, char* out_path, size_t cap)
{
    char idx[2200];
    int beside = 0;
    if (!lf_find_index(idx, sizeof idx, &beside)) return 0;
    if (beside) {
        /* l'elenco sta accanto all'eseguibile: le lib si scaricano LÌ (rpath @loader_path),
         * così il programma funziona anche senza cache utente e senza compilatore installato. */
        char dir[2048];
        if (gmb_paths_exe_dir(dir, sizeof dir) && dir[0])
            return lf_ensure_in_dir(idx, fileName, dir, out_path, cap);
    }
    return gmb_libs_ensure_file_from(idx, fileName, out_path, cap);
}
