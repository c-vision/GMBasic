# tools/ — single home of the monorepo tools

As of 2026-10-29 **all tools live here**. `GMBasic/tools/` and `GMBasicSharp/tools/` were removed:
they were partial copies, and the canonical folder at the root stayed empty.

## Structure

```
tools/
  basic/    gates and generators written in BASIC (28)   e.g. no_stub.bas, check_enum_ids.bas, gen_c_tokenkind.bas
  c/        support tools written in C (5)               e.g. gmb_fixture_stats.c, brain_manifest.c
sh/       wrappers, harnesses and shell gates (27)      e.g. check_file_size.sh, parity4, test_minitests_c99native
data/     data read by the tools                       hf_causal_lm_models.txt, llm_arch_allowlist.tsv, base_commit.txt
  fixtures/ minimal fixtures for the parity tests         *.bas + gmb_torch*.o
  audit/    gmb_audit: builtin coverage per backend      Program.cs (legacy project ref: see README)
  registry-export/  registry exporter (.NET)             Program.cs + update.sh
```

## Rules

1. **A new tool goes here**, in the subfolder of its language. Never inside a project folder (`GMBasic/`, `GMBasicSharp/`): tooling belongs to the root.
2. **`basic/` and `c/` do not mix** (that was the flaw of the old `GMBasic/tools`: 26 `.bas` and 5 `.c` in the same dir).
3. **No output in `tools/`**: no `work/`, `out/`, `build/`, no `.o` or executable. Scratch goes to **`/tmp`**; a persistent output of a tool goes to **`tools/out/`**. `work/` is not used in tooling (it is reserved for the sample data artifacts).
4. **Shell scripts compute the root by themselves**: `$(dirname "$0")/../../GMBasic` (tools that talk to the compiler) or `../../GMBasicSharp` (test kit). Do not use `$(dirname "$0")/..`, which now points to the monorepo root.
5. **ABI**: the `.gmbarch` descriptors live in `LLMEnv/ABI` (sibling of the monorepo), referenced by the settings. No local copy.

## Documentation

Index with one note per tool: `AiWiki/01-Projects/GMBasic/Guide/Tools/Tools-Indice.md`.
The launchers of the C# harnesses (whose projects stay in `GMBasicSharp/`) are in `sh/`; their
reference table is [`KIT-README.md`](KIT-README.md).

`docs/` at the root is **not** the documentation folder: it is reserved for the **printed manual**
and is currently empty (the Clipper 5.3 / controls reference moved to the vault, `Guide/ClipperLib-*`).
The **reports produced by the tools** are in [`reports/`](reports/):
`audit-backends.md`, `audit-stdlib.md`, `recon-stdlib.{md,json}`, `command_desc.tsv`, `manifests/`,
`gmb_env_audit.tsv` (`GMB_*` env audit). The monorepo documentation and the sync log live in the
vault (`AiWiki/01-Projects/GMBasicMonoRepo`); the sync watermark is
[`data/base_commit.txt`](data/base_commit.txt).

`audit/` e `registry-export/` sono progetti .NET **allineati al monorepo**: i `ProjectReference`
puntano a `GMBasicSharp/`, `ResolveRoot()`/`FindMonorepoRoot()` trovano la root da qualunque
directory. Verificato il 2026-11-01: build 0 errori, `audit --list-backends`/`--pcodevm`,
`registry-export --check` e `test_retired_flags.sh` (7/7) verdi.

## Quick invocation

```bash
gmb tools/basic/no_stub.bas --quiet            # anti-stub gate
bash tools/sh/check_file_size.sh --quiet       # 1200-line threshold + escape gate
tools/sh/parity4 <un-sample>.bas               # 4 legs on one sample
tools/sh/test_minitests_c99native              # C99Native leg
bash tools/sh/test_minitests_parity.sh --all   # 4 legs on all MiniTests
bash tools/sh/tools_verify.sh                  # tools/ syntax + C99Native build
bash tools/sh/brains_publish_doc.sh --check     # brains index (BRAINS.md) from the manifest
```

**Brains**: the `.gmbrain.zip` packages are **release assets** of `c-vision/GMBasicBrains`
(never in the tree: `trading` is ~600 MB zipped, beyond the 100 MB per-file limit). The tree of that
repo contains only the **metadata**, generated from the single source `shared/settings/brains-manifest.json`:
`tools/sh/brains_publish_doc.sh` writes `BRAINS.md` (table with `rag`/`graph` shapes, release,
sha256, size) and `README.md`, then commits and pushes (`--check` shows the diff without touching
the repo).
