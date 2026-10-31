# GMBasicIDE

Native macOS IDE for **GMBasic** projects.

GMBasicIDE manages GMBasic projects (`.gmbide.json`), `.bas`/`.inc` sources, `.frm` forms,
compilation, debugging and an integrated terminal. It does not embed an interpreter: it runs the
external GMBasic compiler.

## Features

- Open/create a GMBasic project, manifest and Project Explorer.
- GMBasic source editor with autocomplete, breakpoints and source map.
- Form designer with `.frm` forms (GMBasic layout), toolbox, grid, alignment, z-order and
  Tab Order.
- Export the form into a `.ui.bas` module for GMBasic.
- **Build**: check, build and run through `gmb`.
- **Debug**: breakpoints, continue, step, pause, stack and watch through
  `gmb_chunk_driver` + `gmb_vm_host --debug-interactive`.
- **Tools**: `/bin/zsh` terminal and IDE themes.
- Default theme VB6, selectable from **Tools > Theme**.

There is no internal Visual Basic runtime and `.vbp` projects are not opened.

## Menu

- **File**: New/Open Form, New/Open Project, Open Source, Save Form, Exit.
- **Project**: Add Form, Export Form/Export All Forms.
- **Build**: Check/Build/Run Project, Run Form, Set Toolchain.
- **Debug**: Debug Project.
- **Tools**: Terminal, Theme.
- **View**: Code/Object, Tab Order, panel focus.
- **Format**: alignment, spacing and control order.

## Architecture

```text
src/core/           form model and parser, GMBasic export, C99 utilities
src/platform/mac/   Cocoa/Objective-C shell
  VBEditorWindow, VBDesignerView, VBCodeEditor, VBGmbRunner,
  VBGmbSourceWindow, VBGmbDebugWindow, VBTerminalWindow, VBTheme
samples/HelloGMBasic  example GMBasic project
```

## Build

```bash
make
open build/GMBasicIDE.app
```

## Verification

```bash
make check
./build/GMBasicIDE.app/Contents/MacOS/GMBasicIDE --selftest
./build/GMBasicIDE.app/Contents/MacOS/GMBasicIDE --theme-test
./build/GMBasicIDE.app/Contents/MacOS/GMBasicIDE --gmb-debug-test samples/HelloGMBasic/Main.bas
```

## Limits

- The form designer uses the `.frm` format as a layout format; it does not execute VB6 code.
- The export covers a subset of controls.
- Debug follows the chunk compiler perimeter: entry + imported modules/`include`.

## Agent instructions

The IDE has its own `AGENTS.md` with agent instructions; it is a local, untracked file
(not part of the repository), so it is not linked from here.
