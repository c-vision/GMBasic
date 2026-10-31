# RMLX-10 — compiled decode: execution plan (since 2026-09-03)

Goal: close the ~1.5-3x decode residual vs rapidMLX (mlx.compile + graph-reuse) on
large MoE models (Mixtral, 35B-A3B, Coder, A10B). Supreme rule: copy/adapt 1:1 from the
reference (rapidMLX `vllm_mlx/`, mlx_lm cache.py); no custom hypothesis.

## Established fact (pre-requisite)

`mixtral_attention_block` updates the KV with `mlx_concatenate_axis(&k_full, kv, 2)` **at every step**:
the shapes of the step grow → the whole decode-step is NOT compilable (mlx.compile requires static
shapes). mlx_lm eager (reference) does the same concat and indeed does not compile the decode. rapid uses
a fixed-buffer cache.

Today's measurement (engine vs mlx eager, Mixtral): short-ctx decode engine ~5.3 vs mlx ~7 (1.3x);
ctx 6658: engine (ctx4096 ~2.9-4.0) FASTER than mlx eager (~1 tok/s). The big gap is toward
rapid (compile+paged), not toward mlx eager.

### Feasibility analysis (2026-09-03, from the code)

- `mixtral_attention_block` (line ~3787): cache `[1,n_kv,L,hd]`, every step `k_full=concat(k_cache,k_rope,axis=2)`.
- `gmb_mlx_gqa_sdpa` (`gmb_mlx_ops.h:113`) with window<=0 delegates to `mlx_fast_scaled_dot_product_attention(...,"causal")`
  which derives the key length from the SHAPE of k (offset=kL-qL, fast.cpp). Cache padded to a static shape
  => the causal mode would look at future tokens (zeros). FOR SDPA on a static cache you need: static padded
  shape `[1,n_kv,max_ctx,hd]` + ADDITIVE MASK mode="" with 0 for j<=pos and -inf beyond (the mask is data,
  not shape) — the same mechanism as the window branch (gmb_mlx_ops.h:67-105).
- Positional write with mlx ops only = where(rowsel, k_new, old) => O(max_ctx) copy per step per layer
  (no in-place in mlx). At max_ctx>=4096 it is ~ concat; NOT enough for rapid-parity: a **custom Metal
  scatter kernel into a persistent buffer** is needed + SDPA that reads buffer+offset (dedicated KV
  subsystem, not a patch). For Phase B (compile) alone the additive mask + where-update are enough as a first
  working step; the in-place kernel is the next evolution.
- Runner: per-layer caches created in the decode loop (see k_cache/v_cache init and `has_cache`); size
  max_ctx = effective max_len (not 128k: allocate only the max seen, with rolling trim kv_max_tokens).

## Phase A — Fixed-buffer KV cache (pre-requisite, value on its own)


1. In every attention block (mixtral, qwen3_moe, qwen3_5, gpt_oss, glm4_moe) replace the growing
   concat with a preallocated max_ctx buffer: `k_cache = mlx_zeros([n_layers? n_kv_h, max_ctx, hd])`
   allocated at load/`new_cache`, and per-step positional write at a static shape.
   Reference layout: `mlx_lm/models/cache.py` `KVCache` (update_and_fetch) and mixtral.py cache usage.
2. The attention read uses `cache[:, :, :pos, :]` (variable-shape slice ONLY on the
   attention side) — position passed as a scalar; the decode graph stays shape-static for
   new k/v (slice read can stay shape-dependent outside the compile, or use a max_len mask).
3. Gate: numeric parity vs eager before and after (layer trajectory / decode tokens).
4. Bonus expected even without compile: no O(ctx) copy per step (concat) -> long ctx improve.

## Phase B — compiled closure of the decode

1. Plumb `mlx_closure_new_func_payload` (closure.h) + `mlx_compile` (compile.h) in
   `gmb_host_ai_mlx`. C function `step_fn(outputs, inputs, payload)`: it receives input arrays at
   static shape (token h, weights, k/v buffers, scalar pos) and builds the graph of the step
   (attention+ffn all the layers) returning the lazy output; `mlx_compile(closure, shapeless=false)`.
2. At load/compile: the graph is traced (first call with those shapes); at every token the
   compiled closure is called again with the new arrays (same shapes) -> cache hit -> compiled/fused metal.
   Compile per architecture; `mlx_compile_mode` gate via env (off default).
3. Verification: Mixtral decode parity vs eager (identical tokens for a given seed), then tok/s at ctx8/64/4096.
4. Careful memory management: the compile keeps the traced graphs alive (cache) -> verify
   GPU peak (the compile cache is NOT the live memory limit already in use).
   Interaction with `mlx_set_cache_limit` (uncommitted line ~7057) to be decided.

## Phase C — target measurements (clean GPU, after thermal warmup)

- Mixtral decode ctx8/64/4096 vs mlx eager and vs rapid (same prompt) -> expected: eager-parity
  shortly, approaching rapid (~17 ctx4096).
- 35B-A3B, Coder, A10B decode @ctx4096.

## Caveat

- compile with `shapeless=false` requires identical shapes EVERY step: batch=1 fixed, top_k fixed,
  KV buffer fixed. Prompt length changes (prefill vs decode) = 2 separate traces.
- Do not touch the eager path: keep `GMB_NO_COMPILE=1` as fallback.
- No double-free: the compiled closure owns the graph; the intermediate lazy arrays stay managed
  by the mlx-c refcount (same scheme as the current path).

Related commits: sorted-gather 3c667c9b/59cb0edb; memory fix 91fb3aba; doc MLX-MoE-Sorted-Gather.

---

## Measured baseline 2026-09-08 (M5 Max, load 2,7→4,5; detail in the LLMEnv vault)

Above, the «1,5-3×» gap was **estimated on Mixtral**. Today it is measured model by model, same
day, same machine, adjacent arms (decode `gen=128`, 3 reps, alternating order; rapid =
`/v1/completions` streaming M=128, 2 reps). **tok/s decode | prefill@4096**:

| model (model_type, quant) | fp32 engine | bf16 engine | rapidMLX | outcome |
|---|---|---|---|---|
| Qwen3.6-27B-4bit (`qwen3_5`, affine 4/64) | 25,3 | **31,5** | 28 \| 680 | bf16 **promoted to default** (`74bd7a61e`): 1,245×, **1,13× vs rapid** |
| Qwen3.8-27B-8bit (`qwen3_5`, affine 8/64) | 13,9 | **16,7** | 15-16 \| 586 | bf16 **promoted to default** (`a496b9854`): 1,20×, **1,07× vs rapid** |
| DeepSeek-R1-32B-8bit (`qwen2`, 8/64) | 10,3-11,9 | 10,5 (bf16 **inert**) | 15-16 \| 326 | **0,67× vs rapid — open gap, and it is not the dtype** |

The quality gate that authorized the two promotions (teacher forcing on real text, 600
positions, arms in the same process, alternating order): loss nats/token and top-1 against the human
text are **on par or better in bf16** — 4-bit `0,6711/324` fp32 → `0,6677/326` bf16; 8-bit
`0,6877/325` → `0,6872/327`; no `P(argmax)>1`. This last column, until yesterday, was a **false**
reading: `mlx_array_data_float32` in mlx-c is a reinterpret cast, not a conversion
(`2167d7458`) — the same cause of the SIGSEGV in sampling with bf16 active.

### Hypotheses EXCLUDED by measurement for the DeepSeek-32B residual (not in words)

- **bf16**: 1,03× on `qwen2` (inert) versus 1,20-1,245× on `qwen3_5` → the win is arch-dependent.
- **`GMB_GU_FUSE`** (gate+up in a single GEMV): 1,02× = noise.
- **`GMB_GEMVQ4`** (custom Metal kernel of the «614 GB/s proposal»): **zero** on 4-bit (31,3-31,5
  identical, identical IDS) — there mlx's `qmv_fast` is already at ~505 GB/s.
- **KV/concat bottleneck**: decoding is **flat from ctx 64 to 4096** (9,0 → 7,7 tok/s), and
  `GMB_PHASE` splits the step into attn 27 ms + ffn 80 ms (the FFN is 75%).
- **fixed cost per `generate`**: already falsified (G7, `f≈0,18 s`).

Probable residual: **bandwidth-efficiency of the FFN 8-bit GEMV on those shapes** (~360 GB/s
versus the ~490-505 GB/s reached by the same 8-bit on `qwen3_5`). Next honest step:
a micro-harness that measures gate/up (M=27648, K=5120) and down (M=5120, K=27648) **separately**, before
writing any Q8 kernel in `native/gmb_metal_gemv/` (which today contains ONLY
`gmb_metal_gemv_q4.h`). Phase A/B of this document (FIXKV/COMPILE) has nothing to do with this residual:
they are gated on `mixtral` and the defect here is inside the dense FFN.

Tools and logs of this session in `/tmp/gmb-l8-gate/` (`ab8.sh`, `ab9.sh`, `ab11.sh`,
`gate-stats.py`, `gate-*.log`); `test_mlx_bench` is gitignored, so the `score` and
`sampletext` modes exist only in the working tree.

## 2026-09-08 night — L19 CLOSED: the DeepSeek residual was the dtype, as a PAIR (x, scale)

The «Probable residual» paragraph above is **superseded by measurement**: the separate micro-harness
I suggested has been done (`/tmp/gmb-l19/gemv3.py`, real shapes read from the checkpoints, 16 ops per
eval, bandwidth canary at every measurement, 2 rounds alternating order) and **no Q8 kernel is needed**.

**And the diagnosis «the kernel candidate is `gate_proj`/`up_proj`, not `down_proj`» was aiming at the
wrong GEMV**: gate+up and down read the same number of bytes (27648×5120 in both directions), and with the
correct `(x, scale)` pair the two times **coincide** — `f16: gate 0,325 ms` vs `down 0,324 ms`
(ratio 0,997), while with fp32 `x` the pair was 0,393/0,415. The asymmetry being chased was to a
large extent an effect of the wrong dtype, not a property of the shape: a kernel written on
`gate+up` would have optimized the symptom. No permission on CMake was needed.

**Reconciliation with the in-engine measurement: PARTIAL, recorded honestly.** The separate session re-ran
`GMB_FFN_PHASE` with `bf16=f16` (same `l19-ffn-gemv-diag` branch rebased on
`f5722b617`): ratio **2,45**, not ~2,0 → it moves **in the predicted direction** (2,60 → 2,45) but
a residual asymmetry survives inside the real forward, where the isolated micro-harness does not
see intermediate sigmoid/multiply, LoRA lookups and allocations. How much that residual is worth is told by the
ceiling, not by the argument: with both engines at **508-515 GB/s out of ~520 measured**, the maximum
prize still available on DeepSeek decode is **~7%** (theoretical 546), and the same figure is the
ceiling of any new Q8 kernel. It is not «weeks of guaranteed work»: it is ≤7%, and
the closest experiment (GEMVQ4 on Q4) has already shown it is nil.

`mlx_quantized_matmul` (8-bit affine/64, B=1) takes the fast kernel **only if the dtype of `x`
matches the dtype of the checkpoint's `scales`**. Mean `t/step` per op:

| shape | x=fp32 | x=f16 | x=bf16 |
|---|---|---|---|
| DeepSeek gate `[27648×5120]` (scale **F16**) | 0,393 ms | **0,325** | 0,414 |
| DeepSeek down `[5120×27648]` | 0,415 | **0,324** | 0,405 |
| DeepSeek lm_head `[152064×5120]` | 1,924 | **1,534** | 2,164 |
| Qwen3.8 gate `[17408×5120]` (scale **BF16**) | 0,351 | 0,364 | **0,309** |

bf16-on-scale-f16 is the **worst** mix (below fp32): it is the only reading that reconciles the «+25%
per-op bf16» (Qwen3.8, right pair) with the «1,03× model-level bf16 on qwen2» (wrong pair)
already in the table. Survey of the scale dtypes (safetensors headers only, all local checkpoints):
**F16** → DeepSeek-R1-32B-8bit, Llama-3.2-1B-4bit, Mixtral-8x22B-4bit, Ternary-Bonsai-2bit;
**BF16** → GLM-4.5-Air, Qwen3-Coder-30B-8bit, Qwen3.5-27B-6bit, Qwen3.6-27B-4bit,
Qwen3.6-35B-A3B, Qwen3.8-27B-8bit, Qwen3.8-Flash-Next, gemma-4-31b-4bit, gpt-oss-20b;
**U8 (mxfp4)** → Coder-Next, Qwen3.5-122B-A10B, gpt-oss-120b.

**Code** (`LLMEnv/LLMEngine/mlx/gmb_host_ai_mlx.c`): the ~20 sites «16 bit = bf16» go
through a single accessor (`g_act16`, `gmb_mlx_act_dt()`, `gmb_mlx_is_act16()`, `gmb_mlx_act16_name()`);
`gmb_mlx_bf16_mode_parse` is the only point that decomposes a mode into the full state (two bits +
flavor) and `gmb_ai_mlx_model_set_bf16_mode` accepts `"f16"`/`"fp16"`/`"bf16"` — **no new
exported symbol, no ABI/C#/CMake change** (so no problem-L16 of the 23 copies).
The no-op short circuit also compares the flavor; legacy boolean setters and architectural
default pin bf16 (`g_act16` is a file global); `GMB_KVMETAL` guards corrected to
«not any 16 bit» (the Metal kernel assumes fp32).

**Model-level A/B on DeepSeek-R1-32B-8bit** (`decode 128`, 3 reps, alternating order, machine load
12,6 → 26,1: the absolute is not usable, only the ratio): median `off 9,3` | `bf16 9,5` = **1,02×**
| `f16 11,8` = **1,27×**, with f16 first in all three reps even in the busiest sample and **greedy
IDS of the 128 tokens identical across the three arms** (no fp16 overflow). In the `score` loop (lm_head
per position, hence a bigger GEMV) f16 is worth **1,52×** over off.
Known baseline in a quiet window: `off 11,5` tok/s → `11,5 × 1,27 ≈ 14,6` against the `14,32` of
rapidMLX measured the same day.

**Confirmed, no longer extrapolated — PARITY measured in the same window.** The machine has 128 GB
(the «quiet window» was not needed): the two engines alternate **by turns, never simultaneously** — two
bandwidth-bound workloads in parallel split the bandwidth and the ratio collapses to 1,0 by the law of
physics, not by merit. Correct rapid canon = **inter-token time of the same
streaming call** (`(n-1)/(t_ultimo - t_primo)`), engine canon = the decode-only stopwatch of
`test_mlx_bench decode 128`; `nchunk == completion_tokens == 128` verified at every measurement.

| window | `f16` engine (3 reps) | rapidMLX (3 reps) | anchor `off` |
|---|---|---|---|
| A (load 2,8–7,6) | 13,30 / 13,12 / 13,18 | 11,34\* / 13,39 / 13,34 | 10,59 |
| B (load 3,7–5,5) | **14,60** / 13,00 / 12,80 | **14,80** / 14,78 / 11,85\* | 9,00 |

\* = artifact to discard, not a number: in A the rapid rep1 is the first request on a server just
started (TTFT 10,76 s); in B the rapid rep3 has `win` 10,72 s versus 8,58 (external load spike not
seen by the 1-minute average). **The correct reading is the best adjacent pair: 14,60 versus
14,80 = 0,986×**, and in A 13,18 versus 13,365 = **0,986×**. The ratio is the same in both
windows. In bandwidth terms: `14,60 × 34,81 GB/token = 508 GB/s` versus `14,80 × 34,81 = 515
GB/s`, roofline canary ~520 → **both engines are at 97-99% of the machine ceiling**; the
DeepSeek gap goes from **0,80× to 0,98×** (it was 0,67× before the canon correction).

**Old canon, retracted:** `completion_tokens / (t_nonstream − TTFT_di_un'altra_chiamata)` — the
formula used until now — **inflates the number** when the second call already finds everything warm:
in the middle window it produced `19,30 tok/s = 672 GB/s`, physically impossible above the
ceiling. Every previous rapid number obtained this way must be considered an **upper bound**, not a
measurement. Tools: `/tmp/gmb-l19/ab-vs-rapid-f16.py` (log `ab-vs-rapid-f16.log` = old canon,
`ab-vs-rapid-f16b.log` = inter-token canon).

**Quality gate CLOSED** (`/tmp/gmb-l19/gate-ds-f16.log`, teacher forcing 600 positions × 3 reps
× 3 arms = 1800 steps per arm, machine load 16 → 30 during the run):

| arm | loss/token (nats) | top-1 | `score` tok/s (3 reps) | median |
|---|---|---|---|---|
| `off` (fp32) | **0,5862** | 936/1800 (52,0%) | 7,56 / 8,02 / 7,81 | 7,81 |
| `bf16` | **0,5860** | 936/1800 (52,0%) | 7,32 / 8,35 / 8,55 | 8,35 |
| `f16` | **0,5865** | 933/1800 (51,8%) | 11,49 / 11,59 / 11,69 | 11,59 |

Honest reading: `f16` pays **+0,0003 nats/token** (+0,05%, below the third significant digit) and
**3 positions out of 1800** of top-1 (0,17%) — and they are always the *same* 3, because greedy is
deterministic and the delta is identical in all three reps. Against **1,486×** of decode in the
`score` loop, where the dominant GEMV is `lm_head` (which in the micro-harness was worth 1,25×). It is not
«free»: it is a measured trade-off, and the price stays within the noise of any real use.
Second signal, not sought: the spread of `f16` is **11,49–11,69** (±0,9%) while `off` wanders
**7,56–8,55** (±6,5%) in the same shared window — consuming less bandwidth, `f16` is also less
sensitive to machine noise.

**APPLIED consequence (commit `d8ca31990`, same file): the flavor is chosen from the checkpoint.**
`gmb_mlx_scales_dtype(&m->weights, ...)` samples the dtype of the `.scales` of the first
quantized projection of layer 0 (multishard, `s.ctx != NULL` = triple actually quantized), and
`gmb_mlx_auto_pair_act_dtype(m, dtype_explicit)` applies it as **default** right after
`load_all_shards` — the earliest point where the dtype exists, because it lives in the safetensors and not
in the config. Precedence rules, all verified on a semi-silenced machine:

| model | what it prints | decode |
|---|---|---|
| DeepSeek-R1-32B-8bit, **no env** | `[bf16:auto pair scales=F16] mode=f16 dt=f16 embed=1 matmul=1` | **15,2 tok/s** (was 11,5 fp32) |
| DeepSeek, `GMB_BF16_DENSE=off` | `[bf16:env GMB_BF16_DENSE] fp32 explicit`, no auto line | 11,9 → ratio **1,28×** (the known 1,27) |
| Qwen3.6-27B-4bit (promoted to bf16) | `[bf16:default] mode=both dt=bf16`, no auto line | 31,3 = the promoted number, **unchanged** |
| Llama-3.2-1B-4bit | no bf16 line | unchanged (excluded from task #5) |

**Deliberate scope (not "all models with F16 scales"):** the automatic promotion is limited to
`model_type=qwen2` + `quant_mode=affine` + `quant_bits=8` + **F16** scales, i.e. exactly the
configuration with a green quality gate and measured parity vs rapid. The guard
`if (m->cfg.bfloat16_input || g_bf16_all) return;` puts **before** everything else the families
already promoted: `mixtral`/`qwen3_moe` have F16 scales and are fine in bf16 (+40-50%, GEMM batched),
so pairing them by analogy would be exactly the error documented in §Limits.
The kill-switch remains triple: `GMB_BF16_DENSE=off`, `mlxComputeDtype="off"` in the config, and
`gmb_ai_mlx_model_set_bf16_mode(handle,"off")` at runtime (the explicit always wins, and the auto does not
start if the host has already asked for something).

**Limits not to forget:** (a) the pair rule is measured on **dense GEMV B=1** —
`mixtral` is promoted to bf16 (+40-50%, measured) despite having F16 scales because the MoEs do batched GEMM:
no promotion must be extended by analogy, each family requires its own gate; (b) found and **closed
in the same commit**: `g_bf16_all`/`g_act16` were not zeroed in `gmb_ai_mlx_load_model`, so a
second model in the same process inherited the bits of the first — harmless while it was only a
per-architecture default, **not** harmless now that the automatic pairing reads that state to decide
(`if (m->cfg.bfloat16_input || g_bf16_all) return;`). Zeroed before the per-model default.
Tools: `/tmp/gmb-l19/{gemv3.py,ab-flavor-ds.sh,gate-ds-f16.log}`.

## 2026-09-08 late night — the comparison vs rapid ON A SECOND FAMILY, redone with the correct canon

It is needed because the number that was in this document (`Qwen3.6-27B-4bit`: engine **31,0** vs rapid
**28**, «1,11× ahead») was built on the **retracted** canon §above: that canon inflates rapid,
so «28» was a wrong upper bound and the declared advantage **was not measured**. New
acquisitions with the tool `/tmp/gmb-l19/ab-vs-rapid-family.py` (inter-token canon, validation
`nchunk == usage.completion_tokens`, 3 alternating reps, NEVER simultaneous), engine arm = **no
env** = the default promoted at load-time, which is what users really use:

| rep | order | load | engine (bf16 default) | rapid | ratio |
|---|---|---|---|---|---|
| 1 | eng → rapid | 13,2 / 13,0 | 17,60 | 16,97 (`ct=128 nchunk=128`) | 1,04× |
| 2 | rapid → eng | 12,4 / 14,1 | 19,60 | 14,88 (`ct=128 nchunk=128`) | 1,32× |
| 3 | eng → rapid | 17,0 / 16,5 | 20,60 | 16,44 (`ct=128 nchunk=128`) | 1,25× |

**Medians 19,60 vs 16,44 = 1,19×**; ratio per rep 1,04–1,32, median of the ratios **1,25×**.
`nchunk == ct` in all three: the canon holds. The direction (the engine is NOT behind on this
family) is **confirmed**; the precise value is not, and the absolute values are **¼ below** the quiet
window (31,3 → ~19,6) because the machine was shared (load 13→25 during the run, rapid server
of another AI active): on this machine only the ratio counts, never the tok/s.

**Asymmetry not to forget in this measurement:** the engine arm uses the `decode` mode of the
bench, which prefills **64 synthetic tokens**, while rapid received the real prompt of 1800 characters
(~450 tokens). The decode is **not flat** over that range (document §Excluded hypotheses: 9,0 → 7,7 tok/s
from ctx 64 to 4096, ≈−14%), so the ratio above is **biased by at most ~1,14×** in favour
of the engine. In figures: even removing all the possible bias, ~1,10× would remain — which brings
the number back to the order of magnitude of the previous «1,11×», but **from a measurement with a sane canon**,
not from a canon that inflated the comparison. The `GMB_BF16_DENSE=off` anchor (14,10) **is not
comparable**: it ran last at load 21,3, without a bf16 sibling adjacent to that load.

## 2026-09-08 22:33 — L21: the (x, scale) pair counts ALSO on PREFILL, and the promotion was already the shipped default

The «the major deficit now known is prefill» written above assumed that the 16-bit dtype
changed only the B=1 GEMV. **Measurement that disproves it** (`/tmp/gmb-l19/prefill-dtype.sh`, log
`prefill-dtype.log`): **real** prompt of ~9000 tokens (`docs/comparison/RMLX10-decode-compiled-plan.md`
+ the two corpora, tokenized by the model's `tokenizer.json`), arms **in the same process** in
alternating order (`score`, `nScored=8` because the scoring steps cost an extra `lm_head` and are not
relevant here), and the useful number is again the **ratio**.

DeepSeek-R1-32B-8bit, `prompt=9321` tokens, prefill time (s, 3 reps):

| arm | rep0 | rep1 | rep2 | median | tok/s | vs fp32 |
|---|---|---|---|---|---|---|
| `off` (fp32) | 46,20 | 45,24 | 44,55 | 45,24 | 206 | 1,00× |
| `bf16` | 41,42 | 48,58 | 47,76 | 47,76 | 195 | **0,95×** (worse) |
| `f16` | 25,97 | 27,11 | 31,54 | **27,11** | **344** | **1,67×** |

Qwen3.6-27B-4bit, `prompt=9019` tokens (same BF16 scale family, so the right flavor is bf16):

| arm | rep0 | rep1 | rep2 | median | tok/s | vs fp32 |
|---|---|---|---|---|---|---|
| `off` (fp32) | 26,25 | 26,79 | 26,02 | 26,25 | 344 | 1,00× |
| `both` (bf16) | 18,48 | 18,85 | 18,95 | **18,85** | **479** | **1,39×** |

**What follows, and why it matters more than the sum of the numbers:** (1) the pair rule **is not a
property of the decode**: on DeepSeek bf16 is the *wrong* flavor even at large M (0,95×, within
the noise but definitely not ahead) while f16 is worth **+67%** — more than the +28% of decode, so the
promotion of `d8ca31990` moved the **segment that had been indicated as the biggest
deficit**, without a line of new code; (2) the `+39%` on Qwen3.6 is the **same default** already
shipped (branch `qwen3_5`), which until now had been validated only on the decode: the promoted prefill is not
a user request, it is what runs; (3) `top1controTesto=3` on the 8 scored positions is **identical
in all 9 runs** (it is not the quality gate — that remains the 1800-position session — but
it is the sign that the prefill Δ is not moving the argmax on real text).

**Comparison vs rapidMLX, finished the same minute** (`/tmp/gmb-l19/prefill-vs-rapid.py`, log
`prefill-vs-rapid-{ds,qwen}.log`, canon declared per side: `ntok/prefill_s` the engine,
`prompt_tokens/ttft` rapid — which includes one decode step + tokenization + HTTP queue and
therefore **underestimates** rapid; `--disable-prefix-cache`, 3 alternating reps, NEVER simultaneous, real
prompt identical from ~26 KB → ~9000 tokens per side):

| family | N | engine (3 rep) | median | rapid (3 rep) | median | ratio |
|---|---|---|---|---|---|---|
| Qwen3.6-27B-4bit (`both`, **promoted default**) | 9019/9027 | 419,5 · 431,1 · 434,9 | **431,1** | 432,2 · 424,9 · 440,7 | **432,2** | **0,99×** |
| DeepSeek-R1-32B-8bit (`f16`, **promoted default**) | 9321/9330 | 304,0 · 155,1 · 296,4 | 296,4 | 298,8 · 329,6 | 314,2 | 0,94× (medians) / **1,03×** (paired reps 1 and 3) |

**Verdict: prefill is NOT a deficit.** On Qwen3.6 the measurement is clean (load 4-9, spread ±2% on
both sides) and says **parity**: 0,971 / 1,015 / 0,987 rep by rep. On DeepSeek the engine median
is **biased downward** by a load spike of 17,3 that fell only on the engine leg of rep2 (60,09 s
versus 30,66/31,45): excluding the two non-adjacent measurements, the paired reps 1 and 3 give **1,017× and
1,002×**, that is, parity again. So the «the major deficit now known is prefill (1,2-1,4× behind)»
written above — and the line `Qwen3.6-27B: prefill 185 vs rapid 425 = rapid 2,3×` in the cumulative
table of `GMBasic-vs-RapidMLX-Comparison.md` §13.3 — are **falsified by this measurement**: that
185 predated the bf16 promotion (and in any case that branch today runs at **431-479** tok/s).

**What remains open, after L19-L21, is no longer the dtype:** (a) the **MoE/mxfp4** families (the big
gap of §13.3: 122B-A10B decode 4,2×, gpt-oss-120b 4,1×), (b) **very-long-context decode** (N=16384:
«2,4×» on DeepSeek, §19 — never redone with the correct pair, and it is the only number of that
line that the automatic promotion does not touch by itself), (c) llama/bf16-correctness (task
#5). The medium-context dense is **paired or ahead on both segments**.
> **Updated by L22 (same evening):** point (b) has been redone on the engine side, and the result
> says that **it was not the dtype** — see L22 below. The (a) remains the bulk, the (b) changed
> target (KV, not GEMV).

## L22 — Decode at N=16384 on the paired dense: the O(pos) degradation is NOT dtype (2026-09-08, 23:15-23:20)

**Engine leg done, rapid leg NOT, and stated as such.** DeepSeek-R1-32B-8bit, 3 alternating reps
4096↔16384, fresh process at every run, `auto pair scales=F16 → mode=f16` **verified on stderr at
every load** (not inferred). Window load **8,2→11,6 climbing**, and at the end of the leg it
spiked to **18,6** with `test_ilrun` of another AI at 103% CPU + Rider + opencode: the ratio
vs rapid in this window **is not measurable** and the rapid leg was not launched (it would make
a ratio between two legs taken at different load — exactly the error already made and retracted in
§20/L20). The `vs rapid` at 16384 therefore remains open.

**But the internal curve holds without rapid, and it is the number that was needed** — the degradation
factor decode 4096→16384 is an **intra-engine ratio: same process, same window**, so
contention touches it much less than it touches the absolutes:

| condition | decode @4096 | decode @16384 | **degradation factor** |
|---|---|---|---|
| GMBasic **f16 paired** (this session, load 8-11) | 9,4 / 9,7 / 9,6 → **9,6** | 5,9 / 5,4 / 4,7 → **5,4** | **0,5625** |
| GMBasic **fp32, pre-pairing** (§19, load 3-6) | 9,6 | 5,5 | **0,573** |
| **rapidMLX** (§19, load 3-6) | 14,9 | 13,3 | **0,893** |

**Decomposition of the «2,4×» — this is the discovery, not the absolute number.** The §19 gap was
1,56× at 4096 and 2,42× at 16384: the **additional gap** of the long context is 2,42/1,56 = **1,55×**, and
the ratio of the two degradation factors is 0,893/0,5625 = **1,59×**. They are the same thing within
the noise: **all** the extra penalty at long context is the degradation rate, not a constant
handicap. From this follows a falsifiable and important prediction: since the paired dtype is a
**multiplier** that hits the two points almost equally (0,5625 vs 0,573: the promotion did not
move the factor), **no dtype lever can close the hole at 16K**. The target is the
KV/attention path (cache concat, `qwen2` attention), and the §21 line «DeepSeek @16384 →
never redone after the pair» must be read like this: redone, and the pair **has nothing to do with it**.

**Absolutes not comparable with §20, and it must be written**: 9,6 tok/s at 4096 against the 14,60 of §20 is
**-34%**, but §20 ran at load 4-6 and this one at 8-11 — it is the contention sensitivity already measured
(in-process engine penalized more than the server), not a regression introduced by `d8ca31990`.
None of the absolutes of this session must be put in a comparison table.

*Tools: `/tmp/gmb-l22/gmb-deepseek.log` (6 complete runs, dylib with auto-pair, verified
present with the marker `auto pair scales=F16` — the copy in `test_mlx_bench/bin/Release/` was
**stale** and I reloaded it before measuring: without that check this session would have
measured fp32 believing it was measuring f16).*
