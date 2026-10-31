/* gmb_modelio_shim.c — ABI stabile per il P/Invoke dal C# del MODEL I/O (convert / quantize /
 * inspect). PERCHÉ: il C# (il C#) chiama QUESTE funzioni invece di
 * spawnare gli eseguibili di conversione/quantizzazione/discovery
 * (AGENTS, REGOLA PRIMARIA 2026-09-23: C# -> C via P/Invoke ammesso; C -> C# mai). La LOGICA
 * resta UNA: queste funzioni richiamano le implementazioni C già esistenti, non le ricopiano.
 *
 * Esposte nella dylib `gmb_ai_modelio` (target CMake omonimo). */
#include "gmb_model_quantize.h"
#include "llmdesc.h"              /* .gmbarch (unica fonte; NIENTE GMAD) */

#include <stddef.h>
#include <stdio.h>

/* Il container `GGUF` è DEPRECATO: queste entry non devono più produrlo. Ritornano -1
 * esplicitamente (mai un no-op silenzioso). La conversione supportata è GGUF/MLX via codec
 * (`llmcodec_conv_mlx_to_gguf_arch` / `llmcodec_conv_gguf_to_mlx_arch`, esposte da `llmmodelio_model_convert`). */
int gmb_model_convert_ex(const char* model, const char* out,
                         unsigned l0, unsigned l1, unsigned e0, unsigned e1,
                         int dense, unsigned ctx_override, long ml, long me)
{
    (void)model; (void)out; (void)l0; (void)l1; (void)e0; (void)e1; (void)dense; (void)ctx_override; (void)ml; (void)me;
    return -1;
}

int gmb_modelio_convert(const char* model, const char* out)
{
    return gmb_model_convert_ex(model, out, 0u, 0u, 0u, 0u, 1, 4096u, -1, -1);
}

/* Equivalente di `gmb_model_quantize --model <src> --out <dst> --bits <b> --group <g> [--imatrix p]`. */
int gmb_modelio_quantize(const char* src, const char* out, int bits, int group, const char* imatrix, int expert_bits)
{
    return gmb_model_quantize_ex(src, out, (uint32_t)bits, (uint32_t)group, imatrix,
                                 (uint32_t)(expert_bits > 0 ? expert_bits : 0), 0);
}

/* INSPECT: report del descrittore `.gmbarch` (UNICA fonte). GMAD eliminato. Scrive in `out`
 * (cap byte) e ritorna la lunghezza (>0), oppure -1 su errore di risoluzione. */
int gmb_modelio_inspect(const char* dir, char* out, int cap)
{
    if (dir == NULL || out == NULL || cap <= 0) return -1;
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
