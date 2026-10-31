/* llmmodelio_recover.c — RECOVER (§9.B): composizione gate -> retrain -> re-eval ->
 * ACCEPT/ROLLBACK. Dominio separato ("recupero") estratto da llmmodelio_model.c (AGENTS
 * §0quater: un nuovo dominio vive nel proprio file). Il fine-tune LoRA vive nella dylib
 * `libgmb_ai_finetune` (PUL), usata via dlopen (dipendenza debole, come LLM QUALITY ->
 * engine MLX): `gmbllm` resta linkabile da solo. `llmmodelio_model_gate`,
 * `llmmodelio_model_adapter`, `llmmodelio_model_adapter_save` sono nel core (header comune). */
/* Dl_info/dladdr su Linux sono estensioni GNU di glibc: senza _GNU_SOURCE la struct non
 * esiste ("unknown type name 'Dl_info'"). Va definito PRIMA di ogni include di sistema;
 * su macOS (dove Dl_info e' in <dlfcn.h>) e' innocuo. */
#if defined(__linux__) && !defined(_GNU_SOURCE)
#define _GNU_SOURCE
#endif
#include "llmmodelio_model.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <dlfcn.h>

/* Composizione gate -> retrain -> re-eval -> ACCEPT/ROLLBACK: recupero di qualita' dopo una
 * surgery/quant che il gate rifiuta. */
static void llmo_rm_rf(const char* path) {
    struct stat st;
    if (stat(path, &st) != 0) return;
    if (S_ISDIR(st.st_mode)) {
        DIR* d = opendir(path); struct dirent* e;
        if (d) { while ((e = readdir(d)) != NULL) {
            if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, "..")) continue;
            char ch[4096]; snprintf(ch, sizeof ch, "%s/%s", path, e->d_name); llmo_rm_rf(ch);
        } closedir(d); }
        rmdir(path);
    } else remove(path);
}
static void llmo_copy_if_missing(const char* src, const char* dst) {
    struct stat st; if (stat(dst, &st) == 0) return;
    FILE* in = fopen(src, "rb"); if (!in) return;
    FILE* out = fopen(dst, "wb"); if (!out) { fclose(in); return; }
    char buf[65536]; size_t n;
    while ((n = fread(buf, 1, sizeof buf, in)) > 0) fwrite(buf, 1, n, out);
    fclose(in); fclose(out);
}
/* Stessa risoluzione del path della dylib usata da `gmb_distill.c` (Resources/libs/osx-arm64,
 * risalendo dalla CWD), per non duplicare la logica nei backend. */
static const char* llmo_finetune_dylib(void) {
    static char buf[4096];
    /* (1) Accanto alla dylib CORRENTE (dladdr): libgmb_ai_finetune e' deployata nella stessa
     * directory di libgmb_ai_llm nei bin degli harness e in Resources/libs. */
    Dl_info info;
    if (dladdr((void*)&llmmodelio_model_recover, &info) != 0 && info.dli_fname != NULL) {
        char dir[4096]; snprintf(dir, sizeof dir, "%s", info.dli_fname);
        char* s = strrchr(dir, '/'); if (s != NULL) { *s = 0; snprintf(buf, sizeof buf, "%s/libgmb_ai_finetune.dylib", dir); if (access(buf, R_OK) == 0) return buf; }
    }
    /* (2) Risalendo dalla CWD: <repo>/Resources/libs/osx-arm64 (come gmb_distill.c). */
    char cwd[2048];
    if (getcwd(cwd, sizeof cwd) != NULL) {
        char dir[2048]; snprintf(dir, sizeof dir, "%s", cwd);
        for (int up = 0; up < 10; ++up) {
            snprintf(buf, sizeof buf, "%s/Resources/libs/osx-arm64/libgmb_ai_finetune.dylib", dir);
            if (access(buf, R_OK) == 0) return buf;
            char* s = strrchr(dir, '/');
            if (s == NULL || s == dir) break;
            *s = 0;
        }
    }
    return "libgmb_ai_finetune.dylib";
}
static int llmo_recover_finetune(const char* model, const char* dataset, const char* ckpt_pref, char* err, size_t errcap) {
    void* h = dlopen(llmo_finetune_dylib(), RTLD_NOW | RTLD_LOCAL);
    if (!h) { snprintf(err, errcap, "dlopen libgmb_ai_finetune: %s", dlerror()); return -1; }
    int (*fn)(const char*, char*, size_t) = (int (*)(const char*, char*, size_t))dlsym(h, "gmb_ai_finetune_exec");
    if (!fn) { snprintf(err, errcap, "dlsym gmb_ai_finetune_exec"); return -1; }
    char hold[4096];
    const char* hh = strstr(dataset, ".train.bin");
    if (hh) snprintf(hold, sizeof hold, "%.*s.hold.bin", (int)(hh - dataset), dataset);
    else snprintf(hold, sizeof hold, "%s.hold.bin", dataset);
    char req[8192];
    snprintf(req, sizeof req,
        "{\"id\":1,\"method\":\"finetune\",\"params\":{\"model_dir\":\"%s\",\"dataset\":\"%s\","
        "\"holdout\":\"%s\",\"ckpt\":\"%s\",\"rank\":4,\"layer\":15,\"steps\":60,\"batch\":4,"
        "\"lr\":0.01,\"seed\":7,\"frozen\":\"\"}}", model, dataset, hold, ckpt_pref);
    static char resp[16384];
    int rc = fn(req, resp, sizeof resp);
    if (rc != 0 || strstr(resp, "\"ok\":true") == NULL) { snprintf(err, errcap, "finetune fallito: %.200s", resp); return -1; }
    return 0;
}
int llmmodelio_model_recover(const char* model, const char* dataset, const char* ref, char* report, size_t cap) {
    if (!report || cap == 0) return -1;
    report[0] = 0;
    if (!model || !*model || !dataset || !*dataset || !ref || !*ref) {
        snprintf(report, cap, "{\"verdict\":\"ERROR\",\"reason\":\"attesi <model>, <dataset>, <ref>\"}"); return -1;
    }
    /* Step 1: gate del modello attuale vs ref. Gia' ACCEPT -> nessun training. */
    char g1[512]; g1[0] = 0;
    llmmodelio_model_gate(ref, model, NULL, g1, sizeof g1);
    if (strstr(g1, "\"ACCEPT\"") != NULL) {
        snprintf(report, cap, "{\"verdict\":\"ACCEPT\",\"trained\":false,\"reason\":\"gia' oltre soglia\",\"gate\":%s}", g1);
        return 0;
    }
    if (strstr(g1, "\"ERROR\"") != NULL) {
        snprintf(report, cap, "{\"verdict\":\"ERROR\",\"reason\":\"gate(init)\",\"gate\":%s}", g1); return -1;
    }
    /* Step 2: fine-tune LoRA. */
    char ckpt[4096]; snprintf(ckpt, sizeof ckpt, "%s.recover_ckpt", model);
    char err[512]; err[0] = 0;
    if (llmo_recover_finetune(model, dataset, ckpt, err, sizeof err) != 0) {
        snprintf(report, cap, "{\"verdict\":\"ERROR\",\"reason\":\"%s\"}", err); return -1;
    }
    /* Step 3: adapter RAW -> .safetensors; merge -> <model>.recovered (base intatta). */
    char adp[4200]; snprintf(adp, sizeof adp, "%s.recover_adapter.safetensors", model);
    if (llmmodelio_model_adapter_save(ckpt, adp) != 0) {
        snprintf(report, cap, "{\"verdict\":\"ERROR\",\"reason\":\"adapter_save\"}"); return -1;
    }
    char rec[4200]; snprintf(rec, sizeof rec, "%s.recovered", model);
    llmo_rm_rf(rec);
    if (llmmodelio_model_adapter(model, adp, rec, "1.0") != 0) {
        snprintf(report, cap, "{\"verdict\":\"ERROR\",\"reason\":\"adapter merge\"}"); return -1;
    }
    /* Il merge non riproduce i file accessori: il gate/engine caricano <dir>/tokenizer.json. */
    {
        static const char* aux[] = {"tokenizer.json","tokenizer_config.json","vocab.json","merges.txt",
            "special_tokens_map.json","added_tokens.json","generation_config.json","chat_template.jinja"};
        for (size_t i = 0; i < sizeof aux / sizeof aux[0]; i++) {
            char s[4200], d[4200]; snprintf(s, sizeof s, "%s/%s", model, aux[i]); snprintf(d, sizeof d, "%s/%s", rec, aux[i]);
            llmo_copy_if_missing(s, d);
        }
    }
    /* Pulizia intermedi: adapter + checkpoint RAW (serviti al merge). Resta solo <model>.recovered
     * (e solo se ACCEPT). */
    remove(adp);
    { char t[4200];
      snprintf(t, sizeof t, "%s_best_A.raw", ckpt); remove(t);
      snprintf(t, sizeof t, "%s_best_B.raw", ckpt); remove(t);
      snprintf(t, sizeof t, "%s_best.meta", ckpt); remove(t);
      snprintf(t, sizeof t, "%s.json", ckpt); remove(t); }
    /* Step 4: re-gate. */
    char g2[512]; g2[0] = 0;
    llmmodelio_model_gate(ref, rec, NULL, g2, sizeof g2);
    if (strstr(g2, "\"ACCEPT\"") != NULL) {
        snprintf(report, cap, "{\"verdict\":\"ACCEPT\",\"trained\":true,\"recovered\":\"%s\",\"gate\":%s}", rec, g2);
        return 0;
    }
    llmo_rm_rf(rec);   /* rollback: il recuperato non convince */
    snprintf(report, cap, "{\"verdict\":\"REJECT-FLAG\",\"trained\":true,\"rolled_back\":true,\"gate\":%s}", g2);
    return 0;
}
