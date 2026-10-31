# gmbllm — the non-Apache LLM part of GMBasic (**PUL** license)

> **What it is.** `gmbllm` collects all the LLM code that **PRODUCES or ALTERS model weights** —
> training, quantization (encode/PTQ), surgery/planning — i.e. the **model compiler**. It lives
> **inside GMBasic** and is covered by the GMBasic **PERSONAL USE LICENSE (PUL)**.

## Why it is separate

`LLMEngine` and `FastLLM` are released **open source, Apache-2.0** (in `LLMEnv`). The Apache code
must stay **inference-complete and clean**: no function that produces/transforms weights.
`gmbllm` is the **proprietary** (PUL) part that consumes the Apache engine.

**One-way** dependency:

```
gmbllm (PUL)  ──links──▶  LLMEnv: llmcodec / llmtoken / llmdesc / llmengine_plugin  (Apache-2.0)
                          llm_ai_mlx (inference engine)                              (Apache-2.0)
```

**Never** the other way round (`Apache → PUL` is forbidden). Verification gate:
`LLMEnv/tools/gmb_license_gate.py` (must exit 0).

## What it contains (phased growth)

| Phase | Content | Origin |
|---|---|---|
| **F2** | model compiler / surgery / planning (`llmmodelio_*`) | former `LLMEnv/LLMModelIO` |
| **F3** | quant-**encode** (`*_quant_row`, `iq*_quantize`, `moe_affine_ln`, `a_encode` plugin) | former `LLMEnv/LLMCodec` |
| **F4** | **training** (loss/grad/DPO/AdamW/ckpt) | former `LLMEngine/mlx/llm_mlx_{train,lora,dpo,adamw}` |
| **F5** | MLX↔GGUF **converter** (`llmcodec_gguf_mlx.c` + headers, the 12 self-checks and `gguf_convert.c`) | former `LLMEnv/LLMCodec` |

The **dequant** (needed to *run* a quantized model), the **differentiable forward** (inference
primitive), the **containers** (GGUF/safetensors read/write) and the **shared encode** used by the
runtime engine (`llmcodec_ggml_quant_row`, *actquant* allowlist) **stay Apache** in `LLMEnv`:
without them the engine does not run. The PUL converter **calls** them (PUL→Apache direction).

## Build

Built as a subproject of GMBasic (`native/gmb_runtime_c/CMakeLists.txt`, `add_subdirectory`).
CMake target: **`gmbllm`**. It is linked by `gmb_runtime` and by the `gmb_ai_llm` aggregate, which
exposes the surface to the C# backends via P/Invoke.

## License

**PERSONAL USE LICENSE (PUL) v1.1** — see `LICENSE`. The Apache-2.0 code of `LLMEnv` keeps its
license (Apache-2.0) even when linked from here.
