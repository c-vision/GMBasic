# GMBasicIde — IDE TUI standalone in BASIC

App **completa** scritta in **BASIC** (`.bas`): è la *versione BASIC* dell'IDE TUI la cui logica
è in C (`native/gmb_runtime_c/src/gmb_ide.c`). Si compila in un **eseguibile nativo** (percorso
C99Native: `.bas` → C → binario). **NON** gira dentro la REPL.

- **Origine:** app **originale** di questo progetto (nessun upstream da portare).
- **Licenza:** quella del progetto GMBasic.

## Layout del progetto (convenzione `APPS/`)

```
APPS/GMBasicIde/
  README.md                 <- questo file
  TODO.md                   <- lavoro residuo
  src/GMBasicIde.bas        <- entry point (SUB Main)
  work/                     <- output runtime (rigenerato, gitignored)
```

## Come funziona (una sola logica, tanti host)

- La **logica** (analisi del linguaggio: outline, completion, go-to-def, find-refs, lint, rename,
  snippet, make) è **in C** (`gmb_ide`), esposta a BASIC da wrapper **FFI-friendly** `gmb_idef_*`
  (`native/gmb_runtime_c/src/gmb_ide.c`).
- Questo `.bas` la pilota via **FFI** (`DECLARE … LIB "gmb_gfx"`) e la mostra con i **controlli TUI**
  (registry `gmb_tui_*` + frame **non bloccante** `gmb_ui_frame`). **Nessun nuovo builtin/TokenKind.**
- `gmb_gfx` è una lib **nostra interna** → vive in **`libs-runtime/osx-arm64/`** (vault
  `Knowledge/Librerie-e-Pacchetti.md`). L'FFI la trova in automatico.

## Layout della UI e tasti

```
+----------------------------------------------------------+
| File: [percorso]                                         |
| [ tab1 | tab2 | ... ]                                    |
| CodeArea (sorgente)                 | ListBox (outline)   |
|                                     |                    |
+----------------------------------------------------------+
| StatusBar: messaggi / risultato comando                  |
+----------------------------------------------------------+
```

| Tasto | Azione |
|---|---|
| `F1`  | Aiuto |
| `F2`  | Apri il file indicato nel campo "File:" |
| `F3`  | Salva il buffer corrente nel percorso |
| `F4`  | Outline (analisi simboli) |
| `F5`  | Completion (prefisso "PR") |
| `F6`  | Lint (squiggle nel gutter) |
| `F7`  | Go-to-definition (simbolo selezionato nell'outline) |
| `F8`  | Find-references (simbolo selezionato) |
| `F9`  | Make (scrive il documento normalizzato in `<file>.build.bas`) |
| `F10` | Nuovo tab |
| `ESC` | Esci |

## Come eseguire

```sh
# IL backend (JIT, dev loop)
dotnet run --project Executor -- APPS/GMBasicIde/src/GMBasicIde.bas

# Compilazione+link+run STANDALONE (C99Native) -> binario nativo
tools/gmb_c99native_run.sh APPS/GMBasicIde/src/GMBasicIde.bas
```

Richiede: build nativo presente (`native/gmb_vm_c/build`) e le librerie native in cache
(`~/.gmbasic/libs/<rid>/`, da `gmb install --runtime-only`).

## Stato (v1.0)

- [x] Documento multi-tab nel modello C `gmb_ide` (`gmb_idef_tabopen/tabcount/tabname`).
- [x] CodeArea editabile, sincronizzata al modello C ad ogni comando; outline/completion/lint/goto/refs/make via FFI.
- [x] Layout: campo File (TextInput), TabBar, CodeArea + ListBox(outline), StatusBar.
- [x] Squiggle degli errori di lint nel gutter (`gmb_tui_control_set_errors`).
- [x] Apertura/salvataggio file (I/O BASIC `OPEN`/`LINE INPUT`/`PRINT #`).
- [x] Ricostruzione UI ad ogni cambio (pattern "RegisterUI", come `RegisterIde` della REPL C#).
- [x] Uscita su `ESC`; verificato su IL + compilazione standalone (C99Native → binario nativo).
- [ ] Popup completion inline con inserimento; goto per click sull'outline; ricerca/find nel buffer.
- [ ] Verifica su display (headless non copre GRAPHICS).

## Wrapper C FFI-friendly aggiunti (`native/gmb_runtime_c`)

Per un IDE pilotabile da BASIC senza out-buffer (che il FFI BASIC non può passare):

- `const char* gmb_tui_control_text_ptr(int i)` — testo del controllo i (CodeArea righe '\n'-unite;
  ListBox valore selezionato; TextInput testo). Buffer statico condiviso.
- `void gmb_tui_control_set_text(int i, const char*)` — imposta CodeArea (righe) / ListBox (voci) /
  TextInput; split su '\n'.
- `void gmb_tui_control_set_errors(int i, const char* lines_1based)` — squiggle (righe 1-based, sep '|').
- `int gmb_code_area_goto_line(int i, int line)` — posiziona il cursore del CodeArea e scrolla.

File correlati: `native/gmb_runtime_c/{include/gmb_ide.h,src/gmb_ide.c}` (logica + wrapper `gmb_idef_*`),
`include/gmb_tui_app.h` + `src/gmb_tui_controls_add.c` (controlli + wrapper FFI),
`Todo/(open) - IDE-TUI-BASIC-e-standalone-e-VSCode-LSP` (piano).
