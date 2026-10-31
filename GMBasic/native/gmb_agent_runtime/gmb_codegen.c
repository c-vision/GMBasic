/* gmb_codegen.c — CodeGen loop in C (Fase 2 FaseC-CodegenLoop): PROMPT→brain→.bas→exec→fix.
 * Indipendente dal mondo C# (regola separazione compilatori). Usa il brain one-shot
 * (gmb_brain_ask_sync) e gmb_code_exec (esegue il .bas col runner figlio executor nativo C,
 * che compila: un errore di compilazione appare come '?compilazione C: ...' nell'output).
 * Mai no-op: ogni stato è esplicito (ok | compile-fail | fix-esauriti). */
#include "gmb_codegen.h"
#include "gmb_agent_loop.h"
#include "gmb_code_exec.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Estrae il codice da una risposta markdown ```basic/``` (o ``` senza linguaggio); se non
 * trova fence usa l'intero testo. Scrive in out (cap). */
static void codegen_extract(const char* raw, char* out, size_t cap) {
    out[0] = 0;
    if (!raw || cap == 0) return;
    const char* f = strstr(raw, "```");
    if (!f) { snprintf(out, cap, "%s", raw); return; }
    const char* s = f + 3;
    /* salta eventuale linguaggio su quella riga */
    const char* nl = strchr(s, '\n');
    if (nl) s = nl + 1; else s = f + 3;
    const char* e = strstr(s, "```");
    size_t n = (e && e > s) ? (size_t)(e - s) : strlen(s);
    if (n >= cap) n = cap - 1;
    memcpy(out, s, n); out[n] = 0;
}

/* true se l'output dell'exec suggerisce un errore di COMPILAZIONE (-> fix col LLM). */
static int codegen_compile_failed(const char* out) {
    if (!out) return 0;
    return strstr(out, "?compilazione") || strstr(out, "parse error")
        || strstr(out, "compilazione C") || strstr(out, "errore di compilazione");
}

int gmb_codegen_run(const char* prompt, const char* out_bas, int max_fix, gmb_codegen_result_t* r) {
    if (!r) return -1;
    memset(r, 0, sizeof(*r));
    if (!prompt || !*prompt) { snprintf(r->diag, sizeof(r->diag), "Prompt vuoto"); return -1; }
    if (max_fix <= 0) max_fix = 3;

    char diag[2048] = "";
    for (int attempt = 0; attempt <= max_fix; attempt++) {
        char ask[8192];
        if (attempt == 0)
            snprintf(ask, sizeof(ask), "%s\nRispondi SOLO con il programma BASIC completo "
                     "(puoi usare ```basic ... ```).", prompt);
        else
            snprintf(ask, sizeof(ask),
                "Il codice BASIC generato ha dato errore di compilazione:\n%s\n\n"
                "Richiesta originale: %s\n\nCorreggi e rispondi SOLO col programma BASIC corretto.",
                diag, prompt);

        char raw[16384];
        if (gmb_brain_ask_sync(ask, raw, sizeof(raw)) != 0) {
            snprintf(r->diag, sizeof(r->diag), "LLM: %.1200s", raw[0] ? raw : "brain non disponibile");
            r->ok = 0;
            return -1;
        }
        char code[16384];
        codegen_extract(raw, code, sizeof(code));
        if (!code[0]) { snprintf(r->diag, sizeof(r->diag), "LLM non ha prodotto codice"); r->ok = 0; return -1; }
        snprintf(r->code, sizeof(r->code), "%s", code);

        char runout[16384];
        if (out_bas && *out_bas) {
            FILE* w = fopen(out_bas, "wb");
            if (w) { fwrite(code, 1, strlen(code), w); fclose(w); }
        }
        /* esegui (l'executor compila+run): compile-fail appare nell'output */
        int rce = gmb_code_exec(code, runout, sizeof(runout));
        if (codegen_compile_failed(runout)) {
            snprintf(diag, sizeof(diag), "%.2000s", runout[0] ? runout : "errore di compilazione");
            continue; /* fix */
        }
        /* ok (compilato ed eseguito; il run può avere output logico) */
        snprintf(r->output, sizeof(r->output), "%s", runout[0] ? runout : "");
        r->ok = 1;
        (void)rce;
        return 0;
    }
    snprintf(r->diag, sizeof(r->diag),
        "Fix esauriti dopo %d tentativi. Ultimo diagnostics: %.2000s", max_fix, diag[0] ? diag : "?");
    r->ok = 0;
    return -1;
}

/* Wrapper BASIC-value: ritorna l'output del run o "Error: <diag>". gmb_string_t heap. */
#include "gmb_runtime.h"
/* Parità C# CodeGenLoop: senza completer reale (nessun provider o solo fixture) l'errore
 * è 'Nessun provider LLM configurato', non il diag del brain ( MiniTest gen_AI_AgentCode/Fix). */
static int cg_has_provider(void) {
    const char* prov = gmb_provider_active("completion");
    if (!prov || !*prov) return 0;
    const char* k = gmb_provider_kind(prov);
    if (k && strcmp(k, "fixture") == 0) return 0;
    return 1;
}
gmb_string_t gmb_agent_code(const char* task, const char* outpath) {
    if (!task || !*task) return gmb_string_alloc("AGENTCODE: task mancante");
    if (!cg_has_provider()) return gmb_string_alloc("Error: Nessun provider LLM configurato (AI SET ACTIVE)");
    gmb_codegen_result_t r;
    int rc = gmb_codegen_run(task, (outpath && *outpath) ? outpath : NULL, 3, &r);
    char* tmp = NULL;
    if (rc == 0 && r.ok) asprintf(&tmp, "%s", r.output);
    else asprintf(&tmp, "Error: %s", r.diag[0] ? r.diag : "codegen fallita");
    gmb_string_t s = gmb_string_alloc(tmp ? tmp : "");
    if (tmp) free(tmp);
    return s;
}
gmb_string_t gmb_agent_fix(const char* err, const char* outpath) {
    if (!err || !*err) return gmb_string_alloc("AGENTFIX: errore mancante");
    if (!cg_has_provider()) return gmb_string_alloc("Error: Nessun provider LLM configurato (AI SET ACTIVE)");
    char prompt[4096];
    snprintf(prompt, sizeof(prompt), "Fix error: %s", err);
    gmb_codegen_result_t r;
    int rc = gmb_codegen_run(prompt, (outpath && *outpath) ? outpath : NULL, 3, &r);
    char* tmp = NULL;
    if (rc == 0 && r.ok) asprintf(&tmp, "%s", r.output);
    else asprintf(&tmp, "Error: %s", r.diag[0] ? r.diag : "codegen fallita");
    gmb_string_t s = gmb_string_alloc(tmp ? tmp : "");
    if (tmp) free(tmp);
    return s;
}

