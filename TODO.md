# TODO — main open work

Top-level index of the main open items. Per-project checklists live next to the code and are the
source of truth for details:

- [`GMBasicIDE/TODO-GMBASIC.md`](GMBasicIDE/TODO-GMBASIC.md) — GUI IDE plan (groups G0–G7)
- [`GMBasicTuiIde/TODO.md`](GMBasicTuiIde/TODO.md) — TUI IDE (remaining features + C technical debt)
- [`docs/comparison/TODO-TokenKind-L-P-Refactor.md`](docs/comparison/TODO-TokenKind-L-P-Refactor.md) — Lexer/Parser/Linker refactor phases

## 1. Two IDEs to complete

### 1.1 GUI IDE — `GMBasicIDE` (macOS, VB6-style)

The IDE runs (form designer, code window, build/run through `gmb`, interactive debug); the plan in
[`TODO-GMBASIC.md`](GMBasicIDE/TODO-GMBASIC.md) still has **~51 open items** over 8 groups:

| group | content |
|---|---|
| **G1** (7 open) | project model and `.frm` round-trip |
| **G2** (7 open) | GMBasic language mode (no VB6 runtime, `.vbp` never opened) |
| **G3** (7 open) | Check / Run / Build / Output pipeline, driver freshness, headless rules |
| **G4** (11 open) | designer → `.frm` → `.ui.bas` codegen contract and control coverage |
| **G5** (3 open) | debug: breakpoints, step, stack, watch over `gmb_chunk_driver` + `gmb_vm_host --debug-interactive` |
| **G6** (5 open) | UX, settings dialog, documentation, packaging/distribution |
| **G7** (6 open) | verification matrix (`vbtool check`, headless tests, real-display runs) |

### 1.2 TUI IDE — `GMBasicTuiIde` (written in BASIC)

Working (multi-tab, file I/O, outline, completion, lint, goto, refs, make), these are open
([`TODO.md`](GMBasicTuiIde/TODO.md)):

- [ ] inline **completion popup** with insertion into the CodeArea
- [ ] **goto by interaction** on the outline (needs the ListBox selection event; `gmb_form_poll_event` is empty)
- [ ] **find/replace** in the buffer with highlighting
- [ ] **rename/snippet** exposed as keys (the `gmb_idef_*` wrappers exist)
- [ ] **cursor line/column in the StatusBar** (needs `gmb_code_area_cursor_line`/`_col` by index)
- [ ] **real screen size** (today `gW=100,gH=30` are constants)
- [ ] **verification on a real display** (headless does not cover GRAPHICS)
- [ ] **user guide**
- [ ] C debt: shared static text buffer in `gmb_tui_control_text_ptr`/`set_text`; `\n` vs `|` separator between `set_text` and the factories; no name-based setter for `items`/`tabs`

## 2. Documentation and tooling

- [ ] **`COMMANDS.TXT` in English**: the generator (`tools/basic/gen_commands.bas`) still emits
      Italian labels (`NOME`, `SINTASSI / ESEMPIO`, `DESCRIZIONE`, `RIEPILOGO`, …) and a stale
      header (`Generato da gen_commands.py`). Translating it means regenerating the 542 KB file
      while preserving the hand-curated descriptions.
- [ ] **Remove the stale duplicate** [`docs/implemented/audit/COMMANDS.txt`](docs/implemented/audit/COMMANDS.txt)
      (538 KB, superseded by the root `COMMANDS.TXT`).
- [ ] **TokenKind / Lexer-Parser-Linker refactor**: the remaining phases are tracked in
      `docs/comparison/TODO-TokenKind-L-P-Refactor.md`.

## 3. CI

- [ ] **`libs` workflow is red**: the `build osx-arm64` job fails at the *system dependencies
      (macOS)* step, so nothing after it (release + `libs-manifest.json`) has ever published.
- [ ] Set the publishing inputs it expects: secret **`GMB_MODULES_TOKEN`** (release upload) and
      the variables **`LLMENV_REPO`** / **`LLMENV_DIR`** (engine checkout or prebuilt path).

## 4. Content still in Italian (deliberate, needs a decision)

- [ ] `shared/APPS/DigUp/fixtures/{lease.md,zebra.txt,podcast.txt}` — demo **data** paired with
      Italian queries inside `DigUp.bas`; translating one side alone breaks the demo.
