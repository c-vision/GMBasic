/* gmb_chat_template.c — risoluzione automatica del chat_template (mondo C), parita' con
 * common/AI/Providers/ChatTranscripts.ResolveFromDir. Vedi gmb_chat_template.h.
 * JSON minimale in C (estrazione dei soli campi necessari) + incapsulamento del motore
 * native/llmjinja. Nessuna dipendenza da C#. Niente stub: assenza/errore -> -1 (fallback). */
#include "gmb_chat_template.h"
#include "llmjinja.h"
#include "llm_gguf.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ================= sb minimale ================= */
typedef struct { char* p; size_t n, cap; } ctsb;
static void ctsb_init(ctsb* b) { b->cap = 256; b->n = 0; b->p = (char*)malloc(b->cap); b->p[0] = 0; }
static void ctsb_grow(ctsb* b, size_t need) { while (b->n + need + 1 > b->cap) { b->cap *= 2; b->p = (char*)realloc(b->p, b->cap); } }
static void ctsb_putc(ctsb* b, char c) { ctsb_grow(b, 1); b->p[b->n++] = c; b->p[b->n] = 0; }
static void ctsb_put(ctsb* b, const char* s) { size_t l = strlen(s); ctsb_grow(b, l); memcpy(b->p + b->n, s, l); b->n += l; b->p[b->n] = 0; }
static void ctsb_free(ctsb* b) { free(b->p); b->p = NULL; b->n = b->cap = 0; }

/* ================= file ================= */
static char* read_file(const char* path, size_t* outlen) {
    FILE* f = fopen(path, "rb");
    if (!f) return NULL;
    char* buf = NULL; size_t cap = 0, len = 0;
    int ch;
    while ((ch = fgetc(f)) != EOF) {
        if (len + 1 >= cap) { cap = cap ? cap * 2 : 65536; buf = (char*)realloc(buf, cap); }
        buf[len++] = (char)ch;
    }
    fclose(f);
    if (!buf) { buf = (char*)malloc(1); len = 0; }
    buf[len] = 0;
    if (outlen) *outlen = len;
    return buf;
}
static int has_suffix_ci(const char* s, const char* suf) {
    size_t ls = strlen(s), lf = strlen(suf);
    if (ls < lf) return 0;
    for (size_t i = 0; i < lf; i++) if (tolower((unsigned char)s[ls - lf + i]) != tolower((unsigned char)suf[i])) return 0;
    return 1;
}

/* ================= JSON minimale ================= */
static const char* jskip_ws(const char* p) { while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++; return p; }
static const char* jskip_string(const char* p) {   /* p all'apertura '"'; ritorna dopo la chiusura */
    if (*p != '"') return p;
    p++;
    while (*p) {
        if (*p == '\\') { p += 2; continue; }
        if (*p == '"') return p + 1;
        p++;
    }
    return p;
}
static const char* jskip_value(const char* p) {
    p = jskip_ws(p);
    if (*p == '"') return jskip_string(p);
    if (*p == '{') {
        p++; p = jskip_ws(p);
        if (*p == '}') return p + 1;
        for (;;) {
            p = jskip_ws(p);
            p = jskip_string(p);
            p = jskip_ws(p);
            if (*p != ':') return p;
            p++; p = jskip_value(p);
            p = jskip_ws(p);
            if (*p == ',') { p++; continue; }
            if (*p == '}') return p + 1;
            return p;
        }
    }
    if (*p == '[') {
        p++; p = jskip_ws(p);
        if (*p == ']') return p + 1;
        for (;;) {
            p = jskip_value(p);
            p = jskip_ws(p);
            if (*p == ',') { p++; continue; }
            if (*p == ']') return p + 1;
            return p;
        }
    }
    while (*p && *p != ',' && *p != '}' && *p != ']' && !isspace((unsigned char)*p)) p++;
    return p;
}
/* valore (puntatore all'inizio) del campo `key` nell'oggetto `obj` (top-level), o NULL. */
static const char* jfind(const char* obj, const char* key) {
    obj = jskip_ws(obj);
    if (*obj != '{') return NULL;
    obj++;
    size_t kl = strlen(key);
    for (;;) {
        obj = jskip_ws(obj);
        if (*obj != '"') return NULL;
        const char* ks = obj + 1;
        const char* after = jskip_string(obj);
        const char* ke = after - 1;   /* punta alla '"' di chiusura */
        int match = ((size_t)(ke - ks) == kl && strncmp(ks, key, kl) == 0);
        obj = jskip_ws(after);
        if (*obj != ':') return NULL;
        obj++;
        const char* val = obj;
        obj = jskip_value(obj);
        if (match) return val;
        obj = jskip_ws(obj);
        if (*obj == ',') { obj++; continue; }
        return NULL;
    }
}
static int hexval(char c) { if (c >= '0' && c <= '9') return c - '0'; if (c >= 'a' && c <= 'f') return c - 'a' + 10; if (c >= 'A' && c <= 'F') return c - 'A' + 10; return -1; }
/* decodifica una stringa JSON (p alla '"') in malloc'd; NULL se non e' una stringa. */
static char* jstr(const char* p) {
    p = jskip_ws(p);
    if (*p != '"') return NULL;
    p++;
    ctsb b; ctsb_init(&b);
    while (*p) {
        if (*p == '"') { p++; break; }
        if (*p == '\\') {
            p++;
            switch (*p) {
                case 'n': ctsb_putc(&b, '\n'); p++; break;
                case 't': ctsb_putc(&b, '\t'); p++; break;
                case 'r': ctsb_putc(&b, '\r'); p++; break;
                case 'b': ctsb_putc(&b, '\b'); p++; break;
                case 'f': ctsb_putc(&b, '\f'); p++; break;
                case '/': ctsb_putc(&b, '/'); p++; break;
                case '\\': ctsb_putc(&b, '\\'); p++; break;
                case '"': ctsb_putc(&b, '"'); p++; break;
                case 'u': {
                    p++;
                    int h[4];
                    for (int i = 0; i < 4 && p[i]; i++) h[i] = hexval(p[i]);
                    unsigned cp = ((unsigned)h[0] << 12) | ((unsigned)h[1] << 8) | ((unsigned)h[2] << 4) | (unsigned)h[3];
                    p += 4;
                    if (cp < 0x80) ctsb_putc(&b, (char)cp);
                    else if (cp < 0x800) { ctsb_putc(&b, (char)(0xC0 | (cp >> 6))); ctsb_putc(&b, (char)(0x80 | (cp & 0x3F))); }
                    else { ctsb_putc(&b, (char)(0xE0 | (cp >> 12))); ctsb_putc(&b, (char)(0x80 | ((cp >> 6) & 0x3F))); ctsb_putc(&b, (char)(0x80 | (cp & 0x3F))); }
                    break;
                }
                default: ctsb_putc(&b, *p); p++; break;
            }
            continue;
        }
        ctsb_putc(&b, *p); p++;
    }
    return b.p;
}
/* chat_template: stringa | lista (stringa | {template:string}); ritorna malloc'd o NULL. */
static char* jget_chat_template(const char* json) {
    if (!json) return NULL;
    const char* v = jfind(json, "chat_template");
    if (!v) return NULL;
    v = jskip_ws(v);
    if (*v == '"') { char* s = jstr(v); return (s && *s) ? s : (free(s), NULL); }
    if (*v == '[') {
        v++;
        for (;;) {
            v = jskip_ws(v);
            if (*v == ']' || !*v) break;
            if (*v == '"') { char* s = jstr(v); if (s && *s) return s; free(s); }
            else if (*v == '{') {
                const char* t = jfind(v, "template");
                if (t) { char* s = jstr(t); if (s && *s) return s; free(s); }
            }
            v = jskip_value(v);
            v = jskip_ws(v);
            if (*v == ',') { v++; continue; }
            break;
        }
    }
    return NULL;
}
/* token stringa | {content:string}; ritorna malloc'd o NULL. */
static char* jget_token(const char* json, const char* key) {
    if (!json) return NULL;
    const char* v = jfind(json, key);
    if (!v) return NULL;
    v = jskip_ws(v);
    if (*v == '"') { char* s = jstr(v); return (s && *s) ? s : (free(s), NULL); }
    if (*v == '{') { const char* c = jfind(v, "content"); if (c) { char* s = jstr(c); return (s && *s) ? s : (free(s), NULL); } }
    return NULL;
}

/* Accessor pubblico: stringa `key` (es. "eos_token"/"bos_token") da tokenizer_config.json e poi
 * config.json della cartella MLX; valore stringa o oggetto con campo `content`. Ritorna malloc'd
 * (il chiamante fa free) o NULL se assente. Usato dal brain per lo stop-token reale dei modelli
 * embedded (col default sbagliato il modello non si ferma e degenera). */
char* gmb_chat_template_read_token(const char* model_dir, const char* key) {
    if (!model_dir || !*model_dir || !key || !*key) return NULL;
    char path[1400];
    snprintf(path, sizeof path, "%s/tokenizer_config.json", model_dir);
    char* json = read_file(path, NULL);
    if (json) { char* s = jget_token(json, key); free(json); if (s) return s; }
    snprintf(path, sizeof path, "%s/config.json", model_dir);
    json = read_file(path, NULL);
    if (json) { char* s = jget_token(json, key); free(json); if (s) return s; }
    return NULL;
}

/* ================= gguf ================= */
static char* gguf_strdup_val(const LlmGgufVal* v) {
    if (!v || v->type != GGSTR || !v->s || v->slen == 0) return NULL;
    char* s = (char*)malloc(v->slen + 1);
    memcpy(s, v->s, v->slen); s[v->slen] = 0;
    return s;
}
static char* gguf_chat_template(const char* path, char** bos, char** eos) {
    LlmEngineMoeGguf* g = (LlmEngineMoeGguf*)malloc(sizeof(LlmEngineMoeGguf));
    if (!g) return NULL;
    void* map = NULL; size_t maplen = 0;
    if (!llm_gguf_open_file(path, g, &map, &maplen)) { free(g); return NULL; }
    char* tpl = gguf_strdup_val(llm_gguf_find_val(g, "tokenizer.chat_template"));
    if (bos) { char* b = gguf_strdup_val(llm_gguf_find_val(g, "tokenizer.ggml.bos_token")); if (b) *bos = b; }
    if (eos) { char* e = gguf_strdup_val(llm_gguf_find_val(g, "tokenizer.ggml.eos_token")); if (e) *eos = e; }
    llm_gguf_close_file(g, map, maplen);
    free(g);
    return tpl;
}

/* ================= build ================= */
int gmb_chat_template_build(const char* model_dir, const char* system, const char* transcript,
                            char* out, size_t cap, char* err, size_t errcap) {
    if (err && errcap) err[0] = 0;
    if (!model_dir || !*model_dir || !out || cap == 0) {
        if (err) snprintf(err, errcap, "argomenti mancanti");
        return -1;
    }
    char* tpl = NULL; char* bos = NULL; char* eos = NULL;

    if (has_suffix_ci(model_dir, ".gguf")) {
        tpl = gguf_chat_template(model_dir, &bos, &eos);
    } else {
        char path[1400];
        snprintf(path, sizeof path, "%s/tokenizer_config.json", model_dir);
        char* json = read_file(path, NULL);
        if (json) {
            tpl = jget_chat_template(json);
            bos = jget_token(json, "bos_token");
            eos = jget_token(json, "eos_token");
            free(json);
        }
        if (!tpl) {
            snprintf(path, sizeof path, "%s/chat_template.jinja", model_dir);
            tpl = read_file(path, NULL);
        }
        if (!bos || !eos) {
            snprintf(path, sizeof path, "%s/config.json", model_dir);
            char* cj = read_file(path, NULL);
            if (cj) {
                if (!bos) bos = jget_token(cj, "bos_token");
                if (!eos) eos = jget_token(cj, "eos_token");
                free(cj);
            }
        }
    }

    if (!tpl || !*tpl) {
        free(tpl); free(bos); free(eos);
        if (err) snprintf(err, errcap, "nessun chat_template in %s", model_dir);
        return -1;
    }

    llmjinja_val* vars = llmjinja_obj();
    llmjinja_val* msgs = llmjinja_arr();
    /* Parità MlxProviders.Complete: system (solo se non vuoto) + user, esattamente come
     * `AppendMessage(session, "system", ...)` seguito da `AppendMessage(session, "user", ...)`. */
    if (system && *system) {
        llmjinja_val* sm = llmjinja_obj();
        llmjinja_obj_set(sm, "role", llmjinja_str("system"));
        llmjinja_obj_set(sm, "content", llmjinja_str(system));
        llmjinja_arr_push(msgs, sm);
    }
    llmjinja_val* m = llmjinja_obj();
    llmjinja_obj_set(m, "role", llmjinja_str("user"));
    llmjinja_obj_set(m, "content", llmjinja_str(transcript ? transcript : ""));
    llmjinja_arr_push(msgs, m);
    llmjinja_obj_set(vars, "messages", msgs);
    llmjinja_obj_set(vars, "add_generation_prompt", llmjinja_bool(1));
    llmjinja_obj_set(vars, "bos_token", llmjinja_str(bos ? bos : ""));
    llmjinja_obj_set(vars, "eos_token", llmjinja_str(eos ? eos : ""));

    /* Parità con ChatTranscripts.ResolveFromDir (C#): il path a sessione rende ANCHE il solo
     * system (primo append). Se il template solleva su quel render (es. qwen3_5 "No user query
     * found"), si RIFIUTA il template automatico -> il chiamante ricade su ChatML/DeepSeek, così
     * i due mondi usano lo STESSO formato (mai mixare). */
    {
        llmjinja_val* v2 = llmjinja_obj();
        llmjinja_val* m2 = llmjinja_arr();
        if (system && *system) {
            llmjinja_val* sm2 = llmjinja_obj();
            llmjinja_obj_set(sm2, "role", llmjinja_str("system"));
            llmjinja_obj_set(sm2, "content", llmjinja_str(system));
            llmjinja_arr_push(m2, sm2);
        }
        llmjinja_obj_set(v2, "messages", m2);
        llmjinja_obj_set(v2, "add_generation_prompt", llmjinja_bool(0));
        llmjinja_obj_set(v2, "bos_token", llmjinja_str(bos ? bos : ""));
        llmjinja_obj_set(v2, "eos_token", llmjinja_str(eos ? eos : ""));
        char e2[256] = "";
        char* r2 = llmjinja_render(tpl, v2, e2, sizeof e2);
        llmjinja_val_free(v2);
        if (!r2) {
            llmjinja_val_free(vars);
            free(tpl); free(bos); free(eos);
            if (err) snprintf(err, errcap, "jinja (system-only): %s", e2[0] ? e2 : "render fallito");
            return -1;
        }
        free(r2);
    }

    char jerr[256] = "";
    char* rendered = llmjinja_render(tpl, vars, jerr, sizeof jerr);
    llmjinja_val_free(vars);
    free(tpl); free(bos); free(eos);

    if (!rendered) {
        if (err) snprintf(err, errcap, "jinja: %s", jerr[0] ? jerr : "render fallito");
        return -1;
    }
    size_t n = strlen(rendered);
    if (n + 1 > cap) {
        free(rendered);
        if (err) snprintf(err, errcap, "prompt troppo lungo (%zu > %zu)", n, cap);
        return -1;
    }
    memcpy(out, rendered, n + 1);
    free(rendered);
    return 0;
}
