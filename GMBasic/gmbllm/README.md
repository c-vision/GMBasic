# gmbllm — parte LLM non-Apache di GMBasic (licenza **PUL**)

> **Cos'è.** `gmbllm` raccoglie tutto il codice LLM che **PRODUCE o ALTERA i pesi** di un modello
> — training, quantizzazione (encode/PTQ), surgery/planning — cioè il **model compiler**. Vive
> **dentro GMBasic** ed è coperto dalla **PERSONAL USE LICENSE (PUL)** di GMBasic.

## Perché è separato

`LLMEngine` e `FastLLM` sono rilasciati **open source Apache-2.0** (in `LLMEnv`). Il codice Apache
deve restare **inference-complete e pulito**: nessuna funzione che produce/trasforma pesi.
`gmbllm` è la parte **proprietaria** (PUL) che consuma l'engine Apache.

Dipendenza **monodirezionale**:

```
gmbllm (PUL)  ──linka──▶  LLMEnv: llmcodec / llmtoken / llmdesc / llmengine_plugin  (Apache-2.0)
                          llm_ai_mlx (engine inferenza)                                 (Apache-2.0)
```

**Mai** il contrario (`Apache → PUL` è vietato). Gate di verifica:
`LLMEnv/tools/gmb_license_gate.py` (deve uscire 0).

## Cosa contiene (crescita a fasi)

| Fase | Contenuto | Origine |
|---|---|---|
| **F2** | model compiler / surgery / planning (`llmmodelio_*`) | ex `LLMEnv/LLMModelIO` |
| **F3** | quant-**encode** (`*_quant_row`, `iq*_quantize`, `moe_affine_ln`, plugin `a_encode`) | ex `LLMEnv/LLMCodec` |
| **F4** | **training** (loss/grad/DPO/AdamW/ckpt) | ex `LLMEngine/mlx/llm_mlx_{train,lora,dpo,adamw}` |
| **F5** | **converter** MLX↔GGUF (`llmcodec_gguf_mlx.c` + header, i 12 self-check e `gguf_convert.c`) | ex `LLMEnv/LLMCodec` |

Il **dequant** (per *eseguire* un modello quantizzato), il **forward differenziabile** (primitivo
di inferenza), i **container** (read/write GGUF/safetensors) e l'**encode condiviso** usato
dall'engine a runtime (`llmcodec_ggml_quant_row`, allowlist *actquant*) **restano Apache** in
`LLMEnv`: senza di essi l'engine non gira. Il converter PUL li **chiama** (direzione PUL→Apache).

## Build

Compilato come sottoprogetto di GMBasic (`native/gmb_runtime_c/CMakeLists.txt`, `add_subdirectory`).
Target CMake: **`gmbllm`**. È linkato da `gmb_runtime` e dall'aggregato `gmb_ai_llm`, che espone la
superficie ai backend C# via P/Invoke.

## Licenza

**PERSONAL USE LICENSE (PUL) v1.1** — vedi `LICENSE`. Il codice Apache-2.0 di `LLMEnv` mantiene la
sua licenza (Apache-2.0) anche quando linkato da qui.
