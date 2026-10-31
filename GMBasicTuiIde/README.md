# GMBasicTuiIde — IDE TUI standalone in BASIC (progetto autonomo)

App **completa** scritta in **BASIC** (`.bas`): è la *versione TUI* dell'IDE la cui logica è in C
(`GMBasic/native/gmb_runtime_c/src/gmb_ide.c`). Si compila in un **eseguibile nativo** (percorso
C99Native: `.bas` → C → binario) e **non** gira dentro la REPL.

**Gemella di `GMBasicIDE/`** (submodule, versione **grafica** nativa macOS): stessa logica in C,
due host diversi — TUI qui (BASIC compilato), GUI là (app nativa). La logica **non** è duplicata.

- **Origine:** app originale di questo progetto (nessun upstream da portare).
- **Licenza:** quella del progetto GMBasic.
- **Stato:** promossa a **progetto autonomo del monorepo** il 2026-10-31 (prima era
  `shared/APPS/GMBasicIde`, rinominata perché il nome collideva con il submodule grafico).

## Perché è un progetto autonomo (e non un sample)

Questo IDE è il **banco di misura del footprint**: serve a capire **cosa si porta dietro un
programma BASIC complesso e compilato** — quali librerie native tocca (FFI `gmb_gfx`), quante
risorse usa, quanto pesa il binario e quali dipendenze dinamiche porta con sé. È il caso d'uso più
"carico" che abbiamo: 20+ `DECLARE … LIB "gmb_gfx"`, controlli TUI, I/O file, chiamate FFI a ogni
frame. Per questo vive alla root come progetto, non dentro `shared/APPS/`.

### Come si misura (comandi, non impressioni)

Il runner C99Native emette tutto accanto all'entry file (`GMBasicTuiIde/src/bin/`):
`<base>_<suff>.c` (C generato), `<base>_<suff>` (binario), `<base>_<suff>.libs.txt`
(manifest delle librerie richieste), `libs.txt`, `Resources/`, `settings/`.

```sh
# 1) compilazione nativa (CWD = root del monorepo; il runner entra in GMBasic/)
#    NB: il percorso è relativo a GMBasic/ → `../GMBasicTuiIde/...`
GMB_TUI_SCRIPT=$'\e' bash tools/sh/gmb_c99native_run.sh ../GMBasicTuiIde/src/GMBasicTuiIde.bas

# 2) peso del binario e del C generato
ls -lh GMBasicTuiIde/src/bin/GMBasicTuiIde_out{,.c}

# 3) dipendenze dinamiche reali (macOS; su Linux `ldd`)
otool -L GMBasicTuiIde/src/bin/GMBasicTuiIde_out

# 4) librerie che il runtime carica a RUN time (FFI/`dlopen`), con URL+sha256+size
cat GMBasicTuiIde/src/bin/GMBasicTuiIde_out.libs.txt
```

**Misura del 2026-10-31** (osx-arm64, `-O1`, `C99Native`): binario **1,2 MB**, C generato **81 KB**,
dipendenze dinamiche del binario `libSystem`/`libcurl`/`libz` + `@rpath/libSDL2{,_ttf}`; **10**
librerie caricate a runtime per **≈ 6,0 MB** (la più pesante `libgmb_gfx` 1,4 MB, poi `libglib`
1,3 MB, `libharfbuzz` 1,2 MB, `libSDL2` 0,5 MB, …). Aggiornare questa riga quando si rimisura.

## Layout del progetto

```
GMBasicTuiIde/
  README.md                    <- questo file
  TODO.md                      <- lavoro residuo
  src/GMBasicTuiIde.bas        <- entry point (SUB Main)
  work/                        <- output runtime (C generato + binario), gitignored
```

## Come funziona (una sola logica, tanti host)

- La **logica** (outline, completion, go-to-def, find-refs, lint, rename, snippet, make) è **in C**
  (`gmb_ide`), esposta a BASIC da wrapper **FFI-friendly** `gmb_idef_*`
  (`GMBasic/native/gmb_runtime_c/src/gmb_ide.c`).
- Questo `.bas` la pilota via **FFI** (`DECLARE … LIB "gmb_gfx"`) e la mostra con i **controlli TUI**
  (registry `gmb_tui_*` + frame **non bloccante** `gmb_ui_frame`). **Nessun nuovo builtin/TokenKind.**
- `gmb_gfx` è una lib **nostra interna**: si compila in **`GMBasic/build/libs/<rid>/`** e si
  pubblica in **`~/.gmbasic/libs/<rid>/`** (vault: `Knowledge/Librerie-e-Pacchetti.md`). L'FFI la
  trova in automatico: il loader cerca `libs` → `shared/libs` → `build/libs/<rid>` e poi la cache.

## Layout della UI e tasti

```
+----------------------------------------------------------+
| File: [percorso]                                         |
| [ tab1 | tab2 | ... ]                                    |
| CodeArea (sorgente)                 | ListBox (outline)   |
|                                     |                     |
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

Dalla **root del monorepo** (`GMBasicMonoRepo/`). L'app è **interattiva**
(`' TAG: interactive`): senza un TTY si usa `GMB_TUI_SCRIPT` per scriptare i tasti, altrimenti
chiude subito.

```sh
# compilazione + esecuzione standalone (C99Native)
#   il percorso del .bas è relativo a GMBasic/ (il runner entra lì dentro)
GMB_TUI_SCRIPT=$'\e' bash tools/sh/gmb_c99native_run.sh ../GMBasicTuiIde/src/GMBasicTuiIde.bas

# dev loop su IL (JIT)
tools/sh/harness.sh il GMBasicTuiIde/src/GMBasicTuiIde.bas

# esecuzione scriptata (ESC per uscire)
GMB_TUI_SCRIPT=$'\e' tools/sh/harness.sh il GMBasicTuiIde/src/GMBasicTuiIde.bas
```

Prerequisiti: build nativo presente (`GMBasic/native/gmb_vm_c/build`) e librerie native in cache
(`~/.gmbasic/libs/<rid>/`, da `gmb install --runtime-only`).

## Documentazione

Manuali e note nel vault del monorepo (`01-Projects/GMBasicMonoRepo/`), non qui:
`Todo/(open) - IDE-TUI-BASIC-e-standalone-e-VSCode-LSP.md` (piano A/B/C: IDE in BASIC, IDE in C,
LSP per VS Code).
