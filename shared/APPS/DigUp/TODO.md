# TODO — DigUp (GMBasic port)

Status: **M1** — headless core working on text/documents/code with SQLite index and ranking
(cosine + exact words). See `README.md`. Upstream: <https://github.com/ARahim3/DigUp> (MIT).

## M2 — missing Readers (by file type)

- [ ] **PDF** (page by page, with the page number in the result). LLMEnv has no PDFium;
      `gmb_doc_c`/`ConversionLib` cover part of the documents: verify what is already extractable and
      use that (never duplicate a reader that exists).
- [ ] **Office**: docx/odt/xlsx/ods/pptx/odp — text extraction via `gmb_doc_c` if available.
- [ ] **Images / screenshots** (text OCR) via `gmb_doc_ocr` **where tesseract is in the bundle**
      (today only osx-arm64, see `Todo/(open) - Build-OCR-Tesseract-per-OS` in the vault).
- [ ] **Audio/video**: requires the multimodal embedding of the original (EmbeddingGemma 2).
      LLMEnv **does not** expose it today: **blocked** until there is a supported multimodal
      model. In the meantime: no fake support (explicit error if one tries).

## M3 — Search and quality

- [ ] **Incremental index**: today `reindex = "true"` rebuilds everything; add
      `mtime`/`size` per file and reindex only the changed ones (the app is "a single pass" like
      the original: afterwards, only the new ones).
- [ ] **`code:` prefix** (separate index for the code, like the original) and filtering by
      extension/folder in the results.
- [ ] **Exact-words index** (FTS/`LIKE` with ranking) instead of only the boost on the words:
      invoice numbers and error codes with weights comparable to each other.
- [ ] **Deterministic eval** (like the original's `evals/`): query+expected file, top-1/top-k
      metric, runnable with the fixture provider (no model) and with a real model.
- [ ] **JSON output** of the results (for scripts/CI) — e.g. `[search] format = "json"`.

## M4 — User experience (like the original)

- [ ] **Global hotkey** and **tray**: in the monorepo `TRAY` and `CONTROLS` exist; evaluate a TUI
      (`MENU`/`TBROWSE`) before the pixel GUI.
- [ ] **Open at the point**: open the PDF at the page, audio/video at the minute, code at the line
      (depends on the M2 readers).
- [ ] **Large preview** and image grid.

## M1.1 — unblocking the other legs (done 2026-11-01)

- [x] **IL**: `status`/`index`/`search` green. Workarounds applied (bug in the vault):
      `IO.READFILE` + `STRINGS.SPLIT(CHR$(10))` instead of `IO.READLINES`+`UBOUND`;
      `IF isDir <> 0` instead of `IF isDir` (BIT); DB path to `OpenIndex(p)` not from the global.
- [x] **VM/C** and **C99Native**: `index` (3 files / 3 chunks) and `search` (top-1 `zebra.txt` 0.333).
      C99 note: the array receiving `STRINGS.SPLIT` must be dynamic (`DIM lines() AS STRING`).
- [ ] **Close the two IL bugs** at the root (notes `Bugs/(open) - IL-IF-su-Variabile-BIT-e-SQL-OPEN-
      con-Globale`): when they are fixed, the workarounds above can be removed.

## Constraints (non-negotiable)

- **The model is configured in the settings** (`[llm] model`/`dir`, ADR in the vault): never hardcoded.
- **No stubs**: every capability either works or returns a documented **explicit error**.
- **No `work/`**: the index and the outputs stay in `bin/` (compiler default).
- **4-leg parity** for the BASIC surface that might be added (AGENTS §4.1);
  the app itself runs on IL/PCode/VM/C/C99Native.
- **Licence**: MIT attribution of the original in `README.md`; no Swift/Tauri code copied.
