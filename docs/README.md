# docs/ — monorepo documentation

> Canonical directory structure and "what lives where":
> `Knowledge/Struttura-Monorepo` (vault `AiWiki/01-Projects/GMBasicMonoRepo`).

**The only documentation folder, at the monorepo root.** Since 2026-10-29 there are no more
nested `docs/` folders: the ones that were in `GMBasic/`, `GMBasicSharp/common/ClipperLib/`
and `shared/` have been consolidated here (see "Provenance").

The tooling (script, gate, generator) does **not** live here: it is in [`tools/`](../tools/README.md).
In `docs/` only documents and generated report artifacts remain.

## Structure

| Path | Content | Produced by |
|---|---|---|
| [`Sync-GMBasic-Monorepo.md`](Sync-GMBasic-Monorepo.md) | sync log with the original GMBasic project, by date; base commit and inventory of changes | by hand (log) |
| [`gmb_env_audit.tsv`](gmb_env_audit.tsv) | audit of the `GMB_*` environment variables (category + occurrences + file:line). Referenced by AGENTS.md §"ENVIRONMENT VARIABLES ARE FORBIDDEN" | `tools/basic/audit_gmb_env.bas` |
| [`comparison/`](comparison/) | canonical surface ↔ backend comparison: `audit-backends.md`, `audit-stdlib.md`, `recon-stdlib.{md,json}`, `unimplemented-canonical.md`, TokenKind dispatch CSVs, TokenKind refactor TODO | `tools/audit` (`--all`), `tools/basic/gen_command_desc.bas`, `recon_stdlib.py` |
| [`comparison/manifests/`](comparison/manifests/) | one JSON manifest per backend (`IL`, `PCodeVM`, `VMC`, `C99Native`) with coverage and gap list | `tools/audit` (`--all`) |
| [`comparison/command_desc.tsv`](comparison/command_desc.tsv) | name → syntax/description index used to generate the `COMMANDS.TXT` blocks | `tools/basic/gen_command_desc.bas` |
| [`ClipperLib/clipper53.md`](ClipperLib/clipper53.md) | technical reference for the Clipper 5.3 emulation (`ClipperLib.Clipper`: SAY/GET/READ/BOX/INKEY/ACHOICE/MEMOEDIT/PICTURE, SET …) | by hand |
| [`ClipperLib/controls.md`](ClipperLib/controls.md) | documentation for `ClipperLib.Controls` (78+ TUI controls) and `ClipperLib.TBrowse` | by hand |

## Provenance (2026-10-29 consolidation)

| Before | Now |
|---|---|
| `shared/docs/comparison/**` | `docs/comparison/**` |
| `GMBasic/docs/gmb_env_audit.tsv` | `docs/gmb_env_audit.tsv` (replaces the **stale** copy, with pre-split paths `GMBasic/`…`GMBasicSharp/`) |
| `GMBasicSharp/common/ClipperLib/docs/{clipper53,controls}.md` | `docs/ClipperLib/{clipper53,controls}.md` |
| `docs/Sync-GMBasic-Monorepo.md` | unchanged (it was already at the root) |

The `shared/docs/`, `GMBasic/docs/` and `ClipperLib/docs/` folders **no longer exist**.
Pre-consolidation backup: `/tmp/GMBasicMonoRepo-backup-<data>/`.

## What remains to be decided

- `docs/implemented/audit/COMMANDS.txt` is an **outdated copy** of `COMMANDS.TXT` (same number
  of lines, older descriptions). It should be **deleted** as soon as it is confirmed: nobody reads it.
- `tools/basic/audit_gmb_env.bas` derives the root from `IO.PARENTDIR(IO.CWD())` (it assumes it runs
  from `GMBasic/tools/`): in the monorepo it lands on the root only thanks to the check on the sibling
  `GMBasicSharp/`. It should be made explicit (monorepo root) when that tool is touched.
