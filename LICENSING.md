# Licensing / ownership (F3)

> **The repository is distributed under the PERSONAL USE LICENSE (PUL) v1.1** — see
> [`LICENSE`](LICENSE). **No activation key, no online check, no trial, no dev-mode switch**:
> the software runs as-is.
>
> The table below describes the **planned split** of the code base that has to keep the
> `LLMEnv` engine reusable under **Apache-2.0** (the engine is a separate, public repository);
> entries marked *(proposal)* are not the current distribution license of this repository.

Decided boundary (see ADR `ADR-Confine-Licenza-Engine-Apache-Training-Quant-PUL`):

| Area | License | Notes |
|---|---|---|
| `GMBasic/native/gmb_runtime_c`, `gmb_compiler_c`, `gmb_vm_c`, `gmb_db_c`, `gmb_c99native`, `gmb_chart`, `gmb_doc_c`, `gmb_graph`, `gmb_gfx` | **Apache-2.0** | runtime, VM, compiler, DB: execution only |
| `GMBasic/gmbllm/` | **PUL v1.1** | produces/transforms weights: model compiler, quant encode, training, LoRA |
| `GMBasicSharp/` (frontend, IL, PCode, Executor, REPL, BRT, CommonLib) | **Apache-2.0** (proposal) | no code that produces weights |
| `shared/` (samples, minitests, include, settings, fixtures) | **Apache-2.0** (code) / **CC-BY-4.0** (docs) | corpus and assets |
| `shared/spec/`, `shared/conformance/`, `shared/runtime-abi/` | **Apache-2.0** | contract, runner, ABI |

Rules:
- **One-way** dependencies: `gmbllm` (PUL) links the Apache libs; **never** Apache → PUL.
- `LLMEnv`/`FastLLM` are Apache and stay inference-complete (load/run); everything that
  produces/transforms weights lives in `gmbllm` (PUL).
- `gmbasic-c` stays **private** until `gmbllm` (PUL) is split into a dedicated repo.
- `runtime-abi/ABI_VERSION` is the semver reference of the C→Sharp ABI; CI pins this value.

## Note F5 (2026-10-25)

Decision: **GMBasicSharp stays a C# frontend that uses the C dylibs via P/Invoke**; no C#
reimplementation of the runtime/UI modules. Consequences:

- `gmbasic-sharp` can be public as **Apache source**, but running the native functions requires
  the C runtime (repo `gmbasic-c`, private as long as `gmbllm` is PUL).
- `libgmb_ai_llm` includes `gmbllm` code (**PUL**): the model-compiler/quant/training functions
  require the PUL license even when invoked from C#. Inference-only functions stay Apache.
- The `runtime-abi/` contract is the Sharp↔C compatibility guarantee; every new P/Invoke must be
  added to the symbols and to `check_all.sh`.
