# Sync GMBasicMonoRepo

> [!NOTE] **HISTORICAL document** (sync 2026-10-26/28). The paths cited reflect the state at that time
> (`tests-mixed`, `migration/tools-unclassified`, `GMBASIC.sln`, …). Current structure and paths:
> `Knowledge/Struttura-Monorepo` (vault) and `docs/README.md`.

SYNC GMBasic → GMBasicMonoRepo

> **Structure updated 2026-10-29.** The `migration/` folder **no longer exists**: this log
> is in `docs/Sync-GMBasic-Monorepo.md` and the sync watermark in `tools/data/base_commit.txt`.
> All the tools live in `tools/` at the root (`basic/`, `c/`, `sh/`, `data/`, `fixtures/`,
> `registry-export/`); `migration/tools-unclassified` has been deleted and the paths `GMBasic/tools`
> and `GMBasicSharp/tools` no longer exist. The references to `migration/...` in the dated sections
> below describe the state at that time: they are read as **historical**, not as instructions.
>
> **Consolidated docs 2026-10-29.** There is only one documentation folder: `docs/` at the root
> (index in [`docs/README.md`](README.md)). `shared/docs/`, `GMBasic/docs/` and
> `GMBasicSharp/common/ClipperLib/docs/` no longer exist: the content is in `docs/comparison/`,
> `docs/implemented/`, `docs/ClipperLib/` and `docs/gmb_env_audit.tsv`. The references to
> `shared/docs/...` in the dated sections are **historical** too.
>
> **`tools/` versioned 2026-10-29.** The 78 files of `tools/` were **untracked** (only on disk):
> now they are in git. Two `.py` of the original repo had no counterpart and have been ported
> to BASIC with parity verified against the Python oracle: `check_minitest_expect.bas` (EXPECT
> oracle gate) and `validate_stdlib_csv.bas` (5 checks on the registry CSVs). The inventory
> of the residual `.py` and the reason why they must not be converted is in the vault,
> `Knowledge/Tool-Python-Residui-GMBasic.md`.

## Base
- BASE_COMMIT: `c9ad151ebfaad12f6f312850102c807074de9776` (2026-10-23) — current watermark,
  advanced by the subsequent imports, in `tools/data/base_commit.txt` (`66ab81ddc`).
- **The commit alone is NOT enough**: the GMBasic working tree contained **23 uncommitted
  changes** in `native/` (e.g. `gmb_sdl_console.c` with `extern gmb_tui_menu_any_open`); with
  HEAD alone the C build fails. The initial snapshot therefore uses **working tree + base commit**.
- Inventory of the changes at snapshot time: `git -C GMBasic status --short native | wc -l`
  = 23 (listed below, hash of the inventory file to be regenerated at every sync).

## Initial snapshot (2026-10-23)
- `native/`, `gmbllm/` copied from the **working tree** (excluding build/_deps/objects).
- `Resources/libs` (837 MB, includes untracked dylibs such as `libMoltenVK.dylib`) and
  `libs-runtime` (963 MB) copied from the working tree: local build dependencies, NOT from git.
- `tools/` and `GMBasic/tools` are **temporary symlinks** to `migration/tools-unclassified`
  (the tools must be rewritten per project: F2/F3). To be removed before publication.
- `shared/`, `GMBasicSharp/`, `spec/registry`, `GMBasic/tests-mixed`, `migration/tools-unclassified`
  derive from the HEAD snapshot.

## C build (verified)
- `gmb_runtime`: OK (`cmake -DCMAKE_BUILD_TYPE=Release`).
- `gmb_vm_host`: OK with `-DGMB_HAS_COMMERCIAL_DB=ON -DGMB_VM_LINK_COMPILER=ON`
  (the fresh defaults disable them → `gmb_db.h` missing).

## Next syncs
0. `<base>` = content of `tools/data/base_commit.txt` (update it at the end of the sync).
1. `git -C GMBasic fetch` + `git diff --name-status <base>..HEAD`.
2. Update `<base>` and reapply to the new paths (`GMBasic/`, `GMBasicSharp/`, `shared/`).
3. Record here the commit, files, outcome; if the working tree changes during the port, re-sync.

## TOOLS POLICY (2026-10-23)
- No Python. C → tool in C or GMBasic `.bas`; Sharp → tool in C#; shell only bootstrap/CI.
- The `tools`/`GMBasic/tools` symlinks are temporary and must be replaced by rewritten tools.

## C# REPL restored (2026-10-23)
- `REPL/` copied from `GMBasic-Deprecated/REPL` into `GMBasicSharp/REPL` (26 files).
- The necessary C# originals restored: **complete `common/ClipperLib`** (from Deprecated),
  `SdlConsole*.cs`, `AlertBox.cs`.
- `GMBasicSharp` now uses the **C# originals** for window state/controls: the proxy copy
  `common/CommonLib/Consoles/Tui/State` removed (Sharp side only); `IlBackendRuntime` references
  `ClipperLib.csproj` again; `GmBasicInputSource` → `ClipperLib.App`.
- `ScreenConfig.Palettes.GetConsoleNamed()` restored (C-backed for now).
- **REPL build green** (0 errors).
- The C project stays C-only: no reference to ClipperLib.

## C toolchain + Sharp solution (2026-10-23)
- C: `gmb_runtime`, `gmb_vm_host`, `gmb_compiler_c` (build `gmb`) built; C99Native is
  consumed from sources by the gate (no standalone executable). Configuration:
  `-DGMB_HAS_COMMERCIAL_DB=ON -DGMB_VM_LINK_COMPILER=ON`.
- **12 "IL-only" functions verified ALREADY implemented in C**: C99Native emits them
  (`gmb_c99_emit.c` 9530-9560) and VM/C dispatches them (`gmb_vm_tui.c` 400-419, 402,404,405,
  411-419). The gap of 12 was **stale** (audit manifests not regenerated: it requires `--all`).
- Sharp: `GMBasicSharp/GMBasicSharp.slnx` created (28 main projects); build **0 errors**.
  Fixed `IlBackendCodegen/genrunner` (path `../../../Executor`).
- **C# backend of the C99 emitter retired** from Sharp: moved to `GMBasic-Deprecated/`
  (snapshot 2026-10-23; tests in `tests-GMBASIC.Backend.C99.Tests-2026-10-23`).
- Policy: Python removed from the live paths. `cmake/embed_spirv.py` → `cmake/embed_spirv.cmake`
  (SPIR-V header generation in pure CMake, identical bytes/length); 4 scripts in `shared/` and
  `gmb_db_c/tools/fill_1to1.py` moved to Deprecated.
- Boundaries: `find GMBasic -name '*.cs'` = **0**; Sharp has 2 C test helpers
  (`tests-mixed/.../run_*.c`) to be moved into the C project.
- `migration/_import` (complete snapshot) contains ~270 .py tied to the legacy tools: **temporary**,
  to be excluded from publication; `tools/` and `GMBasic/tools` are temporary symlinks.

## C verification after the SPIR-V change (2026-10-23)
- `gmb_runtime` rebuilt OK; `gmb_gfx_spirv.h` generated by CMake (sprite.vert=1508,
  sprite.frag=636 bytes, identical to the old script).
- `tests/gmb_gaps_selfcheck.c` was **stale** with respect to the API (`snap` → `snap_zone`, edges
  string): updated; the WindowManager test now passes.
- 2 **pre-existing** FAIL remain in the selfcheck: `Graphics Texture Rotation` (needs a GPU/context)
  and `Keyboard read_line` (needs a TTY). Target `gmb_gfx_present_input_selfcheck` does not compile due to
  missing SDL2 headers (test config, not the lib).

## Audit regenerated + boundaries (2026-10-23)
- `--all` audit run on staging (`/var/folders/.../gmb-audit-stage`): canonical surface
  **1383**; **PCodeVM, IL, VMC, C99Native = 1383/1383 (100%)**.
- The 12 historical gaps (`CONTROLS.*` + `WINDOWS.WINDOWCLOSED`) were **false negatives** of the extractor:
  C99Native emits them by name, PCodeVM via `case SysCall.FORM_*`, VMC via `vm_form_*`.
  Patch (legacy tool only in `migration/tools-unclassified/audit/Program.cs`): it recognizes
  `new HashSet<BuiltInFunction>{...}`, the by-name branches of the FORM path and the SysCall cases; C99
  (retired C# transpiler) excluded from `--all`.
- Manifests in `spec/registry/manifests/` (C99Native, IL, PCodeVM, VMC); report in
  `docs/comparison/audit-backends.md` (then in `shared/docs/comparison/`). **To be replicated in the future C audit-tool**: the
  recognition of the dispatch by name/SysCall is necessary (otherwise 12 false gaps).
- Boundaries: **0 C files in `GMBasicSharp`**; `tests-mixed/native_tests/run_tests.c` →
  `GMBasic/native/tests/native_tests/`; `run_db_checks.c` → `GMBasic/native/gmb_db_c/tests/`;
  `NativeCChecks.cs` → Deprecated. `GMBasicSharp/Directory.Build.props` restored (matrix).
  `GMBASIC.Database.Native.Tests` compiles (0 errors); `GMBasicSharp.slnx` 0 errors.

## Verification of the "146 not implemented" (2026-10-24)
- **False alarm**: the audit deduplicated the surface by NAME only (1383), while the canonical
  registry is per (namespace,name) (1523). Tool fixed (key ns+name).
- Set comparison: **canonical 1523 = StdLib 1523**, difference **0**. No function is missing
  from StdLib: neither in Sharp nor in the original GMBasic repo → no modification to GMBasic.
- With the correct surface: IL/PCodeVM/VMC **1523/1523 (100%)**; C99Native **1517/1523**.
- Only real gaps (C99Native, missing emission): `SPEED.SETDURABILITY`, `SPEED.FLUSHDURABILITY`,
  `SPEED.BEGINGROUPCOMMIT`, `SPEED.ENDGROUPCOMMIT`, `SPEED.INFO`, `MOUSE.CLICK`.
  Runtime: set/flush/group-commit exist in gmb_db; `SPEED.INFO` and `MOUSE.CLICK` (area form)
  do not yet have a dedicated C function.

## Sync of new commits + closing SPEED/MOUSE gaps (2026-10-24)
- **New commits** in the original GMBasic: `6fe9765af` (@GET TYPE MiniTest/sample/guide) and
  `7a474842d` (C# emitter parity for typed fields). Copied into `shared/`:
  `minitests/controls/get_types_parity.bas`, `samples/Community/Clipper/ClipperTypedGets.bas`.
  The C# emitter changes do not concern the monorepo (retired backend).
- **Gaps closed on the 4 backends** (not only C99Native): SPEED.SETDURABILITY/FLUSHDURABILITY/
  BEGINGROUPCOMMIT/ENDGROUPCOMMIT/INFO and MOUSE.CLICK (0 arg + area).
  - Shared C: `gmb_db_speed_begin/end_group_commit`, `gmb_db_speed_{set_durability,info}_str`,
    getters `Database_GetDurabilityFull/GroupCommitDepth`, `gmb_mouse_click[_area]` (Clipper).
  - C99Native: entries `emit_bf_speed` + `emit_bf_mouse` (no inline logic).
  - VM/C: builtins 454-458 (`gmb_builtin.c`) + MOUSE.CLICK in `gmb_bf_393`.
  - IL/PCode: `IlRuntime.Speed{Begin,End}GroupCommit`, flush via `SpeedDriverForArea`
    (before it depended on `_speedHandles`, not populated by PCode/VM), IL emission of the tokens.
  - Harness fix `test_ilrun/Program.cs` (ClipperLib → CommonLib proxy).
- MiniTest `speed/gaps.bas` (EXPECT setdur/flush/begin/end/info/click0/area0).
  Run on the single file: **IL OK, PCode OK, VM/C OK, C99Native PASS**.
- Audit regenerated: **IL/PCodeVM/VMC/C99Native = 1523/1523 (100%)**.

## Commit import + commit in GMBasic (2026-10-25)
- Imported into the monorepo the commits **`7a474842d..8b77bfe00`** (6 commits, all `native/`):
  `gmb_compiler.c`, `gmb_c99gpm.c`, `gmb_read.c`, `gmb_vm_core.c`, `gmb_vm_host.c`,
  `gmb_vm_tui.c` — taken from **HEAD** (`git show`), not from the working tree (which has other people's WIP).
- Monorepo builds re-verified: `gmb_ai_llm` + `gmb_runtime` + `gmb_db` + `gmb_vm_host` OK.
- **Commit in GMBasic**: `0bc552167` — 16 files, +231/−8 (runtime/DB/C99Native/VM/C/IL/PCode,
  MiniTest `speed/gaps.bas`). `gmb_builtin.c` committed **only for its own hunks** (46 lines):
  the 419 lines of WIP of another AI stay uncommitted. `test_ilrun/Program.cs` is
  gitignored: local fix not committed.
- Verified state: IL/PCode/VM/C/C99Native PASS on `speed/gaps.bas`; audit 1523/1523.

## C# emitter retired in the ORIGINAL repo (2026-10-25)
- The C# backend of the C99 emitter → `GMBasic-Deprecated/` (snapshot 2026-10-25, including the
  uncommitted WIP of another AI, preserved) and `tests/GMBASIC.Backend.C99.Tests` →
  `.../tests-GMBASIC.Backend.C99.Tests-2026-10-25`.
- Removed: entries in `GMBASIC.sln`, `ProjectReference` in `tools/audit`, `GmbBackendC99` block
  in `Directory.Build.props`/`.targets`, C99 `InternalsVisibleTo` in the 3 csproj, C99 gate/step
  in `tools/{gmb_http_probe,llm_llvm_gate,test_mcp_e2e}.sh`, entries in
  `tools/check_file_size.sh`; scripts `gmb_c99_rebuild.sh`/`worktree_helper.sh` in Deprecated.
- `tools/audit`: the correct version propagated (key ns+name, speed/mouse, PCode SysCall);
  `--c99`/`--registry-c99` now answer "retired" (exit 2).
- `NativeParityTests` test that read the C# emitter: removed (obsolete); `NativeDb.cs` restored.
- Verification: `dotnet build GMBASIC.sln` **0 errors**; `speed/gaps.bas` **IL/PCode/VM/C/C99Native PASS**.
- Note: `tools/check_file_size.sh --quiet` stays red for pre-existing monolithic files
  (`gmb_builtin.c` 18k lines not in the allowlist) — not related to this work.

## Native READ import + C# emitter retirement (2026-10-25)
- Imported from HEAD the commits **`5dff233c4` / `edd4da60b` / `66ab81ddc`** (native READ via P/Invoke,
  PCode/IL aligned): C files copied from HEAD into `GMBasic/native/`; Sharp files without divergences
  copied; `IlRuntime.cs` and `PCodeProcessor.Syscalls.cs` taken from HEAD and the Sharp namespace
  transformation reapplied (CommonLib→ClipperLib) → `GMBasicSharp.slnx` 0 errors.
- Restored in `GMBasic/native/gmb_runtime_c/CMakeLists.txt` the SPIR-V embed in pure CMake
  (`embed_spirv.cmake`): the original HEAD still uses `embed_spirv.py` (Python).
- Build: `gmb_runtime`, `gmb_ai_llm`, `gmb_db`, `gmb_vm_host` OK.
- C# emitter: absent from the monorepo (already removed); `parity4` now 4 legs in the original repo.

## Sharp cleanup after the C99 retirement (2026-10-25)
- Removed from `GMBasicSharp/REPL` the old C99 leg: `#if GMB_C99`/`RunC99Safe` block,
  `--c99`/`UseC99` parsing, POC help and `C99DebugDriver` (stub) moved to
  `GMBasic-Deprecated/GMBasicSharp-REPL-C99-2026-10-25`; `DebugBackend` now {PCode, VmC}.
- `GMBasicSharp.slnx` and `REPL` compile with 0 errors; C: `gmb_runtime`, `gmb_ai_llm`, `gmb_db`,
  `gmb_vm_host`, `gmb_compiler_c` OK.
- Compliance: 0 C# in the C project, 0 C in the Sharp project, 0 .py outside migration/tools/build.

## Pre-repo cleanup (2026-10-25)
- `migration/_import` (duplicated snapshot, 270 .py) moved to
  `GMBasic-Deprecated/monorepo-import-2026-10-25`.
- Removed the `tools/` and `GMBasic/tools/` symlinks → `migration/tools-unclassified`.
- `GMBasic/tools/` is now real and contains only the shell scripts of the C project taken from HEAD
  (`gmb_ensure_ai_libs.sh`, `install_mlx_dylib.sh`, `gmb_c99native_run.sh`, `parity4` 4 legs,
  `test_minitests_c99native`, `check_file_size.sh`, `rebuild_ai_native.sh`, `gmb_http_probe.sh`,
  `llm_llvm_gate.sh`, `test_mcp_e2e.sh`).
- `gmb_vm_host` build OK with the real tools; no `.py` in `GMBasic/tools`.
- `migration/tools-unclassified` remains as a legacy archive (outside the repos to be published).

## F3 — runtime-abi (2026-10-25)
- `runtime-abi/` created: `check_abi.c` (dlopen/dlsym), `CMakeLists.txt`, README and 4 entry-point
  lists extracted from the Sharp bindings: `gmb_db` (51), `gmb_gfx` (385), `gmb_ai_llm` (60),
  `gmb_ai_modelio` (4).
- Verification: **51/51, 385/385, 60/60, 4/4** symbols present in the built dylibs.
- `conformance/` still to be populated (C runner for `.bas` corpora + EXPECT).

## F3 — conformance: toolchain blocker (2026-10-25)
- `runtime-abi` completed and green (51/51, 385/385, 60/60, 4/4).
- `conformance/` **not yet populated**: the runner must use the C toolchain, but in the monorepo:
  - `gmb` resolved the root only via `GMBASIC.sln` → marker
    `native/gmb_runtime_c/include/gmb_runtime.h` added in `gmb_inc_resolve.c` (fix done) and LLMEnv
    fallback `install/../../LLMEnv` in `gmb_cli.c`;
  - the minimal link of `gmb` (`-lm -lpthread`) does not cover the runtime's SDL/OpenSSL → the console
    samples fail on `_SDL_*` (`-lcrypto -lssl` added, SDL remains);
  - `tools/gmb_c99native_run.sh` still uses the paths of the old layout (LLMEnv next to
    GMBasic, settings) → it fails with `llmengine_plugin.h` not found.
- Next: unify root/LLMEnv/link recipe in a single C source (or monorepo settings)
  and then implement `conformance/run_conformance.c` (EXPECT vs output).

## F3 — conformance (first runner) (2026-10-25)
- `conformance/run_conformance.c` created (+ CMake/README): it extracts `' EXPECT:` from the `.bas`,
  runs `GMBasic/tools/gmb_c99native_run.sh`, normalizes (lines joined with `|`, trailing spaces
  removed) and compares. Exit 0 PASS/SKIP, 1 FAIL, 2 setup.
- PASS verified: `bits/bitclear.bas`, `bits/bitcount.bas`, `keyword_named_var.bas`.
- Monorepo toolchain fix: C root without `.sln` recognized by the marker
  `native/gmb_runtime_c/include/gmb_runtime.h` (`gmb_inc_resolve.c`); LLMEnv also searched in
  `install/../../LLMEnv` (`gmb_cli.c`); `gmb_c99native_run.sh` detects the sibling/workspace LLMEnv
  (no env) and links `MoltenVK`.
- Note: `speed/gaps.bas` passes with the official C99Native runner but fails through
  `gmb_c99native_run.sh` ("SPEED.SETDURABILITY not defined") — driver/flags difference to
  investigate (it is not a StdLib gap).

## Fix gmb_c99native_run.sh (2026-10-25)
- Cause of "SPEED.SETDURABILITY not defined": **stale** build driver. Now the script
  rebuilds the `gmb_c99native_driver` target (incremental) before choosing the binary.
- Removed the use of `python3` for the settings: deterministic bootstrap `-O1 -w` + `cc`
  (override `CF_OVERRIDE`). No Python in the tool.
- Verification with `conformance/run_conformance`: PASS on `bits/bitclear`, `bits/bitcount`,
  `keyword_named_var`, `speed/gaps`.
- Patch also applied to the original repo `GMBasic/tools/gmb_c99native_run.sh`.

## F3 — spec (2026-10-25)
- Added `spec/README.md` (contract: append-only registry, TokenKind key, C/Sharp consumption)
  and `spec/VERSION`. `spec/registry` already versioned.
- `conformance/run_conformance` operational (single-file); PASS verified on bits/keyword/speed.

## Multi-file conformance + script fix (2026-10-25)
- `run_conformance` accepts N files (absolute root, paths resolved before the chdir) and prints the
  total with an aggregated exit.
- Verification: `bits/bitclear`, `bits/bitcount`, `keyword_named_var`, `speed/gaps` → **4/4 PASS**.
- `gmb_c99native_run.sh`: driver rebuilt before use (stale fix) and no Python.

## Residual Python cleanup (2026-10-25)
- `GMBasic/tools/test_mcp_e2e.sh` (Python client) archived in Deprecated.
- `GMBasicSharp/tests-mixed/convert_parity.sh`: fixture copies with `cp`; normalization with
  `awk`+`tr` (no Python).
- `GMBasicSharp/tests-mixed/AI.Tests/McpClientTests`: MCP fixture now `/bin/sh` (inline script),
  no `python3`; test `StdioCall_ReusesProcessAcrossCalls` PASS.
- Verification: no `python3` in the live paths of the monorepo.

## Residual Python in the tests (2026-10-25)
- `gmb_compiler_c/tests/gmb_cli_project_test.sh`: JSON with `jq` (no Python).
- `gmb_compiler_c/tests/gmb_cli_minitests_test.sh`: `json_get`/validity/count-order with `jq`.
- `shared/settings/config/` (runtime state, including `mcp.json` with `python3`) moved to
  `GMBasic-Deprecated/monorepo-shared-config-2026-10-25`.
- Only one **path** to torch site-packages remains in `native/gmb_torch/build_host.sh` (link to
  libtorch, not Python execution): external ML dependency to be evaluated in F5.

## F4 — separation verification + repo hygiene (2026-10-25)
- `GMBasicSharp.slnx` builds **without** `SkipNativeAiLibs` / C toolchain (0 errors); `GMBasic`
  builds with CMake (runtime/vm/db/compiler/ai) without .NET.
- Added `.gitignore` (root, GMBasic, GMBasicSharp, shared, conformance, runtime-abi):
  bin/obj/build/work/out, `libs-runtime`, `Resources/libs`, state (graphs/rag/graphify).
- Per-project README: `GMBasic/README.md`, `GMBasicSharp/README.md`, `shared/README.md`
  (build, dependencies, ABI, PUL/Apache license).
- Boundaries: 0 C# in GMBasic, 0 C in GMBasicSharp; no `.py` in the live paths (only one
  path to torch site-packages remains in the C build_host, external ML dependency to be decided in F5).

## F3 — COMPLETED (2026-10-25)
- `spec/` (registry + README + VERSION), `conformance/` (multi-file runner; PASS on bits/keyword/speed),
  `runtime-abi/` (4 lists + `check_abi` + `check_all.sh`, `ABI_VERSION` 1.0.0).
- ABI gate: **51/51, 385/385, 60/60, 4/4 → ABI 1.0.0 OK**.
- Ownership/licenses: `LICENSING.md` (Apache runtime/Sharp/shared; PUL `gmbllm`; monodirectionality).
- F3 criterion satisfied; F4 verified; F5/F6 remain.

## F5 — DECISION (2026-10-25)
- **No C# reimplementation**: GMBasicSharp uses the C dylibs via P/Invoke. C remains the only
  source of runtime/UI/DB/AI; no duplication, parity guaranteed by the ABI contract.
- F5 reduces to: keeping `runtime-abi` complete for every P/Invoke (4 libs verified, others
  listed to be covered), licenses (Sharp Apache + PUL dependency for `gmbllm`).
- Public `gmbasic-sharp` = source; execution requires the C runtime (private/PUL repo).

## F5 — extended ABI (2026-10-25)
- `runtime-abi/check_all.sh` covers **13 dylibs** (db, gfx, ai_llm, ai_modelio, ai_mlx,
  finetune, llamacpp, llmtoken, torch_loader, compress, grep, pkg, moe_stream): **all OK**.
- Dead P/Invokes removed from `MlxProviders` (`gmb_mlx_opts_register`, `gmb_opt_*`,
  `llm_ai_mlx_model_lora_save`, `gmb_mlx_set_moe_swap_dir`): not exported/renamed.
- Open: `GmbEnv.Get` uses `gmb_opt_env` on `gmb_ai_mlx` (not exported); the settings/env fallback
  masks the error, to be rebound/exported.

## ABI fix `GmbEnv` (2026-10-25)
- `gmb_opt_env` is now exported from `libgmb_ai_llm` (link option `-Wl,-u,_gmb_opt_env` on Apple):
  `GmbEnv.Get` no longer falls back for a missing EntryPoint.
- `gmb_ai_llm.symbols` updated (61/61); `check_all.sh` green.
- CMake patch to be applied to the original repo as well (same line in `gmb_runtime_c/CMakeLists.txt`).

## Fix GmbEnv (2026-10-25)
- `GmbEnv` now uses `gmb_ai_llm` (where `gmb_opt_env` is exported with `-Wl,-u,_gmb_opt_env`),
  not `gmb_ai_mlx`. The fallback no longer masks an EntryPointNotFound.
- Sharp builds with 0 errors; `check_all` ABI OK (ai_llm 61/61).

## Local repos created (2026-10-25)
- `GMBasic` → commit `d6ed1fd` (C baseline; includes prebuilt plugin/lib dylibs to be cleaned before publication).
- `GMBasicSharp` → commit `bb05cb3` (C# baseline).
- `shared` → commit `875f649` (samples/minitests/spec/conformance/runtime-abi; 212 MB).
- Workspace `GMBasicMonoRepo` → commit `7f29463` with submodule gitlinks and `.gitmodules` (placeholder
  relative URLs: to be replaced with the definitive remotes). Submodules **not initialized** (gitlinks only).
- `BASE_COMMIT` set back to `66ab81ddc` (last imported commit): **pending sync `66ab..50a6552df`**
  (~20 commits: DYNAMIC VM/C, model-quantize, READ write-through, LIBRARY runner).

## LIBRARY port (CLI + runner) and the entry of GMBasicIDE (2026-10-26)
- Imported into the **C** project the two commits with which the original turned LIBRARY into a real
  product: `ca349c5bf` (`gmb build` produces the `.dylib`, the executable links chart/SDL/MoltenVK)
  and `cb310534f` (LIBRARY leg in the MiniTest runner), applied with `cherry-pick -x` because
  the monorepo clone has a different history.
  - Conflict resolved in `gmb_cli.c`: the monorepo baseline added
    `-lcrypto -lssl` to the link (TLS plugin, resolved from the cache); they were kept in
    **both** forms (library and executable), together with the new `GMB_LIB_LDFLAGS` flags
    and the chart/SDL link. Also fixed the format (`%s` missing) that the merge had
    left: no warning with `-Wall -Wextra`.
  - `samples/modules/README.md` and `tools/check_minitests.sh`: the monorepo had them
    deliberately out of the index. They remain so (the second one, updated, stays on disk
    untracked) instead of reintroducing them.
  - Verification: `cmake --build native/gmb_compiler_c/build --target gmb_cli` and
    `native/gmb_compiler_c/tests/gmb_cli_project_test.sh` → **20/20 PASS**, including
    `LIBRARY → bin/gmbmath.dylib`, exported ABI, no exported `main`, `run` that does not execute.
- **C#** project: no change to import. LIBRARY support was already complete
  (`BasicModule.LibraryName`, `IlProgramCompiler.EmitLibraryEntry`, project without entry
  point accepted in `Orchestrator`), and in Sharp there is no `build --project` CLI that is
  the counterpart of the change made in C.
- **GMBasicIDE** enters as the fourth submodule (`url = ../GMBasicIDE`): the former `VB6Clone`,
  renamed in folder, bundle, identifier, system menu, runtime titles and docs.
  The breakpoint marker in the `.frm` remains `'@VB6CloneBreakpoints` (on-disk format of the
  existing projects, byte-identical round-trip). Build and `make check` green after the rename.
