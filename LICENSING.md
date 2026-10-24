# Licensing / ownership (F3)

Confine deciso (vedi ADR `ADR-Confine-Licenza-Engine-Apache-Training-Quant-PUL`):

| Area | Licenza | Note |
|---|---|---|
| `GMBasic/native/gmb_runtime_c`, `gmb_compiler_c`, `gmb_vm_c`, `gmb_db_c`, `gmb_c99native`, `gmb_chart`, `gmb_doc_c`, `gmb_graph`, `gmb_gfx` | **Apache-2.0** | runtime, VM, compiler, DB: solo esecuzione |
| `GMBasic/gmbllm/` | **PUL v1.1** | produce/trasforma pesi: model compiler, quant encode, training, LoRA |
| `GMBasicSharp/` (frontend, IL, PCode, Executor, REPL, BRT, CommonLib) | **Apache-2.0** (proposta) | nessun codice che produce pesi |
| `shared/` (samples, minitests, include, settings, fixtures) | **Apache-2.0** (codice) / **CC-BY-4.0** (doc) | corpus e asset |
| `shared/spec/`, `shared/conformance/`, `shared/runtime-abi/` | **Apache-2.0** | contratto, runner, ABI |

Regole:
- Dipendenze **monodirezionali**: `gmbllm` (PUL) linka le lib Apache; **mai** Apache → PUL.
- `LLMEnv`/`FastLLM` sono Apache e restano inference-complete (load/run); tutto ciò che
  produce/trasforma pesi vive in `gmbllm` (PUL).
- `gmbasic-c` resta **privato** finché `gmbllm` (PUL) non è separato in un repo dedicato.
- `runtime-abi/ABI_VERSION` è il riferimento semver dell'ABI C→Sharp; la CI pinna questo valore.

## Nota F5 (2026-10-25)

Decisione: **GMBasicSharp resta un frontend C# che usa le dylib C via P/Invoke**; nessuna
reimplementazione C# dei moduli runtime/UI. Conseguenze:

- `gmbasic-sharp` può essere pubblico come **sorgente Apache**, ma per eseguire le funzioni
  native serve il runtime C (repo `gmbasic-c`, privato finché `gmbllm` è PUL).
- `libgmb_ai_llm` include codice `gmbllm` (**PUL**): le funzioni di model-compiler/quant/training
  richiedono licenza PUL anche se invocate da C#. Le funzioni di sola inferenza restano Apache.
- Il contratto `runtime-abi/` è la garanzia di compatibilità Sharp↔C; ogni nuovo P/Invoke va
  aggiunto ai symbols e a `check_all.sh`.
