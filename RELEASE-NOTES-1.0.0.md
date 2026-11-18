# GMBasic 1.0.0 — release notes

**Date:** 2026-11-18 · **RID:** `osx-arm64` (Apple Silicon) · **License:** see `LICENSING.md`

First **compiled** GMBasic release: one archive, no sources to build at install time, no .NET
requirement.

| item | value |
|---|---|
| archive | `gmbasic-1.0.0-osx-arm64.zip` |
| size | 59 MB (1343 files, ~155 MB unpacked) |
| integrity | `gmbasic-1.0.0-osx-arm64.zip.sha256` + `release-manifest.json` (sha256 of **every** file) |
| verification | `gmb doctor --verify` → 1343 OK, 0 tampered, 0 missing |

## Install

```sh
unzip gmbasic-1.0.0-osx-arm64.zip
cd gmbasic-1.0.0-osx-arm64
export PATH="$PWD/bin:$PATH"

gmb doctor              # environment: 18/18 ok
gmb doctor --verify     # checks EVERY file against the manifest
gmb eval "PRINT 40+2"   # 42
```

For a program:

```sh
gmb hello.bas                      # build + run (shortcut)
gmb build --project <dir> --entry <file.bas>
```

## What is inside

- **`bin/`** — 14 executables: `gmb` (compiler/CLI), `gmbnative` (C99Native), `gmbchunk`
  (A1/GMBC bytecode), `gmb_vm_host` (VM/C), `gmbadmin` (pkg/libs/brain/doctor), `gmbrepl`
  (C REPL), `gmbnotebook`, `gmblsp`, `gmblibs`, `gmbfetch`, `gmbbrains`, `gmbfixtures`,
  `gmbtheme`, `gmbimgdiff`.
- **`GMBasic/native/`** — headers and static/dynamic libraries for the shared runtime, the
  compiler, the database engines (ISAM/DBF/SPEED/LEDGER), chart/doc/torch/agent.
- **`shared/`** — `include/` (FFI `.inc` files), `Controls/`, `settings/` (download manifests).
- **`LLMEnv/`** — local LLM engine (**Apache-2.0**): codec/engine/desc/token headers,
  `.gmbarch` descriptors in `ABI/`, `LICENSE` and `NOTICE`.

## Requirements

- **macOS on Apple Silicon** (this RID).
- **`cc`** = Xcode Command Line Tools: `gmb build` needs it to compile the emitted C.
  `gmb doctor` verifies it.
- **Network** only for on-demand downloads (`gmb libs ensure`, `gmb fixtures ensure`,
  `gmb brain install`) — **sha256 verified** before extraction.
- **No .NET**: compilers and runtimes are C.

## What is NOT in this release

- **`gmbcs` / `--engine csharp`** (C# compiler): not included. The engine of this release is
  the **C branch** (`gmb`, C99Native, VM/C). `gmb doctor` reports it as **optional**, so 18/18
  stays green.
- **RID `osx-x64`, `linux-x64`, `linux-arm64`, `win-*`**: this artifact is `osx-arm64` only.
  The cross toolchain (`zig`) and the manifests exist, but this release publishes **one RID**.
- **Samples and MiniTests**: the corpus (`shared/minitests`, `shared/samples`) lives in the
  repository, not in the archive; market feeds are fetched with `gmb fixtures ensure`.

## Verifications run on the artifact

Executed on the **zip unpacked into a temporary directory**, with `LLMDESC_ABI_DIR`/`LLM_ROOT`
removed from the environment (no path from the development checkout in use):

| check | result |
|---|---|
| `shasum -c *.sha256` | OK (also re-downloaded from the release) |
| `gmb doctor` | **18/18 ok** |
| `gmb doctor --verify` | **1343 OK**, 0 tampered, 0 missing |
| `gmb eval "PRINT 40+2"` | `42` |
| `gmb Hello.bas` (textual program) | compiles, links, runs (`hello 1.0`, `42`) |
| `IMPORT AI` + `gmb build` | 2 sources, **0 errors** (complete LLMEnv headers) |
| `otool -l bin/*` | **0** absolute paths (`@loader_path` rpath) |

## Test corpus state at this tag

Full sweep on the **native** leg (C99Native, the reference):

```
C99Native MiniTests: PASS=2128 FAIL=0 SKIP=264
```

Every SKIP is declared: `gen_AI_*` depending on machine state (222), `TAG: heavy` requiring a
real model (30), `TAG: interactive` (12).

Cross-leg over the **three managed legs** (IL, PCodeVM, VM-C), one process per domain:
**122 of 125 domains green**. The remaining 3 need environment dependencies
(`llm/` ×4 requires a model, `train/` the training registry, `tray/` the main-thread pump).

## Licenses

- Runtime, compilers, tools: **PUL-1.1** (see `LICENSING.md`).
- `LLMEnv/` (LLM engine): **Apache-2.0**, with `LICENSE` and `NOTICE` included.
- Third-party components: see `LICENSING.md` and the individual `NOTICE` files.
