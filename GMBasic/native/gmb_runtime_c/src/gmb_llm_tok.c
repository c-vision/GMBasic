/* gmb_llm_tok.c — dominio LLM/TOKENIZER del runtime condiviso (VM/C + C99Native).
 *
 * Perche' esiste: i verbi del tokenizer sono raggiungibili da BASIC in due forme:
 *   - COMANDO  `LLM TENSOR TOKENIZE …` / `LLM TOKENS …`        → STAMPA (legacy, invariato)
 *   - FUNZIONE `LLM.TOKENS(m, t)` / `LLM.DETOKENIZE(m, ids)`    → RESTITUISCE il valore
 *     e comando con `INTO` (`LLM TOKENS m, t INTO x$`)          → ASSEGNA, senza stampa
 *
 * Queste tre funzioni sono la SORGENTE UNICA del ritorno come DATO, condivisa da VM/C e
 * C99Native (AGENTS §0bis: mai duplicare nei backend). Delegando a `llmmodelio_*` (gmbllm)
 * non c'e' una seconda implementazione del tokenizer.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gmb_llm_tok.h"
#include "llmmodelio_model.h"
#include "gmb_runtime.h"   /* gmb_raise_error: errore BASIC catturabile (ON ERROR) */

/* Buffer di lavoro: la dylib/il runtime restano single-thread per task GPM, ma per sicurezza
 * si alloca per chiamata (le stringhe tornate sono poi copiate nel valore BASIC). */
static char* dup_capture(const char* src)
{
    if (src == NULL) return NULL;
    size_t n = strlen(src);
    char* out = (char*)malloc(n + 1);
    if (out == NULL) return NULL;
    memcpy(out, src, n + 1);
    return out;
}

char* gmb_llm_tokens(const char* model, const char* text)
{
    size_t cap = 1u << 16;
    for (int attempt = 0; attempt < 8; attempt++) {
        char* buf = (char*)malloc(cap);
        if (buf == NULL) return NULL;
        int rc = llmmodelio_tokenize_ids_to(model ? model : "", text ? text : "", buf, cap);
        if (rc == -2) { free(buf); cap <<= 1; continue; }   /* troncato: buffer piu' grande */
        if (rc < 0) { free(buf); return NULL; }
        size_t n = strlen(buf);
        if (n + 1 < cap) return buf;   /* non troncato */
        free(buf);
        cap <<= 1;
    }
    return NULL;
}

char* gmb_llm_detokenize(const char* model, const char* ids_csv)
{
    char buf[65536];
    if (llmmodelio_detokenize_to(model ? model : "", ids_csv ? ids_csv : "", buf, sizeof buf) < 0)
        return NULL;
    return dup_capture(buf);
}

/* Output di QUALUNQUE sotto-verbo `LLM TENSOR <sub>` catturato invece che stampato. */
char* gmb_llm_tensor_out(const char* sub, const char* a, const char* b, const char* c)
{
    /* PATH del modello: il primo argomento di `LLM TENSOR LOGITS|TOPK|QUALITY|USAGEEVAL <dir>`
     * passa dal risolutore UNICO del runtime (`path.models_root`, `~`, `%MODELS%`). Si applica
     * SEMPRE e non a una lista di sotto-verbi: le uniche forme che cambiano sono `%MODELS%…` e
     * `~…`, quindi un nome di tensore o un parametro attraversa la funzione INVARIATO. Serve qui
     * (e non solo nel codice emesso da C99Native) perche' la gamba VM/C entra da questa funzione:
     * prima riceveva il token letterale.
     */
    char pathbuf[4096];
    a = gmb_paths_model_path(a, pathbuf, sizeof pathbuf);
    size_t cap = 1u << 16;
    for (int attempt = 0; attempt < 8; attempt++) {
        char* buf = (char*)malloc(cap);
        if (buf == NULL) return NULL;
        int rc = llmmodelio_tensor_cmd_to(sub ? sub : "", a ? a : "", b ? b : "", c ? c : "", buf, cap);
        if (rc != 0) { free(buf); return NULL; }
        size_t n = strlen(buf);
        if (n + 1 < cap) return buf;
        free(buf);
        cap <<= 1;
    }
    return NULL;
}

/* ── Famiglie LLM con output catturabile (2026-10-30, seconda ondata) ─────────────────────
 * Stesso problema dei verbi TENSOR: il comando stampa, la funzione deve restituire. Un solo
 * dispatcher per TUTTE le famiglie: la famiglia sceglie il `*_cmd_to` di gmbllm, il buffer e'
 * adattivo, nessuna duplicazione nei backend. */
char* gmb_llm_cmd_out(int family, const char* sub, const char* a, const char* b, const char* c)
{
    /* Stessa risoluzione della variante `_out` (sopra), per la famiglia TENSOR: il primo
     * argomento e' una dir di modello per LOGITS/TOPK/QUALITY/USAGEEVAL e per gli altri
     * sotto-verbi il risolutore restituisce l'input invariato. */
    char pathbuf[4096];
    if (family == GMB_LLM_FAM_TENSOR) a = gmb_paths_model_path(a, pathbuf, sizeof pathbuf);
    size_t cap = 1u << 16;
    for (int attempt = 0; attempt < 8; attempt++) {
        char* buf = (char*)malloc(cap);
        if (buf == NULL) return NULL;
        int rc;
        switch (family) {
            case GMB_LLM_FAM_TENSOR: rc = llmmodelio_tensor_cmd_to(sub ? sub : "", a ? a : "", b ? b : "", c ? c : "", buf, cap); break;
            case GMB_LLM_FAM_POLICY: rc = llmmodelio_policy_cmd_to(sub ? sub : "", a ? a : "", b ? b : "", c ? c : "", buf, cap); break;
            case GMB_LLM_FAM_ARCH:   rc = llmmodelio_arch_cmd_to  (sub ? sub : "", a ? a : "", b ? b : "", buf, cap); break;
            case GMB_LLM_FAM_MODULE: rc = llmmodelio_module_cmd_to(sub ? sub : "", a ? a : "", b ? b : "", buf, cap); break;
            case GMB_LLM_FAM_KERNEL: rc = llmmodelio_kernel_cmd_to(sub ? sub : "", a ? a : "", b ? b : "", buf, cap); break;
            case GMB_LLM_FAM_KV:     rc = llmmodelio_kv_cmd_to    (sub ? sub : "", a ? a : "", b ? b : "", buf, cap); break;
            default:                 free(buf); return NULL;
        }
        if (rc != 0) { free(buf); return NULL; }
        size_t n = strlen(buf);
        if (n + 1 < cap) return buf;   /* non troncato */
        free(buf);
        cap <<= 1;
    }
    return NULL;
}

/* Il valore delle funzioni LLM non e' mai un silenzio: se il verbo fallisce si alza un errore
 * BASIC catturabile (ON ERROR), come fanno le funzioni gestite con l'eccezione. */
char* gmb_llm_out_or_raise(char* r, const char* what, long line)
{
    if (r == NULL) gmb_raise_error(0, what ? what : "LLM: verbo fallito", line);
    return r;
}
