/* gmb_brain_tools.h — Second Brain: registry, manifest e toolchain in C (sorgente UNICA).
 *
 * Sostituisce integralmente il toolchain C# (`Basic.AI.Knowledge.Brain*`): registry
 * `config/brains.json`, manifest `brain.manifest.json` (formato `gmbrain`, sha256 per file e
 * checksum auto-riferito), EXPORT/SIGN (.gmbrain.zip) e le operazioni di manutenzione
 * (stats, verify, create, register, buildvault, optimize, rename, backup, dedupe, extract,
 * merge, convert, verify-golden, diff, doctor).
 *
 * Regola del progetto: questa è la sola implementazione; CLI (`gmbadmin brain …`) e REPL
 * (`BRAIN …`) la chiamano. Nessuna logica nel mondo C#.
 */
#ifndef GMB_BRAIN_TOOLS_H
#define GMB_BRAIN_TOOLS_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── esito di un'operazione (ToolResult del C#: testo + errore) ───────────── */
typedef struct gmb_brain_result {
    int   ok;      /* 1 = riuscita */
    char* text;    /* messaggio/rapporto (malloc), può essere NULL */
    char* error;   /* errore (malloc), NULL se ok */
} gmb_brain_result_t;

void gmb_brain_result_free(gmb_brain_result_t* r);

/* ── registry (config/brains.json, stesso store del C#) ───────────────────── */
typedef struct gmb_brain_rec {
    char name[128];
    char mode[16];
    char source[1024];
    char vault_path[1024];
    char checksum[160];
    char version[64];
    char installed_at[64];
    char signed_by[64];
    char available[64];      /* "rag,graph" */
    long docs;
    int  attached;
} gmb_brain_rec_t;

/* Info effettiva: voce registrata (se c'è) + stato reale su disco.
 * Ritorna 1 se il cervello è registrato o disponibile, 0 se sconosciuto. */
int gmb_brain_reg_info(const char* name, gmb_brain_rec_t* out);

/* Upsert nel registry. Campi NULL = mantiene il valore esistente. 1/0. */
int gmb_brain_reg_register(const char* name, const char* source, const char* vault_path,
                           const char* mode, const char* checksum, const char* version,
                           long docs, const char* signed_by);
int gmb_brain_reg_unregister(const char* name);

/* Forme disponibili: "rag", "graph", "rag,graph" o "" (nessuna). */
int gmb_brain_reg_available(const char* name, char* out, size_t cap);
/* Artefatti risolti (root installata → registry → dir di build). 1 se esistono. */
int gmb_brain_reg_rag_db(const char* name, char* out, size_t cap);
int gmb_brain_reg_graph_json(const char* name, char* out, size_t cap);
int gmb_brain_reg_root(const char* name, char* out, size_t cap);
int gmb_brain_reg_attached(const char* name);

/* ── manifest (brain.manifest.json) ──────────────────────────────────────── */
/* sha256 di un file in forma "sha256:<HEX>" (malloc), NULL se illeggibile. */
char* gmb_brain_manifest_hash_file(const char* path);
/* Verifica manifest+checksum+file di un cervello installato in `root`.
 * 0 = ok, 1 = manifest assente, -1 = manifest invalido (motivo in err). */
int gmb_brain_manifest_verify(const char* root, char* err, size_t cap);
/* Contenuto di brain.manifest.json (malloc) o NULL. */
char* gmb_brain_manifest_load(const char* root);
/* Nome/versione/mode/docs dichiarati dal manifest (stringhe vuote se assenti). 1 se letto. */
int gmb_brain_manifest_meta(const char* root, char* name, size_t ncap, char* version, size_t vcap,
                            char* mode, size_t mcap, long* docs);

/* ── operazioni (una funzione per comando) ───────────────────────────────── */
gmb_brain_result_t gmb_brain_tool_stats(const char* name, int as_json);
gmb_brain_result_t gmb_brain_tool_verify(const char* name, int as_json);
gmb_brain_result_t gmb_brain_tool_verify_golden(const char* name, const char* golden, int topk, double min_p1);
gmb_brain_result_t gmb_brain_tool_create(const char* name, const char* from_dir, const char* mode,
                                         int do_register, int activate, int dry_run);
gmb_brain_result_t gmb_brain_tool_register(const char* name, const char* mode, const char* source, int activate);
gmb_brain_result_t gmb_brain_tool_buildvault(const char* out_dir);
gmb_brain_result_t gmb_brain_tool_optimize(const char* name, int dry_run, int reindex_fts);
gmb_brain_result_t gmb_brain_tool_rename(const char* name, const char* new_name, int dry_run);
gmb_brain_result_t gmb_brain_tool_backup(const char* name, const char* out_zip);
gmb_brain_result_t gmb_brain_tool_export(const char* name, const char* out_zip);
gmb_brain_result_t gmb_brain_tool_sign(const char* name, const char* dir, const char* out_zip);
gmb_brain_result_t gmb_brain_tool_dedupe(const char* name, int apply, int min_chars, int dry_run);
gmb_brain_result_t gmb_brain_tool_extract(const char* name, const char* out_name, const char* source_filter,
                                          const char* node_filter, int hops, int dry_run);
gmb_brain_result_t gmb_brain_tool_merge(const char* const* names, int n, const char* out_name,
                                        const char* on_conflict, int dry_run);
gmb_brain_result_t gmb_brain_tool_convert(const char* name, const char* to, const char* out_name,
                                          int in_place, int dry_run, int min_edges);
gmb_brain_result_t gmb_brain_tool_diff(const char* a, const char* b, int as_json);
gmb_brain_result_t gmb_brain_tool_doctor(const char* name);

/* Grafo → vault Obsidian (`graph export-obsidian`). */
gmb_brain_result_t gmb_brain_tool_graph_export_obsidian(const char* graphs_dir, const char* name, const char* out_dir);

#ifdef __cplusplus
}
#endif
#endif /* GMB_BRAIN_TOOLS_H */
