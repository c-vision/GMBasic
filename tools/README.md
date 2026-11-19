# tools/ — single home of the monorepo tools

As of 2026-10-29 **all tools live here**. `GMBasic/tools/` and `GMBasicSharp/tools/` were removed:
they were partial copies, and the canonical folder at the root stayed empty.

## Structure

```
tools/
  basic/    gates and generators written in BASIC (49)   e.g. no_stub.bas, check_enum_ids.bas, gen_c_tokenkind.bas
  c/        support tools written in C (17)              e.g. gmb_fixture_stats.c, brain_manifest.c
  sh/       wrappers, harnesses and shell gates (86)     e.g. check_file_size.sh, parity4, test_minitests_c99native
  data/     data read by the tools (3)                   hf_causal_lm_models.txt, llm_arch_allowlist.tsv, base_commit.txt
  fixtures/ minimal fixtures for the parity tests (3)    *.bas + gmb_torch*.o
  audit/    gmb_audit: builtin coverage per backend      Program.cs (legacy project ref: see README)
  registry-export/  registry exporter (.NET)             Program.cs + update.sh
```

The parenthesised numbers are checked against the disk by `bash tools/sh/tools_verify.sh`
(group `readme:conteggi`): `basic/` → `*.bas`, `c/` → `*.c`, `sh/` → `*.sh` + the extensionless
harnesses (`parity4`, `test_minitests_c99native`), `data/` → files, `fixtures/` → `*.bas` (the
`gmb_torch*.o` are build artifacts, not fixtures). A folder that declares a number without a
counting rule fails the gate instead of passing silently.

## Rules

1. **A new tool goes here**, in the subfolder of its language. Never inside a project folder (`GMBasic/`, `GMBasicSharp/`): tooling belongs to the root.
2. **`basic/` and `c/` do not mix** (that was the flaw of the old `GMBasic/tools`: 26 `.bas` and 5 `.c` in the same dir).
3. **No output in `tools/`**: no `work/`, `out/`, `build/`, no `.o` or executable. Scratch goes to **`/tmp`**; a persistent output of a tool goes to **`tools/out/`**. `work/` is not used in tooling (it is reserved for the sample data artifacts).
4. **Shell scripts compute the root by themselves**: `$(dirname "$0")/../../GMBasic` (tools that talk to the compiler) or `../../GMBasicSharp` (test kit). Do not use `$(dirname "$0")/..`, which now points to the monorepo root.
5. **ABI**: the `.gmbarch` descriptors live in `LLMEnv/ABI` (sibling of the monorepo), referenced by the settings. No local copy.

## Documentation

Index with one note per tool: ``.
The launchers of the C# harnesses (whose projects stay in `GMBasicSharp/`) are in `sh/`; their
reference table is [`KIT-README.md`](KIT-README.md).

`docs/` at the root is **not** the documentation folder: it is reserved for the **printed manual**
and is currently empty (the Clipper 5.3 / controls reference moved to the vault, `Guide/ClipperLib-*`).
The **reports produced by the tools** are in [`reports/`](reports/):
`audit-backends.md`, `audit-stdlib.md`, `recon-stdlib.{md,json}`, `command_desc.tsv`, `manifests/`,
`gmb_env_audit.tsv` (`GMB_*` env audit). The monorepo documentation and the sync log live in the
vault (``); the sync watermark is
[`data/base_commit.txt`](data/base_commit.txt).

`audit/` e `registry-export/` sono progetti .NET **allineati al monorepo**: i `ProjectReference`
puntano a `GMBasicSharp/`, `ResolveRoot()`/`FindMonorepoRoot()` trovano la root da qualunque
directory. Verificato il 2026-11-01: build 0 errori, `audit --list-backends`/`--pcodevm`,
`registry-export --check` e `test_retired_flags.sh` (7/7) verdi.

## Quick invocation

```bash
gmb tools/basic/no_stub.bas --quiet            # anti-stub gate
bash tools/sh/check_file_size.sh --quiet       # 1200-line threshold + escape gate
bash tools/sh/check_option_registry.sh         # OptionCoreTable == g_opt_core[] (nomi, ordine, descrizioni)
tools/sh/parity4 <un-sample>.bas               # 4 legs on one sample
tools/sh/test_minitests_c99native              # C99Native leg
bash tools/sh/test_minitests_parity.sh --all   # 4 legs on all MiniTests
bash tools/sh/tools_verify.sh                  # syntax + C99Native + packages + Structure counts
bash tools/sh/vaultgate.sh --scope Guide/Tools  # vault: every note linked from an index (exit 1 on orphans)
bash tools/sh/brains_publish_doc.sh --check     # brains index (BRAINS.md) from the manifest
```

## Package cache and lib freshness

`~/.gmbasic/packages` is a **copy** of `packages/`. When the copy lags behind, the symptom is not
"old library" but errors that look like compiler bugs: an export reads as `non definito`, an old
`.inc` lives next to the new one and emission fails with `la procedura 'X' con N parametri e' gia'
definita`, samples run with semantics that differ from the repo. The same class comes from a `lib/`
never rebuilt after its C sources changed: the BASIC API is there, the symbol is not.

| tool | one-line use | what it does |
|---|---|---|
| `sh/gmb_pkg_cache_sync.sh` | `bash tools/sh/gmb_pkg_cache_sync.sh [--check] [--rebuild] [nome...]` | verified copy workspace → cache (`diff -r`, backup, idempotent); `--check` exits 1 on drift; `--rebuild` runs `packages/<n>/<v>/build.sh` on every lib `gmb_pkg_libcheck.sh` flags stale, then copies |
| `sh/gmb_pkg_libcheck.sh` | `bash tools/sh/gmb_pkg_libcheck.sh [--quiet] [nome...]` | flags every `lib/<rid>/*` older than `src/*.c`/`include/*.h` (exit 1); `NOLIB` = sources without a built lib, not an error |
| `sh/gmb_pkg_cache_hook.sh` | `bash tools/sh/gmb_pkg_cache_hook.sh install\|uninstall\|status\|run` | pre-commit hook block: on a commit touching `packages/` it runs the two checks above on the touched packages only (exit 1 + the fix command) |

The order **build → copy** does not have to be remembered: `bash tools/sh/gmb_pkg_cache_sync.sh
--rebuild` rebuilds the stale libs first and copies after; a failed build stops it with exit 2
and the last lines of the log, instead of copying an old lib.

Both checks are also groups of `tools/sh/tools_verify.sh` (`cache:pacchetti`, `dylib:pacchetti`),
and the hook is installed in `.git/hooks/pre-commit` as a marked block (`install` keeps the rest of
the hook, e.g. `no_python_guard`). Hook and gate call the same two tools: one implementation each.

**Brains**: the `.gmbrain.zip` packages are **release assets** of `c-vision/GMBasicBrains`
(never in the tree: `trading` is ~600 MB zipped, beyond the 100 MB per-file limit). The tree of that
repo contains only the **metadata**, generated from the single source `shared/settings/brains-manifest.json`:
`tools/sh/brains_publish_doc.sh` writes `BRAINS.md` (table with `rag`/`graph` shapes, release,
sha256, size) and `README.md`, then commits and pushes (`--check` shows the diff without touching
the repo).
