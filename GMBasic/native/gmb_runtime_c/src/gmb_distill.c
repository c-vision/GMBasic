/* gmb_distill.c — Stream D, fase D1 (BUILD): jsonl di coppie → .bin train/hold.
 *
 * Vincoli: §0bis (logica nel runtime condiviso), niente stub. Il tokenizer e' quello
 * condiviso (llmtoken, linkato a gmb_runtime). Formato .bin identico a
 * tools/basic/make_lora_dataset.bas: <int32 n> poi per sample <int32 len> <int32 tok...>
 * <int32 target>, interi little-endian.
 *
 * Semplificazione dichiarata del parser JSON: si estraggono i valori stringa dei primi campi
 * noti (prompt/response, problem/solution, question/answer) con un parser minimale (niente
 * escape unicode avanzato oltre \" \\ \n \t \r e \uXXXX base). Sufficiente per i dataset
 * math500/gsm8k; un JSON reale con strutture annidate in quei campi non e' previsto.
 */
#include "gmb_distill.h"
#include "llmtoken.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>
#include <unistd.h>
#include <dlfcn.h>

#define GMB_DISTILL_MAX_TOK 8192
#define GMB_DISTILL_MAX_WIN 512   /* finestra massima memorizzata per sample */
#define GMB_DISTILL_MAX_SAMPLES 16384

static char g_err[512] = "";
const char* gmb_distill_last_error(void) { return g_err; }
static void set_err(const char* m) { snprintf(g_err, sizeof g_err, "%s", m ? m : ""); }

static int g_pairs = 0, g_train = 0, g_hold = 0;
static int g_samplen = 0, g_samplefail = 0, g_sample_done = 0;

int  gmb_distill_pairs(void) { return g_pairs; }
int  gmb_distill_trainsamples(void) { return g_train; }
int  gmb_distill_holdsamples(void) { return g_hold; }

/* --- estrazione stringa JSON minimale: cerca "key" poi la stringa, de-escape in out --- */
static int json_str_field(const char* line, const char* key, char* out, int cap)
{
    if (!line || !key || !out || cap <= 0) return 0;
    char pat[64];
    snprintf(pat, sizeof pat, "\"%s\"", key);
    const char* p = strstr(line, pat);
    if (!p) return 0;
    p = strchr(p + strlen(pat), ':');
    if (!p) return 0;
    p = strchr(p, '"');
    if (!p) return 0;
    p++;
    int n = 0;
    while (*p && *p != '"' && n < cap - 1) {
        if (*p == '\\' && p[1]) {
            p++;
            switch (*p) {
                case 'n': out[n++] = '\n'; break;
                case 't': out[n++] = '\t'; break;
                case 'r': out[n++] = '\r'; break;
                case '"': out[n++] = '"'; break;
                case '\\': out[n++] = '\\'; break;
                case '/': out[n++] = '/'; break;
                default: out[n++] = *p; break;   /* \uXXXX ecc.: si tiene il byte grezzo */
            }
        } else out[n++] = *p;
        p++;
    }
    out[n] = 0;
    return 1;
}

/* --- writer .bin (interi LE) --- */
static int write_int(FILE* f, int32_t v)
{
    unsigned char b[4] = { (unsigned char)(v & 0xFF), (unsigned char)((v >> 8) & 0xFF),
                           (unsigned char)((v >> 16) & 0xFF), (unsigned char)((v >> 24) & 0xFF) };
    return fwrite(b, 1, 4, f) == 4;
}

/* Buffer di sample raccolti: window di T token + target. */
typedef struct { int32_t tok[GMB_DISTILL_MAX_WIN]; int len; int32_t target; } DistSample;

static int append_samples(FILE* f, const DistSample* s, int n)
{
    if (!write_int(f, n)) return 0;
    for (int i = 0; i < n; i++) {
        if (!write_int(f, s[i].len)) return 0;
        for (int k = 0; k < s[i].len; k++) if (!write_int(f, s[i].tok[k])) return 0;
        if (!write_int(f, s[i].target)) return 0;
    }
    return 1;
}

int gmb_distill_build(const char* jsonl_path, const char* tokenizer_json,
                      const char* out_prefix, int window, double hold_ratio, int max_pairs)
{
    g_pairs = g_train = g_hold = 0;
    set_err("");
    if (!jsonl_path || !*jsonl_path || !tokenizer_json || !*tokenizer_json || !out_prefix || !*out_prefix) {
        set_err("build: argomenti mancanti (jsonl/tokenizer/out)");
        return -1;
    }
    if (window < 1 || window > 4096) { set_err("build: window fuori range [1,4096]"); return -1; }
    if (hold_ratio < 0.0 || hold_ratio >= 1.0) { set_err("build: hold_ratio deve stare in [0,1)"); return -1; }

    llmtoken_handle tk = llmtoken_load(tokenizer_json);
    if (!tk) { set_err("build: tokenizer.json non caricabile"); return -1; }
    FILE* fj = fopen(jsonl_path, "rb");
    if (!fj) { set_err("build: jsonl non apribile"); return -1; }

    /* raccogli sample in memoria (poi split + scrittura). Buffer HEAP (niente stack enorme:
       questa funzione gira anche su thread gestiti con stack limitato). */
    DistSample* train = (DistSample*)malloc(sizeof(DistSample) * GMB_DISTILL_MAX_SAMPLES);
    DistSample* hold = (DistSample*)malloc(sizeof(DistSample) * GMB_DISTILL_MAX_SAMPLES);
    char* line = (char*)malloc(65536);
    char* prompt = (char*)malloc(32768);
    char* response = (char*)malloc(32768);
    int32_t* pids = (int32_t*)malloc(sizeof(int32_t) * GMB_DISTILL_MAX_TOK);
    int32_t* rids = (int32_t*)malloc(sizeof(int32_t) * GMB_DISTILL_MAX_TOK);
    if (!train || !hold || !line || !prompt || !response || !pids || !rids) {
        free(train); free(hold); free(line); free(prompt); free(response); free(pids); free(rids);
        llmtoken_free(tk); fclose(fj); set_err("build: OOM"); return -1;
    }
    int ntr = 0, nho = 0;
    int stop = 0;
    int it=0;
    while (!stop && fgets(line, 65536, fj)) {
        size_t ln = strlen(line);
        while (ln && (line[ln - 1] == '\n' || line[ln - 1] == '\r')) line[--ln] = 0;
        if (ln == 0) continue;
        int has = json_str_field(line, "prompt", prompt, 32768)
               || json_str_field(line, "problem", prompt, 32768)
               || json_str_field(line, "question", prompt, 32768);
        if (has) has = json_str_field(line, "response", response, 32768)
                    || json_str_field(line, "solution", response, 32768)
                    || json_str_field(line, "answer", response, 32768);
        if (!has) continue;
        int np = llmtoken_encode(tk, prompt, pids, GMB_DISTILL_MAX_TOK);
        int nr = llmtoken_encode(tk, response, rids, GMB_DISTILL_MAX_TOK);
        if (np <= 0 || nr <= 0) continue;
        g_pairs++;
        /* target = SOLO token di risposta (prompt rimosso dai target). Per ogni token di
           risposta: window = ultimi T token di (prompt + risposta[:i]), target = risposta[i]. */
        for (int i = 0; i < nr; i++) {
            DistSample s; s.len = 0;
            /* finestra: prima i token di prompt, poi i primi i di risposta, tronca agli ultimi T */
            int total = np + i;
            for (int k = 0; k < window && s.len < GMB_DISTILL_MAX_WIN; k++) {
                int idx = total - window + k;   /* indice nel flusso concatenato */
                if (idx < 0) continue;
                int32_t t = idx < np ? pids[idx] : rids[idx - np];
                s.tok[s.len++] = t;
            }
            s.target = rids[i];
            /* Split deterministico: quota hold_ratio sul totale corrente (nessun seed). */
            if (hold_ratio > 0.0 && ((double)(nho + 1) / (double)(ntr + nho + 1)) <= hold_ratio)
                hold[nho++] = s;
            else
                train[ntr++] = s;
            if (ntr >= GMB_DISTILL_MAX_SAMPLES || nho >= GMB_DISTILL_MAX_SAMPLES) { stop = 1; break; }
        }
        if (max_pairs > 0 && g_pairs >= max_pairs) break;
    }
    fclose(fj);
    llmtoken_free(tk);

    char tp[2100], hp[2100];
    snprintf(tp, sizeof tp, "%s.train.bin", out_prefix);
    snprintf(hp, sizeof hp, "%s.hold.bin", out_prefix);
    FILE* ft = fopen(tp, "wb");
    FILE* fh = fopen(hp, "wb");
    if (!ft || !fh) { if (ft) fclose(ft); if (fh) fclose(fh); free(train); free(hold); free(line); free(prompt); free(response); free(pids); free(rids); set_err("build: output non scrivibile"); return -1; }
    int ok = append_samples(ft, train, ntr) && append_samples(fh, hold, nho);
    fclose(ft); fclose(fh);
    free(train); free(hold); free(line); free(prompt); free(response); free(pids); free(rids);
    if (!ok) { set_err("build: scrittura .bin fallita"); return -1; }
    g_train = ntr; g_hold = nho;
    return g_pairs;
}

static char g_report[1024] = "";
const char* gmb_distill_status(void)
{
    gmb_distill_report(g_report, sizeof g_report);
    return g_report;
}

/* mini-estrattore numerico/stringa da opts JSON (riusa json_str_field).
 * Accetta sia il numero nudo (10) sia la stringa numerica ("10"): il codegen C di alcuni
 * backend serializza i numeri dei dict come stringhe (gmb_double_to_string), quindi il
 * parser deve essere tollerante per garantire la parita' dei parametri. */
static int json_int_field(const char* line, const char* key, int def)
{
    if (!line || !key) return def;
    char pat[64]; snprintf(pat, sizeof pat, "\"%s\"", key);
    const char* p = strstr(line, pat); if (!p) return def;
    p = strchr(p + strlen(pat), ':'); if (!p) return def;
    p++; while (*p == ' ') p++;
    if (*p == '"') p++;
    return (int)strtol(p, NULL, 10);
}
static double json_dbl_field(const char* line, const char* key, double def)
{
    if (!line || !key) return def;
    char pat[64]; snprintf(pat, sizeof pat, "\"%s\"", key);
    const char* p = strstr(line, pat); if (!p) return def;
    p = strchr(p + strlen(pat), ':'); if (!p) return def;
    p++; while (*p == ' ') p++;
    if (*p == '"') p++;
    return strtod(p, NULL);
}

int gmb_distill_build_opts(const char* jsonl_path, const char* opts_json, char* resp, int cap)
{
    if (!resp || cap <= 0) return -1;
    char model[1024] = "", tokenizer[1200] = "", out[1200] = "";
    int window = 8, maxp = 0; double hold = 0.1;
    if (opts_json && *opts_json) {
        if (opts_json[0] == '{') {
            json_str_field(opts_json, "model", model, sizeof model);
            json_str_field(opts_json, "tokenizer", tokenizer, sizeof tokenizer);
            json_str_field(opts_json, "out", out, sizeof out);
            window = json_int_field(opts_json, "window", window);
            maxp = json_int_field(opts_json, "max", maxp);
            hold = json_dbl_field(opts_json, "holdout", hold);
        } else {
            /* TRAIN DATASET BUILD <jsonl>, <out>: 2° argomento posizionale = out (non JSON). */
            snprintf(out, sizeof out, "%s", opts_json);
        }
    }
    if (!tokenizer[0]) {
        if (!model[0]) { snprintf(resp, cap, "{\"type\":\"error\",\"data\":{\"message\":\"serve 'model' (dir) o 'tokenizer' (path).\"}}"); return -1; }
        snprintf(tokenizer, sizeof tokenizer, "%s/tokenizer.json", model);
    }
    if (!out[0]) {
        snprintf(out, sizeof out, "%s", jsonl_path ? jsonl_path : "");
        size_t olen = strlen(out);
        if (olen > 6 && strcmp(out + olen - 6, ".jsonl") == 0) out[olen - 6] = 0;
    }
    int n = gmb_distill_build(jsonl_path, tokenizer, out, window, hold, maxp);
    if (n < 0)
        snprintf(resp, cap, "{\"type\":\"error\",\"data\":{\"message\":\"%s\"}}", gmb_distill_last_error());
    else
        snprintf(resp, cap, "{\"type\":\"result\",\"data\":{\"pairs\":%d,\"train\":%d,\"hold\":%d,\"out\":\"%s\"}}",
                 n, gmb_distill_trainsamples(), gmb_distill_holdsamples(), out);
    return n >= 0 ? 0 : -1;
}

int gmb_distill_report(char* out, int cap)
{
    if (!out || cap <= 0) return -1;
    return snprintf(out, cap,
        "{\"pairs\":%d,\"train\":%d,\"hold\":%d,\"samplen\":%d,\"samplefail\":%d,\"error\":\"%s\"}",
        g_pairs, g_train, g_hold, g_samplen, g_samplefail, g_err) < cap ? 0 : -1;
}

/* --- selfcheck: parse jsonl minimale + scrittura/rilettura .bin --- */
int gmb_distill_selfcheck(void)
{
    char tmp[] = "/tmp/gmb_distill_selfcheck.XXXXXX";
    /* jsonl */
    char jp[256]; snprintf(jp, sizeof jp, "%s.jsonl", tmp);
    FILE* f = fopen(jp, "w");
    if (!f) return -1;
    fprintf(f, "{\"prompt\": \"hello world\", \"response\": \"ok\"}\n");
    fprintf(f, "{\"question\": \"two plus two\", \"answer\": \"four\"}\n");
    fclose(f);
    /* parse fields (senza tokenizer: testa solo l'estrazione) */
    char buf[256];
    f = fopen(jp, "r");
    if (!f) return -2;
    if (!fgets(buf, sizeof buf, f)) { fclose(f); return -3; }
    fclose(f);
    char v[128];
    if (!json_str_field(buf, "prompt", v, sizeof v) || strcmp(v, "hello world") != 0) return -4;
    if (!json_str_field(buf, "response", v, sizeof v) || strcmp(v, "ok") != 0) return -5;
    /* scrittura/rilettura .bin di 1 sample */
    char bp[256]; snprintf(bp, sizeof bp, "%s.bin", tmp);
    f = fopen(bp, "wb");
    if (!f) { remove(jp); return -6; }
    DistSample s; s.len = 3; s.tok[0] = 10; s.tok[1] = 20; s.tok[2] = 30; s.target = 40;
    if (!append_samples(f, &s, 1)) { fclose(f); remove(jp); remove(bp); return -7; }
    fclose(f);
    f = fopen(bp, "rb");
    if (!f) { remove(jp); return -8; }
    int32_t hdr = 0; if (fread(&hdr, 4, 1, f) != 1 || hdr != 1) { fclose(f); remove(jp); remove(bp); return -9; }
    int32_t len = 0; if (fread(&len, 4, 1, f) != 1 || len != 3) { fclose(f); remove(jp); remove(bp); return -10; }
    int32_t toks[3]; if (fread(toks, 4, 3, f) != 3 || toks[2] != 30) { fclose(f); remove(jp); remove(bp); return -11; }
    int32_t tgt = 0; if (fread(&tgt, 4, 1, f) != 1 || tgt != 40) { fclose(f); remove(jp); remove(bp); return -12; }
    fclose(f);
    remove(jp); remove(bp);
    return 0;
}

/* ============================ SAMPLE (D1) — dlopen della dylib finetune ============================
 * La generazione (motore MLX) vive nella dylib `gmb_ai_finetune` (come il fine-tune): qui solo il
 * wrapper che costruisce la request e ne legge la risposta, senza subprocess. §0bis: nessuna
 * duplicazione della logica nel backend. */

static const char* dst_dylib_path(void)
{
    static char buf[4096];
    char cwd[2048];
    if (getcwd(cwd, sizeof cwd) != NULL) {
        char dir[2048]; snprintf(dir, sizeof dir, "%s", cwd);
        for (int up = 0; up < 10; ++up) {
            /* artefatti di build, poi cache utente (`Resources/libs` non esiste più) */
            snprintf(buf, sizeof buf, "%s/build/libs/osx-arm64/libgmb_ai_finetune.dylib", dir);
            if (access(buf, R_OK) == 0) return buf;
            snprintf(buf, sizeof buf, "%s/.gmbasic/libs/osx-arm64/libgmb_ai_finetune.dylib", dir);
            if (access(buf, R_OK) == 0) return buf;
            char* s = strrchr(dir, '/');
            if (s == NULL || s == dir) break;
            *s = 0;
        }
    }
    return "libgmb_ai_finetune.dylib";
}

static int dst_call_sym(const char* sym, const char* req, char* body, size_t cap)
{
    static void* h = NULL;
    if (h == NULL) h = dlopen(dst_dylib_path(), RTLD_NOW | RTLD_LOCAL);
    if (h == NULL) {
        snprintf(body, cap, "{\"type\":\"error\",\"data\":{\"message\":\"dlopen libgmb_ai_finetune fallito: %s\"}}", dlerror());
        return -1;
    }
    int (*fn)(const char*, char*, size_t) = (int (*)(const char*, char*, size_t))dlsym(h, sym);
    if (fn == NULL) {
        snprintf(body, cap, "{\"type\":\"error\",\"data\":{\"message\":\"dlsym %s fallito\"}}", sym);
        return -1;
    }
    return fn(req, body, cap);
}
static int dst_call(const char* req, char* body, size_t cap) { return dst_call_sym("gmb_ai_distill_sample_exec", req, body, cap); }

int gmb_distill_samplen(void)   { return g_samplen; }
double gmb_distill_sampledone(void) { return (double)g_sample_done; }
int gmb_distill_samplefail(void){ return g_samplefail; }

int gmb_distill_sample_opts(const char* prompts, const char* opts_json, char* resp, int cap)
{
    if (!resp || cap <= 0) return -1;
    g_samplen = 0; g_samplefail = 0; g_sample_done = 0;
    if (!prompts || !*prompts) { snprintf(resp, cap, "{\"type\":\"error\",\"data\":{\"message\":\"sample: prompts mancante\"}}"); return -1; }
    char model[1024] = "", out[1200] = "", verify[32] = "";
    int k = 4, maxlen = 256, seed = 20260909; double temp = 0.7;
    if (opts_json && *opts_json) {
        json_str_field(opts_json, "model", model, sizeof model);
        json_str_field(opts_json, "out", out, sizeof out);
        json_str_field(opts_json, "verify", verify, sizeof verify);
        k = json_int_field(opts_json, "k", k);
        maxlen = json_int_field(opts_json, "maxlen", maxlen);
        seed = json_int_field(opts_json, "seed", seed);
        temp = json_dbl_field(opts_json, "temp", temp);
    }
    if (!model[0]) { snprintf(resp, cap, "{\"type\":\"error\",\"data\":{\"message\":\"sample: serve 'model' (dir)\"}}"); return -1; }
    if (!out[0]) {
        snprintf(out, sizeof out, "%s", prompts);
        size_t olen = strlen(out);
        if (olen > 6 && strcmp(out + olen - 6, ".jsonl") == 0) out[olen - 6] = 0;
        strncat(out, ".distill.jsonl", sizeof out - strlen(out) - 1);
    }
    char req[8192];
    snprintf(req, sizeof req,
        "{\"id\":1,\"method\":\"distill_sample\",\"params\":{\"model_dir\":\"%s\",\"prompts\":\"%s\","
        "\"out\":\"%s\",\"k\":%d,\"temp\":%g,\"maxlen\":%d,\"verify\":\"%s\",\"seed\":%d}}",
        model, prompts, out, k, temp, maxlen, verify, seed);
    char body[8192];
    int rc = dst_call(req, body, sizeof body);
    snprintf(resp, cap, "%s", body);
    if (rc != 0) return -1;
    g_samplen = json_int_field(body, "pairs", 0);
    g_samplefail = json_int_field(body, "fail", 0);
    g_sample_done = 1;
    set_err("");
    return 0;
}

/* ============================ APPLY / RESTORE (D3) ============================
 * Schema LORA APPLY: APPLY verifica l'adapter (A/B) sul modello (riuso di
 * gmb_ai_lora_apply_exec) e, se `backup` e' dato, salva una copia dei file del checkpoint
 * (<ckpt>_best_{A,B}.raw + .meta) per poterli ripristinare. RESTORE ricopia i file dal backup
 * al checkpoint originale (il prefisso originale e' scritto in <backup>.src). */

static int dst_copy_file(const char* src, const char* dst)
{
    FILE* fi = fopen(src, "rb");
    if (!fi) return -1;
    FILE* fo = fopen(dst, "wb");
    if (!fo) { fclose(fi); return -1; }
    char buf[65536]; size_t n; int ok = 1;
    while ((n = fread(buf, 1, sizeof buf, fi)) > 0)
        if (fwrite(buf, 1, n, fo) != n) { ok = 0; break; }
    fclose(fi); fclose(fo);
    return ok ? 0 : -1;
}

int gmb_distill_apply_opts(const char* ckpt, const char* opts_json, char* resp, int cap)
{
    if (!resp || cap <= 0) return -1;
    if (!ckpt || !*ckpt) { snprintf(resp, cap, "{\"type\":\"error\",\"data\":{\"message\":\"apply: serve il prefisso ckpt\"}}"); return -1; }
    char model[1024] = "", dataset[1200] = "", backup[1200] = "";
    int ngen = 6;
    if (opts_json && *opts_json) {
        json_str_field(opts_json, "model", model, sizeof model);
        json_str_field(opts_json, "dataset", dataset, sizeof dataset);
        json_str_field(opts_json, "backup", backup, sizeof backup);
        ngen = json_int_field(opts_json, "ngen", ngen);
    }
    if (!model[0]) { snprintf(resp, cap, "{\"type\":\"error\",\"data\":{\"message\":\"apply: serve 'model' (dir)\"}}"); return -1; }
    /* backup del checkpoint prima della verifica applica (se richiesto). */
    if (backup[0]) {
        char sp[1400], dp[1400];
        snprintf(sp, sizeof sp, "%s_best_A.raw", ckpt); snprintf(dp, sizeof dp, "%s_A.raw", backup);
        if (dst_copy_file(sp, dp) != 0) { snprintf(resp, cap, "{\"type\":\"error\",\"data\":{\"message\":\"apply: backup A fallito\"}}"); return -1; }
        snprintf(sp, sizeof sp, "%s_best_B.raw", ckpt); snprintf(dp, sizeof dp, "%s_B.raw", backup);
        if (dst_copy_file(sp, dp) != 0) { snprintf(resp, cap, "{\"type\":\"error\",\"data\":{\"message\":\"apply: backup B fallito\"}}"); return -1; }
        snprintf(sp, sizeof sp, "%s_best.meta", ckpt); snprintf(dp, sizeof dp, "%s.meta", backup);
        dst_copy_file(sp, dp);
        char srcp[1400]; snprintf(srcp, sizeof srcp, "%s.src", backup);
        FILE* fs = fopen(srcp, "w"); if (fs) { fprintf(fs, "%s\n", ckpt); fclose(fs); }
    }
    char req[8192];
    snprintf(req, sizeof req,
        "{\"id\":1,\"method\":\"apply\",\"params\":{\"model_dir\":\"%s\",\"ckpt\":\"%s\","
        "\"dataset\":\"%s\",\"ngen\":%d}}", model, ckpt, dataset, ngen);
    char body[8192];
    int rc = dst_call_sym("gmb_ai_lora_apply_exec", req, body, sizeof body);
    snprintf(resp, cap, "%s", body);
    return rc;
}

int gmb_distill_restore_opts(const char* backup, char* resp, int cap)
{
    if (!resp || cap <= 0) return -1;
    if (!backup || !*backup) { snprintf(resp, cap, "{\"type\":\"error\",\"data\":{\"message\":\"restore: serve il prefisso backup\"}}"); return -1; }
    char srcp[1400]; snprintf(srcp, sizeof srcp, "%s.src", backup);
    FILE* fs = fopen(srcp, "r");
    if (!fs) { snprintf(resp, cap, "{\"type\":\"error\",\"data\":{\"message\":\"restore: %s.src mancante (nessun backup registrato)\"}}", backup); return -1; }
    char ckpt[1200]; ckpt[0] = 0;
    if (!fgets(ckpt, sizeof ckpt, fs)) { fclose(fs); snprintf(resp, cap, "{\"type\":\"error\",\"data\":{\"message\":\"restore: ckpt di origine vuoto\"}}"); return -1; }
    fclose(fs);
    size_t cl = strlen(ckpt); while (cl && (ckpt[cl-1] == '\n' || ckpt[cl-1] == '\r')) ckpt[--cl] = 0;
    char sp[1400], dp[1400];
    snprintf(sp, sizeof sp, "%s_A.raw", backup); snprintf(dp, sizeof dp, "%s_best_A.raw", ckpt);
    if (dst_copy_file(sp, dp) != 0) { snprintf(resp, cap, "{\"type\":\"error\",\"data\":{\"message\":\"restore: A non ripristinabile\"}}"); return -1; }
    snprintf(sp, sizeof sp, "%s_B.raw", backup); snprintf(dp, sizeof dp, "%s_best_B.raw", ckpt);
    if (dst_copy_file(sp, dp) != 0) { snprintf(resp, cap, "{\"type\":\"error\",\"data\":{\"message\":\"restore: B non ripristinabile\"}}"); return -1; }
    snprintf(sp, sizeof sp, "%s.meta", backup); snprintf(dp, sizeof dp, "%s_best.meta", ckpt);
    dst_copy_file(sp, dp);
    snprintf(resp, cap, "{\"type\":\"result\",\"data\":{\"ok\":true,\"restored\":\"%s\"}}\n", ckpt);
    return 0;
}

/* ============================ EVAL (D4) — valutazione paired base/adapter ============================ */

static double g_evaltok[2] = { -1, -1 }, g_evalpass[2] = { -1, -1 }, g_evalacc[2] = { -1, -1 };
double gmb_distill_evaltok(int idx) { return (idx >= 0 && idx < 2) ? g_evaltok[idx] : -1; }
double gmb_distill_evalpass(int idx) { return (idx >= 0 && idx < 2) ? g_evalpass[idx] : -1; }
double gmb_distill_evalacc(int idx) { return (idx >= 0 && idx < 2) ? g_evalacc[idx] : -1; }
int gmb_distill_eval_opts(const char* prompts, const char* opts_json, char* resp, int cap)
{
    if (!resp || cap <= 0) return -1;
    g_evaltok[0] = g_evaltok[1] = -1; g_evalpass[0] = g_evalpass[1] = -1; g_evalacc[0] = g_evalacc[1] = -1;
    if (!prompts || !*prompts) { snprintf(resp, cap, "{\"type\":\"error\",\"data\":{\"message\":\"eval: prompts mancante\"}}"); return -1; }
    char model[1024] = "", ckpt[1200] = "", out[1200] = "", verify[32] = "";
    int maxlen = 256, repeat = 3, seed = 20260909; double temp = 0.7;
    if (opts_json && *opts_json) {
        json_str_field(opts_json, "model", model, sizeof model);
        json_str_field(opts_json, "ckpt", ckpt, sizeof ckpt);
        json_str_field(opts_json, "out", out, sizeof out);
        json_str_field(opts_json, "verify", verify, sizeof verify);
        maxlen = json_int_field(opts_json, "maxlen", maxlen);
        repeat = json_int_field(opts_json, "repeat", repeat);
        seed = json_int_field(opts_json, "seed", seed);
        temp = json_dbl_field(opts_json, "temp", temp);
    }
    if (!model[0]) { snprintf(resp, cap, "{\"type\":\"error\",\"data\":{\"message\":\"eval: serve 'model' (dir)\"}}"); return -1; }
    if (!out[0]) { snprintf(out, sizeof out, "work/eval_distill.json"); }
    char req[8192];
    snprintf(req, sizeof req,
        "{\"id\":1,\"method\":\"distill_eval\",\"params\":{\"model_dir\":\"%s\",\"prompts\":\"%s\","
        "\"ckpt\":\"%s\",\"out\":\"%s\",\"maxlen\":%d,\"repeat\":%d,\"temp\":%g,\"verify\":\"%s\",\"seed\":%d}}",
        model, prompts, ckpt, out, maxlen, repeat, temp, verify, seed);
    char body[8192];
    int rc = dst_call_sym("gmb_ai_distill_eval_exec", req, body, sizeof body);
    snprintf(resp, cap, "%s", body);
    if (rc != 0) return -1;
    g_evaltok[0] = json_dbl_field(body, "baseTok", -1);
    g_evalpass[0] = json_dbl_field(body, "basePass", -1);
    g_evaltok[1] = json_dbl_field(body, "distTok", -1);
    g_evalpass[1] = json_dbl_field(body, "distPass", -1);
    g_evalacc[0] = json_dbl_field(body, "baseAcc", -1);
    g_evalacc[1] = json_dbl_field(body, "distAcc", -1);
    set_err("");
    return 0;
}

/* ============================ TRAIN (D2) — head-only via dylib finetune ============================
 * Mappa le opzioni DISTILL a una request finetune (frozen:"all-but-head" per l'head-only) e
 * chiama gmb_ai_finetune_exec via dlopen. Espone loss/hold/ckpt/done dell'ultimo TRAIN. */

static double g_dloss = -1, g_dhold = -1;
static char g_dckpt[1200] = "";
static int g_ddone = 0;
double gmb_distill_loss(void) { return g_dloss; }
double gmb_distill_hold(void) { return g_dhold; }
const char* gmb_distill_ckpt(void) { return g_dckpt; }
double gmb_distill_done(void) { return (double)g_ddone; }

int gmb_distill_train_opts(const char* dataset, const char* opts_json, char* resp, int cap)
{
    if (!resp || cap <= 0) return -1;
    g_ddone = 0;
    if (!dataset || !*dataset) { snprintf(resp, cap, "{\"type\":\"error\",\"data\":{\"message\":\"train: dataset mancante\"}}"); return -1; }
    char model[1024] = "", holdout[1200] = "", ckpt[1200] = "", frozen[32] = "all-but-head";
    int rank = 4, layer = 15, steps = 60, batch = 4, seed = 7;
    double lr = 0.01;
    if (opts_json && *opts_json) {
        json_str_field(opts_json, "model", model, sizeof model);
        json_str_field(opts_json, "holdout", holdout, sizeof holdout);
        json_str_field(opts_json, "ckpt", ckpt, sizeof ckpt);
        json_str_field(opts_json, "frozen", frozen, sizeof frozen);
        rank = json_int_field(opts_json, "rank", rank);
        layer = json_int_field(opts_json, "layer", layer);
        steps = json_int_field(opts_json, "steps", steps);
        batch = json_int_field(opts_json, "batch", batch);
        seed = json_int_field(opts_json, "seed", seed);
        lr = json_dbl_field(opts_json, "lr", lr);
    }
    if (!model[0]) { snprintf(resp, cap, "{\"type\":\"error\",\"data\":{\"message\":\"train: serve 'model' (dir)\"}}"); return -1; }
    if (!holdout[0]) {
        /* default: sostituisci ".train.bin" con ".hold.bin" nel path del dataset. */
        snprintf(holdout, sizeof holdout, "%s", dataset);
        char* p = strstr(holdout, ".train.bin");
        if (p) { snprintf(p, sizeof holdout - (size_t)(p - holdout), ".hold.bin"); }
    }
    if (!ckpt[0]) { snprintf(ckpt, sizeof ckpt, "%s_ckpt", dataset); }
    char req[8192];
    snprintf(req, sizeof req,
        "{\"id\":1,\"method\":\"finetune\",\"params\":{\"model_dir\":\"%s\",\"dataset\":\"%s\","
        "\"holdout\":\"%s\",\"ckpt\":\"%s\",\"rank\":%d,\"layer\":%d,\"steps\":%d,\"batch\":%d,"
        "\"lr\":%g,\"seed\":%d,\"frozen\":\"%s\"}}",
        model, dataset, holdout, ckpt, rank, layer, steps, batch, lr, seed, frozen);
    char body[8192];
    int rc = dst_call_sym("gmb_ai_finetune_exec", req, body, sizeof body);
    snprintf(resp, cap, "%s", body);
    if (rc != 0) return -1;
    g_dloss = json_dbl_field(body, "train", -1);
    g_dhold = json_dbl_field(body, "hold", -1);
    json_str_field(body, "ckpt", g_dckpt, sizeof g_dckpt);
    g_ddone = 1;
    set_err("");
    return 0;
}
