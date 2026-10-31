# GMBasicIDE

IDE nativo macOS per progetti **GMBasic**.

GMBasicIDE gestisce progetti GMBasic (`.gmbide.json`), sorgenti `.bas`/`.inc`, form
`.frm`, compilazione, debug e terminale integrato. Non incorpora un interprete: esegue
il compilatore GMBasic esterno.

## Funzionalità

- Apertura/creazione progetto GMBasic, manifest e Project Explorer.
- Editor sorgente GMBasic con autocomplete, breakpoint e source map.
- Form designer con form `.frm` (layout GMBasic), toolbox, griglia, allineamento,
  z-order e Tab Order.
- Export del form in modulo `.ui.bas` per GMBasic.
- **Build**: check, build e run tramite `gmb`.
- **Debug**: breakpoint, continue, step, pause, stack e watch tramite
  `gmb_chunk_driver` + `gmb_vm_host --debug-interactive`.
- **Tools**: terminale `/bin/zsh` e temi dell’IDE.
- Tema predefinito VB6, selezionabile da **Tools > Theme**.

Non esiste un runtime Visual Basic interno e non vengono aperti progetti `.vbp`.

## Menu

- **File**: New/Open Form, New/Open Project, Open Source, Save Form, Exit.
- **Project**: Add Form, Export Form/Export All Forms.
- **Build**: Check/Build/Run Project, Run Form, Set Toolchain.
- **Debug**: Debug Project.
- **Tools**: Terminal, Theme.
- **View**: Code/Object, Tab Order, focus pannelli.
- **Format**: allineamento, spaziatura e ordine dei controlli.

## Architettura

```text
src/core/           modello e parser form, export GMBasic, utility C99
src/platform/mac/   shell Cocoa/Objective-C
  VBEditorWindow, VBDesignerView, VBCodeEditor, VBGmbRunner,
  VBGmbSourceWindow, VBGmbDebugWindow, VBTerminalWindow, VBTheme
samples/HelloGMBasic  progetto GMBasic di esempio
```

## Build

```bash
make
open build/GMBasicIDE.app
```

## Verifica

```bash
make check
./build/GMBasicIDE.app/Contents/MacOS/GMBasicIDE --selftest
./build/GMBasicIDE.app/Contents/MacOS/GMBasicIDE --theme-test
./build/GMBasicIDE.app/Contents/MacOS/GMBasicIDE --gmb-debug-test samples/HelloGMBasic/Main.bas
```

## Limiti

- Il form designer usa il formato `.frm` come formato di layout; non esegue codice VB6.
- L’export copre un sottoinsieme di controlli.
- Il debug segue il perimetro del chunk compiler: entry + moduli/`include` importati.
