# MLX MoE — Sorted gather prefill (prefill/decode gap fix)

Status: 2026-09-03. Supreme rule applied: copy/adaptation **1:1 from the reference mlx_lm**
(`mlx_lm/models/switch_layers.py` `SwitchGLU`/`_gather_sort`), no custom hypothesis.

## Problem

The engine's MoE ffns called `mlx_gather_qmm(..., sorted_indices=false)` **always**, also at
prefill (large batch). The mlx_lm reference, instead, sorts when `indices.size >= 64`
(= `n_tokens * top_k >= 64`): it sorts the rows by expert and uses the **sorted** path of gather_qmm
(contiguous groups per expert = efficient). The unsorted gather at prefill batch was the bottleneck:

- MoE prefill (GMB_PHASE): ffn = 87-94% of the time.
- 35B-A3B prefill ~3.7x below mlx_lm eager, all of it in the ffn.

## Fix (1:1 mlx_lm SwitchGLU layout)

`do_sort` branch active when `n_tokens * top_k >= 64` (identical to mlx_lm), replicating
`_gather_sort`/`_scatter_unsort` in C:

1. `tt` = flat `top_inds` (T·K) uint32; `order = argsort(tt)`; `inv = argsort(order)`.
2. token rows duplicated per slot: token = `order//K` (via float divide + cast) →
   `take_along_axis(x1, axis=0)` with indices `(M,1,1,1)` → rows `(M,1,1,H)` → reshape `(M,1,H)`.
3. `idx_sorted = tt[order]` (1-D, M), sorted ascending.
4. `gather_qmm(gate/up/down, x=(M,1,H), rhs=idx_sorted, sorted_indices=true)` → out `(M,1,OD)`
   (shape verified empirically vs python, including the trailing singletons).
5. unsort: `take_along_axis(down_out, axis=0)` with `inv` reshaped `(M,1,1)` → `(M,1,H)`
   (NOTE: `mlx_take` C = flatten semantics `axis=None`, NOT gather on axis0 → use
   `take_along_axis`).
6. reshape `(T,K,H)`, scale by probs, `sum` on K → `moe_out (1,T,H)`.

N.B. `mlx_take(&r, a, idx)` on the C API does a flatten (`a.reshape(-1)[idx]`): bug already caught (duns
became 1-D size M).

Functions modified in `LLMEnv/LLMEngine/mlx/gmb_host_ai_mlx.c` (scoring identical to the
existing one per architecture):

- `qwen3_5_moe_ffn_block` (Qwen3.6-35B-A3B): probs = `take_along(gates_sm, top_inds)`, norm_topk optional.
- `qwen3_moe_ffn_block` (Coder-30B-A3B, A10B, gpt): same gates_sm + norm_topk optional.
- `mixtral_ffn_block` (Mixtral-8x22B): probs = `softmax(take_along(gates, top_inds))`.
- Not touched: `gpt_oss_ffn_block` (already conditional sorted `!unsort_moe`); `glm4_moe_ffn_block`
  (host-dependent path: top_inds readback, e_score_correction_bias, imatrix — separate refactor, out of target).

Decode unchanged (n_tokens=1 → `top_k < 64` → unsorted path, like the reference).

## Measurements (prefill append 4096 / decode gen)

| Model | prefill before | prefill after | notes |
|---|---|---|---|
| Qwen3.6-35B-A3B | 1176 | **2755** | ~1.6x from mlx eager (~4500) |
| Mixtral-8x22B-4bit | 20.6 | **215.9** | ≈ rapidMLX ~192 (prefill parity) |
| Qwen3-Coder-30B-A3B | ~450 | **1707** | |
| Qwen3.5-122B-A10B | 242 | **278** | residual ~3x gap (mxfp4) |

Correctness: sorted branch == unsorted (diff = only fp16 rounding, verified on real shapes in
python). Smooth decode on all archs.

## Commits

- `3c667c9b` qwen3_5_moe ffn sorted.
- `59cb0edb` qwen3_moe + mixtral ffn sorted.

## Profiler (for this work)

- `GMB_MEMPROF`: per-layer/per-sub-op GPU memory (it found the Mixtral weight duplication, 148→75GB,
  `91fb3aba`).
- `GMB_PHASE`: attn/ffn split per layer (it forces eval; ok for RATIOS, inflated times).

## Residual gaps (measurements on a free GPU required)

- Mixtral decode: ffn ~92% of decode (GMB_PHASE forced); engine ~1.5x mlx eager even at T=1
  (M<64 → both unsorted). Numbers not stable in session (GPU contention) — to be profiled
  per-sub-op on a clean machine.
- A10B prefill ~3x (mxfp4): path not clarified.
- glm4_moe host-dependent: candidate if GLM-4.5 enters the targets.
