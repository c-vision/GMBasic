# TODO — GMBasicIDE

Full backlog to complete the clone of the Visual Basic 6.0 IDE.
Legend: `P0` blocks fidelity/daily use · `P1` required for a credible clone ·
`P2` completion and polish. `[x]` = done and verified with build/snapshot.

General rule: an item is "done" only if `make` is green with no new warnings,
`make check` passes, and the UI has been verified with a window snapshot
(`--window-snapshot`) and, where needed, with `rea capture-native-ui-scenario`.

---

## M0 — VB6-style IDE shell (visual fidelity)

- [x] **P0** Classic Windows 98/VB6 theme in `VBTheme` (system grays, black text,
      blue titles `#000080`, Tahoma 11 / Courier New 12 fonts).
- [x] **P0** 3D drawing helpers: bevel raised/pressed/sunken, etched, title bar with
      close/min/max, classic buttons, checkbox/radio, toolbar icons.
- [x] **P0** VB6 menu bar inside the window (File, Edit, View, Project, Format, Debug,
      Run, Query, Diagram, Tools, Add-Ins, Window, Help) with working popups.
- [x] **P0** Classic toolbar with 24×24 buttons, separators, drawn icons,
      enabled/disabled states, hover and press.
- [x] **P0** Gray `#808080` MDI workspace with child windows (Designer, Code), their own
      blue captions, sunken client border.
- [x] **P0** 2-column toolbox, icons only, tooltip with the control name, selection
      with pressed bevel.
- [x] **P0** Custom Project Explorer: caption "Project - <name>", +/− box, dotted lines,
      form/module icons, FRM/BAS/CLS tags, blue selection.
- [x] **P0** Properties: caption "Properties - <object>", classic object combo,
      grid table with names on a gray face and white editable values.
- [x] **P0** Designer: blue form caption with close box, 60-twip dot grid,
      dashed selection and black handles, no modern shadow/gradient.
- [x] **P0** Status bar with sunken panels (message, DESIGN/RUNNING mode,
      coordinates/size).
- [x] **P0** Window title in VB6 format:
      `<Progetto> - Microsoft Visual Basic [design|code|run] - [<Form> (Form)]`.
- [x] **P0** Code window with the two VB6 combos at the top: `(General)` / `(Declarations)`
      and a procedure list derived from `vb_code_handlers`, with jump to the routine.
- [x] **P0** VB6-style runtime window: `BorderStyle` respected (borderless/fixed/sizable),
      controls drawn with bevels and classic fonts (label, textbox with caret, button,
      check/radio, combo, list, scrollbar), no Aqua widgets.
- [x] **P0** Data and DataGrid controls: toolbox tools with VB6 icons, dedicated
      properties (Data: Connect/DatabaseName/RecordSource/RecordsetType/BOFAction/
      EOFAction/ReadOnly/Exclusive/Options; DataGrid: DataSource/ColumnHeaders/
      DefColWidth/RowHeight/Allow*), classic drawing in designer and runtime
      (record navigator, grid with headers), `.frm` round-trip verified by
      `--designer-test`.
- [x] **P2** Editable DataGrid at runtime: in-memory grid (`VbGrid` in
      `src/core/vb_grid.c`, no Cocoa dependency) with selectable cells, editing
      with F2/Enter or by typing, Esc/Enter, backspace and Del, arrows and Tab/Shift+Tab
      (Ctrl+Tab exits), column resizing by dragging the
      header edge. Covered by `vbtool grid` (inside `make check`, no window).
- [ ] **P2** Data control and DataGrid with persistent data: recordset loaded from a file
      or a service (no ADO/Jet on macOS) fed by `RecordSource`, first/previous/next/last
      navigation and field binding; today the sheet is in memory and is
      emptied when the window closes.
- [x] **P0** `MsgBox` in Win98 style: gray window with icon, text and classic button.
- [x] **P0** `InputBox` and dialogs with multiple buttons/icons (Yes/No/Cancel, Critical, Question)
      in Win98 style, not an Aqua `NSAlert`.
- [ ] **P1** Menus: separators, shortcuts shown on the right, submenus (Align,
      Make Same Size, Order), checkmarks for toggle items, correct disabled states.
- [ ] **P1** Toolbar/toolbox icons refined to 16×16/24×24 with the VB6 palette (no
      gradients, black + gray + blue), no generic glyphs.
- [ ] **P1** Pixel snapping: all lines crisp at 1px on 1x and 2x displays; verify
      that bevels are not blurred.
- [ ] **P1** Font fallback: if Tahoma is missing use Verdana/Geneva without changing
      the panel metrics.
- [ ] **P1** Form Layout Window (preview of the form position on screen) and
      Object Browser (list of project classes/members).
- [ ] **P1** Working Menu Editor: create/edit `VB.Menu`, caption, name,
      checked/enabled/visible, separators, nesting levels.
- [ ] **P2** Menu bar with highlighted Windows-style accelerators (File, Edit, ...).
- [ ] **P2** Italian localization of menus and messages (optional, toggle).

---

## M1 — Form designer (complete editing)

- [ ] **P0** 8 resize handles (corners + sides) with correct cursor and
      symmetric resizing.
- [ ] **P0** Control creation: click on the tool + drag on the form; double click
      on the tool creates the control with default size.
- [ ] **P0** Moving with snap to the form grid and `Alt` to move by 1 twip;
      arrows = 15 twip, Shift+arrows = 1 twip.
- [ ] **P0** Deletion with Delete, `Esc` cancels the active tool.
- [ ] **P0** Containers (`Frame`, `PictureBox`) with clipping of children and correct
      relative coordinates during drag/resize.
- [ ] **P0** Editing properties that change the canvas: `ClientWidth`, `ClientHeight`,
      `Caption`, `BorderStyle`, `BackColor` with immediate refresh.
- [x] **P1** Multi-selection: Shift+click, selection rectangle (also additive with
      Shift), shared handles, group proportional move and resize,
      multiple deletion. Verified by `--designer-test`.
- [ ] **P1** Group constraints: block multi-resize on controls with different
      parents (today the drag falls back to marquee) with visual feedback.
- [x] **P1** Alignment and spacing: Align Left/Center/Right/Top/Middle/Bottom,
      Make Same Width/Height/Size, Horizontal/Vertical Spacing (Make Equal/Increase/
      Decrease/Remove), Center in Form. Reference for Make Same Size = active
      control; requires a shared parent. Covered by `--designer-test`.
- [x] **P1** Z-order: Bring to Front / Send to Back from the Format > Order submenu,
      also on multi-selection (the group's internal order is preserved). The drawing
      order is the order of the `Begin` blocks in the `.frm`: round-trip and reload verified
      by `--designer-test`, and the runtime builds the controls in the same order.
- [x] **P1** Tab order: `View > Tab Order...` dialog in VB6 style (list, Up/Down arrows,
      OK/Cancel) that lists the controls by `TabIndex` and renumbers it 0..n-1; editing
      `TabIndex` from Properties swaps with the control that occupied the index; new
      controls receive the next free index. Covered by `--designer-test`.
      The runtime also navigates with Tab/Shift+Tab according to `TabIndex`, with a
      dashed rectangular focus in Win98 style and Space activation for buttons/check.
      The order is recalculated at every Tab and respects `Visible`/`Enabled`/`TabStop`
      changed during execution, including controls inside nested Frames.
      `Enabled = False` on Frame/PictureBox really disables the children (no Tab, click
      or focus; captions and fields in Win98 gray) without touching their `Enabled` flag, which
      becomes active again when the container is re-enabled. Covered by `--runtime-tab-test`.
      Residual limit: focus leaving a removed control is folded onto
      the next one of the rebuilt order and does not remember the last visited.
- [ ] **P1** Copy / Cut / Paste / Duplicate controls, with offset and unique names.
- [ ] **P1** Undo/Redo for all designer operations (move, resize,
      creation, deletion, properties), with stack and ⌘Z/⇧⌘Z shortcuts.
- [ ] **P1** Control locking (`Locked`) and selection with right click → context menu.
- [x] **P1** Format toolbar (second row, VB6 Form Editor style) with align/size/spacing;
      Format menu buttons and items enabled only with a valid selection (disabled
      icons faded as on Win9x).
- [ ] **P2** Configurable grid (show/hide, step), alignment guides.
- [ ] **P2** Runtime form preview from the designer ("Start" toggle).
- [ ] **P2** `VB.Menu` control support visible only in the Menu Editor, not on the form.

---

## M2 — Code editor

- [ ] **P0** `(General)` combo and procedure combo in the code window (see M0).
- [ ] **P0** Insertion of handler stubs for all events of the selected control
      (Click, DblClick, Change, KeyPress, GotFocus, LostFocus, MouseDown/Up/Move,
      Load/Unload/Activate for the form, Timer for the timer).
- [ ] **P0** VB6 syntax highlighting: blue keywords, green comments, black strings;
      no modern colors.
- [ ] **P0** Preserve encoding and line ending (CRLF) on save; no loss of
      trailing empty lines.
- [ ] **P1** Auto-indent after `If`, `For`, `Do`, `Select Case`, `Sub`, `Function`,
      `With`; dedent on `End ...`, `Else`, `Loop`, `Next`.
- [ ] **P1** Tab/Shift+Tab to indent blocks.
- [ ] **P1** Find & Replace (⌘F), Find Next, Go to Line (⌘L).
- [ ] **P1** Native undo/redo reliable even after re-highlighting.
- [ ] **P1** Detection of current handlers and synchronization with the procedure combo.
- [ ] **P2** Minimal IntelliSense: keyword, control names and handler completion.
- [ ] **P2** Bookmarks and task list (`TODO`).
- [ ] **P2** Optional line numbers and printing.

---

## M3 — Runtime and VB6 interpreter

- [ ] **P0** Statements: `If/ElseIf/Else/End If`, `Select Case`, `For/Next`, `For Each`,
      `Do/Loop`, `While/Wend`, `With/End With`, `Exit`, `GoTo` with labels.
- [ ] **P0** Declarations: `Dim`, `ReDim`, `Static`, `Const`, one-dimensional arrays,
      `Option Explicit`, `Sub`/`Function` with `ByVal`/`ByRef` parameters.
- [ ] **P0** Types and conversions: Integer, Long, Single, Double, String, Boolean, Date,
      Variant, `CInt`, `CLng`, `CSng`, `CDbl`, `CStr`, `CBool`.
- [ ] **P0** Operators and precedence, `&` concatenation, comparisons, `And/Or/Not`,
      `Mod`, `Like`.
- [ ] **P0** Built-in functions: strings (`Left`, `Right`, `Mid`, `Len`, `Trim`,
      `UCase`, `LCase`, `InStr`, `Replace`, `Split`, `Join`), math (`Abs`, `Int`,
      `Rnd`, `Sqr`), date (`Now`, `Date`, `Time`, `Format`), `MsgBox`, `InputBox`.
- [ ] **P0** Runtime controls: get/set properties, `AddItem`, `Clear`,
      `Print`, `Visible`, `Enabled`, `Text`, `Caption`, `Value`.
- [x] **P1** Runtime errors: an invalid `SetFocus` raises VB6 error 5 with a
      Win98 modal dialog (error icon, `Run-time error '5':` + message) and
      `End`/`Debug` buttons: `End` closes the form, `Debug` enters break mode on the line
      hit (yellow bar + arrow in the gutter), so the handler can continue with
      F8/F5 instead of stopping at the error. Without break mode (CLI, tests) the old
      stop behavior remains. The interpreter reports handler and absolute line. Covered by
      `--runtime-error-test`, `--runtime-error-snapshot` and `--runtime-break-test`.
- [x] **P1** Runtime focus: `<Control>.SetFocus` (also `Call ...`) and
      `GotFocus`/`LostFocus` events on every focus change (mouse, Tab, access key,
      `SetFocus`), with `Form_Load` executed before the first `GotFocus`. `SetFocus` on
      a disabled, hidden or non-focusable control is a no-op with diagnostics;
      `TabStop = False` does not prevent it (it only excludes from the Tab cycle). Covered by
      `--runtime-focus-test`. Limit: during `GotFocus` the first responder of the
      window is not updated yet.
- [x] **P1** Runtime access key (`&` in the caption): Alt+letter activates the control
      (button → `Click`, CheckBox/OptionButton → toggle + `Click`), the letter is
      underlined as on Win98 and `&&` shows a literal `&`. The Label moves the focus
      to the next control of the tab order, `UseMnemonic = False` disables
      the mnemonic and disabled/hidden controls (or inside disabled Frames) do not
      respond. Covered by `--runtime-access-key-test`.
- [ ] **P0** Events: `Form_Load`, `Form_Unload`, `Click`, `DblClick`, `Change`,
      `KeyPress`, `GotFocus`, `LostFocus`, `MouseDown/Up/Move`, `Timer`.
- [ ] **P0** Runtime rendering identical to the designer for all supported controls.
- [ ] **P1** Control array (`Index`), `Load`/`Unload` of controls.
- [x] **P1** VB6 error handling: `On Error Resume Next`, `On Error GoTo <label>`,
      `On Error GoTo 0`, `Resume` / `Resume Next` / `Resume <label>` and `Err`
      (`Number`, `Description`, `Source`, `Clear`, `Raise`). The classic
      `End`/`Debug` dialog appears **only** for unhandled errors (or raised inside an
      already active handler). Procedural scope, so `Resume Next` also works
      inside `If`. Covered without a window by the `runtime errors` section of `vbtool check`.
      Limit: a `Resume` that returns
      to an instruction that fails again is stopped by a guard after 10000 rounds
      (VB6 would loop), and `On Error GoTo -1` is not recognized.
- [ ] **P1** Generic `GoTo`/`GoSub` with labels and `Select Case`: only the jumps of the
      error handlers are interpreted.
- [ ] **P1** Collections, user-defined `Type`, `Enum`.
- [ ] **P1** File I/O: `Open/Close`, `Print #`, `Input #`, `Line Input #`, `EOF`.
- [ ] **P1** Timer with real interval, clean stop on close, `DoEvents`.
- [ ] **P1** Form closing with `Unload Me`, `End`, and coherent stop from toolbar.
- [ ] **P2** Declared API calls (`Declare`) with stub/explicit error, not crash.
- [ ] **P2** Minimal `App`, `Screen`, `Clipboard`.

---

## M4 — Project system

- [ ] **P0** Add/Remove form, module, class, UserControl with file creation and
      `.vbp` update.
- [ ] **P0** Dirty tracking for project, form and modules; save prompt on
      close and on project switch.
- [ ] **P0** Faithful `.vbp` round-trip: item order, `Module=`, `Class=`,
      `UserControl=`, `Startup=`, `Name=`, references.
- [ ] **P0** Missing file handling: clear message, disabled item, no crash.
- [ ] **P1** Project properties: name, startup object, title, icon, version.
- [ ] **P1** References/Components: metadata-only dialog, persistence in the `.vbp`.
- [ ] **P1** Multiple open MDI windows (form and code) with activation and closing.
- [ ] **P1** Recent projects and reopening at startup.
- [ ] **P2** Drag&drop of `.vbp`/`.frm` on the window and on the Dock.

---

## M5 — Debug

- [ ] **P1** Start/Break/End toolbar with coherent states and F5 / Shift+F5 shortcuts.
      (Break/End and the BREAK state are there; the toolbar does not reflect the three states yet.)
- [x] **P1** Break during execution: `Debug > Break` / toolbar Break request the pause and
      the interpreter stops before the next instruction; the nested event loop
      keeps the IDE alive and the status bar says `BREAK`. `Shift+F5` interrupts, closing the
      form during the pause terminates the procedure. Covered by `--runtime-break-test`
      (also the interactive path with real F8/F5).
- [x] **P1** Step Into (F8) and Step Over (F10) at instruction level, with highlighting
      of the current line (VB6 yellow bar + arrow in the gutter) and no pause on
      `Dim`, `End If`, `Else` or labels. Covered by `--runtime-break-test`.
- [ ] **P1** Step Out (F11), call stack and local variables: the interpreter has no call
      stack, so one cannot yet walk back to the caller.
- [ ] **P1** Focus changes (`GotFocus`/`LostFocus` from `SetFocus`) are not yet
      steppable: they run atomically inside the instruction that triggers them.
- [ ] **P2** Watch window and Immediate window (`? espressione`).
- [x] **P2** Breakpoints in the gutter: click on the column → red dot (only on
      executable instructions, otherwise beep), persistence in the `.frm` as comment
      `'@GMBasicIDEBreakpoints: ...` (VB6 ignores it, byte-identical round-trip) and stop
      of the interpreter on the line hit. Covered by `--breakpoint-test`. The dots
      follow code changes: every edit is compared with the previous text
      (prefix/suffix diff, region widened to whole lines), so lines inserted or
      removed above a breakpoint move it together with its instruction, Enter at line start
      makes it move down and deleting the instruction removes it (like VB6). It stays bound to the
      form open in the code editor: `.bas` modules have no breakpoints.
- [x] **P2** Runtime error dialog with "Debug" to open the offending line.

---

## M6 — Tests, quality, documentation

- [x] **P0** Headless harness `vbtool check` (inside `make check`): sample
      round-trip, execution of a handler with C host, breakpoint markers and
      byte-identical save/load/save, breakpoint mapping on line insert/delete,
      Data/DataGrid defaults, DataGrid suite. Links only the C core: no
      window. Still to be covered: util, expression evaluator and the main
      statements beyond those of the sample.
- [ ] **P0** Bring into the CLI harness the last self-tests still inside the app
      (`--designer-test`, `--runtime-*-test`): as long as they live in Cocoa they require a
      window and are not part of `make check`.
- [ ] **P0** Project fixtures: `ControlsGallery`, `Events`, `Loops`, `MultiForm`,
      `MenuDemo`; each with verify + expected run.
- [ ] **P0** `vbtool lint` to validate `.frm`/`.vbp` and report missing properties.
- [ ] **P1** Visual regression: `rea capture-native-ui-scenario` + PNG comparison
      with threshold, on Designer, Code, Runtime.
- [ ] **P1** Accessibility check: menu bar, toolbox, explorer, properties reachable
      from the AX tree with correct roles and names.
- [ ] **P1** Performance: form with 200 controls < 16 ms per frame; typing without
      lag > 50 ms; no memory leak in the open/close project cycle.
- [ ] **P1** README with screenshots, build instructions, known limits, roadmap.
- [ ] **P2** Architecture documentation in `docs/` and continuous update of
      `AGENTS.md`.

---

## M7 — Packaging

- [ ] **P1** App icon in VB6 style and `Info.plist` with `.vbp`/`.frm` document types.
- [ ] **P1** Universal build arm64 + x86_64 (`-arch arm64 -arch x86_64`).
- [ ] **P2** Signing and notarization (only when requested by the user).
- [ ] **P2** Error log to file in `~/Library/Logs/GMBasicIDE/`.

---

## M8 — Integration with GMBasic

- [ ] **P1** Adapt the IDE to GMBasic with **C99Native** and **VB6-compatible `.frm`** forms:
      folder project (`.frm` + `.bas`/`.inc` + manifest), GMBasic language mode,
      Check/Run/Build with output and diagnostics, designer that generates `.ui.bas`. The necessary
      C99Native omissions (project/file list, pixel Label, diagnostic map, CLI) are
      prerequisites tracked in [TODO-GMBASIC.md](TODO-GMBASIC.md).

---

## Definition of "done" for every UI item

1. `make` with no new warnings.
2. `make check` green.
3. Window snapshot attached to the verification, with a check of colors and metrics.
4. No Aqua/rounded/gradient element in the chrome.
5. `AGENTS.md` (status) and this `TODO.md` (checkbox) updated.
