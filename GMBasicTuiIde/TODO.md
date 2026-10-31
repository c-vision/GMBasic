# TODO — GMBasicTuiIde (TUI IDE written in BASIC)

Status: **v1.0** — working IDE (multi-tab, file I/O, outline/completion/lint/goto/refs/make),
verified on IL and as a standalone build (C99Native). See `README.md`.

A **standalone project of the monorepo** since 2026-10-31 (previously `shared/APPS/GMBasicIde`); it
also serves as a **footprint benchmark** for a compiled BASIC program (see README §"Why it is a
standalone project").

## Remaining features

- [ ] **Inline completion popup** with insertion: open a popup ListBox with the candidates
      (`gmb_idef_complete`) and insert the choice into the CodeArea at the cursor line/column.
- [ ] **Interaction-driven goto** on the outline (today: F7 reads the selected item). It needs the
      ListBox selection event (in TEXT `gmb_form_poll_event` is empty → use the callbacks or a
      TBROWSE with `gmb_tui_control_selrow`).
- [ ] **Search/find in the buffer** (forward/backward), replace, with highlighting.
- [ ] **Rename/snippet** exposed as keys (the `gmb_idef_rename`/`snippet` wrappers already exist).
- [ ] **Columns/caret in the StatusBar** (cursor line/column) — needs a getter
      `gmb_code_area_cursor_line`/`_col` by index (today only by pointer) or expose it.
- [ ] **Real screen size** (today `gW=100,gH=30` are fixed) — use the terminal size.
- [ ] **Verification on a real display** (headless does not cover GRAPHICS); exercise the
      interactive loop.
- [ ] **User guide** in the vault (`Guide/utente/`).

## Technical debt / C improvements

- [ ] `gmb_tui_control_text_ptr`/`set_text` use a shared static buffer: documented, but beware of
      nested calls.
- [ ] `gmb_tui_control_set_text` for ListBox uses '\n'; the factories (`gmb_tui_add_*`) use '|'
      (`tui_split`). Harmonize the separator (or wrap) to reduce the asymmetry.
- [ ] No name-based setter for `items`/`tabs` in TUI (only the index-based setter added here).

## Links

- Plan: vault `Todo/(open) - IDE-TUI-BASIC-e-standalone-e-VSCode-LSP.md`.
- C logic + wrappers: `native/gmb_runtime_c/{include/gmb_ide.h,src/gmb_ide.c}`.
- TUI controls: `native/gmb_runtime_c/{include/gmb_tui_app.h,src/gmb_tui_controls_add.c}`.
