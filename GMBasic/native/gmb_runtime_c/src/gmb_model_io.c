/* gmb_model_io.c — Model I/O in-process (namespace LLM). Vedi gmb_model_io.h.
 *
 * PERCHÉ: `gmb_embedded.c` delegava convert/quantize/inspect ai subprocess
 * `gmb_model_quantize` / `gmb_model_quantize` / `gmb-discover` via system(). Ora chiama
 * QUESTE funzioni in-process: stessa logica C (una sola sorgente), nessun subprocess, e i 3
 * backend C (C99Native, VM/C) si allineano a IL/PCodeVM (che vanno via P/Invoke
 * sullo stesso runtime). */
#include "gmb_model_io.h"

#include "gmb_model_quantize.h"   /* gmb_model_quantize_ex */
#include "llmdesc.h"              /* .gmbarch: descrittore architettura (unica fonte; NIENTE GMAD) */
#include "gmb_runtime.h"          /* gmb_paths_libs_cache_dir / gmb_rid */
#include "llmcodec.h"             /* llmcodec_add_plugin_dir */

#include <stddef.h>
#include <stdio.h>

/* `GGUF` DEPRECATO: nessun uso. La conversione supportata è GGUF/MLX via codec. */
/* I codec (libllmcodec_*.{dylib,so,dll}) NON stanno accanto all'eseguibile: il downloader li
 * mette nella cache (`~/.gmbasic/libs/<rid>/`). Il registry dei codec cerca in
 * Resources/plugins e nella CWD — cioè mai dove sono davvero in un'app installata: qui gli si
 * registra la cache. Idempotente, una volta sola per processo. */
static void model_io_register_plugin_dir(void)
{
    static int done = 0;
    char cache[2048], dir[2200];
    if (done) return;
    done = 1;
    gmb_paths_libs_cache_dir(cache, sizeof cache);
    if (!cache[0]) return;
    snprintf(dir, sizeof dir, "%s/%s", cache, gmb_rid());
    llmcodec_add_plugin_dir(dir);
}

int gmb_model_io_convert(const char* model_dir, const char* out_path)
{
    model_io_register_plugin_dir();
    (void)model_dir; (void)out_path;
    return -1;   /* convert: implementato nella lega LLM (questo punto resta l'entry) */
}

int gmb_model_io_quantize(const char* src_dir, const char* out_dir, int bits, int group,
                          const char* imatrix_path, int expert_bits)
{
    model_io_register_plugin_dir();
    if (src_dir == NULL || out_dir == NULL) return -1;
    char rsrc[2048], rdst[2048];
    gmb_paths_resolve_model(src_dir, rsrc, sizeof rsrc);
    gmb_paths_resolve_model(out_dir, rdst, sizeof rdst);
    if (rsrc[0] && rdst[0]) { src_dir = rsrc; out_dir = rdst; }
    if (bits < 1 || bits > 8) return -1;
    if (group <= 0) group = 64;
    return gmb_model_quantize_ex(src_dir, out_dir, (unsigned)bits, (unsigned)group,
                                 imatrix_path, (uint32_t)(expert_bits > 0 ? expert_bits : 0), 0);
}

int gmb_model_io_inspect(const char* dir, char* out, int cap)
{
    if (dir == NULL || out == NULL || cap <= 0) return -1;
    char rdir[2048];
    gmb_paths_resolve_model(dir, rdir, sizeof rdir);
    if (rdir[0]) dir = rdir;
    /* `.gmbarch` (LLMDesc) = UNICA fonte di architettura. GMAD eliminato. */
    LlmDesc d; char path[4096];
    if (llmdesc_resolve(dir, &d, path, sizeof path) != 0) return -1;
    char st[160] = ""; int off = 0;
    for (int i = 0; i < d.n_struct && off < 140; i++)
        off += snprintf(st + off, sizeof st - (size_t)off, "%s%s=%s", i ? ", " : "", d.structures[i].key, d.structures[i].type);
    const char* mod = llmdesc_modality(&d);
    return snprintf(out, (size_t)cap,
        ".gmbarch: %s\narch=%s\nstructures: %s\nrecipe=%s boost_fmt=%s\nroles=%d quant=%d codecs=%d modality=%s\n",
        path[0] ? path : "(generico)", d.arch[0] ? d.arch : "-", st[0] ? st : "-",
        d.recipe[0] ? d.recipe : "-", d.boost_fmt[0] ? d.boost_fmt : "-",
        d.n_roles, d.n_quant, d.n_codec, mod ? mod : "text");
}

int gmb_model_io_inspect_print(const char* dir)
{
    char buf[16384];
    int n = gmb_model_io_inspect(dir, buf, (int)sizeof(buf));
    if (n <= 0) return -1;
    fwrite(buf, 1, (size_t)n, stdout);
    return 0;
}
