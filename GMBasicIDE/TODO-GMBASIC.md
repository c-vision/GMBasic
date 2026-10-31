# TODO — GMBasicIDE come IDE per GMBasic

**Status:** plan updated on 2026-10-22 after three product decisions:

1. the IDE edits **native GMBasic** (it does not translate VB6 into GMBasic);
2. the compiler is **C99Native** (single target; IL is no longer the default nor the fallback);
3. forms are **VB6-compatible `.frm`** (layout), with the GMBasic code in `.bas` files.

No source of GMBasic or GMBasicIDE has been modified by this document. The probes
produced only files in `/tmp` and build artifacts in the `work/` directory of the GMBasic repo.

**Scope:** make GMBasicIDE the IDE shell (project, editor, designer, properties, output,
run) of the GMBasic compiler, without embedding GMBasic in the IDE process.

**Legend:** `P0` = needed for the first real use · `P1` = required for a complete flow ·
`P2` = polish or research. `[x]` = done and verified.

Sources read (vault `~/Documents/AiWiki/01-Projects/GMBasic`):
`_Index_GMBasic.md`, `Guide/utente/Controlli-Pixel-Proprieta-Metodi-Guida-Utente.md`,
`Guide/utente/Controlli-Utente-Struttura.md`, `Guide/utente/Finestre-Eventi-Guida-Utente.md`,
`Guide/utente/Clipper-Controls-Reference.md`, `Guide/utente/CLI-Flags.md`,
`Todo/(open) - IDE-TUI-BASIC-e-standalone-e-VSCode-LSP.md`,
`Piani/(open) - Modello-Eventi-Controlli-Finestre.md`.

Sources read (repo `~/Documents/Projects/GMBasic`):
`README.md`, `Executor/`, `test_ilrun/`, `test_graphics_visible/`,
`native/gmb_compiler_c/frontend/`, `native/gmb_c99native/driver/`,
`native/gmb_c99native/src/gmb_c99_emit_controls.c`,
`native/gmb_runtime_c/include/gmb_ide.h`, `native/gmb_runtime_c/include/gmb_form.h`,
`common/Registry/canonical.registry.json`, `samples/Community/graphics/ControlsFullDemo.bas`,
`tools/gmb_c99native_run.sh`.

Sources read (this repo): `src/core/vb_parse.c`, `src/core/vb_model.h`,
`src/platform/mac/VBEditorWindow.m`, `src/platform/mac/VBCodeEditor.m`, `src/tools/vbtool.c`.

**Summary of the changes compared with the previous version:**

- single target **C99Native**: no default IL Run, no "graphics only on IL";
- **VB6 `.frm`** forms (layout) instead of `.gmbform` JSON; code in `.bas`, generated `.ui.bas`;
- added the **upstream omissions** O1–O10 (pixel Label, project/CLI, diagnostic map,
  checkbox/combo/frame/timer and `.frm` round-trip);
- measured the real capability of C99Native (section 1.1) and its limits;
- **correction 2026-10-24**: multi-file resolution via `IMPORT` **already exists** (measured in
  section 1.1); what is missing is the project/CLI, not the linking. O7 reworded and downgraded to P1.
- **O7 done (2026-10-24)** in GMBasic: `gmb_inc_resolve` is recursive, reads the settings
  `importFolders`/`importFiles`, follows the `IMPORT` of every module and uses
  `compiler.searchFolders` (default `include`/`Controls`/`<install>/libs/*`) in place of
  the hardcoded list — same rules in the C# Orchestrator. Plus the project CLI
  `gmb check/build/run --project` (exclusions, entry, `NAMESPACE`/`IMPORT`, `--json`).
  Sample `samples/modules/FolderResolution`.
- **O8 implemented (2026-10-24)** in GMBasic: `#line` per statement (`gmb_c99_emit.c`),
  module segments in `gmb_inc_resolve`, `--line-map <json>` in the driver. `cc` and the emission
  errors cite `file:riga` of the `.bas`; gate `native/gmb_c99native/tests/c99_line_map_test.sh`.

---

## 0. Confirmed decisions and scope

| # | Decision | Consequence |
|---|---|---|
| D1 | **Native GMBasic** | The IDE colors, completes and checks the GMBasic grammar; no VB6→GMBasic translator. |
| D2 | **C99Native** | Check, Run and Build use the C99Native driver/emitter. IL is not required; the VB6 interpreter is not opened for GMBasic. |
| D3 | **VB6-compatible `.frm` forms** | The layout lives in a VB6 `.frm` (`VERSION 5.00`, `Begin VB.Form`, `Begin VB.*`, properties in twip); the GMBasic code lives in `.bas`. No `.gmbform` JSON format. |
| D4 | **Process boundary** | The IDE invokes a stable CLI (`gmb check/run/build --json`); it does not link the compiler nor the GMBasic runtime. |
| D5 | **Explicit omissions** | If C99Native lacks a needed control/feature, either it is implemented upstream (section 6) or the designer rejects it with a message. Never silent emulation. |

**Non-goals (MVP):**

- rewriting GMBasic inside `src/core/vb_code.c` or in the GMBasicIDE VB6 runtime;
- linking `gmb_compiler_c`, the C# assemblies or the GMBasic VM/C into the IDE process;
- translating **VB6** code into GMBasic (`Control.Property` properties, `Private Sub` handlers,
  control array, `Me`, `DoEvents`, `Option Explicit`): the `.frm` is compatible as a **layout**,
  not as executable VB6 code;
- a complete GMBasic debugger, semantic IntelliSense over all 1567 built-ins, complete Menu Editor,
  ADO data binding.

The principle remains that of the core: **all testable logic lives in C99 in `src/core/`**,
Cocoa only does I/O and UI. The boundary with GMBasic is a versioned CLI contract.

---

## 1. What language and what compiler GMBasic are

GMBasic **is not Visual Basic 6**: it is a compiled BASIC, with a much wider surface and a
UI model that lives in the code. The target is C99Native, so the truth about the grammar is the
**C front-end + C99Native emitter**, not the C# registry nor the IL runners.

| Aspect | Verified fact | Consequence for the IDE |
|---|---|---|
| Nature | Compiler with C and C# front-ends and several historical backends; the chosen target is `native/gmb_c99native`, which emits C and links it to the C runtime. | The IDE compiles and launches a process; it does not interpret. |
| Lexicon | **Case-insensitive** keywords; the C lexer has **1285** `GMB_KEYWORD_ENTRY` entries; many words are **soft keywords** recognized by the parser (`BUTTON`, `TEXTBOX`, `CONTROL`, `LABEL`, …), not by the lexer. | The syntax highlighting table must be generated from the **C99Native** lexer + parser, not copied from the C# registry. |
| Surface | `common/Registry/canonical.registry.json`: **1567** procedures, **115** commands, **284** constants. | Highlight/completion: keywords from the compiler, procedure names from the registry; synchronization test. |
| Syntax | Statements per line or separated by `:`; comments `'` and `REM`; continuation `_`; optional BASIC line numbers; labels. | A dedicated GMBasic lexer is needed; the current VB6 one is not enough. |
| Types | `STRING`, `BOOLEAN`, `BYTE`, `INTEGER`, `LONG`, `DATE`, `SINGLE`, `DOUBLE`, `CURRENCY`; user `TYPE`/`CLASS`. | Type highlighting; no VB6 types that are absent. |
| Declarations | `DIM … AS`, `CONST`, `TYPE`, `ENUM`, `CLASS … INHERITS`, `NAMESPACE`, `IMPORT`, `DECLARE … LIB`. | Outline and procedure combo must know classes, namespaces and types, not only `Sub`/`Function`. |
| Control | `IF/ELSEIF/END IF`, `SELECT CASE`, `FOR/NEXT`, `FOR EACH`, `DO/LOOP`, `WHILE/WEND`, `GOTO`/`GOSUB`, `ON ERROR`/`RESUME`/`ERR`, `TRY/CATCH`. | The "line executable" and the breakpoints are not the VB6 ones: in GMBasic mode breakpoints are disabled. |
| UI | `CONTROLS` + `WINDOWS` namespaces; **cell-based** controls in TUI and **pixel-based** with `SCREEN`; `WINDOW PUMP`, `ON …`, properties as clauses (`NAME`, `TAG`, `TOOLTIP`, `VISIBLE`, `ENABLED`, `TABINDEX`, `TABSTOP`, `STYLE`) and `CONTROL SET/GET`. | The designer generates pixel code; the chrome is SDL/GMBasic, not VB6: it is not a 100% WYSIWYG preview. |
| Project | The compiler has no native `.vbp`/`.frm` project. `DiscoverSourceFiles` scans `*.bas` and `*.inc` recursively, plus `include/` and `Controls/`; `app.settings.json` and `Resources/system.settings.json` contain the settings. | The IDE adds the `.frm` as layout and its own manifest, and generates `.ui.bas`; the compiler keeps ignoring `.frm`. |
| Build | `gmb_c99native_driver` emits C (`--no-exe`) and by default tries to link; the bare link fails (`gmb_runtime.h` not found), the full link is in `tools/gmb_c99native_run.sh`. | A stable CLI is needed that embeds the full link and project discovery. |
| Multi-file | **Already solved via `IMPORT`**: a `.bas`/`.inc` module declaring `NAMESPACE X` enters the build if the source writes `IMPORT X`; the driver prepends its text (measured: `Main.bas` + `Ui.bas` → `gmb_user_frmShow()` in the emitted C). It searches the program dir, `include/`, `Controls/` and `<root>/libs/*`: **it is not recursive**, and a file without `NAMESPACE` or not imported is not compiled. | No concatenation bridge: the codegen emits `NAMESPACE`/`IMPORT` and the project stays on a flat dir (or `include/`). The CLI needs the file list/exclusions and the entry (O7). |
| Diagnostics | Front-end: `parse error: <msg>` with the line, rc=3. Emitter: `emissione: <file>:<riga>: <msg>`, rc=4 (it used to be `riga N` of the merged text). `cc` errors: **cite the `.bas`** thanks to the `#line` (O8, done on 2026-10-24). | The IDE directly uses `file:riga`; `--line-map <json>` gives the anchors for those who compile the C on their own. |
| Driver | `tools/gmb_c99native_driver` may be **stale** compared with `native/gmb_compiler_c/build/gmb_c99native_driver`; `tools/gmb_c99native_run.sh` prefers the most recent build. | The CLI must build/verify the driver and declare the version; do not use the binary in `tools/` blindly. |
| Toolchain | On this machine: .NET **10.0.301**, Apple clang **21.0.0**, cmake **4.4.3**, C99Native driver compiled. `GMBASIC_DEV_MODE=1` is needed for licensing in development. | First-run check of `cc`, `cmake`, driver, GMBasic root and runtime libraries. |

### 1.1 What C99Native really does (measured on 2026-10-22)

Probe run with `tools/gmb_c99native_run.sh /tmp/gmbprobe/ui3.bas c99ui3s` and
`native/gmb_compiler_c/build/gmb_c99native_driver … --no-exe`.

| Construct | C99Native outcome | Notes for the codegen |
|---|---|---|
| `SCREEN 12, 320, 200, 32` + `WINDOW PUMP` | **Works** | The log says `window 320x200 (logical)`, software renderer. |
| `WINDOW SET TITLE "…"` | **Works** | Emits `gmb_winstmt_set_title("", …)`. |
| `BUTTON x,y,w,h,"…" ON CLICK handler` | **Works** | With `NAME`/`STYLE` it goes through `gmb_form_add_button_css`, which also applies `name=`, `tabindex=`, etc. |
| `TEXTBOX x,y,w,h INTO var` | **Works only with `INTO` + a variable declared `STRING`** | Without `INTO`: `CONTROL 'TEXTBOX' non supportato dall'emitter nativo`. With `DIM var` (numeric) the string pointer ends up in a `double`: wrong. The codegen must emit `DIM var AS STRING` at module level. |
| `CHECKBOX x,y,"…" INTO var` | **Pixel path present in the current source** (`gmb_control_add_checkbox`) | The property is applied with `gmb_tui_last_handle()`: in graphics that value is the last TUI control, not the checkbox just created. `NAME`/`TABINDEX` are lost. To be fixed (O2). Fixed size 200×22: the VB6 layout is not respected. |
| `LABEL …` | **TUI only** | `gmb_label_init` registers a TUI control; there is no `gmb_form_add_label`. In a pixel form it does not appear. To be implemented (O1). |
| `PIXCOMBOBOX` / `PIXLISTBOX` | **Parser: `statement non riconosciuto`** | The branches exist in the emitter but are not reachable from the syntax. Pixel ComboBox/ListBox are missing (O3). |
| `ON WINDOW CLOSE GOSUB handler` | **Works** | Without `GOSUB` the emitter emits `gmb_window_unregister_event("CLOSE")`; the correct form is with `GOSUB`/`GOTO`. |
| `WINDOWCLOSED()` | **Works** | `gmb_form_closed()` in the loop. |
| `TIMER` / `TI` in an expression | **Not available** | `TIMER` stays an undefined C identifier; `TI` is not resolved to `gmb_timer()` in an expression. A Timer needs a builtin/control (O6) or another time source. |
| Form with `FRAME`/`PICTUREBOX` children | **No pixel container** | `gmb_form_add_frame`/`add_picturebox` do not exist; the children must be flattened or O4 is needed. |
| `#line` / C→BASIC map | **Done (O8)** | Every statement emits `#line <riga bas> "<file bas>"`; the driver accepts `--line-map <file.json>` with the anchors `c_line → file/riga`. Measured: `cc` on a MiniTest reports `toggle_parity.bas:5:55: error: ...` instead of `out.c:132`. |
| Multi-file `IMPORT` | **Works** | `Main.bas` with `IMPORT UI` + `Ui.bas` with `NAMESPACE UI`: the emitted C contains `gmb_user_frmShow()` and the body. The lookup is by namespace, not by file name; without `IMPORT` (or with the module in a subfolder) → `simbolo non trovato`, rc=4. |

### 1.2 What does NOT exist compared with VB6

- Pixel `LABEL`, `FRAME`/`PICTUREBOX` as containers, `TIMER` as a control.
- Single `OPTIONBUTTON` (`RADIO` exists as an option group).
- Control array (`Index`, `Load`/`Unload` of controls).
- `ME`, `DOEVENTS`, `Option Explicit`.
- VB6 property `Control.Property = valore`: clauses and `CONTROL SET/GET/GETS` are used.
- Access key `&` in captions: to be verified; do not assume the VB6 semantics.
- Executable VB6 encoding in the `.frm`: the file is compatible as a **layout**, not as code.

These absences are not all "bugs to close" for the MVP: the designer must **reject them or
flag them** explicitly. Section 6 lists the ones that the first flow really needs.

---

## 2. What already exists in GMBasicIDE (and what must be adapted)

| Area | Status | Evidence | Work for GMBasic |
|---|---|---|---|
| Shell, theme, menus, toolbar, status bar | Ready | `VBTheme`, `VBMenuBarView`, `VBToolbarView`, `VBEditorWindow` | Reuse; `GMBASIC — C99Native` mode indicator. |
| Project Explorer | Ready for `.vbp` | `VBProjectOutlineView` | Folder nodes, `.frm` (form), `.bas` (code), `.inc`, generated `.ui.bas`; dirty marker. |
| Code editor | Ready, VB6 highlighting | `VBCodeEditor`, `vb_code_highlight` with fixed `keywords[]` | GMBasic language mode + generated table; `SUB`/`FUNCTION` handlers; no automatic `Private Sub`. |
| `.bas` modules | Already editable and saved | `codeForItem:` + `_moduleCodes` + `saveModules` | Move the cache and the saving into `src/core/` (`VbDocument`), so `vbtool` tests them without windows. |
| Designer | Ready, twip coordinates, multi-selection, z-order, tab order | `VBDesignerView`, `vb_model.h` | Reuse `VbForm` as the `.frm` model; filter of supported controls; twip→pixel scale. |
| Properties | Ready, VB6 property names | `VBPropertyPanel` | VB6 → GMBasic clause mapping; disable what does not exist; do not lose the values in the `.frm`. |
| Project | `.vbp`/`.frm` | `vb_parse.c`, `vb_project_*` | **Reuse `vb_form_parse`/`serialize`** for the `.frm`; new `VbGmbProject` for manifest and documents. |
| Run | In-process VB6 interpreter | `VBRuntimeWindow`, `vb_run_event` | **Replace** with the external `gmb run` process (C99Native); no VB6 interpreter for GMBasic. |
| Build | Absent | no `NSTask`/`fork` in the project | `VBProcessRunner` (Cocoa) + command specification in core. |
| Diagnostics | Absent | `-diagnostics` is only a status string | Error model, Output/Error List panel, click-to-line. |
| Breakpoint/step | VB6 only | `VbForm.breakpoints`, gutter | **Disable** in GMBasic mode until a real DAP exists. |
| Form round-trip | VB6 `.frm` | `vb_form_parse`, `vb_form_serialize` | Preserve EOL (CRLF/LF), non-Font `BeginProperty` blocks, unknown properties; test with a real VB6 form. |
| Headless harness | Ready | `vbtool check` | New `gmb *` sections; no window. |

Important note: the modules are already editable and saved, but the cache lives in the Cocoa window
(`_moduleCodes`, `saveModules`). For GMBasic — where every file is a module and the project is a
folder — this logic must move into a C project document, testable from `vbtool`.

---

## 3. Proposed architecture

```
GMBasicIDE (Cocoa)
  ├─ shell / designer / editor / properties          (riuso)
  ├─ VbGmbProject       src/core/vb_gmb_project.c    manifest + .frm + .bas + .inc
  ├─ VbGmbLanguage      src/core/vb_gmb_lang.c       keyword table generata, highlight, handlers
  ├─ VbGmbCodegen       src/core/vb_gmb_codegen.c    .frm -> .ui.bas + stub handler
  ├─ VbGmbBuild         src/core/vb_gmb_build.c      comandi, env, diagnostica, mappa righe
  ├─ VbDocument         src/core/vb_document.c       cache/testo/dirty/salvataggio
  └─ VBProcessRunner    src/platform/mac/...         NSTask, pipe, stop, log
        │
        │  contratto CLI (nessun linking)
        ▼
   GMBasic C99Native: gmb check/run/build --json
```

### 3.1 Layout of a GMBasic project managed by the IDE

```
HelloForm/
  HelloForm.gmbide.json      # manifest IDE: form, codice, entry, backend c99native
  app.settings.json          # impostazioni compilatore GMBasic (convenzione esistente)
  frmMain.frm                # layout VB6 (sorgente di verità del designer)
  frmMain.bas                # handler GMBasic dell'utente
  frmMain.ui.bas             # GENERATO: creazione controlli + frmMain_Show()
  Main.bas                   # SUB Main, generato una volta poi utente
```

Rules:

- `frmMain.frm` is the only source of the layout. It contains `VERSION 5.00`, `Begin VB.Form`,
  the VB6 controls with twip properties and the code section with only the `Attribute`s and a
  comment `' GMBasic code: frmMain.bas`. It is a `.frm` **valid for VB6 as a layout**.
- `frmMain.bas` is the only source of the form code. It is never overwritten.
- `frmMain.ui.bas` is generated, deterministic and regenerable; it is not edited by hand.
- `Main.bas` is created once (with `SUB Main` calling the startup form) and then it belongs to
  the user. If it is missing, the IDE recreates it only after confirmation.
- The GMBasic compiler ignores `.frm` and `.gmbide.json`; it only sees `.bas`/`.inc`.
- The `.frm` does **not** contain executable GMBasic code. This is a deliberate choice:
  - it avoids two copies of the same code (`.frm` and `.bas`);
  - it keeps the `.frm` openable from VB6 without syntax errors;
  - it allows importing an existing VB6 `.frm` and showing its VB6 code read-only,
    without pretending that it is compilable by GMBasic.

### 3.2 `HelloForm.gmbide.json` manifest

```json
{
  "version": 1,
  "name": "HelloForm",
  "language": "gmbasic",
  "backend": "c99native",
  "entry": "Main.bas",
  "startupForm": "frmMain",
  "forms": [
    {
      "layout": "frmMain.frm",
      "code": "frmMain.bas",
      "generated": "frmMain.ui.bas"
    }
  ]
}
```

- The paths are relative to the project folder.
- The manifest contains no absolute paths nor credentials; the toolchain root and
  `GMBASIC_DEV_MODE` live in the app settings.
- If the manifest is missing, the IDE can infer the entry (`Main.bas` or the first `.bas`) and show
  a warning; it must not guess the forms.
- The commands are not in the manifest: the IDE uses `gmb` from the settings, with a configurable fallback.

Why not a `.vbp`: a `.vbp` listing GMBasic `.bas` files is not compilable by VB6 and
would betray the compatibility expectation. The `.frm` is the compatible artifact; the project
remains a folder with a manifest. (Import/export of `.vbp` as a container of layouts only: P2.)

Why not a `.gmbform` JSON: the `.frm` already has all the layout, the z-order, the tab order and the
properties; a second format would be a copy to keep in sync, against the request for a VB6
format.

### 3.3 `.frm` round-trip

The existing parser/serializer is the starting point, but a real VB6 `.frm` needs:

- **EOL**: the parser normalizes to LF; the serializer writes LF. A VB6 `.frm` is normally CRLF.
  Add `VbForm.eol` (detected at load) and use it in `vb_form_serialize`; new forms in
  GMBasic mode use CRLF. The current HelloWorld fixture is LF and must stay byte-identical.
- **Unknown properties**: the parser keeps them in `VbProp`; the serializer writes them back.
  Verify that the order and spacing stay stable enough for an identical second save;
  for an imported `.frm` a normalization on the first save is acceptable,
  provided that no property is lost.
- **Non-Font `BeginProperty`**: today the serializer writes only the `Font` block; other blocks
  (e.g. `Picture`, `OLEDragImage`) become flat properties and lose the structure. Either the raw
  block is preserved, or the form is rejected with an explicit error. Do not silently save
  a `.frm` that VB6 would not reopen.
- **Code**: the code section of the `.frm` is preserved as it is. For a new form it is the
  comment/attributes; for an imported VB6 `.frm` it is VB6 code read-only.
- **Form name**: the form name is the one of the `Begin VB.Form` (e.g. `frmMain`); the `Attribute`s
  must match. The designer does not rename the form without updating the `.frm` and the
  manifest.

Acceptance: `vb_form_load` → `vb_form_save` on a CRLF fixture with `Font` and unknown
properties does not lose properties; the second save is byte-identical. The HelloWorld fixture
stays byte-identical on the first save.

### 3.4 Codegen contract

`frmMain.ui.bas` (generated):

```basic
' frmMain.ui.bas — GENERATED by GMBasicIDE. Do not edit.
' source: frmMain.frm   code: frmMain.bas   hash: <sha256 del layout>
IMPORT WINDOWS
IMPORT CONTROLS

DIM txtNome AS STRING

SUB frmMain_Show()
    SCREEN 12, 640, 480, 32
    WINDOW SET TITLE "Form1"

    BUTTON 24, 24, 120, 32, "OK" NAME "cmdOK" TABINDEX 0 ON CLICK frmMain_cmdOK_Click
    TEXTBOX 24, 80, 240, 28 INTO txtNome NAME "txtNome" TABINDEX 1
    LABEL 24, 124, 240, 20, "Nome" NAME "lblNome" TABINDEX 2

    CALL frmMain_Load()
    ON WINDOW CLOSE GOSUB frmMain_Unload
    DO WHILE WINDOWCLOSED() = 0
        WINDOW PUMP 16
    LOOP
END SUB
```

`frmMain.bas` (user, stubs created once):

```basic
SUB frmMain_Load()
END SUB

SUB frmMain_cmdOK_Click()
END SUB

SUB frmMain_Unload()
END SUB
```

`Main.bas`:

```basic
IMPORT WINDOWS
IMPORT CONTROLS

SUB Main()
    CALL frmMain_Show()
END SUB
```

Rules:

- **Handler names**: `frmX_<control>_<event>` (e.g. `frmMain_cmdOK_Click`). GMBasic does not have the
  VB6 form scope and top-level `SUB`s are global; the prefix avoids collisions.
  `Form_Load`/`Form_Unload` become `frmX_Load`/`frmX_Unload`.
- **Value variables**: `DIM <control> AS STRING` at module level in `.ui.bas` for TextBox,
  ComboBox/ListBox (when available), DatePicker; `DIM <control> AS DOUBLE`/`BOOLEAN` for
  CheckBox, Radio, Slider. The top-level `DIM` is verified in a single file; cross-file
  visibility must be verified with the project (O7). Documented fallback: access with
  `CONTROL GET/GETS "<name>", "text"`.
- **Events**: the codegen wires the events provided by the mapping table; it creates the stubs in
  `frmMain.bas` only if they are missing. If the user deletes a wired handler, `Check` must give a
  clear error ("SUB ... not defined"), not a crash.
- **Order**: the emission order is the z-order of the `.frm` (children after the container, as in the
  file). For flattened Frame/PictureBox, the children coordinates are made absolute.
- **Tab order**: VB6 has one `TabIndex` per container; GMBasic has a global order per
  window. The codegen computes a stable DFS order: container, then children by `TabIndex`,
  then following siblings; it assigns global `TABINDEX` 0..n-1. Document the difference.
- **Generated**: `frmMain.ui.bas` never contains `SUB Main`, user handlers or custom
  code. The hash in the header allows detecting a manual change: the IDE warns and
  does not overwrite without confirmation.
- **Determinism**: same `.frm` + same generator version → same `.ui.bas` byte for
  byte. The golden tests verify it.

### 3.5 Project and file discovery

The compiler **is not single-file**: `IMPORT <NS>` plus `NAMESPACE <NS>` in the imported module
are enough to compile several `.bas` together (section 1.1). The native driver looks for modules in the
program directory, in `include/` going up, in `Controls/` and in `<root>/libs/*`; it does **not**
descend into subfolders. The C# `Orchestrator` is wider, but it remains `IMPORT`-driven: a file
that nobody imports is not compiled.

**Aligned on 2026-10-24.** The native driver (`gmb_inc_resolve.c`) now does the same things as
the C# Orchestrator: **recursive** scan of the root, `importFolders`/`importFiles` from the
settings, `IMPORT` graph followed on **every** loaded file (the dependency is emitted before the
dependent one), name shadowing between project and global folders. They were five measured
divergences: flat dir, imports only from the entry, `forms/` invisible, `app.settings.json`
ignored, no transitivity.

The `include/`, `Controls/`, `<install>/include`, `<install>/Controls`,
`<install>/libs/*` folders remain, but as **defaults** of `compiler.searchFolders` in the settings of the
**compiler** (not of the app): `{project}`/`{install}` as placeholders, trailing `*` = every
subfolder, absolute paths allowed, recursive scan of every entry. A project overrides them from
`app.settings.json`; `ImportFolders`/`ImportFiles` keep living in the app
settings and are resolved both against the project and against the install root.

Measured on the `samples/modules/FolderResolution` sample: C# and C99Native load the **same 8
modules** and compile; the native C prints one line per folder. Gate:
`native/gmb_c99native/tests/c99_module_resolve_test.sh` (15 assertions: recursion, import
folder/file outside the root, glob, transitivity, exclusion of non-imported, order, shadowing,
defaults, absolute paths and `searchFolders: []`).

**Project CLI: implemented (2026-10-24).** `native/gmb_compiler_c/src/cli/gmb_cli.c`,
CMake target `gmb_cli` → `build/gmb`:

```
gmb check --project <dir> [--entry <f>] [--json] [--strict]
gmb build --project <dir> [--entry <f>] [--out <exe>] [--json]
gmb run   --project <dir> [--entry <f>] [--json]
```

The discovery is not duplicated: the CLI calls `gmb_project_sources` / `gmb_resolve_module_files`
(the same as the compiler). Explicit entry or single `SUB Main` (otherwise error with the
candidates); `work/`, `bin/`, `obj/`, `.git/`, `.vs/`, `.build/`, `node_modules/` excluded;
missing `NAMESPACE` = warning (`--strict` turns it into an error); **unresolved `IMPORT` = error**
with `file:riga`, recognizing the builtin namespaces (`gmb_ns.h`: `IMPORT TRADING`, `CONSOLE`…
do not require a file). Stable exits: 0 ok · 2 project/compilation · 3 parse · 4
emission/cc · 5 link · 6 runtime. Gate: `native/gmb_compiler_c/tests/gmb_cli_project_test.sh`
(14 assertions).

Declared limit of the link: `build` links `libgmb_runtime.a` + `-lm -lpthread`, enough for
console programs. Those who use chart/db/AI/torch require the complete recipe of
`tools/gmb_c99native_run.sh`; the CLI says so explicitly when the runtime archive is missing.

### Suite verification: `gmb minitests`

```bash
build/gmb minitests                        # riepilogo + istogramma CC-FAIL per categoria
build/gmb minitests --json > oggi.json     # per il diff fra due run
build/gmb minitests --baseline ieri.json   # rosso SOLO per le regressioni
build/gmb minitests --filter trading/ --cc clang   # sottoinsieme / A-B del compilatore

tools/check_minitests.sh                   # GATE: usa il baseline versionato
tools/check_minitests.sh --update          # rigenera il baseline dopo un fix
```

It replaces the ad hoc sweep of `tools/c99_compile_errors.sh` (which is now an alias delegating
here): ordered recursive discovery, **the same `-I` recipe as `build`** (`build_inc()`), TAG
`interactive`/`heavy` excluded as before, exit 0/2/1. No link, no execution: it measures
the codegen. Command gate: `native/gmb_compiler_c/tests/gmb_cli_minitests_test.sh` (31).
Measurement 2026-10-24: 1948 files, 1914 executed, **1830 PASS · 37 CC-FAIL · 47 DRIVER-FAIL · 0 crash**.
After the `LIBRARY` fix (same day, §12): **1831 PASS · 36 CC-FAIL · 47 DRIVER-FAIL · 0
crash**.
Versioned baseline: `native/gmb_compiler_c/tests/minitests_baseline.json` (83 known FAIL); the
gate is `tools/check_minitests.sh` (exit 0 no regression, 2 regression, 1 setup).


**No concatenation bridge**: the codegen already generates importable modules (`NAMESPACE` in
`frmX.ui.bas` + `IMPORT` in `frmX.bas`/`Main.bas`), so the demo can run on a flat dir
without waiting for O7.

---

## 4. VB6 → GMBasic mapping

### 4.1 Form and window

| VB6 | GMBasic | Status |
|---|---|---|
| `Form` | `SCREEN 12, w, h, 32` + controls + `WINDOW PUMP` | Works; w/h in pixels. |
| `Caption` | `WINDOW SET TITLE "…"` | Works. |
| `ClientWidth`/`ClientHeight` | `SCREEN` arguments | twip / 15, rounded; test. |
| Window `Left`/`Top` | `WINDOW MOVE` | P2. |
| `BorderStyle` | `WINDOW FRAME SINGLE/DOUBLE/ROUNDED` | To be verified on the `SCREEN` path; P1. |
| `BackColor` | `STYLE "bg=#RRGGBB"` / window background | To be verified for the window; P1. |
| `Font` | `STYLE` / font API | Only if the runtime exposes it per control; P1. |
| `StartUpPosition` | `WINDOW MOVE` | P2. |
| `ScaleMode` | always pixels | Fixed conversion 15 twip = 1 px. |
| `Caption` with `&` | literal text | Verify access key; P2. |

### 4.2 Controls

| VB6 | GMBasic | Status |
|---|---|---|
| Label | `LABEL x, y, w, h, "testo"` | **P0 upstream O1**: today `LABEL` is TUI only and has no height. |
| TextBox | `TEXTBOX x, y, w, h INTO var$` | Works with `DIM var AS STRING`; without `INTO` the emitter rejects it. |
| CommandButton | `BUTTON x, y, w, h, "caption"` + `ON CLICK` | Works; `NAME`/`TABINDEX` via CSS-like spec. |
| CheckBox | `CHECKBOX x, y, "caption" INTO var` | Pixel present but fixed size and properties not applied (O2). |
| OptionButton | `RADIO` (group) | **Not equivalent**: a group, not one control per option. O5 or explicit rejection. |
| ComboBox | `COMBOBOX` (TUI) / `PIXCOMBOBOX` (unreachable emitter) | **P1 O3**: the parser rejects the pixel variant. |
| ListBox | `LISTBOX` (TUI) / `PIXLISTBOX` (unreachable emitter) | **P1 O3**. |
| Frame | — | **No pixel container** (O4). MVP: flattened children + label/border; P1: real container. |
| PictureBox | `CANVAS` / `IMAGE` | Drawing only; no children (O4). |
| HScrollBar / VScrollBar | `SCROLLBAR`; `PIXSLIDER` is emitter only | Adaptation (orientation from w/h); the parser does not reach `PIXSLIDER` (as in O3). |
| Timer | — | **No control**; `TIMER`/`TI` are not usable expressions (O6). The designer Timer stays disabled until O6 exists. |
| Data / DataGrid | `GRID`; `PIXGRID` is emitter only | P2, without ADO binding; parser to be verified. |
| Menu | `MENU` (TUI); `PIXMENU` is emitter only | P2; Menu Editor absent. |
| CommonDialog | `FILEDIALOG`/`PRINTERDIALOG`/`COLORPICKER`/`DATEPICKER` | By type; P2. |
| StatusBar | `STATUSBAR` | Direct, limited panels; P1. |
| ProgressBar | `PROGRESS` | Direct; P1. |
| Image | `IMAGE` | Pixel-only; P1. |
| Line / Shape | `LINE` / `CIRCLE` / `PSET` | They are not controls; P2. |
| DriveListBox / DirListBox / FileListBox | `FILEEXPLORER` | Adaptation; P2. |
| ListView | `DATALIST` / `TBROWSE` | Adaptation; P2. |
| TreeView | `TREEVIEW` (TUI); `PIXTREEVIEW` is emitter only | P2; pixel to be verified. |
| RichTextBox | `RICHTEXT` | Pixel-only; P2. |
| UpDown | `NUMERICUPDOWN` / `SPINNER` | Adaptation; P2. |
| SSTab | `TAB` | Adaptation; P2. |
| Slider (not standard VB6) | `SLIDER` | Direct; P2. |
| Chart | `CHART` | Pixel-only; P2. |
| OLE / Toolbar / CoolBar / ImageList | — | **Not supported**: explicit rejection. |

The P1/P2 rows are not all verified: `PIX*` in the emitter does not imply that the parser
accepts it (this is the case of O3). The designer enables a control only after a capability probe
in the project; the untested items stay disabled with a tooltip.

### 4.3 Properties

| VB6 | GMBasic | Notes |
|---|---|---|
| `Name` | `NAME "…"` clause / `name=` in the spec | Mandatory for the handlers and `CONTROL *`. |
| `Caption` | creation argument / `caption` / `CONTROL SET` | For Label/Button it is the argument; for the form `WINDOW SET TITLE`. |
| `Text` | `INTO` variable / `CONTROL SET "x", "text"` | For TextBox the value is the variable; initial text assigned before creation. |
| `Visible` / `Enabled` | `VISIBLE n` / `ENABLED n` / `visible=`/`enabled=` | The base spec supports both; test for every type. |
| `TabIndex` / `TabStop` | `TABINDEX n` / `TABSTOP n` | Global order computed (see 3.4). |
| `ToolTipText` / `Tag` | `TOOLTIP "…"` / `TAG "…"` | Supported in the base spec. |
| `Left`/`Top`/`Width`/`Height` | creation arguments / `CONTROL MOVE` | twip / 15. |
| `BackColor`/`ForeColor`/`Font` | `STYLE "bg=#… fg=#… font=…"` | Only the tokens supported by the runtime; verify. |
| `BorderStyle`/`Locked`/`MaxLength`/`PasswordChar` | `STYLE` or `CONTROL SET` | To be verified per type; otherwise "non mappato" badge. |
| `Index` (control array), `DataSource` | — | Not supported; explicit error. |

### 4.4 Events

| VB6 | GMBasic | Notes |
|---|---|---|
| `Form_Load` | call to `frmX_Load()` in `frmX_Show()` | Generated stub; if missing, clear compilation error. |
| `Form_Unload` | `ON WINDOW CLOSE GOSUB frmX_Unload` | Verified: `GOSUB` is required; without it the emitter unregisters. |
| `Click` / `DblClick` | `ON CLICK` / `ON DBLCLICK` | `Click` verified on Button; the others to be tested. |
| `Change` | `ON CHANGE` | Direct; test. |
| `KeyPress`/`KeyDown`/`KeyUp` | `ON KEYPRESS`/`ON KEYDOWN`/`ON KEYUP` | **Without arguments**: `KeyAscii` does not arrive; use the state getters. |
| `MouseDown`/`MouseUp`/`MouseMove` | `ON MOUSEDOWN`/… | **Without** button/x/y; use `MOUSE.X/Y/BUTTON`. |
| `GotFocus`/`LostFocus` | `ON GOTFOCUS`/`ON LOSTFOCUS` | Direct; test. |
| `Validate` | `ON VALIDATE` | Direct; test. |
| `DragOver`/`DragDrop` | `ON DRAGOVER`/`ON DRAGDROP` | Direct; test. |
| `Form_Resize`/`Activate`/`Deactivate` | `ON WINDOW RESIZE/ACTIVATE/DEACTIVATE GOSUB` | To be verified on the SDL runtime. |
| `Timer` | — | O6; no native event. |

---

## 5. Omissions to implement (upstream prerequisites)

Items O1–O9 are changes to the **GMBasic** repo; O10 is a change to the **GMBasicIDE core**.
Without them, the IDE can only work with a declared subset.

| ID | Omission | Priority | Acceptance criterion |
|---|---|---|---|
| O1 | **Pixel Label** | P0 | `gmb_form_add_label(x,y,w,h,text)` / `gmb_control_add_label`; syntax `LABEL x,y,w,h,"text"` (5 arguments) distinct from the 4-argument TUI one; text drawn with theme, `NAME`/`STYLE`/`VISIBLE` applied; SDL dummy test. |
| O2 | **Complete pixel CheckBox** | P1 | API with `w,h` (or `_ex`); `NAME`/`TABINDEX`/`VISIBLE`/`ENABLED` applied to the returned widget, not to `gmb_tui_last_handle()`; rendering and property test. |
| O3 | **Pixel ComboBox/ListBox** | P1 | The parser accepts the pixel variant (or makes `COMBOBOX`/`LISTBOX` polymorphic with `SCREEN`); `INTO`; runtime `gmb_form_add_dropdown`/`listbox`; test. |
| O4 | **Frame/PictureBox container** | P1 | Pixel container with children, clipping, offset and `Enabled=False` that disables the children; codegen and designer; nested test. MVP without O4: explicit flattening with badge. |
| O5 | **Pixel OptionButton** | P1 | Single control with group (or radio-group API with explicit layout); VB6 semantics; test. Without O5: explicit rejection. |
| O6 | **Timer** | P1 | `TIMER()` builtin in ms (or `TIMER` control) usable in an expression; the codegen generates the interval dispatch; test. Without O6: Timer disabled. |
| O7 | **Project and file discovery** | **Done 2026-10-24** | Aligned resolution (`gmb_inc_resolve`: recursion, `importFolders`/`importFiles`, `compiler.searchFolders`, transitive `IMPORT`, shadowing) **and** `gmb check/build/run --project` CLI: exclusions (`work/`, `bin/`, `obj/`, `.git/`, …), explicit entry or single `SUB Main`, missing `NAMESPACE` = warning (`--strict` = error), unresolved `IMPORT` = `file:riga` error with the builtin namespaces recognized, `--json`, exit 0/2/3/4/5/6. Plus `gmb minitests`: **single point of suite verification** (driver → C → `cc -fsyntax-only`) with `--baseline` to be red only on regressions; gate `tools/check_minitests.sh` + versioned baseline `native/gmb_compiler_c/tests/minitests_baseline.json`; `tools/c99_compile_errors.sh` is now only an alias. Gate: `c99_module_resolve_test.sh` (15) + `gmb_cli_project_test.sh` (14) + `gmb_cli_minitests_test.sh` (31), sample `samples/modules/FolderResolution`. Limit: the link is console-only (`libgmb_runtime`), chart/db/AI require the recipe of `tools/gmb_c99native_run.sh`. |
| O8 | **C→BASIC diagnostic map** | **Done 2026-10-24** | Both implemented: `#line` per statement and `--line-map <json>`. `gmb_resolve_includes_mapped` produces the segments (imported module + entry file), the emitter uses them and `c99_fail` cites `file:riga`. Test: `native/gmb_c99native/tests/c99_line_map_test.sh` (8 assertions). Only the exposure in `gmb check --json` is left, which is O9. |
| O9 | **Stable `gmb` CLI** | **P0** | `check/run/build --json`, stable exit codes, stdout data/stderr progress, `doctor`, driver version, full link. |
| O10 | **VB6 `.frm` round-trip** | **P0** (GMBasicIDE) | EOL preserved (CRLF/LF), unknown properties kept, non-Font `BeginProperty` blocks preserved or rejected; identical second save; CRLF fixture. |

### 5.1 Priority for the first flow

**Real P0:** O1, O9, O10 — **O8 is done** (2026-10-24). With these one can build a form with
Label/TextBox/Button, compile it and launch it from C99Native, with clickable errors. The multi-file
resolution is not a prerequisite: it already exists (section 1.1).

**P1:** O2, O3, O4, O5, O6 — **O7 is done** (resolution + project CLI). They complete
most of the VB6 forms.

**P2:** Data/DataGrid, Menu Editor, vector graphics, debug.

---

## 6. Work plan

### G0 — Toolchain contract and C99Native omissions (P0, upstream)

- [ ] **G0.1** `gmb` CLI with `check`, `build`, `run`, `doctor`; `--project <dir>`, `--json`,
      `--out <dir>`, `--backend c99native` (the only accepted value in the MVP), `--headless`
      for the tests (`SDL_VIDEODRIVER=dummy`).
- [x] **G0.2** Project (O7): recursive discovery + settings (C#/native parity), CLI
      `gmb check/build/run --project` with exclusions, entry, `NAMESPACE`/`IMPORT` and `--json`.
      The link is console-only: the complete recipe (`tools/gmb_c99native_run.sh`) is still to be
      integrated for chart/db/AI (see O7).
- [~] **G0.3** Structured diagnostics: the C→BASIC part (O8) is done — `#line`, `--line-map`,
      `emissione: file:riga`. What is left is the JSON format `{"file","line","column","severity",
      "code","message"}` and the exit code table (0 ok, 2 compilation, 3 parse, 4
      emission, 5 link, 6 runtime) in the CLI, that is O9.
- [ ] **G0.4** Fresh and declared driver: `gmb doctor` builds/verifies the driver, prints
      version/hash and does not use a stale `tools/gmb_c99native_driver`.
- [ ] **G0.5** Pixel Label (O1); test with `SDL_VIDEODRIVER=dummy` and with a real window.
- [x] **G0.6** `#line`/map (O8): done — `#line` per statement, `--line-map`, emission errors
      with `file:riga`; the gate is `native/gmb_c99native/tests/c99_line_map_test.sh`.
      The test on a real C error is `gmb minitests` (the alias `tools/c99_compile_errors.sh`
      delegates there), which now prints the `.bas`.
- [ ] **G0.7** Document the C99Native matrix (section 1.1) in `Guide/utente/`; no
      IL promises.
- [ ] **G0.8** (P1) O2–O6 in order: checkbox properties, combo/list, frame, option button,
      timer.

**IDE fallback** (if G0 is not available right away): configurable commands + tolerant diagnostics
parser. It is not the target and must be flagged in the logs; the concatenation is no longer needed.
**Acceptance:** a project with `Main.bas`, `frmMain.ui.bas`, `frmMain.bas` compiles and runs;
an error in `frmMain.bas` is reported with file/line; `gmb doctor` says what is missing.

### G1 — Project model and `.frm` (P0)

- [ ] **G1.1** `VbGmbProject` in `src/core/vb_gmb_project.c/.h`: folder loading, `.gmbide.json`
      manifest, `.frm`/`.bas`/`.inc` discovery, exclusions; layout→code→generated
      association.
- [ ] **G1.2** Reuse of `vb_form_load`/`vb_form_save` for the layout; no `.gmbform` adapter.
- [ ] **G1.3** O10: `VbForm.eol`, preservation of unknown properties, non-Font `BeginProperty`
      blocks; CRLF fixture; identical second save test.
- [ ] **G1.4** `VbDocument`: path, text, dirty, kind, encoding/EOL, atomic save; move here
      the `_moduleCodes`/`saveModules` cache that is today in `VBEditorWindow.m`.
- [ ] **G1.5** GMBasic Project Explorer: folders, `.frm` (form), `.bas` (code), `.inc`,
      `.ui.bas` (generated, read-only); double click, dirty marker, Save All.
- [ ] **G1.6** Import of a VB6 `.frm`: layout in the designer; VB6 code in a read-only tab
      with banner "not compiled by GMBasic"; no translation.
- [ ] **G1.7** (P2) `.vbp` import/export as a list of layouts, not as a compilable project.

**Acceptance:** `vbtool gmb-project <fixture>` lists form/code/entry; opening, modifying and
saving leaves the `.frm` with no losses; the imported VB6 code is not rewritten.

### G2 — GMBasic language mode (P0)

- [ ] **G2.1** Keyword table generator from the **C99Native lexer** + soft keyword list
      of the parser; registry for the procedure/command/constant names. Output
      `src/core/vb_gmb_keywords.h` with source hash/version.
- [ ] **G2.2** `VbCodeLanguage` (`VB6`, `GMBASIC`) and `vb_code_highlight(..., language, out)`;
      comments `'`/`REM`, continuation `_`, strings, numbers, operators, labels, line numbers,
      hard/soft keywords; case-insensitive.
- [ ] **G2.3** GMBasic outline and procedure combo: `SUB`, `FUNCTION`, `PUBLIC CLASS`, `TYPE`,
      `ENUM`, `NAMESPACE`, `GENERATOR`, labels; no VB6 `Property Get/Let`.
- [ ] **G2.4** Handler template: `SUB frmX_<control>_<event>()` / `END SUB`; the events combo
      uses the GMBasic names and the form prefix; no automatic `Private Sub`.
- [ ] **G2.5** In GMBasic mode disable the breakpoint gutter, F8/F10, VB6 `Form_Load` and
      any VB6 auto-handler.
- [ ] **G2.6** (P1) Completion: keywords + project symbols + registry; if available, use
      `gmb ide --json`/LSP instead of duplicating the analysis.
- [ ] **G2.7** Synchronization test: `make check` fails if the table is older than the C99Native
      lexer or if the count does not match.

**Acceptance:** fixture `samples/GMBasic/Language.bas`; span snapshots; no VB6 keyword
colored by mistake; GMBasic handler inserted from the combo.

### G3 — Check / Run / Build / Output (P0)

- [ ] **G3.1** `VbGmbBuild` in core: command specification, cwd, env, timeout, stop, exit code,
      quoting; no hand-built shell string.
- [ ] **G3.2** Cocoa `VBProcessRunner` (`NSTask`, pipe, streaming, process group stop,
      termination) and `VBOutputView` in VB6 style (monospace, white background, no Aqua).
- [ ] **G3.3** Diagnostics parser: CLI JSON; fallback on `parse error`/`emissione`/`cc`;
      dedup, sort, click → `jumpToLine`; no invented lines.
- [ ] **G3.4** Commands and states: **Check** (C99Native, no link, multi-file), **Run**
      (C99Native, SDL window), **Build** (C99Native, executable), **Stop**; status bar
      `CHECKING`/`BUILDING`/`RUNNING`/`ERROR`.
- [ ] **G3.5** First-run `gmb doctor`: GMBasic root, driver, cc, cmake, runtime/SDL, license;
      Win98 dialog with the exact missing command; no crash.
- [ ] **G3.6** (P1) Log in `~/Library/Logs/GMBasicIDE/gmbasic.log`: command line, cwd, exit code,
      first/last lines; no secrets.
- [ ] **G3.7** Run environment: `GMBASIC_HEADLESS=0`, `SDL_VIDEODRIVER=cocoa` (default),
      `GMBASIC_DEV_MODE` only if enabled; headless test with `SDL_VIDEODRIVER=dummy`.

**Acceptance:** a file with an error shows file/line and the click jumps there; Run opens the
SDL window and Stop closes it without orphans; Build produces a binary or a clear error; the IDE stays
responsive.

### G4 — Designer → `.frm` → `.ui.bas` (P0/P1)

- [ ] **G4.1** The designer in GMBasic mode loads/saves `.frm` with `vb_form_load/save`;
      no layout sidecar.
- [ ] **G4.2** Mapping table as **data** in core (VB6 type → GMBasic verb, properties,
      `ok`/`adattato`/`non supportato` state, notes). It feeds designer, properties, codegen and
      documentation.
- [ ] **G4.3** Deterministic codegen `frmX.ui.bas`: header+hash, `DIM` value vars,
      `SCREEN`, `WINDOW SET TITLE`, controls in z-order, `frmX_Load`, `ON WINDOW CLOSE GOSUB`,
      `WINDOW PUMP` loop.
- [ ] **G4.4** Stubs in `frmX.bas` for the wired events, names `frmX_<control>_<event>`, never
      overwritten; `Main.bas` generated once.
- [ ] **G4.5** twip→pixel conversion: `VB_TWIPS_PER_PIXEL = 15`, integers, no drift;
      round-trip test.
- [ ] **G4.6** Properties panel: mapping of `Name`, `Caption`, `Text`, `Visible`, `Enabled`,
      `TabIndex`, `TabStop`, `ToolTip`, `Tag`, `BackColor`/`ForeColor`/`Font` (if supported);
      unmapped values stay in the `.frm` with a badge, they are not deleted.
- [ ] **G4.7** Controls: Label/TextBox/Button (P0); CheckBox (O2); ComboBox/ListBox (O3);
      Frame/PictureBox (O4, with P0 flattening); OptionButton (O5); Timer (O6); Data/DataGrid
      and Menu (P2). The unsupported ones are disabled in the toolbox with a tooltip.
- [ ] **G4.8** Stable global tab order (DFS) and z-order; test with a nested form.
- [ ] **G4.9** Badge "adattato"/"non supportato" in the designer; (P2) "Preview" launches `Run`.
- [ ] **G4.10** (P2) Reverse round-trip: parse of `.ui.bas` → `.frm` for code-first projects.
- [ ] **G4.11** (P2) Menu Editor limited to `MENU "File|Edit"`; no complete VB6 submenus.

**Acceptance:** golden `*.ui.bas`; caption/position change → save → green `Check` →
`Run` shows the control in the expected position; an unsupported control produces a clear
error and **not** invalid code; the second save is byte-identical.

### G5 — Debug (P2)

- [ ] **G5.1** In GMBasic mode the breakpoint gutter and F8/F10 are **disabled** with a
      message "GMBasic debugger not available"; do not reuse the VB6 break mode.
- [ ] **G5.2** Only if GMBasic exposes a real DAP (`--debug-tcp` is documented, but it has not been
      found in the current tree): DAP client with breakpoints, step, call stack, locals.
- [ ] **G5.3** Until then: Run/Stop, output, jump to the error line; no fake
      yellow bar.

### G6 — UX, documentation, packaging (P1/P2)

- [ ] **G6.1** "GMBASIC — C99Native" indicator in the title/status; contextual Project/Run
      menus; no meaningless VB6 items (Menu Editor, Components, Data).
- [ ] **G6.2** Settings dialog: GMBasic root, `gmb` path, fixed `c99native` backend,
      dev mode; persistence in Application Support, not in the project.
- [ ] **G6.3** Open folder / recent projects / drag&drop of a folder or of a `.bas`;
      (P2) document types in `Info.plist`.
- [ ] **G6.4** Documentation: "GMBasic mode" section in `AGENTS.md`, README with known limits,
      user guide with the mapping table and the `.frm`→`.ui.bas` flow.
- [ ] **G6.5** License/distribution: do not embed GMBasic; `GMBASIC_DEV_MODE` only in
      development; respect the compiler editions.

### G7 — Verification (P0)

- [ ] **G7.1** `vbtool check` sections (headless, C core only):
      - `gmb project`: manifest, discovery, form/code association, exclusions;
      - `gmb form`: `.frm` load/save, EOL, unknown properties, `BeginProperty`, CRLF fixture;
      - `gmb language`: spans (hard/soft keyword, comments, strings, types, numbers, labels),
        handlers, case-insensitive, no VB6 keyword;
      - `gmb codegen`: `.frm` → `.ui.bas` golden, z-order, tab order, twip, stub, hash,
        idempotence;
      - `gmb diagnostics`: parse/emission/cc with and without the map, multi-file;
      - `gmb mapping`: every VB6 control/property has a state; no silent fallback;
      - `gmb command`: quoting, env, temp dir, exit code; no process launched by the core.
- [ ] **G7.2** Fixtures in `samples/GMBasic/`:
      `HelloForm` (Label/TextBox/Button), `MultiFile` (Main+Ui+User), `ImportedVB6`
      (CRLF, Font, VB6 code), `SyntaxError`, `Unsupported` (Timer/OptionButton/control array),
      `Nested` (Frame/PictureBox).
- [ ] **G7.3** Golden files + diff; mutation test (break parser/codegen and verify that the
      test fails).
- [ ] **G7.4** Headless end-to-end: `gmb check`/`gmb run` with `SDL_VIDEODRIVER=dummy` on a
      fixture that does a few `WINDOW PUMP` and exits; asserted exit code and output.
- [ ] **G7.5** Manual visual inspection with `--window-snapshot` for output pane, error list,
      designer badge; no automatic GUI test (repo rule).
- [ ] **G7.6** (P2) Performance: opening 50 files < 200 ms, regeneration of 50 controls < 50 ms.

---

## 7. Operations matrix (C99Native)

| Operation | Target command | What it covers | Limits |
|---|---|---|---|
| **Check** | `gmb check --project <dir> --json` | C99Native front-end + emitter, multi-file, no link | does not execute; runtime errors not covered. |
| **Run** | `gmb run --project <dir> --json` | build+link+execution, SDL window | external window, not embedded; debug absent. |
| **Build** | `gmb build --project <dir> --out <dir> --json` | native executable | the full link depends on the GMBasic runtime. |
| **Headless test** | `gmb run --project <dir> --headless` | execution without a window (`SDL_VIDEODRIVER=dummy`) | does not test the real rendering. |
| **Doctor** | `gmb doctor --json` | driver, cc, cmake, runtime, SDL, license | does not install dependencies. |
| **Debug** | — | — | no verified DAP. |

Run environment: cwd = project folder, `GMBASIC_HEADLESS=0`, `SDL_VIDEODRIVER=cocoa` (default),
`GMBASIC_DEV_MODE=1` only in development. For Check/Build: same cwd, explicit `GMBASIC_ROOT`,
no `GMBASIC_HEADLESS=1`. Output: stdout data, stderr progress/diagnostics.

For **Run** the JSON must not mix with the app output: the CLI emits a single
startup/diagnostic event (or writes them to a control fd/file), then it leaves the child process
stdout/stderr raw. The IDE distinguishes the build phase (JSON diagnostics) from the execution (user output).

Operational note: the driver writes into `work/` next to the source if no `out.c` is passed;
the IDE must pass a temporary directory for **Check** and a build dir for **Build**, to avoid
dirtying the project. The project CLI must do the same.

---

## 8. Risks and mitigations

| # | Risk | Impact | Mitigation |
|---|---|---|---|
| R1 | A non-imported name used as a statement becomes an implicit variable without an error; and a module without `NAMESPACE` (or not reachable from an `IMPORT`) does not enter the build | medium | The **resolution** is aligned (recursion + settings + transitivity, 2026-10-24); O7/G0.2 is still open: mandatory `NAMESPACE` in the generated file and error on unresolved names. |
| R2 | Pixel Label absent; checkbox/combo/list incomplete | high | O1 P0, O2/O3 P1; the designer filters and declares the limits. |
| R3 | The GMBasic chrome is SDL, not VB6 | high | Designer = layout editor + codegen, not WYSIWYG preview; badge. |
| R4 | `.frm` compatible as layout, not as code | high | Document it; imported VB6 code read-only; no translation. |
| R5 | ~~`cc` errors without `#line`~~ — closed by O8 | — | `#line` + `--line-map`; only the generated glue is left (e.g. the call in `int main`) which has no BASIC line of its own. |
| R6 | Stale `tools/` driver | medium | G0.4 `doctor`; use the most recent build; version/hash. |
| R7 | Huge and evolving language surface | medium | Table generated from the C99Native lexer + registry; hash and synchronization test; pinned commit. |
| R8 | Recursive discovery of all `.bas` files | medium | Manifest + exclusions; warn about foreign files; no generated file in the root. |
| R9 | `app.settings.json`/runtime resolved against the cwd | medium | cwd = project, explicit `GMBASIC_ROOT`, command line log. |
| R10 | Licensing/dev mode | medium | Never `GMBASIC_DEV_MODE` by default; document it; no bypass. |
| R11 | Global handler names in GMBasic | medium | `frmX_` prefix; collision test between two forms. |
| R12 | Timer/debug promised and not available | medium | Disable with a message; O6/DAP only when real. |
| R13 | Twip/DPI and different sizes | low | Fixed conversion 15; test on real VB6 forms; no drift. |

---

## 9. Still open decisions

1. **Code section of the `.frm`**: layout-only with a comment (recommended) or embedded GMBasic
   code with a `.bas` mirror? The second duplicates the code and makes the `.frm` non-VB6.
2. **Manifest**: `.gmbide.json` (recommended) or `.vbp` as container? The `.vbp` with GMBasic
   `.bas` is not compilable by VB6; use it only for layout import/export (P2).
3. **Frame/PictureBox**: flattening with absolute coordinates (P0) or real pixel container
   (O4, P1)? Recommended: flattening for the demo, O4 right after.
4. **ComboBox/ListBox**: implement O3 before the demo or limit the demo to
   Label/TextBox/Button/CheckBox? Recommended: O3 in P1, demo with the four basic controls.
5. **Timer**: `TIMER()` builtin (O6) or generation of a loop with `SLEEP`/`TI`? Recommended:
   O6, because `TI`/`TIMER` today are not usable expressions.
6. **CLI**: upstream `gmb` (recommended) or local wrapper calling driver+script? The wrapper
   adds neither project nor line map; it is fine only as a bridge.
7. **Handler naming**: `frmX_<control>_<event>` (recommended) or namespace per form? The GMBasic
   namespace exists but must be verified on the linker and on the codegen.
8. ~~**Bridge build**~~: closed. Multi-file `IMPORT` works; the generated file declares `NAMESPACE` and
   whoever uses it emits `IMPORT`. O7 is still open for file list/exclusions/entry.

---

## 10. Recommended first increment (end-to-end demo)

1. **O1 + O9** — Pixel Label and minimal CLI (O8 diagnostic map is done; O7 project right after).
2. **G1** — open a folder with `HelloForm.gmbide.json`, `frmMain.frm`, `frmMain.bas`,
   `Main.bas`; Explorer with form/code/generated.
3. **G2** — GMBasic language mode and highlighting; `frmMain_cmdOK_Click` handler generated.
4. **G3** — Check with clickable errors, C99Native Run, Output, Stop.
5. **G4** — designer on `.frm`: Label/TextBox/Button, caption/position change, deterministic
   `.ui.bas`, handler stubs.
6. **G7** — `HelloForm` fixture + headless test; manual snapshot.

**Demo:** open `samples/GMBasic/HelloForm`, change the caption and move the button in the
designer, write `frmMain_cmdOK_Click`, press Check (no errors), Run (SDL window with
the button in the expected position), Stop; the `.frm` stays openable by VB6 as a layout and the
second save is identical.

---

## 11. Things NOT to do

- Do not use IL as the default or fallback: the target is C99Native; if a feature is missing, it is declared.
- Do not compile/link GMBasic inside GMBasicIDE: the boundary is the process.
- Do not implement the GMBasic semantics in `vb_code.c`: the VB6 interpreter stays for VB6
  projects and must not "almost work" with GMBasic.
- Do not translate VB6 code into GMBasic: the `.frm` is compatible as a layout, not as source.
- Do not put GMBasic code in the `.frm` by default: the file must stay a valid VB6 `.frm`.
- Do not use TUI controls (`LABEL`, `COMBOBOX`, `LISTBOX`) in a pixel form: they are not visible.
- Do not maintain 1285 keywords by hand: generate them from the C99Native lexer and test the synchronization.
- Do not invent file/line: if `cc` has no map, show the raw message.
- Never overwrite `Main.bas`, `frmX.bas` or a hand-modified `.ui.bas` without confirmation.
- Do not promise a VB6 preview of the GMBasic runtime: the window is SDL and the chrome is different.
- Do not introduce dependencies (package manager, framework) for this integration: CLI + C99 +
  Cocoa, like the rest of the project.

---

## 12. Checks already done (so as not to redo them)

### GMBasicIDE

- `make check` → **0**, all sections `PASS` (last run before this plan).
- `vb_form_parse/serialize` on the `HelloWorld` sample is LF, not CRLF; the serializer writes LF.
- `serialize_control` treats only `Font.` as `BeginProperty`; other `BeginProperty` blocks
  are flattened into properties.
- No `NSTask`/spawn in the repo; modules editable/saved via `_moduleCodes`/`saveModules`;
  no output/diagnostics panel.

### GMBasic / C99Native (2026-10-22)

- `tools/gmb_c99native_run.sh /tmp/gmbprobe/ui3.bas c99ui3s` with `DIM txtNome AS STRING`:
  **rc=0**, log `[gmb_gfx] window 320x200 (logical)`, `renderer=software`.
- Same form without `INTO`: `CONTROL 'TEXTBOX' non supportato dall'emitter nativo`.
- `TEXTBOX` with numeric `DIM txtNome`: it compiles but passes a `double*` to `gmb_form_add_textbox`
  (expected `gmb_string_t*`); the codegen must use `STRING`.
- `LABEL 10,130,120,"Etichetta" INTO lab`: emits `gmb_label_init` (TUI); no
  `gmb_form_add_label` in the runtime. In pixel mode it does not appear.
- `CHECKBOX` with the fresh driver: `gmb_control_add_checkbox(...)`; the next line applies
  the properties to `gmb_tui_last_handle()`, which in graphics is not the checkbox. `NAME`/`TABINDEX`
  are not guaranteed.
- `PIXCOMBOBOX`/`PIXLISTBOX`: `parse error: statement non riconosciuto`, rc=3; the emitter
  branches are not reachable.
- `ON WINDOW CLOSE GOSUB OnClose`: emits `gmb_window_register_event("CLOSE","OnClose")`;
  without `GOSUB` it emits `gmb_window_unregister_event("CLOSE")`.
- `TIMER` in an expression: emits the identifier `TIMER` (undefined C); `TIMER()` gives
  `simbolo non trovato: funzione o subroutine 'TIMER' non definita`, rc=4; `TI` stays `TI`.
- O8 (2026-10-24): every statement emits `#line <riga bas> "<file bas>"` before its C
  (measured: `#line 4 "/…/Ui.bas"` above the `gmb_print_string` of the module). `--line-map`
  writes the anchors: `{"c_line": 80, "line": 4, "file": "…/Ui.bas"}`. Emission error with
  `TIMER()` in a module → `emissione: …/Ui.bas:5: simbolo non trovato…`, rc=4 (before: `riga 13`
  of the merged text). `native/gmb_c99native/tests/c99_line_map_test.sh`: 8/8 PASS.
- O8, regression: the sweep over all the MiniTests gave the **same** 58 CC-FAIL and 51
  DRIVER-FAIL with and without `#line` (same files), so the directives do not change
  compilability; the `cc` errors now cite the `.bas` (e.g. `toggle_parity.bas:5:55: error:`).
- O7, resolution (2026-10-24): the `sample samples/modules/FolderResolution/AllFolders.bas`
  compiles and runs on C99Native (8 modules from 8 different folders, transitivity `DEMO_CHAIN → DEMO_DEEP`),
  and the C# Orchestrator loads the same 8 modules. `c99_module_resolve_test.sh`: 15/15 PASS.
  `app.settings.json` was ignored by C# (`LoadSettings` received the entry file instead of
  the folder): fixed in `SettingsManager.LoadSettings`.
- O7, CLI (2026-10-24): `build/gmb` (target `gmb_cli`) on `samples/modules/FolderResolution`:
  `gmb check` → rc=0, **7 project sources, 8 modules, 0 warnings**; `gmb run` → the 8 lines
  of the sample, rc=0; `gmb check` on a project with `IMPORT FANTASMA` → **rc=2** with
  `Main.bas:1: IMPORT FANTASMA non risolto`; `--strict` promotes `no-namespace` to an error; two
  `SUB Main` → error inviting `--entry`. `gmb_cli_project_test.sh`: **14/14 PASS**.
  Build in three steps: driver `--no-exe`, `cc -c` with the `-I` list of the runtime/LLMEnv, link
  `libgmb_runtime.a` + `-lm -lpthread` (console).
- O7, regression: the sweep over all the MiniTests drops from 58 CC-FAIL / 51 DRIVER-FAIL to
  **53 / 47** (no new error class, zero `già definita`): the recursion and `modules/`
  recover 5 C cases and 4 emission ones. The set is a superset of the previous one (same
  `IMPORT` + transitivity + more candidates), so a new error would have shown up as an increase.
- Suite verification, SINGLE point (2026-10-24, after O7): `gmb minitests` replaces the
  ad hoc sweep of `tools/c99_compile_errors.sh`. Recursive and ORDERED discovery (`readdir` is not:
  without `qsort` two runs gave useless diffs), same `-I` recipe as `gmb build`
  (`build_inc()`, single copy), exclusion of `' TAG: interactive`/`heavy` as before
  (`--all`/`--interactive`/`--heavy` to include them), `--filter`, `--driver`/`--cc` for
  the A/B, histogram of the CC-FAILs by category (quotes → `'*'`, parentheses removed, cut at word
  boundary: otherwise the same error ends up in several categories).
  **Number measured today: 1948 files, 1914 executed (7 interactive + 27 heavy excluded), 1831
  PASS, 36 CC-FAIL, 47 DRIVER-FAIL, 0 crash** (`native/gmb_compiler_c/build/gmb minitests`,
  ~2m30s). The set matched **file by file** with the one of the old script (empty diff on CC and
  DRIVER), so the tool change did not change the verdict: the 16 fewer CC-FAIL compared
  with `1a4715122` come from the fixes landed in the meantime, not from the gate. The last two
  numbers (1831/36) are after the `LIBRARY` fix.
- Gate exit code: 0 all PASS · 2 at least one FAIL · 1 broken setup (driver/root/cc). With
  a floor of 83 files a "green if 0 FAIL" would be red forever, so `--baseline
  <json>` compares with a recorded run: red only for **regressions** (`REGRESSIONE
  <file>`), and `RISOLTO <file>` for the files that pass again. `--json` is stable between runs
  (same order, same numbers): it is the diff to put in CI.
- Gate of the gate: `native/gmb_compiler_c/tests/gmb_cli_minitests_test.sh`, **31/31 PASS** on a
  synthetic root (recursion, TAG, `--cc /usr/bin/false` → CC-FAIL, driver killed by
  SIGSEGV → `crash` and not CC-FAIL, `--driver`/root/filter non-existent → rc 1, `--baseline`
  identical/repaired/regression/different case, deterministic JSON). `gmb_cli_project_test.sh`
  stays 14/14.
- Recorded baseline (2026-10-24): `native/gmb_compiler_c/tests/minitests_baseline.json`
  (35 KB, 83 known FAIL) + `tools/check_minitests.sh`. Two non-obvious choices:
  (a) `--update` considers **rc=2 a success** — a baseline with FAIL is the normal case, only
  rc=1 (driver/root) or an empty JSON leave the file as it was, and the write is atomic
  (tmp + `mv`) because an interruption must not truncate the gate reference;
  (b) the comparison is **case-insensitive**: on macOS the folder is called `minitests`, on a
  Linux checkout `MiniTests`, and without normalization all the known files would be "new".
  Alone the floor does not make the gate unusable: one is red only for the regressions.
- Reproducible baseline (2026-10-24): the `cc` errors pointing to the **generated C** cited
  `/tmp/gmb-minitests.<random>/out.c`, so every `--update` produced a spurious one-line
  diff and the recorded reference pointed to a non-existent file. In the JSON (and in the
  readable output) the path becomes `<out.c>`: the regeneration is now idempotent
  (verified: `--update` → clean `git status` for the baseline).
- Gate bug found by the first run without arguments: `"${args[@]}"` with `set -u` and an empty
  array makes **bash 3.2** (macOS) abort with `args[@]: unbound variable` — that is, what failed was
  exactly the bare CI invocation, not the ones with `--json`/`--filter` tried before.
  Fixed with `${args[@]+"${args[@]}"}` and verified with a full run (rc=0, 37 CC-FAIL,
  0 new FAIL).
- Isolated case emerged from the gate, solved: `constructs/LibraryDirective.bas` did not fail because of the
  types but because the `LIBRARY` mode emitted `#include "gmb_host.h"` — a header **absent from the
  repo**, copied from the `basic2c` transpiler of GMTrading (where `gbmtraderuntime` ships it).
  See the dedicated item further below: emitter and runner corrected, test PASS on the native C
  backend.
- Note (not O8): `MiniTests/tui/widgets_all_parity.bas`, `statusbar_parity.bas` and
  `MiniTests/controls/keyword_controls.bas` made the driver crash (SIGSEGV/SIGBUS) even
  with the directives disabled: it was a defect of the source at the time, not of the map.
  **Re-verified 2026-10-24: 0 crash over the 1914 executed files** (`driver_crash: 0`).
- Multi-file (re-verified on 2026-10-24): `Main.bas` with `IMPORT UI` calling `frmShow`
  defined in a `Ui.bas` with `NAMESPACE UI` **compiles**; the emitted C contains
  `gmb_user_frmShow()` with the body. Without `IMPORT` → `simbolo non trovato: funzione o
  subroutine 'frmShow' non definita`, rc=4. With `Ui.bas` in `forms/` → same error: the
  search is not recursive. A call as a statement (`frmShow` without parentheses) without
  `IMPORT` gives no error: it becomes an implicit variable (to be closed in O7).
- `LIBRARY` did not compile, in any case (2026-10-24, solved). Two distinct causes, both
  invisible to a gate that only looks at the final verdict:
  (a) **ghost header**: `gmb_c99_emit.c` emitted `#include "gmb_host.h"` + four host
  entry-points (`<lib>_set_host/_configure/_flush/_query`) that call `gmb_host_set`,
  `gmb_host_set_token`, `gmb_host_serve`, `gmb_host_query` and read `gmb_config_t.transport`.
  None of those symbols exists in GMBasic: they belong to the **trading host contract**, shipped by
  `gbmtraderuntime` (GMTrading) and copied here from the `basic2c` transpiler which is its common
  source. In GMBasic it produced `fatal error: 'gmb_host.h' file not found` on EVERY `LIBRARY`
  file; the include was at the top, so not even the green library types came
  to the surface. Removed: the documented ABI v1 (`Guide/utente/LIBRARY-gmblib-Guide.md` §2) is
  `<lib>_init`/`<lib>_fini` + ctor/dtor + `<lib>_<Proc>` for the `PUBLIC`s, exactly what
  the C# emits (`C99CodeEmitter.EmitLibraryProgram`), which indeed never had the host
  glue. The `ONINIT`/`ONBAR`/`ONTRADE`/`ONDONE` hooks remain (they only forward to `gmb_user_*`,
  so they create no dependencies) and appear only if the source defines them.
  (b) **runner without library leg**: `tools/test_minitests_c99native` always linked an
  executable, and a `LIBRARY` by contract **has no `main`** → after fix (a) it failed with
  `undefined _main` on a correct file. Now `' TAG: library` links `-shared -fPIC` and verifies
  the ABI with `nm -gU`: `_<lib>_init`, `_<lib>_fini` and one `_<lib>_<Proc>` for every `PUBLIC
  SUB|FUNCTION` of the source (names read from the file, not a hardcoded list), plus the absence of
  `_main`. The negative verdict is real: a probe with a `PUBLIC SUB` with an array argument
  (legitimately not exportable in v1) gives `ABI: simboli non esportati: _probe_Run`.
  Verified end-to-end: MiniTest **PASS** (`minilib_Add/Mul/Greet`), sample
  `samples/Community/C99/GmbLibMath.bas` → `gmbmath.dylib` with `gmbmath_Add/Mul/Greet/init/fini`
  and no `main`, gate `tools/check_minitests.sh` → `RISOLTO`, baseline regenerated at 83 known
  FAIL, 0 regressions, `constructs` 40/41 (the known `types_operator` gap remains).
  (c) **`gmb build` compiled a library as an executable** (2026-10-25, solved): the project CLI
  had no `LIBRARY` leg — it emitted the C, then linked an executable and died
  with `_main` undefined on a correct C. Now the mode is read from the **artifact**
  (absence of `int main`, the same probe as `tools/gmb_c99native_run.sh`), the link is
  `-shared -fPIC -Wl,-undefined,dynamic_lookup` toward `<progetto>/bin/<nome-direttiva>.dylib`,
  `pick_entry` accepts a project without `SUB Main` when a module declares `LIBRARY "..."`
  (that is the entry), `run` builds and does not execute, and `--json` carries `"library": true`.
  Verified by loading the artifact with `dlopen` and calling the ABI: `Add(2,3)=5`,
  `Mul(1.5,4)=6`, `Greet("GMB")="Ciao, GMB"`; 6 new assertions in
  `gmb_cli_project_test.sh`. Still red, but **pre-existing and not of LIBRARY**, is the link
  of the executable: the minimal recipe of `gmb build` (only `libgmb_runtime.a`) no longer covers
  the SDLs that the console runtime drags with it (see `build → bin/Main eseguibile`).
  (d) **the executable link recipe was incomplete** (2026-10-26, solved): the console
  runtime drags the graphics engine (`gmb_tui_app` → SDL2/SDL2_ttf/SDL2_mixer, Vulkan via
  MoltenVK) plus chart and zlib, so even a program with a single `PRINT` died on
  `_SDL_*`/`_vk*`/`gmb_chart_*`. Now the link of `build` adds `libgmb_chart.a` (static
  next to the runtime), the `-L`/`-Wl,-rpath` of the existing dirs (the user cache
  `~/.gmbasic/libs/<rid>`, `/usr/lib`) and the `-l` **only if the library is really there** (so on
  Linux, without MoltenVK, the line stays valid instead of stopping on a non-existent `-l`), plus the
  Apple frameworks. The whole lib-set of `tools/gmb_c99native_run.sh` is not copied (db/agent/doc/
  AI): it is needed by the programs that use those domains. The `-rpath` is not cosmetic: without it, the link
  succeeds and the startup dies with `Library not loaded: @rpath/libSDL2.dylib`. Verified:
  `gmb_cli_project_test.sh` 20/20 PASS, including `build → bin/Main eseguibile` and
  `run → stdout del programma` (formerly FAIL), `LibraryDirective` still PASS and `--out` unchanged.
- `tools/gmb_c99native_driver` is from 2026-10-10; the `native/gmb_compiler_c/build/…` build is from
  2026-10-22 and `tools/gmb_c99native_run.sh` prefers the most recent. The old driver emitted
  `gmb_tui_add_checkbox` for the checkbox; the fresh one uses `gmb_control_add_checkbox`. `LABEL`
  stays TUI in both.
- `canonical.registry.json`: 1567 procedures, 115 commands, 284 constants; C lexer: 1285 entries.

### To be verified before the codegen

- `WINDOW FRAME` and the `STYLE` tokens for font/border/color on the `SCREEN` path.
- `WINDOWCLOSED()` and `ON WINDOW CLOSE` with the real SDL window (not only dummy).
- `PIX*`/`COMBOBOX`/`LISTBOX` after O3.
- Cross-file visibility of top-level `DIM` with the project (O7); fallback
  `CONTROL GET/GETS`.
- `Form_Resize`/`Activate`/`Deactivate` with `ON WINDOW … GOSUB`.
- Access key `&` in captions and `MOUSE.X/Y` in handlers without arguments.
- Real DAP (`--debug-tcp`) before promising debug.

---

## 13. Quick references

- GMBasicIDE code: `src/core/vb_parse.c` (`.frm`), `src/core/vb_model.h` (`VbForm`/`VbControl`),
  `src/platform/mac/VBEditorWindow.m` (shell), `src/platform/mac/VBCodeEditor.m` (editor),
  `src/tools/vbtool.c` (harness).
- GMBasic CLI/gate: `native/gmb_compiler_c/src/cli/gmb_cli.c` (`check/build/run/minitests`),
  `native/gmb_compiler_c/tests/gmb_cli_minitests_test.sh`,
  `native/gmb_c99native/src/gmb_c99_emit_controls.c` (controls),
  `native/gmb_runtime_c/include/gmb_form.h` (pixel API),
  `native/gmb_runtime_c/src/gmb_form.c` (runtime),
  `native/gmb_compiler_c/frontend/include/gmb_fn_lookup.h` (names),
  `tools/gmb_c99native_run.sh` (reference link).
- Vault: `Guide/utente/Finestre-Eventi-Guida-Utente.md` (`ON WINDOW`),
  `Guide/utente/Controlli-Pixel-Proprieta-Metodi-Guida-Utente.md` (controls),
  `Todo/(open) - IDE-TUI-BASIC-e-standalone-e-VSCode-LSP.md` (TUI/LSP IDE).
