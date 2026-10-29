# MLX MoE — Sorted gather prefill (fix gap prefill/decode)

Stato: 2026-09-03. Regola suprema applicata: copia/adattamento **1:1 dalla reference mlx_lm**
(`mlx_lm/models/switch_layers.py` `SwitchGLU`/`_gather_sort`), nessuna ipotesi custom.

## Problema

Gli ffn MoE del motore chiamavano `mlx_gather_qmm(..., sorted_indices=false)` **sempre**, anche a
prefill (batch grande). La reference mlx_lm, invece, ordina quando `indices.size >= 64`
(= `n_tokens * top_k >= 64`): sorta le righe per esperto e usa il percorso **sorted** di gather_qmm
(gruppi contigui per esperto = efficiente). Il gather unsorted a batch prefill era il collo:

- prefill MoE (GMB_PHASE): ffn = 87-94% del tempo.
- 35B-A3B prefill ~3.7x sotto mlx_lm eager, tutto nell'ffn.

## Fix (layout 1:1 mlx_lm SwitchGLU)

Branch `do_sort` attivo quando `n_tokens * top_k >= 64` (identico a mlx_lm), replicando
`_gather_sort`/`_scatter_unsort` in C:

1. `tt` = flat `top_inds` (T·K) uint32; `order = argsort(tt)`; `inv = argsort(order)`.
2. righe token duplicate per slot: token = `order//K` (via float divide + cast) →
   `take_along_axis(x1, axis=0)` con indices `(M,1,1,1)` → rows `(M,1,1,H)` → reshape `(M,1,H)`.
3. `idx_sorted = tt[order]` (1-D, M), ordinato crescente.
4. `gather_qmm(gate/up/down, x=(M,1,H), rhs=idx_sorted, sorted_indices=true)` → out `(M,1,OD)`
   (shape verificata empiricamente vs python, inclusi i trailing singleton).
5. unsort: `take_along_axis(down_out, axis=0)` con `inv` reshape `(M,1,1)` → `(M,1,H)`
   (NOTA: `mlx_take` C = flatten semantics `axis=None`, NON gather su axis0 → usare
   `take_along_axis`).
6. reshape `(T,K,H)`, scale per probs, `sum` su K → `moe_out (1,T,H)`.

N.B. `mlx_take(&r, a, idx)` sulla C API fa flatten (`a.reshape(-1)[idx]`): bug già colto (duns
diveniva 1-D size M).

Funzioni modificate in `LLMEnv/LLMEngine/mlx/gmb_host_ai_mlx.c` (scoring identico a quella
esistente per architettura):

- `qwen3_5_moe_ffn_block` (Qwen3.6-35B-A3B): probs = `take_along(gates_sm, top_inds)`, norm_topk opz.
- `qwen3_moe_ffn_block` (Coder-30B-A3B, A10B, gpt): idem gates_sm + norm_topk opz.
- `mixtral_ffn_block` (Mixtral-8x22B): probs = `softmax(take_along(gates, top_inds))`.
- Non toccati: `gpt_oss_ffn_block` (già sorted condizionale `!unsort_moe`); `glm4_moe_ffn_block`
  (path host-dependent: readback top_inds, e_score_correction_bias, imatrix — refactor separato, fuori target).

Decode invariato (n_tokens=1 → `top_k < 64` → path unsorted, come la reference).

## Misure (prefill append 4096 / decode gen)

| Modello | prefill prima | prefill dopo | note |
|---|---|---|---|
| Qwen3.6-35B-A3B | 1176 | **2755** | ~1.6x da mlx eager (~4500) |
| Mixtral-8x22B-4bit | 20.6 | **215.9** | ≈ rapidMLX ~192 (parità prefill) |
| Qwen3-Coder-30B-A3B | ~450 | **1707** | |
| Qwen3.5-122B-A10B | 242 | **278** | gap ~3x residuo (mxfp4) |

Correttezza: branch sorted == unsorted (diff = solo rounding fp16, verificato su shape reali in
python). Decode fluido su tutte le arch.

## Commit

- `3c667c9b` qwen3_5_moe ffn sorted.
- `59cb0edb` qwen3_moe + mixtral ffn sorted.

## Profiler (per questo lavoro)

- `GMB_MEMPROF`: memoria GPU per-layer/per-sub-op (trovò la duplicazione pesi Mixtral, 148→75GB,
  `91fb3aba`).
- `GMB_PHASE`: split attn/ffn per layer (forza eval; ok per RAPPORTI, tempi gonfiati).

## Gap residui (misure a GPU libera richieste)

- Decode Mixtral: ffn ~92% del decode (GMB_PHASE forzato); motore ~1.5x mlx eager anche a T=1
  (M<64 → entrambi unsorted). Numeri non stabili in sessione (contention GPU) — da profilare
  per-sub-op in macchina pulita.
- Prefill A10B ~3x (mxfp4): percorso non chiarito.
- glm4_moe host-dependent: candidato se GLM-4.5 entra nei target.
