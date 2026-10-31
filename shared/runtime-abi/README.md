# runtime-abi — contratto ABI C → Sharp

Versione ABI: **`ABI_VERSION` = 1.0.0**. Gate: `./runtime-abi/check_all.sh`.

| `*.symbols` | Dylib | Verifica |
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

`check_abi.c` fa `dlopen` + `dlsym`; exit 1 se manca un simbolo, 2 se la dylib non si apre.

`check_all.sh` **cerca le dylib dove il loader le cerca davvero** (2026-10-31):

1. `<monorepo>/GMBasic/build/libs/<rid>/` — build locale del checkout (vince in sviluppo);
2. `~/.gmbasic/libs/<rid>/` — cache utente (bundle pubblicati);
3. `~/.gmbasic/libs/ml/<rid>/` — componenti ML (torch loader);
4. `<monorepo>/GMBasic/native/gmb_db_c/build/` — `gmb_db`.

Una lib **attesa e assente** è un **errore** (`--allow-missing` la declassa a `SKIP`). Prima lo
script cercava in `GMBasic/Resources/libs` e `GMBasic/libs-runtime` (cartelle soppresse): 12 lib
su 13 risultavano SKIP e stampava comunque «ABI: OK», quindi non controllava nulla — ed è così
che il drift dei simboli (cervelli e fixture non elencati) è passato inosservato.

**Regola invariata** (`LICENSING.md`): ogni nuovo `P/Invoke` va aggiunto al `<lib>.symbols`
corrispondente (e il gate va eseguito quando si tocca il confine C ↔ C#).

## Aperto

- `gmb_tray` / `llm_vision_apple` sono x64-only su host arm: gate su runner x64.
- Le dylib private/PUL (`gmb_ai_llm`) restano nel repo C privato; Sharp pubblico = sorgente.
