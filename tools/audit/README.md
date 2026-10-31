# tools/audit — `gmb_audit`

Builtin coverage audit per backend: it extracts the canonical surface (enum
`BuiltInFunction` + StdLib) and the `case`/dispatch of each backend, and produces a reconcilable
JSON manifest. READ-ONLY tool: it does not modify any backend.

## Provenance

Source taken **verbatim** from `migration/tools-unclassified/audit/` (deleted on 2026-10-29
together with the rest of `tools-unclassified`), where it had been patched for the monorepo: dedup
key `(namespace, nome)` instead of the name alone, recognition of the dispatch by name and by
`case SysCall` (without it, 12 CONTROLS/WINDOWS functions showed up as false gaps), SPEED/MOUSE.
`Program.cs` is identical to the deleted file (verified with `diff`).

## Status of the project references — TO BE REPOINTED BEFORE USE

`audit.csproj` and the `ResolveRoot()` resolver in `Program.cs` are still those of the
pre-monorepo layout:

- the `ProjectReference` point to `../../common/...` (and one to `../...csproj`, which does not exist):
  in the monorepo the C# projects live in `GMBasicSharp/common/...`;
- `ResolveRoot()` looks for `GMBASIC.sln`, which is not there in the monorepo (there is
  `GMBasicSharp/GMBasicSharp.slnx`), and falls back to walking up from `bin/`;
- the internal paths (`common/Compiler/BuiltInFunction.cs`, `native/gmb_c99native/src/...`,
  `native/gmb_vm_c/gmb_vm_core.c`) are relative to the root of the C# project / of the C project,
  not to the monorepo root;
- the output (`--out-dir` defaults to `Root + docs/comparison/manifests`, and the report
  `Root + docs/comparison/audit-backends.md`) must end up in the **`docs/` of the monorepo
  root** (the only documentation folder, index `docs/README.md`): as long as `ResolveRoot()`
  returns the C# project root, use `--out-dir <monorepo>/docs/comparison/manifests`.

Same situation as `tools/registry-export/registry-export.csproj` (legacy references). The
project **was not compiled nor executed** after the move: no verification is claimed.

## Documented usage (original form)

```
dotnet run --project tools/audit -- --all
dotnet run --project tools/audit -- --pcodevm [--json out.json]
dotnet run --project tools/audit -- --list-backends
```

`test_retired_flags.sh` verifies that the retired flags (`--c99`, `--registry-c99`) exit with 2 and
do not produce files. This script too assumes the pre-monorepo layout.

## Why it is kept

AGENTS.md §12 points to it as the canonical audit infrastructure: every new command/function must
be implemented so that the audit counts it. The dispatch recognition logic
(by name, by `HashSet<BuiltInFunction>{...}`, by `case SysCall.FORM_*`, by `vm_form_*`) is
knowledge to preserve: `docs/Sync-GMBasic-Monorepo.md` notes that it must be replicated in any
C rewrite of the audit, otherwise the 12 false gaps are reproduced.
