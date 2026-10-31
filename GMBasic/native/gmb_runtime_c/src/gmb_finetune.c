/* gmb_finetune.c — fine-tune/deploy LoRA MLX IN-PROCESS (mondo C).
 * Vedi gmb_finetune.h. Regole: runtime condiviso C (nessuna duplicazione nei backend), mai stub
 * (errore esplicito). Chiama la dylib `gmb_ai_finetune` (native/gmb_ai_finetune) con dlopen+call:
 * nessun subprocess (la logica C si chiama, non si spawna). */
#include "gmb_finetune.h"
#include "gmb_runtime.h"   /* gmb_opt_env */
#include "gmb_ipc_protocol.h"
#include "gmb_job_registry.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <dlfcn.h>

#define GMB_FT_MAX_OUT 4096

/* Percorso della dylib del fine-tune (in-process): GMB_AI_LIB_DIR, oppure risalendo dalla CWD
 * fino a `build/libs/<rid>/` e alla cache utente `~/.gmbasic/libs/<rid>/` (stessa convenzione
 * della dylib MLX; `Resources/libs` non esiste più). */
static const char* ft_dylib_path(void)
{
    static char buf[4096];
    const char* env = gmb_opt_env("GMB_AI_LIB_DIR");
    if (env && *env) {
        snprintf(buf, sizeof buf, "%s/libgmb_ai_finetune.dylib", env);
        if (access(buf, R_OK) == 0) return buf;
    }
    char cwd[2048];
    if (getcwd(cwd, sizeof cwd) != NULL) {
        char dir[2048]; snprintf(dir, sizeof dir, "%s", cwd);
        for (int up = 0; up < 10; ++up) {
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

/* Chiama IN-PROCESS una funzione del runtime fine-tune (nessun subprocess).
 * `sym` = "gmb_ai_finetune_exec" | "gmb_ai_lora_apply_exec"; request JSON in, response JSON out. */
static int ft_call(const char* sym, const char* req, char* body, size_t cap)
{
    static void* h = NULL;
    if (h == NULL) h = dlopen(ft_dylib_path(), RTLD_NOW | RTLD_LOCAL);
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

/* estrae un valore JSON: cerca "\"train\":" e poi atof dopo la cifra. */
static double ft_json_dbl(const char* body, const char* key, double dflt)
{
    char pat[64]; snprintf(pat, sizeof pat, "\"%s\"", key);
    const char* k = strstr(body, pat);
    if (!k) return dflt;
    k = strchr(k, ':');
    if (!k) return dflt;
    while (*k && (*k == ':' || *k == ' ' || *k == '\t')) k++;
    return strtod(k, NULL);
}

int gmb_ai_finetune_run(const char* model_dir, const char* dataset, const char* holdout,
                        const char* ckpt, int rank, int layer, int steps, int batch,
                        double lr, int seed, const char* frozen, char* out, size_t cap)
{
    if (!out || cap == 0) return -1;
    out[0] = 0;

    /* R2: costruisci richiesta IPC JSON e invia al server nativo. */
    char req[GMB_IPC_MAX_LINE];
    if (gmb_ipc_build_finetune(model_dir, dataset, holdout, ckpt, rank, layer,
                               steps, batch, lr, seed, frozen, req, sizeof req) != 0) {
        snprintf(out, cap, "error: richiesta IPC finetune troppo lunga");
        return -1;
    }
    char body[GMB_FT_MAX_OUT];
    int rc = ft_call("gmb_ai_finetune_exec", req, body, sizeof body);
    if (rc != 0) {
        snprintf(out, cap, "error: %s", body[0] ? body : "runtime gmb_ai_finetune non disponibile");
        return -1;
    }

    /* parla json strutturato: verifica ok e tipo */
    int rtype = gmb_ipc_response_type(body);
    if (rtype != 0) {
        char msg[512]; msg[0] = 0;
        gmb_ipc_extract(body, "message", msg, sizeof msg);
        snprintf(out, cap, "error: %s", msg[0] ? msg : "risposta IPC non result");
        return -1;
    }
    gmb_ipc_result_ok(body);
    double train = gmb_ipc_extract_dbl(body, "train", -1);
    double hold = gmb_ipc_extract_dbl(body, "hold", -1);
    long g0 = gmb_ipc_extract_long(body, "greedyBefore", -1);
    long g1 = gmb_ipc_extract_long(body, "greedyAfter", -1);
    double btrain = gmb_ipc_extract_dbl(body, "baseTrain", -1);
    double bhold = gmb_ipc_extract_dbl(body, "baseHold", -1);
    long demo = gmb_ipc_extract_long(body, "demoChanged", 0);
    snprintf(out, cap,
             "ok train=%.5f hold=%.5f baseTrain=%.5f baseHold=%.5f greedyBefore=%ld greedyAfter=%ld demoChanged=%ld",
             train, hold, btrain, bhold, g0, g1, demo);
    return 0;
}

int gmb_ai_lora_apply_run(const char* ckpt_prefix, const char* model_dir, const char* dataset,
                          char* out, size_t cap)
{
    if (!out || cap == 0) return -1;
    out[0] = 0;
    /* R2: IPC request */
    char req[GMB_IPC_MAX_LINE];
    if (gmb_ipc_build_apply(ckpt_prefix, model_dir, dataset, req, sizeof req) != 0) {
        snprintf(out, cap, "error: richiesta IPC apply troppo lunga");
        return -1;
    }
    char body[GMB_FT_MAX_OUT];
    int rc = ft_call("gmb_ai_lora_apply_exec", req, body, sizeof body);
    if (rc != 0) {
        snprintf(out, cap, "error: %s", body[0] ? body : "runtime gmb_ai_finetune (apply) non disponibile");
        return -1;
    }
    /* risposta JSON strutturata */
    if (gmb_ipc_response_type(body) != 0) {
        char msg[512]; msg[0] = 0;
        gmb_ipc_extract(body, "message", msg, sizeof msg);
        snprintf(out, cap, "error: %s", msg[0] ? msg : "apply fallito");
        return -1;
    }
    char msg[512]; msg[0] = 0;
    gmb_ipc_extract(body, "message", msg, sizeof msg);
    /* Ritorna il messaggio GREZZO: il prefisso "ok: " lo aggiunge lo strato opts
     * (gmb_ai_lora_apply_run_opts), così lo stato coincide col canone C#/IL
     * (`"ok: " + msg`) su tutti e 5 i backend. */
    if (strstr(msg, "nessun effetto")) {
        snprintf(out, cap, "error: %s", msg);
        return -1;
    }
    snprintf(out, cap, "%s", msg[0] ? msg : "ok");
    return 0;
}

static int ft_json_str(const char* body, const char* key, char* out, size_t cap);

/* TRAIN DPO <model>, {chosen, rejected, ckpt, rank, layer, steps, batch, lr, beta, seed}.
 * Delega all'orchestrazione DPO della dylib gmb_ai_finetune (gmb_ai_dpo_exec). */
static void ft_state_set_train(double train);   /* def. dopo gli static di stato (sotto) */

int gmb_ai_dpo_run_opts(const char* model_dir, const char* json_opts, char* out, size_t cap)
{
    char chosen[512] = "", rejected[512] = "", bck[512] = "work/dpo";
    int rank = 4, layer = 15, steps = 40, batch = 2, seed = 7;
    double lr = 5e-3, beta = 0.1;
    if (json_opts && json_opts[0]) {
        ft_json_str(json_opts, "chosen", chosen, sizeof chosen);
        ft_json_str(json_opts, "rejected", rejected, sizeof rejected);
        /* §4: TRAIN DPO <model>, <pairs>, <out> — <pairs> è un PREFISSO: chosen/rejected
         * derivano per convenzione se non dati esplicitamente (parità col managed). */
        { char pairs[512] = ""; ft_json_str(json_opts, "pairs", pairs, sizeof pairs);
          if (pairs[0]) {
              if (!chosen[0]) snprintf(chosen, sizeof chosen, "%s.chosen.bin", pairs);
              if (!rejected[0]) snprintf(rejected, sizeof rejected, "%s.rejected.bin", pairs);
          } }
        if (ft_json_str(json_opts, "ckpt", bck, sizeof bck) != 0) snprintf(bck, sizeof bck, "%s", "work/dpo");
        rank = (int)ft_json_dbl(json_opts, "rank", rank);
        layer = (int)ft_json_dbl(json_opts, "layer", layer);
        steps = (int)ft_json_dbl(json_opts, "steps", steps);
        batch = (int)ft_json_dbl(json_opts, "batch", batch);
        seed = (int)ft_json_dbl(json_opts, "seed", seed);
        lr = ft_json_dbl(json_opts, "lr", lr);
        beta = ft_json_dbl(json_opts, "beta", beta);
    }
    /* §4: passo ESATTO di default (parità col managed AiFinetuneState.Dpo); 'exact':0 → SPSA. */
    int exact = (int)ft_json_dbl(json_opts, "exact", 1);
    if (!model_dir || !model_dir[0]) { snprintf(out, cap, "error: opzione 'model' (path modello MLX) mancante."); return -1; }
    if (!chosen[0]) { snprintf(out, cap, "error: opzione 'chosen' (dataset .bin preferite) mancante."); return -1; }
    if (!rejected[0]) { snprintf(out, cap, "error: opzione 'rejected' (dataset .bin rifiutate) mancante."); return -1; }
    char req[2048];
    snprintf(req, sizeof req,
        "{\"id\":1,\"method\":\"dpo\",\"params\":{\"model_dir\":\"%s\",\"chosen\":\"%s\",\"rejected\":\"%s\","
        "\"ckpt\":\"%s\",\"rank\":%d,\"layer\":%d,\"steps\":%d,\"batch\":%d,\"lr\":%g,\"beta\":%g,\"seed\":%d,\"exact\":%d}}\n",
        model_dir, chosen, rejected, bck, rank, layer, steps, batch, lr, beta, seed, exact);
    char body[GMB_FT_MAX_OUT];
    int rc = ft_call("gmb_ai_dpo_exec", req, body, sizeof body);
    if (rc != 0) { snprintf(out, cap, "error: %s", body[0] ? body : "runtime DPO non disponibile"); return -1; }
    if (gmb_ipc_response_type(body) != 0) {
        char msg[512]; msg[0] = 0; gmb_ipc_extract(body, "message", msg, sizeof msg);
        snprintf(out, cap, "error: %s", msg[0] ? msg : "DPO fallito"); return -1;
    }
    double train = gmb_ipc_extract_dbl(body, "train", -1);
    double base = gmb_ipc_extract_dbl(body, "baseTrain", -1);
    ft_state_set_train(train);   /* stato: LLM.FINETUNELOSS() dopo DPO (parità col managed TrainLoss) */
    snprintf(out, cap, "ok train=%.5f baseTrain=%.5f beta=%.4f", train, base, beta);
    return 0;
}

/* TRAIN GRPO <model>, {dataset|train, ckpt, rank, layer, group, maxnew, steps, lr, clip, kl,
 * temperature, seed}. G-opt2 — RLHF senza critic (RLVR): delega a gmb_ai_grpo_exec. */
int gmb_ai_grpo_run_opts(const char* model_dir, const char* json_opts, char* out, size_t cap)
{
    char dataset[512] = "", bck[512] = "work/grpo";
    int rank = 4, layer = 15, steps = 40, group = 4, maxnew = 1, seed = 7;
    double lr = 5e-3, clip = 0.2, kl = 0.0, temperature = 1.0;
    if (json_opts && json_opts[0]) {
        ft_json_str(json_opts, "dataset", dataset, sizeof dataset);
        if (!dataset[0]) ft_json_str(json_opts, "train", dataset, sizeof dataset);
        if (ft_json_str(json_opts, "ckpt", bck, sizeof bck) != 0) snprintf(bck, sizeof bck, "%s", "work/grpo");
        rank = (int)ft_json_dbl(json_opts, "rank", rank);
        layer = (int)ft_json_dbl(json_opts, "layer", layer);
        steps = (int)ft_json_dbl(json_opts, "steps", steps);
        group = (int)ft_json_dbl(json_opts, "group", group);
        maxnew = (int)ft_json_dbl(json_opts, "maxnew", maxnew);
        seed = (int)ft_json_dbl(json_opts, "seed", seed);
        lr = ft_json_dbl(json_opts, "lr", lr);
        clip = ft_json_dbl(json_opts, "clip", clip);
        kl = ft_json_dbl(json_opts, "kl", kl);
        temperature = ft_json_dbl(json_opts, "temperature", temperature);
    }
    if (!model_dir || !model_dir[0]) { snprintf(out, cap, "error: opzione 'model' (path modello MLX) mancante."); return -1; }
    if (!dataset[0]) { snprintf(out, cap, "error: opzione 'dataset' (o 'train') mancante."); return -1; }
    char req[2048];
    snprintf(req, sizeof req,
        "{\"id\":1,\"method\":\"grpo\",\"params\":{\"model_dir\":\"%s\",\"dataset\":\"%s\",\"ckpt\":\"%s\","
        "\"rank\":%d,\"layer\":%d,\"steps\":%d,\"group\":%d,\"maxnew\":%d,\"lr\":%g,\"clip\":%g,\"kl\":%g,\"temperature\":%g,\"seed\":%d}}\n",
        model_dir, dataset, bck, rank, layer, steps, group, maxnew, lr, clip, kl, temperature, seed);
    char body[GMB_FT_MAX_OUT];
    int rc = ft_call("gmb_ai_grpo_exec", req, body, sizeof body);
    if (rc != 0) { snprintf(out, cap, "error: %s", body[0] ? body : "runtime GRPO non disponibile"); return -1; }
    if (gmb_ipc_response_type(body) != 0) {
        char msg[512]; msg[0] = 0; gmb_ipc_extract(body, "message", msg, sizeof msg);
        snprintf(out, cap, "error: %s", msg[0] ? msg : "GRPO fallito"); return -1;
    }
    double train = gmb_ipc_extract_dbl(body, "grpo", -1);
    double base = gmb_ipc_extract_dbl(body, "base", -1);
    ft_state_set_train(train);   /* stato: parità col managed TrainLoss */
    snprintf(out, cap, "ok grpo=%.5f base=%.5f group=%d", train, base, group);
    return 0;
}

/* ------------------------- stato ultima run (persistente) -------------------- */
static int    s_ok = 0;
static double s_train = -1, s_hold = -1;
static long   s_g0 = -1, s_g1 = -1;
/* B3: misure che certificano l'apprendimento (prima calcolate e mai lette). */
static double s_base_train = -1, s_base_hold = -1;
static void ft_state_set_train(double train) { s_ok = 1; s_train = train; }
static long   s_demo = 0;
static char   s_ckpt[512] = "";
static char   s_status[GMB_FT_MAX_OUT] = "idle";

static void ft_set_state(int ok, const char* ckpt, double train, double hold, long g0, long g1)
{
    s_ok = ok; s_train = train; s_hold = hold; s_g0 = g0; s_g1 = g1;
    snprintf(s_ckpt, sizeof s_ckpt, "%s", ckpt ? ckpt : "");
    snprintf(s_status, sizeof s_status, "%s", ok ? "ok" : "error");
}

/* helper: trova chiave JSON e mette in out (stringa) */
static int ft_json_str(const char* body, const char* key, char* out, size_t cap)
{
    char pat[64]; snprintf(pat, sizeof pat, "\"%s\"", key);
    const char* k = strstr(body, pat);
    if (!k) return -1;
    k = strchr(k, ':');
    if (!k) return -1;
    while (*k && (*k == ':' || *k == ' ' || *k == '\t' || *k == '"')) k++;
    size_t n = 0;
    while (k[n] && k[n] != '"' && n + 1 < cap) { out[n] = k[n]; n++; }
    out[n] = 0;
    return 0;
}

int gmb_ai_finetune_run_opts(const char* model_dir, const char* json_opts, char* out, size_t cap)
{
    char bds[512] = "work/rule.train.bin";
    char bho[512] = "work/rule.hold.bin";
    char bck[512] = "work/ft";
    int rank = 4, layer = 15, steps = 60, batch = 4, seed = 7;
    double lr = 0.01;
    if (json_opts && json_opts[0]) {
        if (ft_json_str(json_opts, "dataset", bds, sizeof bds) != 0) snprintf(bds, sizeof bds, "%s", "work/rule.train.bin");
        if (ft_json_str(json_opts, "holdout", bho, sizeof bho) != 0) snprintf(bho, sizeof bho, "%s", "work/rule.hold.bin");
        if (ft_json_str(json_opts, "ckpt", bck, sizeof bck) != 0) snprintf(bck, sizeof bck, "%s", "work/ft");
        rank = (int)ft_json_dbl(json_opts, "rank", rank);
        layer = (int)ft_json_dbl(json_opts, "layer", layer);
        steps = (int)ft_json_dbl(json_opts, "steps", steps);
        batch = (int)ft_json_dbl(json_opts, "batch", batch);
        seed = (int)ft_json_dbl(json_opts, "seed", seed);
        lr = ft_json_dbl(json_opts, "lr", lr);
    }
    /* §4: opzioni 'frozen' (es. all-but-head) ed 'engine' (solo mlx). */
    char frozen[32] = ""; if (json_opts) ft_json_str(json_opts, "frozen", frozen, sizeof frozen);
    { char eng[32] = ""; if (json_opts) ft_json_str(json_opts, "engine", eng, sizeof eng);
      if (eng[0] && strcasecmp(eng, "mlx") != 0) { snprintf(out, cap, "error: engine '%s' non supportato (solo mlx)", eng); return -1; } }
    int rc = gmb_ai_finetune_run(model_dir, bds, bho, bck, rank, layer, steps, batch, lr, seed, frozen, out, cap);
    if (rc == 0) {
        double t = -1, h = -1; long g0 = -1, g1 = -1;
        char* p = strstr(out, "train="); if (p) t = atof(p + 6);
        p = strstr(out, "hold=");  if (p) h = atof(p + 5);
        p = strstr(out, "greedyBefore="); if (p) g0 = atol(p + 13);
        p = strstr(out, "greedyAfter=");  if (p) g1 = atol(p + 12);
        /* B3: baseline e demoChanged. "baseTrain="/"baseHold=" hanno la T/H maiuscola,
         * quindi NON collidono con i match minuscoli "train="/"hold=" sopra. */
        double bt = -1, bh = -1; long dm = 0;
        p = strstr(out, "baseTrain=");   if (p) bt = atof(p + 10);
        p = strstr(out, "baseHold=");    if (p) bh = atof(p + 9);
        p = strstr(out, "demoChanged="); if (p) dm = atol(p + 12);
        s_base_train = bt; s_base_hold = bh; s_demo = dm;
        ft_set_state(1, bck, t, h, g0, g1);
        /* R2.4: registra il job nel registro storico. */
        gmb_job_registry_add(GMB_JOB_FINETUNE, GMB_JOB_STATUS_OK, out,
                             t, h, bt, bh, g0, g1, dm, bck);
    } else {
        /* Run fallita: nessun checkpoint salvato → Ckpt "" (canone C#/IL e guida:
         * "'' se nessuna run ok"). Prima restava il ckpt RICHIESTO (default "work/ft")
         * → divergenza misurata su VM/C vs IL/PCode (probe parità 2026-10-01). */
        ft_set_state(0, "", -1, -1, -1, -1);
        s_base_train = -1; s_base_hold = -1; s_demo = 0;
        snprintf(s_status, sizeof s_status, "%s", out ? out : "error");
        gmb_job_registry_add(GMB_JOB_FINETUNE, GMB_JOB_STATUS_ERROR,
                             out ? out : "error", -1, -1, -1, -1, -1, -1, 0, bck);
    }
    return rc;
}

/* APPLY aggiorna SOLO Ok/Status: le misure di training (loss/hold/base/demo/greedy) e il
 * Ckpt restano quelli dell'ultimo FINETUNE — canone C#/IL (AiFinetuneState.Apply), parità
 * 4 backend (probe 2026-10-01). Prima azzerava le misure e sovrascriveva il ckpt col
 * prefisso passato → divergenza VM/C vs IL/PCode. */
static void ft_set_apply_status(int ok, const char* status)
{
    s_ok = ok;
    snprintf(s_status, sizeof s_status, "%s", status ? status : (ok ? "ok" : "error"));
}

int gmb_ai_lora_apply_run_opts(const char* ckpt_prefix, const char* json_opts, char* out, size_t cap)
{
    const char* model = "";
    const char* dataset = "";
    char mod_buf[512], ds_buf[512];
    if (json_opts && json_opts[0]) {
        if (ft_json_str(json_opts, "model", mod_buf, sizeof mod_buf) == 0) model = mod_buf;
        if (ft_json_str(json_opts, "dataset", ds_buf, sizeof ds_buf) == 0) dataset = ds_buf;
    }
    if (!model[0]) { snprintf(out, cap, "error: opzione 'model' (path modello MLX) mancante."); ft_set_apply_status(0, out); return -1; }
    if (!dataset[0]) { snprintf(out, cap, "error: opzione 'dataset' (file .bin per il prompt demo) mancante."); ft_set_apply_status(0, out); return -1; }
    int rc = gmb_ai_lora_apply_run(ckpt_prefix, model, dataset, out, cap);
    if (rc == 0) {
        char st[GMB_FT_MAX_OUT];
        snprintf(st, sizeof st, "ok: %s", out ? out : "");
        ft_set_apply_status(1, st);
        gmb_job_registry_add(GMB_JOB_LORA_APPLY, GMB_JOB_STATUS_OK, out ? out : "ok", -1, -1, -1, -1, -1, -1, 0, ckpt_prefix);
    } else {
        ft_set_apply_status(0, out);
        gmb_job_registry_add(GMB_JOB_LORA_APPLY, GMB_JOB_STATUS_ERROR, out ? out : "error", -1, -1, -1, -1, -1, -1, 0, ckpt_prefix);
    }
    return rc;
}

int gmb_ai_finetune_done(void) { return s_ok ? -1 : 0; }
double gmb_ai_finetune_loss(void) { return s_train; }
double gmb_ai_finetune_hold(void) { return s_hold; }
long gmb_ai_finetune_greedy(void) { return s_g0; }
/* B3: misure che certificano l'apprendimento (prima calcolate e mai esposte). */
long gmb_ai_finetune_greedy_after(void) { return s_g1; }
long gmb_ai_finetune_demo_changed(void) { return s_demo; }
double gmb_ai_finetune_base_train(void) { return s_base_train; }
double gmb_ai_finetune_base_hold(void) { return s_base_hold; }
const char* gmb_ai_finetune_ckpt(void) { return s_ckpt; }
const char* gmb_ai_finetune_status(void) { return s_status; }
/* R2.4: job registry accessor. */
int gmb_ai_finetune_last_job_id(void) { return gmb_job_registry_last_id(); }

/* ------------------------- query del registro run (TRAIN RUNS) --------------- */
int gmb_ai_runs_count(void) { return gmb_job_registry_count(); }

/* TRAIN RUNS PRUNE: svuota il registro (ritorna il numero di run rimosse). */
int gmb_ai_runs_clear(void)
{
    int n = gmb_job_registry_count();
    gmb_job_registry_clear();
    return n;
}

/* TRAIN STOP <id>: rimuove la run dal registro. 1 se trovata. */
int gmb_ai_runs_stop(int id) { return gmb_job_registry_remove(id); }

/* TRAIN EXPORT <out>[, <run>]: esporta l'adapter della run `id` (id<=0 → l'ultima con ckpt).
 * Copia <ckpt>_best_{A,B}.raw -> <out>_best_{A,B}.raw. Ritorna il numero di file copiati
 * (0 se nessuna run con ckpt). */
int gmb_ai_train_export_run(const char* out, int id)
{
    if (!out || !out[0]) return -1;
    const char* ck = NULL;
    if (id > 0) { const gmb_job_record* r = gmb_job_registry_get(id); if (r && r->ckpt[0]) ck = r->ckpt; }
    else {
        int n = gmb_job_registry_count();
        for (int i = 0; i < n; i++) { const gmb_job_record* r = gmb_job_registry_at(i); if (r && r->ckpt[0]) ck = r->ckpt; }
    }
    if (!ck) return 0;
    const char* sfx[2] = { "_best_A.raw", "_best_B.raw" };
    char src[1200], dst[1200]; int copied = 0;
    for (int s = 0; s < 2; s++) {
        snprintf(src, sizeof src, "%s%s", ck, sfx[s]);
        snprintf(dst, sizeof dst, "%s%s", out, sfx[s]);
        FILE* in = fopen(src, "rb");
        if (!in) continue;
        FILE* o = fopen(dst, "wb");
        if (o) { char buf[65536]; size_t rd; while ((rd = fread(buf, 1, sizeof buf, in)) > 0) fwrite(buf, 1, rd, o); fclose(o); copied++; }
        fclose(in);
    }
    snprintf(dst, sizeof dst, "%s_best.meta", out);
    FILE* m = fopen(dst, "w"); if (m) { fprintf(m, "exported_from=%s\n", ck); fclose(m); }
    return copied;
}

/* TRAIN EXPORT <out> (compat): esporta l'ultima run. */
int gmb_ai_train_export(const char* out) { return gmb_ai_train_export_run(out, 0); }

/* TRAIN LORA LIST: adapter (ckpt) registrati dalle run LORA APPLY. JSON. Ownership chiamante. */
char* gmb_ai_lora_list(void)
{
    int n = gmb_job_registry_count();
    size_t cap = 128 + (size_t)(n > 0 ? n : 1) * 300;
    char* out = (char*)malloc(cap);
    if (!out) return NULL;
    int cnt = 0;
    for (int i = 0; i < n; i++) {
        const gmb_job_record* r = gmb_job_registry_at(i);
        if (r && r->kind == GMB_JOB_LORA_APPLY && r->ckpt[0]) cnt++;
    }
    size_t off = (size_t)snprintf(out, cap, "{\"count\":%d,\"adapters\":[", cnt);
    int k = 0;
    for (int i = 0; i < n; i++) {
        const gmb_job_record* r = gmb_job_registry_at(i);
        if (!r || r->kind != GMB_JOB_LORA_APPLY || !r->ckpt[0]) continue;
        off += (size_t)snprintf(out + off, cap - off, "%s\"%s\"", (k ? "," : ""), r->ckpt);
        k++;
    }
    if (off < cap) snprintf(out + off, cap - off, "]}");
    return out;
}

/* TRAIN DATASET STATS <file>: statistiche del dataset — byte + (se è un dataset .bin del
 * fine-tune: int32 count; per record int32 n, n×int32 ids, int32 target) n/esempi, token e
 * distribuzione dei target. OWNERSHIP del chiamante (free). */
char* gmb_ai_dataset_stats(const char* path)
{
    char* out = (char*)malloc(512);
    if (!out) return NULL;
    long bytes = -1; long samples = -1, tokens = -1; long tmin = 0, tmax = 0; double tsum = 0; long tn = 0;
    if (path && path[0]) {
        FILE* f = fopen(path, "rb");
        if (f) {
            fseek(f, 0, SEEK_END); bytes = ftell(f); fseek(f, 0, SEEK_SET);
            int32_t count = 0;
            if (bytes >= 4 && fread(&count, 4, 1, f) == 1 && count > 0 && count < 100000000) {
                samples = 0; tokens = 0; tmin = 0x7fffffff; tmax = -0x7fffffff;
                for (int32_t i = 0; i < count; i++) {
                    int32_t n = 0;
                    if (fread(&n, 4, 1, f) != 1 || n < 0 || n > 100000000) { samples = -1; break; }
                    if (fseek(f, (long)n * 4, SEEK_CUR) != 0) { samples = -1; break; }
                    int32_t tgt = 0;
                    if (fread(&tgt, 4, 1, f) != 1) { samples = -1; break; }
                    samples++; tokens += n;
                    if (tgt < tmin) tmin = tgt; if (tgt > tmax) tmax = tgt; tsum += (double)tgt; tn++;
                }
            }
            fclose(f);
        }
    }
    if (samples >= 0 && tn > 0)
        snprintf(out, 512,
            "{\"file\":\"%s\",\"bytes\":%ld,\"samples\":%ld,\"tokens\":%ld,"
            "\"target_min\":%ld,\"target_max\":%ld,\"target_mean\":%.2f}",
            path, bytes, samples, tokens, tmin, tmax, tsum / (double)tn);
    else if (samples >= 0)
        snprintf(out, 512, "{\"file\":\"%s\",\"bytes\":%ld,\"samples\":%ld,\"tokens\":%ld}", path, bytes, samples, tokens);
    else
        snprintf(out, 512, "{\"file\":\"%s\",\"bytes\":%ld}", path, bytes);
    return out;
}

/* ------------------------- TRAIN CONFIG (default di sessione) ---------------- */
/* Default storici del fine-tune (stessi del runtime C# AiFinetuneState): rank=4
 * layer=15 steps=60 batch=4 seed=7 lr=0.01. TRAIN CONFIG SHOW/SET li leggono/aggiornano. */
static int    s_cfg_rank = 4, s_cfg_layer = 15, s_cfg_steps = 60, s_cfg_batch = 4, s_cfg_seed = 7;
static double s_cfg_lr = 0.01;

char* gmb_ai_train_config_json(void)
{
    char* out = (char*)malloc(256);
    if (!out) return NULL;
    snprintf(out, 256,
        "{\"rank\":%d,\"layer\":%d,\"steps\":%d,\"batch\":%d,\"seed\":%d,\"lr\":%.5f}",
        s_cfg_rank, s_cfg_layer, s_cfg_steps, s_cfg_batch, s_cfg_seed, s_cfg_lr);
    return out;
}

/* TRAIN CONFIG SET <key>, <value>: aggiorna un default e ritorna il JSON aggiornato
 * (ownership del CHIAMANTE). Chiave ignota -> JSON invariato. */
char* gmb_ai_train_config_set(const char* key, const char* val)
{
    if (key && val) {
        if (strcasecmp(key, "rank") == 0) s_cfg_rank = atoi(val);
        else if (strcasecmp(key, "layer") == 0) s_cfg_layer = atoi(val);
        else if (strcasecmp(key, "steps") == 0) s_cfg_steps = atoi(val);
        else if (strcasecmp(key, "batch") == 0) s_cfg_batch = atoi(val);
        else if (strcasecmp(key, "seed") == 0) s_cfg_seed = atoi(val);
        else if (strcasecmp(key, "lr") == 0) s_cfg_lr = atof(val);
    }
    return gmb_ai_train_config_json();
}

/* TRAIN RUNS SHOW <id>: JSON di una run (stesso formato di un elemento di runs[]),
 * "" se l'id non esiste. OWNERSHIP: buffer malloc del CHIAMANTE (free()). */
char* gmb_ai_runs_item(int id)
{
    const gmb_job_record* r = gmb_job_registry_get(id);
    char* out = (char*)malloc(768);
    if (!out) return NULL;
    if (!r) { out[0] = 0; return out; }
    snprintf(out, 768,
        "{\"id\":%d,\"kind\":\"%s\",\"status\":\"%s\",\"train\":%.5f,\"hold\":%.5f,"
        "\"baseTrain\":%.5f,\"baseHold\":%.5f,\"greedyBefore\":%ld,\"greedyAfter\":%ld,"
        "\"demoChanged\":%ld,\"ckpt\":\"%s\"}",
        r->id,
        r->kind == GMB_JOB_LORA_APPLY ? "lora_apply" : "finetune",
        r->status == GMB_JOB_STATUS_OK ? "ok"
            : (r->status == GMB_JOB_STATUS_RUNNING ? "running" : "error"),
        r->train_loss, r->hold_loss, r->base_train, r->base_hold,
        r->greedy_before, r->greedy_after, r->demo_changed, r->ckpt);
    return out;
}

/* JSON: {"count":N,"runs":[{id,kind,status,train,hold,baseTrain,baseHold,greedyBefore,
 * greedyAfter,demoChanged,ckpt},...]}. OWNERSHIP: buffer malloc del CHIAMANTE (free()). */
char* gmb_ai_runs_json(void)
{
    int n = gmb_job_registry_count();
    size_t cap = 128 + (size_t)(n > 0 ? n : 1) * 768;
    char* out = (char*)malloc(cap);
    if (!out) return NULL;
    size_t off = (size_t)snprintf(out, cap, "{\"count\":%d,\"runs\":[", n);
    for (int i = 0; i < n; i++) {
        const gmb_job_record* r = gmb_job_registry_at(i);
        if (!r) continue;
        if (off >= cap) break;
        off += (size_t)snprintf(out + off, cap - off,
            "%s{\"id\":%d,\"kind\":\"%s\",\"status\":\"%s\",\"train\":%.5f,\"hold\":%.5f,"
            "\"baseTrain\":%.5f,\"baseHold\":%.5f,\"greedyBefore\":%ld,\"greedyAfter\":%ld,"
            "\"demoChanged\":%ld,\"ckpt\":\"%s\"}",
            (i ? "," : ""), r->id,
            r->kind == GMB_JOB_LORA_APPLY ? "lora_apply" : "finetune",
            r->status == GMB_JOB_STATUS_OK ? "ok"
                : (r->status == GMB_JOB_STATUS_RUNNING ? "running" : "error"),
            r->train_loss, r->hold_loss, r->base_train, r->base_hold,
            r->greedy_before, r->greedy_after, r->demo_changed, r->ckpt);
    }
    if (off < cap) snprintf(out + off, cap - off, "]}");
    return out;
}
