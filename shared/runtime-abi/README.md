# runtime-abi — C → Sharp ABI contract

ABI version: **`ABI_VERSION` = 1.0.0**. Gate: `./runtime-abi/check_all.sh`.

| `*.symbols` | Dylib | Verified |
|---|---|---|
| `gmb_db` | `libgmb_db.dylib` | **51/51** |
| `gmb_gfx` | `libgmb_gfx.dylib` | **385/385** |
| `gmb_ai_llm` | `libgmb_ai_llm.dylib` | **61/61** |
| `gmb_ai_modelio` | `libgmb_ai_modelio.dylib` | **4/4** |
| `gmb_ai_mlx` | `libgmb_ai_mlx.dylib` | **40/40** |
| `gmb_ai_finetune` | `libgmb_ai_finetune.dylib` | **5/5** |
| `llm_ai_llamacpp` | `libllm_ai_llamacpp.dylib` | **27/27** |
| `llmtoken` | `libllmtoken.dylib` | **6/6** |
| `gmb_torch_loader` | `libgmb_torch_loader.dylib` | **9/9** |
| `gmb_compress` | `libgmb_compress.dylib` | **3/3** |
| `gmb_grep` | `libgmb_grep.dylib` | **8/8** |
| `gmb_pkg` | `libgmb_pkg.dylib` | **128/128** |
| `llmengine_moe_stream` | `libllmengine_moe_stream.dylib` | **20/20** |

`check_abi.c` does `dlopen` + `dlsym`; exit 1 if a symbol is missing, 2 if the dylib cannot be
opened.

`check_all.sh` **looks for the dylibs where the loader actually looks** (2026-10-31):

1. `<monorepo>/GMBasic/build/libs/<rid>/` — local build of the checkout (wins in development);
2. `~/.gmbasic/libs/<rid>/` — user cache (published bundles);
3. `~/.gmbasic/libs/ml/<rid>/` — ML components (torch loader);
4. `<monorepo>/GMBasic/native/gmb_db_c/build/` — `gmb_db`.

A library that is **expected and missing** is an **error** (`--allow-missing` downgrades it to
`SKIP`). Previously the script looked in `GMBasic/Resources/libs` and `GMBasic/libs-runtime`
(folders that no longer exist): 12 of 13 libs came out as SKIP and it still printed «ABI: OK», so
it verified nothing — which is how the symbol drift (brains and fixtures not listed) went
unnoticed.

**Unchanged rule** (`LICENSING.md`): every new `P/Invoke` must be added to the matching
`<lib>.symbols` (and the gate must be run when the C ↔ C# boundary is touched).

## Open

- `gmb_tray` / `llm_vision_apple` are x64-only on arm hosts: gate on an x64 runner.
- The private/PUL dylibs (`gmb_ai_llm`) stay in the private C repo; public Sharp = source only.
