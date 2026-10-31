/* gmb_brain_mlx.c — Brain MLX per il loop C (vedi header).
 * dlopen di libllmtoken + libgmb_ai_mlx (Resources/libs/osx-arm64 o DYLD path).
 * I simboli engine/tokenizer si risolvono SOLO a runtime: questo file compila/link ovunque
 * senza toccare l'infrastruttura di build (§regola infrastruttura). */
#include "gmb_brain_mlx.h"
#include "gmb_runtime.h"   /* gmb_opt_env */
#include "gmb_chat_template.h"
#include "llmengine.h"
#include <ctype.h>
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_IDS 16384
#define MAX_NEW 512

typedef void* (*tok_load_fn)(const char*);
typedef void* (*tok_load_json_fn)(const char*, size_t);
typedef void* (*tok_load_tiktoken_fn)(const char*, const char*);
typedef void (*tok_free_fn)(void*);
typedef int (*tok_encode_fn)(void*, const char*, int32_t*, int);
typedef int (*tok_decode_fn)(void*, const int32_t*, int, char*, int);
typedef char* (*gguf_tok_json_fn)(const char*, size_t*);
typedef void (*free_str_fn)(char*);
/* ABI comune dei motori: risolta per NOME (una sola dlsym), poi si usa la vtable. */
typedef const LlmEngineLlmEngine* (*engine_get_fn)(void);

typedef struct {
    void* tok_lib; void* mlx_lib;
    tok_load_fn tok_load; tok_free_fn tok_free; tok_encode_fn tok_enc; tok_decode_fn tok_dec;
    tok_load_tiktoken_fn tok_load_tiktoken;   /* Kimi: tiktoken.model (niente tokenizer.json) */
    /* GGUF single-file (DS4): tokenizer dal GGUF invece di <dir>/tokenizer.json. Opzionali:
     * se assenti si resta al percorso tokenizer.json (nessun errore finché non serve). */
    tok_load_json_fn tok_load_json; gguf_tok_json_fn gguf_tok_json; free_str_fn mlx_free_str;
    const LlmEngineLlmEngine* eng;   /* vtable del motore (ABI comune); load/free/session/generate via eng */
    void* tok; void* model; void* sess;
    int is_gguf;
    int32_t eos;
    char err[512];
    char model_dir[1024];   /* per il chat_template automatico (gmb_chat_template) */
} mlx_brain_t;

static mlx_brain_t* g_brain = NULL;

/* System prompt risolto dell'agente (parità AgentRecord.ResolveSystemPrompt). Vive a livello di
 * modulo perché gmb_agent_ask lo imposta PRIMA che gmb_brain_ask_sync avvii/registri il brain
 * (first-call): gmb_brain_mlx_complete lo legge al render. Usato solo dal render col chat_template. */
#define GMB_BRAIN_MLX_SYS_CAP 8192
static char g_system_prompt[GMB_BRAIN_MLX_SYS_CAP];
static int g_system_set = 0;

void gmb_brain_mlx_set_system(const char* sys) {
    if (!sys || !*sys) { gmb_brain_mlx_clear_system(); return; }
    snprintf(g_system_prompt, sizeof g_system_prompt, "%s", sys);
    g_system_set = 1;
}
void gmb_brain_mlx_clear_system(void) { g_system_prompt[0] = 0; g_system_set = 0; }

/* Limite di token nuovi (parità agent.MaxTokens C#). <=0 → default MAX_NEW. Come il system,
 * è impostato da gmb_agent_ask prima di gmb_brain_ask_sync (first-call). */
static int g_max_new = 0;
void gmb_brain_mlx_set_max_new(int n) { g_max_new = (n > 0) ? n : 0; }

/* ===== Harmony (gpt-oss): normalizzazione dell'output GREZZO del modello =====
 * Il modello genera i canali grezzi (es. "<|channel|>analysis<|message|>...<|channel|>final
 * <|message|>Roma.<|return|>"). Va normalizzato PRIMA del cleanup marker, altrimenti al
 * re-render il template Harmony solleva ("<|channel|> tags in the content field").
 * Parità con NormalizeHarmonyOutput (MlxProviders.cs C#). */
static const char* const HARMONY_MARKERS[] = {
    "<|channel|>", "<|start|>", "<|message|>", "<|end|>",
    "<|return|>", "<|call|>", "<|constrain|>", "<|final|>"
};
static int harmony_has_marker(const char* s) {
    for (size_t i = 0; i < sizeof(HARMONY_MARKERS) / sizeof(HARMONY_MARKERS[0]); i++)
        if (strstr(s, HARMONY_MARKERS[i])) return 1;
    return 0;
}
/* Se `raw` contiene marker Harmony, scrive il testo normalizzato in out[cap] e ritorna 1;
 * altrimenti copia raw in out e ritorna 0. Stessa logica del C#: canale `final` -> testo dopo,
 * tagliato a <|return|>/<|end|>/<|call|>; altrimenti rimozione di TUTTI i marker <|...|>;
 * trim; se vuoto -> testo originale. */
static int harmony_normalize(const char* raw, char* out, size_t cap) {
    if (!raw) raw = "";
    if (!*raw || !harmony_has_marker(raw)) { snprintf(out, cap, "%s", raw); return 0; }
    const char* finalTag = "<|channel|>final<|message|>";
    const char* f = strstr(raw, finalTag);
    if (f) {
        const char* body = f + strlen(finalTag);
        size_t len = strlen(body);
        const char* cutEnds[] = { "<|return|>", "<|end|>", "<|call|>" };
        for (int i = 0; i < 3; i++) {
            const char* k = strstr(body, cutEnds[i]);
            if (k) { size_t kl = (size_t)(k - body); if (kl < len) len = kl; }
        }
        size_t s0 = 0, s1 = len;
        while (s0 < s1 && isspace((unsigned char)body[s0])) s0++;
        while (s1 > s0 && isspace((unsigned char)body[s1 - 1])) s1--;
        if (s1 > s0) { snprintf(out, cap, "%.*s", (int)(s1 - s0), body + s0); return 1; }
        snprintf(out, cap, "%s", raw);   /* final vuoto: non perdere la risposta */
        return 0;
    }
    /* Nessun canale final: rimuovi TUTTI i marker <|...|>. Per <|channel|> si rimuove anche il
     * NOME del canale fino al marker successivo (es. "<|channel|>analysis<|message|>"). */
    size_t w = 0;
    for (size_t i = 0; raw[i] && w + 1 < cap; ) {
        if (raw[i] == '<' && raw[i + 1] == '|') {
            const char* close = strstr(raw + i + 2, "|>");
            if (close) {
                size_t after = (size_t)(close - raw) + 2;
                if (strncmp(raw + i, "<|channel|>", 11) == 0) {
                    const char* nameStart = strstr(raw + after, "<|");
                    if (nameStart) {
                        const char* nameClose = strstr(nameStart + 2, "|>");
                        if (nameClose) { i = (size_t)(nameClose - raw) + 2; continue; }
                    }
                }
                i = after; continue;
            }
        }
        out[w++] = raw[i++];
    }
    out[w] = 0;
    size_t s0 = 0; while (s0 < w && isspace((unsigned char)out[s0])) s0++;
    size_t s1 = w; while (s1 > s0 && isspace((unsigned char)out[s1 - 1])) s1--;
    if (s1 > s0) { memmove(out, out + s0, s1 - s0); out[s1 - s0] = 0; return 1; }
    snprintf(out, cap, "%s", raw);
    return 0;
}

static void* try_load(const char* name) {
    void* h = dlopen(name, RTLD_NOW | RTLD_LOCAL);
    if (h) return h;
    const char* env = gmb_opt_env("GMB_MLX_LIB_DIR");
    char buf[1024];
    if (env && *env) { snprintf(buf, sizeof(buf), "%s/%s", env, name); h = dlopen(buf, RTLD_NOW | RTLD_LOCAL); if (h) return h; }
    const char* home = getenv("GMBASIC_HOME");
    if (home && *home) { snprintf(buf, sizeof(buf), "%s/Resources/libs/osx-arm64/%s", home, name); h = dlopen(buf, RTLD_NOW | RTLD_LOCAL); if (h) return h; }
    /* fallback: repo GMBasic accanto (da cwd o env assoluto) */
    snprintf(buf, sizeof(buf), "/Users/gaetano/Documents/Projects/GMBasic/Resources/libs/osx-arm64/%s", name);
    h = dlopen(buf, RTLD_NOW | RTLD_LOCAL);
    if (!h) { snprintf(buf, sizeof(buf), "/Users/gaetano/Documents/Projects/LLMEnv/LLMEngine/mlx/build-gmbasic/%s", name); h = dlopen(buf, RTLD_NOW | RTLD_LOCAL); }
    return h;
}

static void* sym(void* lib, const char* n) { return lib ? dlsym(lib, n) : NULL; }

int gmb_brain_mlx_start(const char* model_dir, char* err, size_t errcap) {
    if (err) err[0] = 0;
    if (!model_dir || !*model_dir) { if (err) snprintf(err, errcap, "model_dir mancante"); return -1; }
    mlx_brain_t* b = (mlx_brain_t*)calloc(1, sizeof(*b));
    if (!b) { if (err) snprintf(err, errcap, "memoria"); return -1; }
    snprintf(b->model_dir, sizeof(b->model_dir), "%s", model_dir);
    b->tok_lib = try_load("libllmtoken.dylib");
    b->mlx_lib = try_load("libgmb_ai_mlx.dylib");
    if (!b->tok_lib || !b->mlx_lib) {
        snprintf(b->err, sizeof(b->err), "dylib non trovata (tok=%p mlx=%p)", (void*)b->tok_lib, (void*)b->mlx_lib);
        if (err) snprintf(err, errcap, "%s", b->err);
        free(b); return -1;
    }
    b->tok_load = (tok_load_fn)sym(b->tok_lib, "llmtoken_load");
    b->tok_free = (tok_free_fn)sym(b->tok_lib, "llmtoken_free");
    b->tok_enc  = (tok_encode_fn)sym(b->tok_lib, "llmtoken_encode");
    b->tok_dec  = (tok_decode_fn)sym(b->tok_lib, "llmtoken_decode");
    b->tok_load_json = (tok_load_json_fn)sym(b->tok_lib, "llmtoken_load_json");
    b->tok_load_tiktoken = (tok_load_tiktoken_fn)sym(b->tok_lib, "llmtoken_load_tiktoken");
    b->gguf_tok_json = (gguf_tok_json_fn)sym(b->mlx_lib, "llm_ai_mlx_gguf_tokenizer_json");
    b->mlx_free_str  = (free_str_fn)sym(b->mlx_lib, "llm_ai_mlx_free_str");
    /* ABI comune LLMEnv: un solo simbolo (`llmengine_llm_engine_mlx`) -> vtable del motore MLX. */
    { engine_get_fn get = (engine_get_fn)sym(b->mlx_lib, "llmengine_llm_engine_mlx");
      if (get) b->eng = get(); }
    if (!b->tok_load || !b->tok_enc || !b->tok_dec || b->eng == NULL ||
        !b->eng->load_model || !b->eng->create_session || !b->eng->append_ids || !b->eng->generate_ids) {
        snprintf(b->err, sizeof(b->err), "simboli engine mancanti (dylib non allineata)");
        if (err) snprintf(err, errcap, "%s", b->err);
        if (b->tok_lib) dlclose(b->tok_lib); if (b->mlx_lib) dlclose(b->mlx_lib);
        free(b); return -1;
    }
    /* Modello MLX single-file GGUF (oggi DS4/DeepSeek V4 Flash): tokenizer dal GGUF. */
    { size_t mdl = strlen(model_dir);
      b->is_gguf = (mdl > 5 && strcmp(model_dir + mdl - 5, ".gguf") == 0); }
    if (b->is_gguf) {
        if (!b->gguf_tok_json || !b->tok_load_json || !b->mlx_free_str) {
            snprintf(b->err, sizeof(b->err), "dylib non allineata: tokenizer dal GGUF non disponibile");
            if (err) snprintf(err, errcap, "%s", b->err); goto fail;
        }
        size_t jl = 0;
        char* js = b->gguf_tok_json(model_dir, &jl);
        if (!js || jl == 0) {
            if (js) b->mlx_free_str(js);
            snprintf(b->err, sizeof(b->err), "tokenizer dal GGUF fallito: %s", model_dir);
            if (err) snprintf(err, errcap, "%s", b->err); goto fail;
        }
        b->tok = b->tok_load_json(js, jl);
        b->mlx_free_str(js);
    } else {
        char tj[1024]; snprintf(tj, sizeof(tj), "%s/tokenizer.json", model_dir);
        b->tok = b->tok_load(tj);
        if (!b->tok && b->tok_load_tiktoken) {
            /* Kimi (kimi_linear): NON ha tokenizer.json ma un vocabolario tiktoken. */
            char tm[1024], tc[1024];
            snprintf(tm, sizeof(tm), "%s/tiktoken.model", model_dir);
            snprintf(tc, sizeof(tc), "%s/tokenizer_config.json", model_dir);
            FILE* tf = fopen(tm, "rb");
            if (tf) { fclose(tf); b->tok = b->tok_load_tiktoken(tm, tc); }
        }
        if (!b->tok) { snprintf(b->err, sizeof(b->err), "tokenizer.json non caricato: %s", tj); if (err) snprintf(err, errcap, "%s", b->err); goto fail; }
    }
    if (!b->tok) { snprintf(b->err, sizeof(b->err), "tokenizer non caricato: %s", model_dir); if (err) snprintf(err, errcap, "%s", b->err); goto fail; }
    b->model = b->eng->load_model(model_dir);
    if (!b->model) { snprintf(b->err, sizeof(b->err), "modello non caricato: %s", model_dir); if (err) snprintf(err, errcap, "%s", b->err); goto fail; }
    b->sess = b->eng->create_session(b->model);
    if (!b->sess) { snprintf(b->err, sizeof(b->err), "sessione non creata"); if (err) snprintf(err, errcap, "%s", b->err); goto fail; }
    /* eos: per i modelli a CARTELLA MLX usare l'eos_token REALE di tokenizer_config.json
     * (es. Llama-3.2 = <|eot_id|>): l'euristica <|im_end|>/151645 NON è l'eos di Llama e il
     * modello non si ferma → degenerazione a maxtokens. Se assente/non codificabile si ricade
     * sull'euristica. Per .gguf/DS4 resta l'euristica attuale (nessuna modifica). Override
     * comune GMB_MLX_EOS. */
    { const char* eosTok = b->is_gguf ? "<｜end▁of▁sentence｜>" : "<|im_end|>";
      int32_t eid[8]; int en = 0;
      if (!b->is_gguf) {
          /* eos_token reale del modello (stringa o oggetto `content`) dal tokenizer_config.json */
          char* et = gmb_chat_template_read_token(model_dir, "eos_token");
          if (et && *et) { en = b->tok_enc(b->tok, et, eid, 8); }
          free(et);
      }
      if (en == 0) en = b->tok_enc(b->tok, eosTok, eid, 8);   /* fallback euristica */
      b->eos = (en == 1) ? eid[0] : (b->is_gguf ? 1 : 151645);
      const char* e = gmb_opt_env("GMB_MLX_EOS"); if (e && *e) b->eos = atoi(e); }
    if (g_brain) gmb_brain_mlx_stop();
    g_brain = b;
    return 0;
fail:
    if (b->sess) b->eng->destroy_session(b->sess);
    if (b->model) b->eng->free_model(b->model);
    if (b->tok) b->tok_free(b->tok);
    if (b->tok_lib) dlclose(b->tok_lib); if (b->mlx_lib) dlclose(b->mlx_lib);
    free(b); return -1;
}

int gmb_brain_mlx_complete(void* ctx, const char* transcript, char* out, size_t cap) {
    mlx_brain_t* b = (mlx_brain_t*)ctx;
    if (!b || !b->sess || !out || cap == 0) return -1;
    char prompt[16384];
    const char* sysmsg =
        "Sei un agente che compie azioni. Rispondi con una azione "
        "{\"action\":{\"tool\":\"...\",\"args\":{...}}} o con la risposta finale in testo libero.";
    const char* DS_BOS = "<｜begin▁of▁sentence｜>";
    /* transcript gia' formattato dal layer (DeepSeek BOS): usalo verbatim, come prima. */
    int already = (b->is_gguf && transcript && strncmp(transcript, DS_BOS, strlen(DS_BOS)) == 0);
    int pl = -1;
    /* PRIMARIO: chat_template reale del modello (parita' con ChatTranscripts.ResolveFromDir C#).
     * Assente/errore -> fallback alla formattazione scritta a mano (DeepSeek/ChatML). */
    if (!already) {
        char terr[256];
        const char* sys = g_system_set ? g_system_prompt : NULL;
        if (gmb_chat_template_build(b->model_dir, sys, transcript, prompt, sizeof prompt, terr, sizeof terr) == 0)
            pl = (int)strlen(prompt);
    }
    if (pl < 0) {
        /* Fallback scritto a mano: usa il SYSTEM PROMPT DELL'AGENTE (g_system_prompt, settato da
         * gmb_agent.c) quando presente — parità col C# (ChatTranscripts.Resolve → BuildChatMl con
         * `agent.ResolveSystemPrompt()`); altrimenti l'istruzione agente di default. Prima usava
         * SEMPRE il testo fisso → col fallback (es. qwen3_5 che rifiuta il template) i due mondi
         * divergevano (system diverso). */
        const char* sys_eff = (g_system_set && g_system_prompt[0]) ? g_system_prompt : sysmsg;
        if (already) {
            pl = snprintf(prompt, sizeof(prompt), "%s", transcript);
        } else if (b->is_gguf) {
            pl = snprintf(prompt, sizeof(prompt),
                "<｜begin▁of▁sentence｜><｜User｜>%s\n%s<｜Assistant｜>", sys_eff, transcript ? transcript : "");
        } else {
            pl = snprintf(prompt, sizeof(prompt),
                "<|im_start|>system\n%s<|im_end|>\n"
                "<|im_start|>user\n%s<|im_end|>\n<|im_start|>assistant\n", sys_eff, transcript ? transcript : "");
        }
    }
    if (pl <= 0 || pl >= (int)sizeof(prompt)) return -1;
    /* Diagnostica parità C#/C (branch-first): GMB_DUMP_PROMPT=1 stampa su stderr il prompt RESO
     * finale. Il lato C# fa lo stesso in MlxProviders.AppendMessage. */
    int32_t ids[MAX_IDS];
    int n = b->tok_enc(b->tok, prompt, ids, MAX_IDS);
    if (n <= 0) return -1;
    int32_t out_ids[MAX_NEW];
    /* Limite = maxtokens dell'agente (se impostato), altrimenti MAX_NEW. */
    int gen_max = (g_max_new > 0 && g_max_new < MAX_NEW) ? g_max_new : MAX_NEW;
    int got = b->eng->generate_ids(b->sess, gen_max, b->eos, out_ids);
    if (got <= 0) { out[0] = 0; return 0; }
    char* raw = (char*)malloc(cap + 1);
    if (!raw) { out[0] = 0; return 0; }
    int need = b->tok_dec(b->tok, out_ids, got, raw, (int)cap);
    if (need < 0) { free(raw); out[0] = 0; return 0; }
    raw[cap] = 0;
    /* Harmony (gpt-oss): normalizza l'output grezzo (canale `final` / rimozione marker <|...|>)
     * PRIMA del cleanup marker qui sotto. harmony_normalize copia raw in `norm` se non tocca il
     * testo, quindi `src` è sempre l'input giusto. */
    char* norm = (char*)malloc(cap + 1);
    const char* src = raw;
    if (norm) { harmony_normalize(raw, norm, cap + 1); src = norm; }
    /* pulizia marker chat e label di turno (rumore di template) */
    size_t rp = 0, wp = 0;
    int last_nl = 0;
    while (src[rp]) {
        if (strncmp(src + rp, "<|im_start|>", 12) == 0) { rp += 12; continue; }
        if (strncmp(src + rp, "<|im_end|>", 10) == 0)   { rp += 10; continue; }
        if (strncmp(src + rp, "</s>", 4) == 0)          { rp += 4; continue; }
        /* marker DeepSeek V4 (DS4): turni e delimitatori di sequenza */
        if (strncmp(src + rp, "<｜Assistant｜>", strlen("<｜Assistant｜>")) == 0) { rp += strlen("<｜Assistant｜>"); continue; }
        if (strncmp(src + rp, "<｜User｜>", strlen("<｜User｜>")) == 0) { rp += strlen("<｜User｜>"); continue; }
        if (strncmp(src + rp, "<｜end▁of▁sentence｜>", strlen("<｜end▁of▁sentence｜>")) == 0) { rp += strlen("<｜end▁of▁sentence｜>"); continue; }
        if (strncmp(src + rp, "<｜begin▁of▁sentence｜>", strlen("<｜begin▁of▁sentence｜>")) == 0) { rp += strlen("<｜begin▁of▁sentence｜>"); continue; }
        /* riga che è solo 'user' o 'assistant' (label di turno) → scarta */
        {
            const char* nl = strchr(src + rp, '\n');
            size_t lin = nl ? (size_t)(nl - (src + rp)) : strlen(src + rp);
            size_t ts = rp; while (ts < rp + lin && (src[ts] == ' ' || src[ts] == '\t' || src[ts] == '\r')) ts++;
            size_t te = rp + lin; while (te > ts && (src[te-1] == ' ' || src[te-1] == '\t' || src[te-1] == '\r')) te--;
            size_t tl = te - ts;
            if ((tl == 4 && strncasecmp(src + ts, "user", 4) == 0) ||
                (tl == 9 && strncasecmp(src + ts, "assistant", 9) == 0)) {
                rp += lin; if (nl) rp += 1; continue;
            }
        }
        if (src[rp] == '\n' && last_nl >= 2) { rp++; continue; } /* compatta blank */
        if (src[rp] == '\n') last_nl++; else last_nl = 0;
        out[wp++] = src[rp++];
    }
    while (wp > 0 && (out[wp-1] == '\n' || out[wp-1] == ' ' || out[wp-1] == '\t')) wp--;
    out[wp] = 0;
    free(norm);
    free(raw);
    return 0;
}

void gmb_brain_mlx_stop(void) {
    if (!g_brain) return;
    mlx_brain_t* b = g_brain; g_brain = NULL;
    if (b->sess) b->eng->destroy_session(b->sess);
    if (b->model) b->eng->free_model(b->model);
    if (b->tok) b->tok_free(b->tok);
    if (b->tok_lib) dlclose(b->tok_lib);
    if (b->mlx_lib) dlclose(b->mlx_lib);
    free(b);
}

/* Auto-wire in gmb_agent_run_goal: GMB_MLX_MODEL_DIR -> brain MLX (prima di GMB_BRAIN_URL). */
int gmb_brain_mlx_auto(const char* model_dir, char* log, size_t logcap) {
    char err[512] = "";
    if (gmb_brain_mlx_start(model_dir, err, sizeof(err)) != 0) {
        if (log) snprintf(log, logcap, "brain MLX non avviato: %s", err);
        return -1;
    }
    gmb_agent_brain_register(gmb_brain_mlx_complete, g_brain);
    if (log) snprintf(log, logcap, "brain MLX attivo (%s)", model_dir);
    return 0;
}
