/* llmmodelio_derive.c — B5: pipeline di derivazione RIPRODUCIBILE (artefatto `.gmbderive`).
 *
 * Un `.gmbderive` è la **sequenza di pass** (come la pass-pipeline di MLIR) che produce una
 * versione da una base: `BASE → PASS… → OUT`. È il "compilatore riproducibile": la stessa ricetta
 * (su base+policy identiche) riproduce la stessa versione, ed è auditabile/provenance.
 *
 * Formato (testo, un'istruzione per riga; `#` = commento):
 *   BASE   <path>                      modello di partenza (GGUF/MLX)
 *   POLICY <KEY> <VALUE>               imposta una chiave di policy (es. ROUTED_EXPERT iq2_xxs)
 *   PRUNE  <keep>                      LLM PRUNE  (prune esperti, criterio da POLICY PRUNE_EXPERT)
 *   QUANT  [<bpw>]                     LLM QUANT  (policy per-ruolo, o auto-bpw se <bpw>)
 *   ABLATE <pattern>                   LLM MODEL ABLATE
 *   ADAPTER <safetensors> [<alpha>]    LLM MODEL ADAPTER (LoRA)
 *   OUT    <path>                      output finale
 *
 * Superficie: `LLM DERIVE <recipe>[, <out>]` (alias → LLM TENSOR DERIVE). Ritorna 0/-1.
 * Ogni pass è il runtime condiviso esistente (`llmmodelio_model_*`) — nessuna logica duplicata. */
#include "llmmodelio_model.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>

enum { OP_PRUNE, OP_QUANT, OP_QUANTAUTO, OP_ABLATE, OP_ADAPTER };
typedef struct { int kind; char a[1024]; char b[64]; } DeriveOp;

static int derive_apply(int kind, const char* in, const char* dst, const char* a, const char* b)
{
    switch (kind) {
        case OP_PRUNE:     return llmmodelio_model_prune(in, dst, a && *a ? (int64_t)atoll(a) : 0);
        case OP_QUANT:     return llmmodelio_model_quantize(in, dst);
        case OP_QUANTAUTO: return llmmodelio_model_quantize_auto(in, dst, a && *a ? atof(a) : 4.0);
        case OP_ABLATE:    return llmmodelio_model_ablate(in, dst, a ? a : "");
        case OP_ADAPTER:   return llmmodelio_model_adapter(in, a ? a : "", dst, (b && *b) ? b : "1");
    }
    return -1;
}

int llmmodelio_derive(const char* recipe, const char* out_override, FILE* out, int* handled)
{
    if (handled) *handled = 0;
    if (recipe == NULL || !*recipe) return -1;
    FILE* fo = out ? out : stdout;
    if (handled) *handled = 1;

    FILE* rf = fopen(recipe, "r");
    if (rf == NULL) return -1;

    char base[1024] = "", outp[1024] = "";
    DeriveOp ops[64]; int nops = 0;
    char line[2100];
    while (fgets(line, sizeof line, rf) != NULL) {
        char* s = line; while (*s == ' ' || *s == '\t') s++;
        size_t L = strlen(s); while (L && (s[L-1] == '\n' || s[L-1] == '\r' || s[L-1] == ' ')) s[--L] = 0;
        if (!*s || *s == '#') continue;
        char* sp = s; while (*sp && *sp != ' ' && *sp != '\t') sp++;
        char kw[32]; size_t kl = (size_t)(sp - s); if (kl >= sizeof kw) kl = sizeof kw - 1;
        memcpy(kw, s, kl); kw[kl] = 0;
        while (*sp == ' ' || *sp == '\t') sp++;
        if (strcasecmp(kw, "BASE") == 0) { snprintf(base, sizeof base, "%s", sp); }
        else if (strcasecmp(kw, "OUT") == 0) { snprintf(outp, sizeof outp, "%s", sp); }
        else if (strcasecmp(kw, "POLICY") == 0) {
            char* vs = sp; while (*vs && *vs != ' ' && *vs != '\t') vs++;
            char key[64]; size_t vl = (size_t)(vs - sp); if (vl >= sizeof key) vl = sizeof key - 1;
            memcpy(key, sp, vl); key[vl] = 0; while (*vs == ' ' || *vs == '\t') vs++;
            llmmodelio_model_policy_set(key, vs);
        }
        else if (nops < 64) {
            DeriveOp* op = &ops[nops];
            op->a[0] = 0; op->b[0] = 0;
            if (strcasecmp(kw, "PRUNE") == 0) { op->kind = OP_PRUNE; snprintf(op->a, sizeof op->a, "%s", sp); nops++; }
            else if (strcasecmp(kw, "QUANT") == 0) {
                if (*sp) { op->kind = OP_QUANTAUTO; snprintf(op->a, sizeof op->a, "%s", sp); } else op->kind = OP_QUANT; nops++;
            }
            else if (strcasecmp(kw, "ABLATE") == 0) { op->kind = OP_ABLATE; snprintf(op->a, sizeof op->a, "%s", sp); nops++; }
            else if (strcasecmp(kw, "ADAPTER") == 0) {
                op->kind = OP_ADAPTER;
                char* as = sp; while (*as && *as != ' ' && *as != '\t') as++;
                snprintf(op->a, sizeof op->a, "%.*s", (int)(as - sp), sp);
                while (*as == ' ' || *as == '\t') as++;
                snprintf(op->b, sizeof op->b, "%s", as);
                nops++;
            }
            /* keyword sconosciuta: ignorata (leniente) */
        }
    }
    fclose(rf);
    if (out_override && *out_override) snprintf(outp, sizeof outp, "%s", out_override);
    if (!*base || !*outp) { fprintf(stderr, "LLM DERIVE: ricetta senza BASE o OUT\n"); return -1; }
    /* risolvi BASE/OUT relativi alla dir della RICETTA (portabile, come #include) */
    char rdir[1200] = ""; { const char* sl = strrchr(recipe, '/'); if (sl != NULL) { size_t dl = (size_t)(sl - recipe + 1); if (dl < sizeof rdir) { memcpy(rdir, recipe, dl); rdir[dl] = 0; } } }
    char baseR[2400], outR[2400];
    if (base[0] == '/') snprintf(baseR, sizeof baseR, "%s", base); else snprintf(baseR, sizeof baseR, "%s%s", rdir, base);   /* BASE relativa alla ricetta */
    snprintf(outR, sizeof outR, "%s", outp);   /* OUT relativo alla CWD (l'artefatto del chiamante) */
    /* crea la dir di output (e dei pass intermedi) se manca */
    { char d[2400]; snprintf(d, sizeof d, "%s", outR); char* sl = strrchr(d, '/'); if (sl != NULL) { *sl = 0; if (*d) mkdir(d, 0755); } }

    const char* cur = baseR;
    for (int i = 0; i < nops; i++) {
        char dst[2400];
        if (i == nops - 1) snprintf(dst, sizeof dst, "%s", outR);
        else { const char* sl = strrchr(outR, '/'); if (sl) snprintf(dst, sizeof dst, "%.*s/.derive_step%d.gguf", (int)(sl - outR + 1), outR, i); else snprintf(dst, sizeof dst, ".derive_step%d.gguf", i); }
        if (derive_apply(ops[i].kind, cur, dst, ops[i].a, ops[i].b) != 0) {
            fprintf(stderr, "LLM DERIVE: pass %d fallito\n", i); return -1;
        }
        cur = dst;
    }
    /* nessun pass: copia BASE -> OUT */
    if (nops == 0) {
        FILE* fi = fopen(baseR, "rb"); FILE* do_ = fopen(outR, "wb");
        if (fi == NULL || do_ == NULL) { if (fi) fclose(fi); if (do_) fclose(do_); return -1; }
        char buf[1 << 20]; size_t n; while ((n = fread(buf, 1, sizeof buf, fi)) > 0) fwrite(buf, 1, n, do_);
        fclose(fi); fclose(do_);
    }
    fprintf(fo, "LLM DERIVE '%s': base=%s ops=%d out=%s\n", recipe, baseR, nops, outR);
    /* REGISTRY ↔ DERIVE: registra da solo la versione prodotta (provenance: ricetta, n. pass, base).
     * Tag = basename di OUT senza estensione. L'ADD annota la size; il TAG vi aggiunge la ricetta. */
    {
        const char* ob = strrchr(outR, '/'); ob = ob ? ob + 1 : outR;
        char tag[256]; snprintf(tag, sizeof tag, "%s", ob); { char* dot = strrchr(tag, '.'); if (dot) *dot = 0; }
        const char* rb = strrchr(recipe, '/'); rb = rb ? rb + 1 : recipe;
        const char* bb = strrchr(baseR, '/'); bb = bb ? bb + 1 : baseR;
        struct stat stb; long long sz = (stat(outR, &stb) == 0) ? (long long)stb.st_size : 0;
        char metrics[512]; snprintf(metrics, sizeof metrics, "size=%lld;recipe=%s;ops=%d;base=%s", sz, rb, nops, bb);
        (void)llmmodelio_registry_cmd("ADD", outR, tag, fo, NULL);
        (void)llmmodelio_registry_cmd("TAG", tag, metrics, fo, NULL);
    }
    return 0;
}
