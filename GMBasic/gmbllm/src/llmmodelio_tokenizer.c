/* llmmodelio_tokenizer.c — B2: tokenizer / vocab come sub-verbi di `LLM TENSOR`.
 *
 * Wiring sul tokenizer di GMBasic (`llmtoken`, licenza Apache — FONTE UNICA della
 * tokenizzazione): nessuna nuova logica di tokenizzazione/decodifica. Sorgente del tokenizer:
 *   - file .gguf  -> token/merges EMBEDDED (via engine MLX, dlopen a dipendenza debole);
 *   - dir MLX/HF  -> <dir>/tokenizer.json.
 *
 * Sotto-verbi (sub; a = <model>, b = argomento):
 *   TOKENIZE   | TOKENS   <model>, <testo>          -> n + ids
 *   DETOKENIZE | DETOK    <model>, <id,id,...>      -> n + testo
 *   VOCAB                 <model>[, <filtro>]       -> vocab_size (+ primi match)
 *   TOKENIZER  | TOKINFO  <model>                   -> fonte + vocab_size
 *
 * L'output va su `out` (il FILE* del chiamante = LOUT): così la variante `_to` lo cattura e i
 * 4 backend lo vedono identico. Errori ESPLICITI (mai no-op silenziosi, §0bis).
 * Non tocca lexer/parser/registry: il sotto-verbo di LLM TENSOR è già un argomento STRINGA
 * generico instradato a `llmmodelio_tensor_cmd` su tutte le gambe. */
#include "llmmodelio_model.h"
#include "llmtoken.h"
#include "llmjinja.h"
#include "llmmodelio_infer.h"   /* B4: llmmodelio_imatrix_compute (calibrazione) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <dlfcn.h>
#include <sys/stat.h>

/* Legge un file di testo in `out` (cap byte). 0 ok / -1 errore. */
static int read_text_file(const char* path, char* out, size_t cap)
{
    if (cap == 0) return -1;
    FILE* f = fopen(path, "rb");
    if (f == NULL) { out[0] = 0; return -1; }
    size_t n = fread(out, 1, cap - 1, f);
    out[n] = 0;
    fclose(f);
    return 0;
}

/* Estrae il primo valore stringa del campo JSON `field` (stringa singola oppure PRIMO elemento
 * di un array), de-escapando le sequenze comuni (\n \t \r \" \\ \/). 0 ok / -1 non trovato.
 * (\uXXXX passato letterale: i chat template sono quasi sempre ASCII.) */
static int json_first_string(const char* json, const char* field, char* out, size_t cap)
{
    if (json == NULL || field == NULL || cap == 0) return -1;
    char pat[128]; snprintf(pat, sizeof pat, "\"%s\"", field);
    const char* p = strstr(json, pat);
    if (p == NULL) return -1;
    p += strlen(pat);
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
    if (*p != ':') return -1;
    p++;
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
    if (*p == '[') {
        p++;
        while (*p == ' ' || *p == ',' || *p == '\t' || *p == '\n' || *p == '\r') p++;
    }
    if (*p != '"') return -1;
    p++;
    size_t o = 0;
    while (*p && *p != '"' && o + 1 < cap) {
        char ch = *p++;
        if (ch == '\\' && *p) {
            char e = *p++;
            switch (e) {
                case 'n': ch = '\n'; break;
                case 't': ch = '\t'; break;
                case 'r': ch = '\r'; break;
                case '"': ch = '"'; break;
                case '\\': ch = '\\'; break;
                case '/': ch = '/'; break;
                default: ch = e; break;
            }
        }
        out[o++] = ch;
    }
    out[o] = 0;
    return o > 0 ? 0 : -1;
}

/* Carica il tokenizer di <model> (dir MLX/HF, file .gguf o .json). NULL su errore (silenzioso:
 * l'errore è propagato al chiamante — comando: raise; funzione: -1). */
static llmtoken_handle llmtok_open(const char* model)
{
    if (model == NULL || !*model) return NULL;
    size_t ml = strlen(model);
    if (ml > 5 && strcasecmp(model + ml - 5, ".json") == 0) return llmtoken_load(model);
    if (ml > 5 && strcasecmp(model + ml - 5, ".gguf") == 0) {
        void* h = dlopen("libgmb_ai_mlx.dylib", RTLD_NOW | RTLD_LOCAL);
        if (h == NULL) h = dlopen("libllm_ai_mlx.dylib", RTLD_NOW | RTLD_LOCAL);
        if (h == NULL) return NULL;
        typedef char* (*tokfn)(const char*, size_t*);
        typedef void  (*freefn)(char*);
        tokfn TJ = (tokfn)dlsym(h, "llm_ai_mlx_gguf_tokenizer_json");
        if (TJ == NULL) return NULL;
        size_t jl = 0; char* js = TJ(model, &jl);
        if (js == NULL) return NULL;
        llmtoken_handle th = llmtoken_load_json(js, jl);
        freefn FS = (freefn)dlsym(h, "llm_ai_mlx_free_str");
        if (FS) FS(js); else free(js);
        return th;
    }
    if (ml > 5 && strcasecmp(model + ml - 6, ".model") == 0) return llmtoken_load_sp(model);
    char tp[1300];
    snprintf(tp, sizeof tp, "%s/tokenizer.json", model);
    llmtoken_handle th = llmtoken_load(tp);
    if (th != NULL) return th;
    /* Fallback SentencePiece NATIVO: modelli con SOLO `tokenizer.model` (es. ERNIE-4.5). */
    char sp[1300];
    snprintf(sp, sizeof sp, "%s/tokenizer.model", model);
    return llmtoken_load_sp(sp);
}

/* Apre il tokenizer di un modello (dir / .json / .gguf / .model) con fallback SentencePiece.
 * Esposto per i chiamanti che costruivano il path `<dir>/tokenizer.json` a mano (es. i verbi
 * LLM TENSOR LOGITS/QUALITY/USAGEEVAL e l'export GGUF). */
llmtoken_handle llmmodelio_tokenizer_open(const char* model) { return llmtok_open(model); }

/* Carica il chat template di <model> in `out`: GGUF -> tokenizer.chat_template (engine);
 * dir -> chat_template.jinja oppure tokenizer_config.json["chat_template"]. 0 ok / -1 assente. */
static int chat_template_of(const char* model, char* out, size_t cap)
{
    out[0] = 0;
    size_t ml = strlen(model);
    if (ml > 5 && strcasecmp(model + ml - 5, ".gguf") == 0) {
        void* h = dlopen("libgmb_ai_mlx.dylib", RTLD_NOW | RTLD_LOCAL);
        if (h == NULL) h = dlopen("libllm_ai_mlx.dylib", RTLD_NOW | RTLD_LOCAL);
        if (h == NULL) return -1;
        typedef char* (*meta_fn)(const char*, size_t*);
        typedef void  (*freefn)(char*);
        meta_fn CM = (meta_fn)dlsym(h, "llm_ai_mlx_gguf_chat_meta");
        if (CM == NULL) return -1;
        size_t jl = 0; char* js = CM(model, &jl);
        if (js == NULL) return -1;
        int rc = json_first_string(js, "chat_template", out, cap);
        freefn FS = (freefn)dlsym(h, "llm_ai_mlx_free_str");
        if (FS) FS(js); else free(js);
        return (rc == 0 && out[0]) ? 0 : -1;
    }
    char jf[1300];
    snprintf(jf, sizeof jf, "%s/chat_template.jinja", model);
    read_text_file(jf, out, cap);
    if (!out[0]) {
        char cfg[1300]; snprintf(cfg, sizeof cfg, "%s/tokenizer_config.json", model);
        const size_t sc = 4u * 1024u * 1024u;
        char* js = (char*)malloc(sc);
        if (js != NULL) { if (read_text_file(cfg, js, sc) == 0) json_first_string(js, "chat_template", out, cap); free(js); }
    }
    return out[0] ? 0 : -1;
}

/* B4 — carica i token del dataset di calibrazione: `.bin` = int32 grezzi; altrimenti un file di
 * testo tokenizzato col tokenizer del modello (fonte unica llmtoken). Ritorna n (>0) o -1. */
static int b4_load_tokens(const char* model, const char* dataset, int32_t* ids, int cap)
{
    if (dataset == NULL || !*dataset || cap <= 0) return -1;
    size_t dl = strlen(dataset);
    if (dl > 4 && strcasecmp(dataset + dl - 4, ".bin") == 0) {
        FILE* bf = fopen(dataset, "rb");
        if (bf == NULL) return -1;
        int n = (int)fread(ids, sizeof(int32_t), (size_t)cap, bf);
        fclose(bf);
        return n;
    }
    llmtoken_handle th = llmtok_open(model);
    if (th == NULL) return -1;
    char* txt = (char*)malloc(8u * 1024u * 1024u);
    if (txt == NULL) { llmtoken_free(th); return -1; }
    if (read_text_file(dataset, txt, 8u * 1024u * 1024u) != 0) { free(txt); llmtoken_free(th); return -1; }
    int n = llmtoken_encode(th, txt, ids, cap);
    free(txt); llmtoken_free(th);
    return n;
}

int llmmodelio_tokenizer_cmd(const char* sub, const char* a, const char* b, const char* c, FILE* out, int* handled)
{
    const char* s = sub ? sub : "";
    const char* model = a ? a : "";
    const char* arg = b ? b : "";
    (void)c;
    FILE* o = out ? out : stdout;
    if (handled) *handled = 0;

    int is_tok = strcasecmp(s, "TOKENIZE") == 0 || strcasecmp(s, "TOKENS") == 0;
    int is_det = strcasecmp(s, "DETOKENIZE") == 0 || strcasecmp(s, "DETOK") == 0;
    int is_voc = strcasecmp(s, "VOCAB") == 0;
    int is_inf = strcasecmp(s, "TOKENIZER") == 0 || strcasecmp(s, "TOKINFO") == 0;
    int is_tpl = strcasecmp(s, "TEMPLATE") == 0 || strcasecmp(s, "CHATTEMPLATE") == 0;
    int is_tpltest = strcasecmp(s, "TEMPLATETEST") == 0 || strcasecmp(s, "APPLYTEMPLATE") == 0 || strcasecmp(s, "CHATTEST") == 0;
    int is_cal = strcasecmp(s, "CALIBRATE") == 0;   /* B4 */
    int is_act = strcasecmp(s, "ACTIVATIONSTATS") == 0 || strcasecmp(s, "ACTSTATS") == 0;   /* B4 */
    if (!(is_tok || is_det || is_voc || is_inf || is_tpl || is_tpltest || is_cal || is_act)) return 0;   /* non è un nostro sotto-verbo */
    if (handled) *handled = 1;

    /* B4 — LLM CALIBRATE <model>, <dataset>[, <out.imatrix>]: imatrix (importanza attivazioni). */
    if (is_cal) {
        if (!*model || !*arg) return -1;
        const char* outp = (c && *c) ? c : "work/model.imatrix.gguf";
        int32_t ids[65536];
        int nids = b4_load_tokens(model, arg, ids, 65536);
        if (nids <= 0) return -1;
        { char d[1200]; snprintf(d, sizeof d, "%s", outp); char* sl = strrchr(d, '/'); if (sl != NULL) { *sl = 0; if (*d) mkdir(d, 0755); } }
        if (llmmodelio_imatrix_compute(model, ids, nids, outp, 512) != 0) return -1;
        fprintf(o, "LLM CALIBRATE: model=%s dataset=%s imatrix=%s tokens=%d\n", model, arg, outp, nids);
        return 0;
    }
    /* B4 — LLM TENSOR ACTIVATIONSTATS <model>, <dataset>: statistiche di attivazione (routing
     * reale sui token del dataset) + aggregato esperti attivi. */
    if (is_act) {
        if (!*model || !*arg) return -1;
        int32_t ids[65536];
        int nids = b4_load_tokens(model, arg, ids, 65536);
        if (nids <= 0) return -1;
        int nl = 0, ne = 0;
        double* cts = llmmodelio_routing_counts(model, ids, nids, &nl, &ne);
        if (cts == NULL) return -1;
        int active = 0;
        for (int i = 0; i < nl * ne; i++) if (cts[i] > 0.0) active++;
        fprintf(o, "LLM TENSOR ACTIVATIONSTATS '%s': layers=%d experts=%d tokens=%d active=%d\n",
                model, nl, ne, nids, active);
        free(cts);
        return 0;
    }

    if (is_tok) {
        if (!*model) return -1;
        llmtoken_handle th = llmtok_open(model);
        if (th == NULL) return -1;
        int32_t ids[4096];
        int n = llmtoken_encode(th, arg, ids, 4096);
        llmtoken_free(th);
        if (n < 0) return -1;
        fprintf(o, "LLM TENSOR TOKENIZE '%s': n=%d ids=", model, n);
        for (int i = 0; i < n; i++) fprintf(o, "%s%d", i ? "," : "", (int)ids[i]);
        fprintf(o, "\n");
        return 0;
    }
    if (is_det) {
        if (!*model || !*arg) return -1;
        llmtoken_handle th = llmtok_open(model);
        if (th == NULL) return -1;
        int32_t ids[4096]; int n = 0;
        char* p = (char*)arg; char* endp = NULL;
        while (*p && n < 4096) {
            while (*p == ' ' || *p == ',' || *p == '\t') p++;
            if (!*p) break;
            long v = strtol(p, &endp, 10);
            if (endp == p) break;
            ids[n++] = (int32_t)v;
            p = endp;
        }
        char txt[65536]; txt[0] = 0;
        int nb = llmtoken_decode(th, ids, n, txt, (int)sizeof txt - 1);
        llmtoken_free(th);
        if (nb < 0) return -1;
        txt[sizeof txt - 1] = 0;
        fprintf(o, "LLM TENSOR DETOKENIZE '%s': n=%d text=%s\n", model, n, txt);
        return 0;
    }
    if (is_voc) {
        if (!*model) return -1;
        llmtoken_handle th = llmtok_open(model);
        if (th == NULL) return -1;
        size_t ntok = 0; const char* const* toks = llmtoken_export_tokens(th, &ntok);
        fprintf(o, "LLM TENSOR VOCAB '%s': vocab_size=%zu\n", model, ntok);
        if (*arg && toks) {
            int shown = 0;
            for (size_t i = 0; i < ntok && shown < 50; i++) {
                const char* t = toks[i] ? toks[i] : "";
                if (strstr(t, arg)) { fprintf(o, "  [%zu] '%s'\n", i, t); shown++; }
            }
            fprintf(o, "  match=%d (filtro '%s')\n", shown, arg);
        }
        llmtoken_free(th);
        return 0;
    }
    /* is_tpl: mostra il chat template del modello (diagnostica: template errato = output diverso
     * a parità di grafo). GGUF -> tokenizer.chat_template via engine; dir -> chat_template.jinja
     * oppure tokenizer_config.json["chat_template"]. */
    if (is_tpl) {
        if (!*model) return -1;
        char tpl[65536];
        if (chat_template_of(model, tpl, sizeof tpl) != 0) return -1;
        fprintf(o, "LLM TENSOR TEMPLATE '%s':\n%s\n", model, tpl);
        return 0;
    }
    /* is_tpltest: RENDE il chat template su un transcript. Messaggi = "role:content|role:content".
     * Jinja (llmjinja) = fonte unica Apache; bos/eos vuoti (il template li referenzia). */
    if (is_tpltest) {
        if (!*model || !*arg) return -1;
        char tpl[65536];
        if (chat_template_of(model, tpl, sizeof tpl) != 0) return -1;
        llmjinja_val* root = llmjinja_obj();
        llmjinja_val* msgs = llmjinja_arr();
        char buf[65536]; snprintf(buf, sizeof buf, "%s", arg);
        char* save = NULL;
        for (char* tk = strtok_r(buf, "|", &save); tk != NULL; tk = strtok_r(NULL, "|", &save)) {
            char* colon = strchr(tk, ':');
            const char* role = tk; const char* content = "";
            if (colon != NULL) { *colon = 0; content = colon + 1; }
            llmjinja_val* m = llmjinja_obj();
            llmjinja_obj_set(m, "role", llmjinja_str(role));
            llmjinja_obj_set(m, "content", llmjinja_str(content));
            llmjinja_arr_push(msgs, m);
        }
        llmjinja_obj_set(root, "messages", msgs);
        llmjinja_obj_set(root, "bos_token", llmjinja_str(""));
        llmjinja_obj_set(root, "eos_token", llmjinja_str(""));
        char err[256]; err[0] = 0;
        char* rendered = llmjinja_render(tpl, root, err, sizeof err);
        llmjinja_val_free(root);
        if (rendered == NULL) return -1;
        fprintf(o, "LLM TENSOR TEMPLATETEST '%s':\n%s\n", model, rendered);
        free(rendered);
        return 0;
    }
    /* is_inf */
    if (!*model) return -1;
    llmtoken_handle th = llmtok_open(model);
    if (th == NULL) return -1;
    size_t ntok = 0; (void)llmtoken_export_tokens(th, &ntok);
    const char* gm = llmtoken_gguf_model(th);
    fprintf(o, "LLM TENSOR TOKENIZER '%s': vocab_size=%zu source=%s\n", model, ntok, gm ? gm : "tokenizer.json");
    llmtoken_free(th);
    return 0;
}

/* B2 — `LLM.TOKENCOUNT(<model>, <testo>)`: numero di token del testo col tokenizer di <model>.
 * Ritorna >=0 oppure -1 (errore). Stessa fonte unica `llmtoken` dei sotto-verbi. */
int llmmodelio_token_count(const char* model, const char* text)
{
    if (model == NULL || !*model) return -1;
    llmtoken_handle th = llmtok_open(model);
    if (th == NULL) return -1;
    size_t tlen = text ? strlen(text) : 0;
    size_t tcap = tlen + 16;
    int32_t* ids = (int32_t*)malloc(tcap * sizeof(int32_t));
    if (ids == NULL) { llmtoken_free(th); return -1; }
    int n = llmtoken_encode(th, text ? text : "", ids, (int)tcap);
    free(ids);
    llmtoken_free(th);
    return n < 0 ? -1 : n;
}

/* ── B2 (2026-10-30) — i verbi del tokenizer come DATO, non come stampa ─────────────────────
 * `llmmodelio_tokenize_ids_to` e `llmmodelio_detokenize_to` sono la SORGENTE UNICA delle
 * versioni-funzione (`LLM.TOKENS`, `LLM.DETOKENIZE`) e del comando con `INTO`: scrivono il
 * risultato nel buffer del chiamante (nessuna scrittura su stdout). */

int llmmodelio_tokenize_ids_to(const char* model, const char* text, char* out, size_t cap)
{
    if (out == NULL || cap == 0) return -1;
    out[0] = 0;
    if (model == NULL || !*model) return -1;
    llmtoken_handle th = llmtok_open(model);
    if (th == NULL) return -1;
    /* NESSUN buffer fisso: il testo puo' essere lungo (corpus interi). Un limite a 4096
     * troncava E sforava (llmtoken_encode scrive fino a cap: con 60k token -> overflow di
     * stack, SIGBUS). Cap = byte del testo + 16 (un token non puo' superare un byte). */
    size_t tlen = text ? strlen(text) : 0;
    size_t tcap = tlen + 16;
    int32_t* ids = (int32_t*)malloc(tcap * sizeof(int32_t));
    if (ids == NULL) { llmtoken_free(th); return -1; }
    int n = llmtoken_encode(th, text ? text : "", ids, (int)tcap);
    if (n < 0) { free(ids); llmtoken_free(th); return -1; }
    if ((size_t)n >= tcap) { free(ids); llmtoken_free(th); return -1; }  /* troncato: errore esplicito */
    llmtoken_free(th);
    size_t pos = 0;
    for (int i = 0; i < n; i++) {
        int w = snprintf(out + pos, cap - pos, "%s%d", i ? "," : "", (int)ids[i]);
        if (w < 0) { free(ids); return -1; }
        if ((size_t)w >= cap - pos) { out[cap - 1] = 0; free(ids); return -2; }  /* TRONCATO: riprova piu' grande */
        pos += (size_t)w;
    }
    free(ids);
    return n;
}

int llmmodelio_detokenize_to(const char* model, const char* ids_csv, char* out, size_t cap)
{
    if (out == NULL || cap == 0) return -1;
    out[0] = 0;
    if (model == NULL || !*model) return -1;
    llmtoken_handle th = llmtok_open(model);
    if (th == NULL) return -1;
    int32_t ids[4096];
    int n = 0;
    const char* p = ids_csv ? ids_csv : "";
    while (*p && n < 4096) {
        while (*p == ' ' || *p == ',') p++;
        if (!*p) break;
        ids[n++] = (int32_t)strtol(p, (char**)&p, 10);
        while (*p == ' ' || *p == ',') p++;
    }
    char buf[8192];
    int w = llmtoken_decode(th, ids, n, buf, (int)sizeof buf);
    llmtoken_free(th);
    if (w < 0) return -1;
    snprintf(out, cap, "%s", buf);
    return 0;
}

