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

## Project references — verificate 2026-11-01

Lo stato pre-monorepo è stato sanato. Verificato in questo albero:

- i `ProjectReference` di `audit.csproj` puntano a `GMBasicSharp/common/...` (StdLib, Compiler,
  IlBackendRuntime, BackendSupport);
- `ResolveRoot()` cerca `GMBasicSharp/common/Ast/TokenKind.cs` risalendo da CWD/assembly: trova la
  root del monorepo da qualunque directory;
- `RepoRoot` = `GMBasicSharp/..`, `NativeRoot` = `<monorepo>/GMBasic/native`, `SharedRoot` =
  `<monorepo>/shared`; i default di output sono `tools/reports/manifests` e
  `tools/reports/audit-backends.md` sotto la root del monorepo;
- `dotnet build tools/audit/audit.csproj` → 0 errori; `--list-backends` e
  `--pcodevm --json <file>` girano e producono il manifest;
- `test_retired_flags.sh` è stato riallineato al monorepo (root a `../..`, niente `.sln` finto):
  **7/7 PASS**.

Nessuna riparazione di path è più necessaria; se in futuro la root cambia, i marker sono
`GMBasic` + `GMBasicSharp` + `shared`.

## Documented usage (original form)

```
dotnet run --project tools/audit -- --all
dotnet run --project tools/audit -- --pcodevm [--json out.json]
dotnet run --project tools/audit -- --list-backends
```

`test_retired_flags.sh` verifies that the retired flags (`--c99`, `--registry-c99`) exit with 2 and
do not produce files; the script is now monorepo-aware (7/7 PASS on 2026-11-01).

## Why it is kept

AGENTS.md §12 points to it as the canonical audit infrastructure: every new command/function must
be implemented so that the audit counts it. The dispatch recognition logic
(by name, by `HashSet<BuiltInFunction>{...}`, by `case SysCall.FORM_*`, by `vm_form_*`) is
knowledge to preserve: la nota del vault `Memory/Sync-GMBasic-Monorepo-2026-10-26.md` dice che
C rewrite of the audit, otherwise the 12 false gaps are reproduced.
