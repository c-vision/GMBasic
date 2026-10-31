/* llmmodelio_registry.c — B3: model registry / versioning (LLM REGISTRY <op>).
 *
 * Un registro persistente di "versioni" di modello: ogni voce = tag → path + flag promoted +
 * metriche (k=v). Superficie: `LLM REGISTRY <op>, <a>, <b>` (alias top-level riscritto dal
 * parser a `LLM TENSOR REGISTRY <op>, <a>, <b>` → llmmodelio_registry_cmd(op, a, b)).
 *
 *   ADD     <path>, <tag>       registra/aggiorna una versione (tag obbligatorio)
 *   LIST                        elenca le versioni (ordine di registrazione)
 *   SHOW    <tag>               mostra path + promoted + metriche di un tag
 *   TAG     <tag>, <k=v;...>    imposta/aggiorna le metriche di un tag
 *   PROMOTE <tag>               marca il tag come "promosso" (corrente)
 *   REMOVE  <tag>               rimuove un tag
 *
 * Store: file TSV `work/models.registry` (override: opzione registro `MODEL_REGISTRY`), una riga
 * per versione: `tag \t path \t promoted(0|1) \t k=v;k=v`. Nessuna dipendenza esterna.
 * Output deterministico su `out` (= LOUT, così la variante _to lo cattura -> parità 4 backend). */
#include "llmmodelio_model.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <dirent.h>

#define REG_MAX 64
typedef struct { char tag[128]; char path[1024]; int promoted; char metrics[512]; } RegEntry;

/* Dimensione "peso" di una versione: file singolo = st_size; DIRECTORY (checkpoint MLX/HF con
 * piu' file) = somma RICORSIVA dei file (la stat della dir darebbe solo la dimensione dell'entry). */
static long long reg_path_size(const char* path) {
    struct stat st; if (stat(path, &st) != 0) return 0;
    if (!S_ISDIR(st.st_mode)) return (long long)st.st_size;
    DIR* d = opendir(path); if (d == NULL) return 0;
    long long total = 0; struct dirent* e;
    while ((e = readdir(d)) != NULL) {
        if (strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0) continue;
        char c[1300]; snprintf(c, sizeof c, "%s/%s", path, e->d_name);
        total += reg_path_size(c);
    }
    closedir(d);
    return total;
}

static const char* reg_path(void)
{
    const char* p = llmmodelio_model_policy_get("MODEL_REGISTRY");
    return (p && *p) ? p : "work/models.registry";
}

static int reg_load(const char* path, RegEntry* e, int cap)
{
    FILE* f = fopen(path, "r");
    if (f == NULL) return 0;
    int n = 0; char line[2048];
    while (n < cap && fgets(line, sizeof line, f) != NULL) {
        size_t L = strlen(line); while (L && (line[L-1] == '\n' || line[L-1] == '\r')) line[--L] = 0;
        if (!*line) continue;
        char* t1 = strchr(line, '\t'); if (!t1) continue; *t1++ = 0;
        char* t2 = strchr(t1, '\t'); if (!t2) continue; *t2++ = 0;
        char* t3 = strchr(t2, '\t'); if (t3) *t3++ = 0; else t3 = (char*)"";
        snprintf(e[n].tag, sizeof e[n].tag, "%s", line);
        snprintf(e[n].path, sizeof e[n].path, "%s", t1);
        e[n].promoted = (t2[0] == '1');
        snprintf(e[n].metrics, sizeof e[n].metrics, "%s", t3);
        n++;
    }
    fclose(f);
    return n;
}

static int reg_save(const char* path, const RegEntry* e, int n)
{
    /* Crea la dir dello store se manca (es. `work/`). */
    char d[1200]; snprintf(d, sizeof d, "%s", path);
    char* sl = strrchr(d, '/');
    if (sl != NULL) { *sl = 0; if (*d) mkdir(d, 0755); }
    FILE* f = fopen(path, "w");
    if (f == NULL) return -1;
    for (int i = 0; i < n; i++)
        fprintf(f, "%s\t%s\t%d\t%s\n", e[i].tag, e[i].path, e[i].promoted, e[i].metrics);
    fclose(f);
    return 0;
}

static int reg_find(const RegEntry* e, int n, const char* tag)
{
    for (int i = 0; i < n; i++) if (strcasecmp(e[i].tag, tag) == 0) return i;
    return -1;
}

int llmmodelio_registry_cmd(const char* op, const char* a, const char* b, FILE* out, int* handled)
{
    const char* o = op ? op : "";
    const char* x = a ? a : "";
    const char* y = b ? b : "";
    FILE* fo = out ? out : stdout;
    if (handled) *handled = 0;
    int is = strcasecmp(o, "ADD") == 0 || strcasecmp(o, "LIST") == 0 || strcasecmp(o, "SHOW") == 0
          || strcasecmp(o, "TAG") == 0 || strcasecmp(o, "PROMOTE") == 0 || strcasecmp(o, "REMOVE") == 0
          || strcasecmp(o, "MEASURE") == 0 || strcasecmp(o, "CLEAR") == 0;
    if (!is) return 0;
    if (handled) *handled = 1;

    const char* path = reg_path();
    RegEntry e[REG_MAX];
    int n = reg_load(path, e, REG_MAX);

    if (strcasecmp(o, "CLEAR") == 0) {
        (void)reg_save(path, e, 0);                 /* riscrive vuoto */
        fprintf(fo, "LLM REGISTRY CLEAR\n");
        return 0;
    }

    if (strcasecmp(o, "ADD") == 0) {
        if (!*x || !*y) { return -1; }               /* path, tag obbligatori */
        int i = reg_find(e, n, y);
        if (i < 0) { if (n >= REG_MAX) return -1; i = n++; e[i].tag[0] = 0; e[i].promoted = 0; e[i].metrics[0] = 0; }
        snprintf(e[i].tag, sizeof e[i].tag, "%s", y);
        snprintf(e[i].path, sizeof e[i].path, "%s", x);
        /* Metriche AUTOMATICHE: la size (byte) del file, se presente — così il registro annota
         * sempre il "peso" della versione senza dipendere da TAG manuale. (PPL: via LLM QUALITY
         * / QUANTERROR, esplicito, e poi TAG.) Se il file non esiste, resta vuoto (back-compat). */
        { long long sz = reg_path_size(x);
          if (sz > 0) snprintf(e[i].metrics, sizeof e[i].metrics, "size=%lld", sz);
          else e[i].metrics[0] = 0; }
        if (reg_save(path, e, n) != 0) return -1;
        fprintf(fo, "LLM REGISTRY ADD: tag=%s path=%s\n", y, x);
        return 0;
    }
    if (strcasecmp(o, "MEASURE") == 0) {
        if (!*x) return -1;                          /* tag obbligatorio */
        int i = reg_find(e, n, x);
        if (i < 0) { fprintf(fo, "LLM REGISTRY MEASURE '%s': not found\n", x); return 0; }
        /* Metriche AUTOMATICHE: size del file + (se il chiamante passa un prompt/dataset) la PPL
         * via LLM QUALITY (forward). Cosi' REGISTER->QUALITY e' agganciato: `MEASURE <tag>, <prompt>`.
         * Senza prompt resta la sola size. PPL non misurabile -> `ppl=na` (mai silenzioso). */
        char m[512]; m[0] = 0;
        long long sz = reg_path_size(e[i].path); if (sz > 0)
            snprintf(m, sizeof m, "size=%lld", sz);
        if (*y) {
            double ppl = -1; int tok = 0;
            if (llmmodelio_model_ppl(e[i].path, y, &ppl, &tok) == 0 && ppl >= 0.0)
                snprintf(m + strlen(m), sizeof m - strlen(m), "%sppl=%.4f", m[0] ? ";" : "", ppl);
            else snprintf(m + strlen(m), sizeof m - strlen(m), "%sppl=na", m[0] ? ";" : "");
        }
        if (m[0]) snprintf(e[i].metrics, sizeof e[i].metrics, "%s", m);
        if (reg_save(path, e, n) != 0) return -1;
        fprintf(fo, "LLM REGISTRY MEASURE '%s': path=%s metrics=%s\n", e[i].tag, e[i].path, e[i].metrics);
        return 0;
    }
    if (strcasecmp(o, "LIST") == 0) {
        fprintf(fo, "LLM REGISTRY LIST: n=%d tags=", n);
        for (int i = 0; i < n; i++) fprintf(fo, "%s%s", i ? "," : "", e[i].tag);
        fprintf(fo, "\n");
        return 0;
    }
    if (strcasecmp(o, "SHOW") == 0) {
        if (!*x) return -1;
        int i = reg_find(e, n, x);
        if (i < 0) { fprintf(fo, "LLM REGISTRY SHOW '%s': not found\n", x); return 0; }
        fprintf(fo, "LLM REGISTRY SHOW '%s': path=%s promoted=%d metrics=%s\n",
                e[i].tag, e[i].path, e[i].promoted, e[i].metrics);
        return 0;
    }
    if (strcasecmp(o, "TAG") == 0) {
        if (!*x) return -1;
        int i = reg_find(e, n, x);
        if (i < 0) { fprintf(fo, "LLM REGISTRY TAG '%s': not found\n", x); return 0; }
        snprintf(e[i].metrics, sizeof e[i].metrics, "%s", y);
        if (reg_save(path, e, n) != 0) return -1;
        fprintf(fo, "LLM REGISTRY TAG '%s': metrics=%s\n", e[i].tag, e[i].metrics);
        return 0;
    }
    if (strcasecmp(o, "PROMOTE") == 0) {
        if (!*x) return -1;
        int i = reg_find(e, n, x);
        if (i < 0) { fprintf(fo, "LLM REGISTRY PROMOTE '%s': not found\n", x); return 0; }
        for (int j = 0; j < n; j++) e[j].promoted = 0;   /* un solo "corrente" */
        e[i].promoted = 1;
        if (reg_save(path, e, n) != 0) return -1;
        fprintf(fo, "LLM REGISTRY PROMOTE '%s'\n", e[i].tag);
        return 0;
    }
    /* REMOVE */
    if (!*x) return -1;
    int i = reg_find(e, n, x);
    if (i < 0) { fprintf(fo, "LLM REGISTRY REMOVE '%s': not found\n", x); return 0; }
    for (int j = i; j + 1 < n; j++) e[j] = e[j + 1];
    n--;
    if (reg_save(path, e, n) != 0) return -1;
    fprintf(fo, "LLM REGISTRY REMOVE '%s'\n", x);
    return 0;
}
