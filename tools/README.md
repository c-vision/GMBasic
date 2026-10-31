# tools/ — sede unica dei tool del monorepo

Da 2026-10-29 **tutti i tool vivono qui**. `GMBasic/tools/` e `GMBasicSharp/tools/` sono state rimosse:
erano copie parziali, e la cartella canonica alla radice restava vuota.

## Struttura

```
tools/
  basic/    gate e generatori scritti in BASIC (28)      es. no_stub.bas, check_enum_ids.bas, gen_c_tokenkind.bas
  c/        tool di supporto scritti in C (5)            es. gmb_fixture_stats.c, brain_manifest.c
sh/       wrapper, harness e gate shell (27)           es. check_file_size.sh, parity4, test_minitests_c99native
data/     dati letti dai tool                          hf_causal_lm_models.txt, llm_arch_allowlist.tsv, base_commit.txt
  fixtures/ fixture minime per i test di parity          *.bas + gmb_torch*.o
  audit/    gmb_audit: copertura builtin per backend     Program.cs (rif. di progetto legacy: vedi README)
  registry-export/  esportatore del registro (.NET)      Program.cs + update.sh
```

## Regole

1. **Un tool nuovo va qui**, nella sottocartella del suo linguaggio. Mai in un submodule.
2. **`basic/` e `c/` non si mescolano** (era il difetto della vecchia `GMBasic/tools`: 26 `.bas` e 5 `.c` nella stessa dir).
3. **Niente output in `tools/`**: nessun `work/`, `out/`, `build/`, nessun `.o` o eseguibile. Lo scratch va in **`/tmp`**; un output persistente di un tool va in **`tools/out/`**. `work/` non si usa nel tooling (è riservata agli artefatti-dati dei sample).
4. **Gli script shell calcolano la radice da soli**: `$(dirname "$0")/../../GMBasic` (tool che parlano col compilatore) o `../../GMBasicSharp` (kit di test). Non usare `$(dirname "$0")/..`, che ora punta alla radice del monorepo.
5. **ABI**: i descrittori `.gmbarch` stanno in `LLMEnv/ABI` (sibling del monorepo), puntati dai settings. Nessuna copia locale.

## Documentazione

Indice con una nota per tool: `AiWiki/01-Projects/GMBasic/Guide/Tools/Tools-Indice.md`.
I launcher degli harness C# (i cui progetti restano in `GMBasicSharp/`) sono in `sh/`; la loro
tabella di riferimento è [`KIT-README.md`](KIT-README.md).

La documentazione del monorepo (non dei tool) sta in **una sola** cartella: `docs/` alla radice,
indice in [`docs/README.md`](../docs/README.md). Non esistono `docs/` annidate (`shared/docs`,
`GMBasic/docs`, `ClipperLib/docs` sono state consolidate il 2026-10-29). Dentro `docs/`:
`Sync-GMBasic-Monorepo.md` (log di sync, watermark in [`data/base_commit.txt`](data/base_commit.txt)),
`gmb_env_audit.tsv` (audit env `GMB_*`), `comparison/` (report e manifest dell'audit),
`ClipperLib/` (riferimento Clipper 5.3 e controlli).

`audit/` e `registry-export/` sono progetti .NET con **riferimenti di progetto legacy**
(`../../common/...`, `GMBASIC.sln`): vanno ripuntati a `GMBasicSharp/` prima di eseguirli. Vedi
 [`audit/README.md`](audit/README.md).

## Invocazione rapida

```bash
gmb tools/basic/no_stub.bas --quiet            # gate anti-stub
bash tools/sh/check_file_size.sh --quiet       # soglia 1200 righe + gate escape
tools/sh/parity4 <un-sample>.bas               # 4 gambe su un sample
tools/sh/test_minitests_c99native              # gamba C99Native
bash tools/sh/test_minitests_parity.sh --all   # 4 gambe su tutti i MiniTest
bash tools/sh/tools_verify.sh                  # sintassi di tools/ + build C99Native
```
