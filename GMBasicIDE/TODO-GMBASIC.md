# TODO — GMBasicIDE come IDE per GMBasic

**Stato:** piano aggiornato il 2026-10-22 dopo tre decisioni di prodotto:

1. l'IDE edita **GMBasic nativo** (non traduce VB6 in GMBasic);
2. il compilatore è **C99Native** (unico target; IL non è più il default né il fallback);
3. i form sono **`.frm` compatibili VB6** (layout), con il codice GMBasic in file `.bas`.

Nessun sorgente di GMBasic o di GMBasicIDE è stato modificato da questo documento. Le prove
hanno prodotto solo file in `/tmp` e artefatti di build in `work/` del repo GMBasic.

**Perimetro:** rendere GMBasicIDE la shell IDE (progetto, editor, designer, properties, output,
run) del compilatore GMBasic, senza incorporare GMBasic nel processo dell'IDE.

**Legenda:** `P0` = serve per il primo uso reale · `P1` = necessario per un flusso completo ·
`P2` = rifinitura o ricerca. `[x]` = fatto e verificato.

Fonti lette (vault `~/Documents/AiWiki/01-Projects/GMBasic`):
`_Index_GMBasic.md`, `Guide/utente/Controlli-Pixel-Proprieta-Metodi-Guida-Utente.md`,
`Guide/utente/Controlli-Utente-Struttura.md`, `Guide/utente/Finestre-Eventi-Guida-Utente.md`,
`Guide/utente/Clipper-Controls-Reference.md`, `Guide/utente/CLI-Flags.md`,
`Todo/(open) - IDE-TUI-BASIC-e-standalone-e-VSCode-LSP.md`,
`Piani/(open) - Modello-Eventi-Controlli-Finestre.md`.

Fonti lette (repo `~/Documents/Projects/GMBasic`):
`README.md`, `Executor/`, `test_ilrun/`, `test_graphics_visible/`,
`native/gmb_compiler_c/frontend/`, `native/gmb_c99native/driver/`,
`native/gmb_c99native/src/gmb_c99_emit_controls.c`,
`native/gmb_runtime_c/include/gmb_ide.h`, `native/gmb_runtime_c/include/gmb_form.h`,
`common/Registry/canonical.registry.json`, `samples/Community/graphics/ControlsFullDemo.bas`,
`tools/gmb_c99native_run.sh`.

Fonti lette (questo repo): `src/core/vb_parse.c`, `src/core/vb_model.h`,
`src/platform/mac/VBEditorWindow.m`, `src/platform/mac/VBCodeEditor.m`, `src/tools/vbtool.c`.

**Sintesi delle modifiche rispetto alla versione precedente:**

- target unico **C99Native**: niente Run IL di default, niente "grafica solo su IL";
- form **`.frm` VB6** (layout) invece di `.gmbform` JSON; codice in `.bas`, `.ui.bas` generato;
- aggiunte le **omissioni upstream** O1–O10 (Label pixel, progetto/CLI, mappa diagnostica,
  checkbox/combo/frame/timer e round-trip `.frm`);
- misurata la capability reale di C99Native (sezione 1.1) e i suoi limiti;
- **correzione 2026-10-24**: la risoluzione multi-file via `IMPORT` **esiste già** (misurata in
  sezione 1.1); manca il progetto/CLI, non il linking. O7 riformulata e declassata a P1.
- **O7 fatto (2026-10-24)** in GMBasic: `gmb_inc_resolve` è ricorsivo, legge
  `importFolders`/`importFiles` dei settings, segue gli `IMPORT` di ogni modulo e usa
  `compiler.searchFolders` (default `include`/`Controls`/`<install>/libs/*`) al posto
  dell'elenco cablato — stesse regole nell'Orchestrator C#. In più il CLI di progetto
  `gmb check/build/run --project` (esclusioni, entry, `NAMESPACE`/`IMPORT`, `--json`).
  Sample `samples/modules/FolderResolution`.
- **O8 implementato (2026-10-24)** in GMBasic: `#line` per statement (`gmb_c99_emit.c`),
  segmenti dei moduli in `gmb_inc_resolve`, `--line-map <json>` nel driver. `cc` e gli errori
  di emissione citano `file:riga` del `.bas`; gate `native/gmb_c99native/tests/c99_line_map_test.sh`.

---

## 0. Decisioni confermate e perimetro

| # | Decisione | Conseguenza |
|---|---|---|
| D1 | **GMBasic nativo** | L'IDE colora, completa e controlla la grammatica GMBasic; nessun traduttore VB6→GMBasic. |
| D2 | **C99Native** | Check, Run e Build usano il driver/emitter C99Native. IL non è richiesto; non si apre l'interprete VB6 per GMBasic. |
| D3 | **Form `.frm` compatibili VB6** | Il layout vive in un `.frm` VB6 (`VERSION 5.00`, `Begin VB.Form`, `Begin VB.*`, proprietà in twip); il codice GMBasic vive in `.bas`. Niente formato `.gmbform` JSON. |
| D4 | **Confine a processo** | L'IDE invoca un CLI stabile (`gmb check/run/build --json`); non linka il compilatore né il runtime GMBasic. |
| D5 | **Omissioni esplicite** | Se C99Native non ha un controllo/feature necessario, o si implementa upstream (sezione 6) o il designer lo rifiuta con un messaggio. Mai emulazione silenziosa. |

**Non obiettivi (MVP):**

- riscrivere GMBasic dentro `src/core/vb_code.c` o nel runtime VB6 di GMBasicIDE;
- linkare `gmb_compiler_c`, le assembly C# o la VM/C di GMBasic nel processo dell'IDE;
- tradurre codice **VB6** in GMBasic (proprietà `Control.Property`, handler `Private Sub`,
  control array, `Me`, `DoEvents`, `Option Explicit`): il `.frm` è compatibile come **layout**,
  non come codice eseguibile VB6;
- un debugger GMBasic completo, IntelliSense semantico su tutti i 1567 built-in, Menu Editor
  completo, data binding ADO.

Il principio resta quello del core: **tutta la logica testabile sta in C99 in `src/core/`**,
Cocoa fa solo I/O e UI. Il confine con GMBasic è un contratto CLI versionato.

---

## 1. Che lingua e che compilatore sono GMBasic

GMBasic **non è Visual Basic 6**: è un BASIC compilato, con una superficie molto più larga e un
modello UI che vive nel codice. Il target è C99Native, quindi la verità sulla grammatica è il
**front-end C + emitter C99Native**, non il registry C# né i runner IL.

| Aspetto | Fatto verificato | Conseguenza per l'IDE |
|---|---|---|
| Natura | Compilatore con front-end C e C# e più backend storici; il target scelto è `native/gmb_c99native`, che emette C e lo linka al runtime C. | L'IDE compila e lancia un processo; non interpreta. |
| Lessico | Keyword **case-insensitive**; il lexer C ha **1285** entry `GMB_KEYWORD_ENTRY`; molte parole sono **soft keyword** riconosciute dal parser (`BUTTON`, `TEXTBOX`, `CONTROL`, `LABEL`, …), non dal lexer. | La tabella del syntax highlighting va generata dal lexer **C99Native** + parser, non copiata dal registry C#. |
| Superficie | `common/Registry/canonical.registry.json`: **1567** procedure, **115** comandi, **284** costanti. | Highlight/completion: keyword dal compilatore, nomi delle procedure dal registry; test di sincronia. |
| Sintassi | Statement per riga o separati da `:`; commenti `'` e `REM`; continuazione `_`; numeri di riga BASIC opzionali; label. | Serve un lexer GMBasic dedicato; quello VB6 attuale non basta. |
| Tipi | `STRING`, `BOOLEAN`, `BYTE`, `INTEGER`, `LONG`, `DATE`, `SINGLE`, `DOUBLE`, `CURRENCY`; `TYPE`/`CLASS` utente. | Highlight dei tipi; niente tipi VB6 non presenti. |
| Dichiarazioni | `DIM … AS`, `CONST`, `TYPE`, `ENUM`, `CLASS … INHERITS`, `NAMESPACE`, `IMPORT`, `DECLARE … LIB`. | Outline e procedure combo devono conoscere classi, namespace e tipi, non solo `Sub`/`Function`. |
| Controllo | `IF/ELSEIF/END IF`, `SELECT CASE`, `FOR/NEXT`, `FOR EACH`, `DO/LOOP`, `WHILE/WEND`, `GOTO`/`GOSUB`, `ON ERROR`/`RESUME`/`ERR`, `TRY/CATCH`. | Il "line executable" e i breakpoint non sono quelli VB6: in modalità GMBasic i breakpoint sono disabilitati. |
| UI | Namespace `CONTROLS` + `WINDOWS`; controlli **a celle** in TUI e **a pixel** con `SCREEN`; `WINDOW PUMP`, `ON …`, proprietà come clausole (`NAME`, `TAG`, `TOOLTIP`, `VISIBLE`, `ENABLED`, `TABINDEX`, `TABSTOP`, `STYLE`) e `CONTROL SET/GET`. | Il designer genera codice pixel; il chrome è SDL/GMBasic, non VB6: non è un preview WYSIWYG al 100%. |
| Progetto | Il compilatore non ha un progetto `.vbp`/`.frm` nativo. `DiscoverSourceFiles` scansiona ricorsivamente `*.bas` e `*.inc`, più `include/` e `Controls/`; `app.settings.json` e `Resources/system.settings.json` contengono le impostazioni. | L'IDE aggiunge il `.frm` come layout e un manifest proprio, e genera `.ui.bas`; il compilatore continua a ignorare `.frm`. |
| Build | `gmb_c99native_driver` emette C (`--no-exe`) e di default prova a linkare; il link nudo fallisce (`gmb_runtime.h` non trovato), il link completo è in `tools/gmb_c99native_run.sh`. | Serve un CLI stabile che incorpori il link completo e la scoperta del progetto. |
| Multi-file | **Già risolto via `IMPORT`**: un modulo `.bas`/`.inc` che dichiara `NAMESPACE X` entra nel build se il sorgente scrive `IMPORT X`; il driver ne prepende il testo (misurato: `Main.bas` + `Ui.bas` → `gmb_user_frmShow()` nel C emesso). Cerca nella dir del programma, `include/`, `Controls/` e `<root>/libs/*`: **non è ricorsivo**, e un file senza `NAMESPACE` o non importato non viene compilato. | Nessun ponte di concatenazione: il codegen emette `NAMESPACE`/`IMPORT` e il progetto resta su una dir piatta (o `include/`). Al CLI serve l'elenco file/esclusioni e la entry (O7). |
| Diagnostica | Front-end: `parse error: <msg>` con la riga, rc=3. Emitter: `emissione: <file>:<riga>: <msg>`, rc=4 (era `riga N` del testo unito). Errori `cc`: **citano il `.bas`** grazie ai `#line` (O8, fatto il 2026-10-24). | L'IDE usa direttamente `file:riga`; `--line-map <json>` dà gli anchor per chi compila il C per conto proprio. |
| Driver | `tools/gmb_c99native_driver` può essere **stantio** rispetto a `native/gmb_compiler_c/build/gmb_c99native_driver`; `tools/gmb_c99native_run.sh` preferisce il build più recente. | Il CLI deve costruire/verificare il driver e dichiarare la versione; non usare il binario in `tools/` a occhi chiusi. |
| Toolchain | Su questa macchina: .NET **10.0.301**, Apple clang **21.0.0**, cmake **4.4.3**, driver C99Native compilato. `GMBASIC_DEV_MODE=1` serve per il licensing in sviluppo. | First-run check di `cc`, `cmake`, driver, root GMBasic e librerie runtime. |

### 1.1 Cosa fa davvero C99Native (misurato il 2026-10-22)

Prova eseguita con `tools/gmb_c99native_run.sh /tmp/gmbprobe/ui3.bas c99ui3s` e
`native/gmb_compiler_c/build/gmb_c99native_driver … --no-exe`.

| Costrutto | Esito C99Native | Note per il codegen |
|---|---|---|
| `SCREEN 12, 320, 200, 32` + `WINDOW PUMP` | **Funziona** | Il log dice `window 320x200 (logical)`, renderer software. |
| `WINDOW SET TITLE "…"` | **Funziona** | Emette `gmb_winstmt_set_title("", …)`. |
| `BUTTON x,y,w,h,"…" ON CLICK handler` | **Funziona** | Con `NAME`/`STYLE` passa da `gmb_form_add_button_css`, che applica anche `name=`, `tabindex=`, ecc. |
| `TEXTBOX x,y,w,h INTO var` | **Funziona solo con `INTO` + variabile dichiarata `STRING`** | Senza `INTO`: `CONTROL 'TEXTBOX' non supportato dall'emitter nativo`. Con `DIM var` (numerico) il puntatore stringa finisce in un `double`: sbagliato. Il codegen deve emettere `DIM var AS STRING` a livello modulo. |
| `CHECKBOX x,y,"…" INTO var` | **Percorso pixel presente nel sorgente attuale** (`gmb_control_add_checkbox`) | La proprietà viene applicata con `gmb_tui_last_handle()`: in grafica quel valore è l'ultimo controllo TUI, non il checkbox appena creato. `NAME`/`TABINDEX` vanno persi. Da correggere (O2). Dimensione fissa 200×22: il layout VB6 non è rispettato. |
| `LABEL …` | **Solo TUI** | `gmb_label_init` registra un controllo TUI; non esiste `gmb_form_add_label`. In una form pixel non compare. Da implementare (O1). |
| `PIXCOMBOBOX` / `PIXLISTBOX` | **Parser: `statement non riconosciuto`** | I rami esistono nell'emitter ma non sono raggiungibili dalla sintassi. ComboBox/ListBox pixel mancano (O3). |
| `ON WINDOW CLOSE GOSUB handler` | **Funziona** | Senza `GOSUB` l'emitter emette `gmb_window_unregister_event("CLOSE")`; la forma corretta è con `GOSUB`/`GOTO`. |
| `WINDOWCLOSED()` | **Funziona** | `gmb_form_closed()` nel ciclo. |
| `TIMER` / `TI` in espressione | **Non disponibile** | `TIMER` resta un identificatore C non definito; `TI` non viene risolto a `gmb_timer()` in espressione. Per un Timer serve un builtin/controllo (O6) o un'altra sorgente di tempo. |
| Form con figli `FRAME`/`PICTUREBOX` | **Nessun contenitore pixel** | Non esistono `gmb_form_add_frame`/`add_picturebox`; i figli vanno appiattiti o serve O4. |
| `#line` / mappa C→BASIC | **Fatto (O8)** | Ogni statement emette `#line <riga bas> "<file bas>"`; il driver accetta `--line-map <file.json>` con gli anchor `c_line → file/riga`. Misurato: `cc` su un MiniTest riporta `toggle_parity.bas:5:55: error: ...` invece di `out.c:132`. |
| `IMPORT` multi-file | **Funziona** | `Main.bas` con `IMPORT UI` + `Ui.bas` con `NAMESPACE UI`: il C emesso contiene `gmb_user_frmShow()` e il corpo. La lookup è per namespace, non per nome file; senza `IMPORT` (o con il modulo in una sottocartella) → `simbolo non trovato`, rc=4. |

### 1.2 Cosa NON esiste rispetto a VB6

- `LABEL` pixel, `FRAME`/`PICTUREBOX` come contenitori, `TIMER` come controllo.
- `OPTIONBUTTON` singolo (esiste `RADIO` come gruppo di opzioni).
- Control array (`Index`, `Load`/`Unload` di controlli).
- `ME`, `DOEVENTS`, `Option Explicit`.
- Proprietà VB6 `Control.Property = valore`: si usano clausole e `CONTROL SET/GET/GETS`.
- Access key `&` nelle caption: da verificare; non assumere la semantica VB6.
- Codifica eseguibile VB6 nel `.frm`: il file è compatibile come **layout**, non come codice.

Queste assenze non sono tutte "bug da colmare" per l'MVP: il designer deve **rifiutarle o
marcarle** esplicitamente. La sezione 6 elenca quelle che servono davvero al primo flusso.

---

## 2. Cosa c'è già in GMBasicIDE (e cosa va adattato)

| Area | Stato | Evidenza | Lavoro per GMBasic |
|---|---|---|---|
| Shell, tema, menu, toolbar, status bar | Pronto | `VBTheme`, `VBMenuBarView`, `VBToolbarView`, `VBEditorWindow` | Riuso; indicatore modalità `GMBASIC — C99Native`. |
| Project Explorer | Pronto per `.vbp` | `VBProjectOutlineView` | Nodi cartella, `.frm` (form), `.bas` (codice), `.inc`, `.ui.bas` generato; dirty marker. |
| Code editor | Pronto, highlighting VB6 | `VBCodeEditor`, `vb_code_highlight` con `keywords[]` fisso | Language mode GMBasic + tabella generata; handler `SUB`/`FUNCTION`; niente `Private Sub` automatico. |
| Moduli `.bas` | Già editabili e salvati | `codeForItem:` + `_moduleCodes` + `saveModules` | Spostare la cache e il salvataggio in `src/core/` (`VbDocument`), così `vbtool` li testa senza finestre. |
| Designer | Pronto, coordinate twip, multi-selezione, z-order, tab order | `VBDesignerView`, `vb_model.h` | Riusare `VbForm` come modello del `.frm`; filtro controlli supportati; scala twip→pixel. |
| Properties | Pronto, nomi proprietà VB6 | `VBPropertyPanel` | Mapping VB6 → clausole GMBasic; disabilitare ciò che non esiste; non perdere i valori nel `.frm`. |
| Progetto | `.vbp`/`.frm` | `vb_parse.c`, `vb_project_*` | **Riusare `vb_form_parse`/`serialize`** per il `.frm`; nuovo `VbGmbProject` per manifest e documenti. |
| Run | Interprete VB6 in-process | `VBRuntimeWindow`, `vb_run_event` | **Sostituire** con processo esterno `gmb run` (C99Native); nessun interprete VB6 per GMBasic. |
| Build | Assente | nessun `NSTask`/`fork` nel progetto | `VBProcessRunner` (Cocoa) + specifica comandi in core. |
| Diagnostica | Assente | `-diagnostics` è solo una stringa di stato | Modello errori, pannello Output/Error List, click-to-line. |
| Breakpoint/step | Solo VB6 | `VbForm.breakpoints`, gutter | **Disabilitare** in modalità GMBasic finché non esiste un DAP reale. |
| Form round-trip | `.frm` VB6 | `vb_form_parse`, `vb_form_serialize` | Preservare EOL (CRLF/LF), blocchi `BeginProperty` non-Font, proprietà sconosciute; test con form VB6 reale. |
| Harness headless | Pronto | `vbtool check` | Nuove sezioni `gmb *`; nessuna finestra. |

Nota importante: i moduli sono già modificabili e salvati, ma la cache vive nella finestra Cocoa
(`_moduleCodes`, `saveModules`). Per GMBasic — dove tutti i file sono moduli e il progetto è una
cartella — questa logica va spostata in un documento di progetto C, testabile da `vbtool`.

---

## 3. Architettura proposta

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

### 3.1 Layout di un progetto GMBasic gestito dall'IDE

```
HelloForm/
  HelloForm.gmbide.json      # manifest IDE: form, codice, entry, backend c99native
  app.settings.json          # impostazioni compilatore GMBasic (convenzione esistente)
  frmMain.frm                # layout VB6 (sorgente di verità del designer)
  frmMain.bas                # handler GMBasic dell'utente
  frmMain.ui.bas             # GENERATO: creazione controlli + frmMain_Show()
  Main.bas                   # SUB Main, generato una volta poi utente
```

Regole:

- `frmMain.frm` è l'unico sorgente del layout. Contiene `VERSION 5.00`, `Begin VB.Form`,
  i controlli VB6 con proprietà in twip e la sezione codice con solo gli `Attribute` e un
  commento `' GMBasic code: frmMain.bas`. È un `.frm` **valido per VB6 come layout**.
- `frmMain.bas` è l'unico sorgente del codice della form. Non viene mai sovrascritto.
- `frmMain.ui.bas` è generato, deterministico e rigenerabile; non si modifica a mano.
- `Main.bas` viene creato una volta (con `SUB Main` che chiama la form di startup) e poi è
  dell'utente. Se manca, l'IDE lo ricrea solo dopo conferma.
- Il compilatore GMBasic ignora `.frm` e `.gmbide.json`; vede solo `.bas`/`.inc`.
- Il `.frm` **non** contiene codice GMBasic eseguibile. Questa è una scelta deliberata:
  - evita due copie dello stesso codice (`.frm` e `.bas`);
  - mantiene il `.frm` apribile da VB6 senza errori di sintassi;
  - permette di importare un `.frm` VB6 esistente e mostrarne il codice VB6 in sola lettura,
    senza fingere che sia compilabile da GMBasic.

### 3.2 Manifest `HelloForm.gmbide.json`

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

- I percorsi sono relativi alla cartella del progetto.
- Il manifest non contiene percorsi assoluti né credenziali; il root del toolchain e
  `GMBASIC_DEV_MODE` stanno nelle impostazioni dell'app.
- Se il manifest manca, l'IDE può inferire l'entry (`Main.bas` o il primo `.bas`) e mostrare
  un avviso; non deve indovinare le form.
- I comandi non sono nel manifest: l'IDE usa `gmb` dal settings, con fallback configurabile.

Perché non un `.vbp`: un `.vbp` che elenca i `.bas` GMBasic non è compilabile da VB6 e
tradirebbe l'aspettativa di compatibilità. Il `.frm` è l'artefatto compatibile; il progetto
resta una cartella con manifest. (Import/export `.vbp` come contenitore di soli layout: P2.)

Perché non un `.gmbform` JSON: il `.frm` ha già tutto il layout, lo z-order, il tab order e le
proprietà; un secondo formato sarebbe una copia da tenere sincronizzata, contro la richiesta di
formato VB6.

### 3.3 Round-trip del `.frm`

Il parser/serializer esistente è il punto di partenza, ma per un `.frm` VB6 reale servono:

- **EOL**: il parser normalizza a LF; il serializer scrive LF. Un `.frm` VB6 è normalmente CRLF.
  Aggiungere `VbForm.eol` (rilevato al load) e usarlo in `vb_form_serialize`; i form nuovi in
  modalità GMBasic usano CRLF. Il fixture HelloWorld attuale è LF e deve restare byte-identico.
- **Proprietà sconosciute**: il parser le conserva in `VbProp`; il serializer le riscrive.
  Verificare che l'ordine e la spaziatura restino stabili abbastanza per un secondo salvataggio
  identico; per un `.frm` importato è accettabile una normalizzazione al primo salvataggio,
  purché nessuna proprietà vada persa.
- **`BeginProperty` non-Font**: oggi il serializer scrive solo il blocco `Font`; altri blocchi
  (es. `Picture`, `OLEDragImage`) diventano proprietà piatte e perdono la struttura. O si
  preserva il blocco raw, o si rifiuta il form con un errore esplicito. Non salvare in silenzio
  un `.frm` che VB6 non riaprirebbe.
- **Codice**: la sezione codice del `.frm` si conserva così com'è. Per un form nuovo è il
  commento/attributi; per un `.frm` VB6 importato è codice VB6 in sola lettura.
- **Form name**: il nome del form è quello del `Begin VB.Form` (es. `frmMain`); gli `Attribute`
  devono corrispondere. Il designer non rinomina il form senza aggiornare il `.frm` e il
  manifest.

Accettazione: `vb_form_load` → `vb_form_save` su un fixture CRLF con `Font` e proprietà
sconosciute non perde proprietà; il secondo salvataggio è byte-identico. Il fixture HelloWorld
resta byte-identico al primo salvataggio.

### 3.4 Contratto di codegen

`frmMain.ui.bas` (generato):

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

`frmMain.bas` (utente, stub creati una volta):

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

Regole:

- **Nomi handler**: `frmX_<control>_<event>` (es. `frmMain_cmdOK_Click`). GMBasic non ha lo
  scope di form di VB6 e i `SUB` di primo livello sono globali; il prefisso evita collisioni.
  `Form_Load`/`Form_Unload` diventano `frmX_Load`/`frmX_Unload`.
- **Variabili valore**: `DIM <control> AS STRING` a livello modulo in `.ui.bas` per TextBox,
  ComboBox/ListBox (quando disponibili), DatePicker; `DIM <control> AS DOUBLE`/`BOOLEAN` per
  CheckBox, Radio, Slider. La `DIM` top-level è verificata in un file singolo; la visibilità
  cross-file va verificata con il progetto (O7). Fallback documentato: accesso con
  `CONTROL GET/GETS "<name>", "text"`.
- **Eventi**: il codegen collega gli eventi previsti dalla tabella di mapping; crea gli stub in
  `frmMain.bas` solo se mancano. Se l'utente cancella un handler collegato, `Check` deve dare un
  errore chiaro ("SUB ... non definita"), non un crash.
- **Ordine**: l'ordine di emissione è lo z-order del `.frm` (figli dopo il contenitore, come nel
  file). Per i Frame/PictureBox appiattiti, le coordinate dei figli sono rese assolute.
- **Tab order**: VB6 ha un `TabIndex` per contenitore; GMBasic ha un ordine globale per la
  finestra. Il codegen calcola un ordine DFS stabile: contenitore, poi figli per `TabIndex`,
  poi fratelli successivi; assegna `TABINDEX` globali 0..n-1. Documentare la differenza.
- **Generato**: `frmMain.ui.bas` non contiene mai `SUB Main`, handler utente o codice
  personalizzato. L'hash nell'header permette di rilevare una modifica manuale: l'IDE avvisa e
  non sovrascrive senza conferma.
- **Determinismo**: stessi `.frm` + stessa versione del generatore → stesso `.ui.bas` byte per
  byte. I test golden lo verificano.

### 3.5 Progetto e scoperta dei file

Il compilatore **non è mono-file**: `IMPORT <NS>` più `NAMESPACE <NS>` nel modulo importato
bastano a compilare insieme più `.bas` (sezione 1.1). Il driver nativo cerca i moduli nella
directory del programma, in `include/` risalendo, in `Controls/` e in `<root>/libs/*`; **non**
scende nelle sottocartelle. Il C# `Orchestrator` è più largo, ma resta `IMPORT`-driven: un file
che nessuno importa non viene compilato.

**Allineate il 2026-10-24.** Il driver nativo (`gmb_inc_resolve.c`) fa ora le stesse cose
dell'Orchestrator C#: scansione **ricorsiva** della radice, `importFolders`/`importFiles` dei
settings, grafo `IMPORT` seguito su **ogni** file caricato (la dipendenza è emessa prima del
dipendente), shadowing per nome fra progetto e cartelle globali. Erano cinque divergenze
misurate: dir piatta, import solo dalla entry, `forms/` invisibile, `app.settings.json`
ignorato, nessun transito.

Le cartelle `include/`, `Controls/`, `<install>/include`, `<install>/Controls`,
`<install>/libs/*` restano, ma come **default** di `compiler.searchFolders` nei settings del
**compilatore** (non dell'app): `{project}`/`{install}` come placeholder, `*` finale = ogni
sottocartella, percorsi assoluti ammessi, scansione ricorsiva di ogni voce. Un progetto le
sostituisce da `app.settings.json`; `ImportFolders`/`ImportFiles` continuano a vivere nei
settings dell'app e sono risolti sia rispetto al progetto sia alla root dell'install.

Misurato sul sample `samples/modules/FolderResolution`: C# e C99Native caricano gli **stessi 8
moduli** e compilano; il C nativo stampa una riga per cartella. Gate:
`native/gmb_c99native/tests/c99_module_resolve_test.sh` (15 asserzioni: ricorsione, import
folder/file fuori dalla radice, glob, transito, esclusione dei non importati, ordine, shadowing,
default, path assoluti e `searchFolders: []`).

**CLI di progetto: implementato (2026-10-24).** `native/gmb_compiler_c/src/cli/gmb_cli.c`,
target CMake `gmb_cli` → `build/gmb`:

```
gmb check --project <dir> [--entry <f>] [--json] [--strict]
gmb build --project <dir> [--entry <f>] [--out <exe>] [--json]
gmb run   --project <dir> [--entry <f>] [--json]
```

La discovery non è duplicata: il CLI chiama `gmb_project_sources` / `gmb_resolve_module_files`
(la stessa del compilatore). Entry esplicita o unico `SUB Main` (altrimenti errore con i
candidati); `work/`, `bin/`, `obj/`, `.git/`, `.vs/`, `.build/`, `node_modules/` esclusi;
`NAMESPACE` mancante = avviso (`--strict` lo rende errore); **`IMPORT` non risolto = errore**
con `file:riga`, riconoscendo i namespace builtin (`gmb_ns.h`: `IMPORT TRADING`, `CONSOLE`…
non richiedono un file). Exit stabili: 0 ok · 2 progetto/compilazione · 3 parse · 4
emissione/cc · 5 link · 6 runtime. Gate: `native/gmb_compiler_c/tests/gmb_cli_project_test.sh`
(14 asserzioni).

Limite dichiarato del link: `build` linka `libgmb_runtime.a` + `-lm -lpthread`, sufficiente per
i programmi console. Chi usa chart/db/AI/torch richiede la ricetta completa di
`tools/gmb_c99native_run.sh`; il CLI lo dice esplicitamente quando l'archivio runtime manca.

### Verifica della suite: `gmb minitests`

```bash
build/gmb minitests                        # riepilogo + istogramma CC-FAIL per categoria
build/gmb minitests --json > oggi.json     # per il diff fra due run
build/gmb minitests --baseline ieri.json   # rosso SOLO per le regressioni
build/gmb minitests --filter trading/ --cc clang   # sottoinsieme / A-B del compilatore

tools/check_minitests.sh                   # GATE: usa il baseline versionato
tools/check_minitests.sh --update          # rigenera il baseline dopo un fix
```

Sostituisce lo sweep ad hoc di `tools/c99_compile_errors.sh` (che ora è un alias che delega
qui): scoperta ricorsiva ordinata, **la stessa ricetta `-I` di `build`** (`build_inc()`), TAG
`interactive`/`heavy` esclusi come prima, exit 0/2/1. Nessun link, nessuna esecuzione: misura
il codegen. Gate del comando: `native/gmb_compiler_c/tests/gmb_cli_minitests_test.sh` (31).
Misura 2026-10-24: 1948 file, 1914 eseguiti, **1830 PASS · 37 CC-FAIL · 47 DRIVER-FAIL · 0 crash**.
Dopo il fix di `LIBRARY` (stesso giorno, §12): **1831 PASS · 36 CC-FAIL · 47 DRIVER-FAIL · 0
crash**.
Baseline versionato: `native/gmb_compiler_c/tests/minitests_baseline.json` (83 FAIL noti); il
gate è `tools/check_minitests.sh` (exit 0 nessuna regressione, 2 regressione, 1 setup).


**Nessun ponte di concatenazione**: il codegen genera già moduli importabili (`NAMESPACE` in
`frmX.ui.bas` + `IMPORT` in `frmX.bas`/`Main.bas`), quindi la demo può girare su una dir piatta
senza aspettare O7.

---

## 4. Mappatura VB6 → GMBasic

### 4.1 Form e finestra

| VB6 | GMBasic | Stato |
|---|---|---|
| `Form` | `SCREEN 12, w, h, 32` + controlli + `WINDOW PUMP` | Funziona; w/h in pixel. |
| `Caption` | `WINDOW SET TITLE "…"` | Funziona. |
| `ClientWidth`/`ClientHeight` | argomenti di `SCREEN` | twip / 15, arrotondato; test. |
| `Left`/`Top` della finestra | `WINDOW MOVE` | P2. |
| `BorderStyle` | `WINDOW FRAME SINGLE/DOUBLE/ROUNDED` | Da verificare sul path `SCREEN`; P1. |
| `BackColor` | `STYLE "bg=#RRGGBB"` / sfondo finestra | Da verificare per la finestra; P1. |
| `Font` | `STYLE` / API font | Solo se il runtime lo espone per il controllo; P1. |
| `StartUpPosition` | `WINDOW MOVE` | P2. |
| `ScaleMode` | sempre pixel | Conversione fissa 15 twip = 1 px. |
| `Caption` con `&` | testo letterale | Verificare access key; P2. |

### 4.2 Controlli

| VB6 | GMBasic | Stato |
|---|---|---|
| Label | `LABEL x, y, w, h, "testo"` | **P0 upstream O1**: oggi `LABEL` è solo TUI e non ha altezza. |
| TextBox | `TEXTBOX x, y, w, h INTO var$` | Funziona con `DIM var AS STRING`; senza `INTO` l'emitter rifiuta. |
| CommandButton | `BUTTON x, y, w, h, "caption"` + `ON CLICK` | Funziona; `NAME`/`TABINDEX` via CSS-like spec. |
| CheckBox | `CHECKBOX x, y, "caption" INTO var` | Pixel presente ma dimensione fissa e proprietà non applicate (O2). |
| OptionButton | `RADIO` (gruppo) | **Non equivalente**: un gruppo, non un controllo per opzione. O5 o rifiuto esplicito. |
| ComboBox | `COMBOBOX` (TUI) / `PIXCOMBOBOX` (emitter non raggiungibile) | **P1 O3**: il parser rifiuta la variante pixel. |
| ListBox | `LISTBOX` (TUI) / `PIXLISTBOX` (emitter non raggiungibile) | **P1 O3**. |
| Frame | — | **Nessun contenitore pixel** (O4). MVP: figli appiattiti + etichetta/bordo; P1: vero contenitore. |
| PictureBox | `CANVAS` / `IMAGE` | Solo disegno; niente figli (O4). |
| HScrollBar / VScrollBar | `SCROLLBAR`; `PIXSLIDER` è solo emitter | Adattamento (orientamento da w/h); il parser non raggiunge `PIXSLIDER` (come O3). |
| Timer | — | **Nessun controllo**; `TIMER`/`TI` non sono espressioni utilizzabili (O6). Il Timer del designer resta disabilitato finché non c'è O6. |
| Data / DataGrid | `GRID`; `PIXGRID` è solo emitter | P2, senza binding ADO; parser da verificare. |
| Menu | `MENU` (TUI); `PIXMENU` è solo emitter | P2; Menu Editor assente. |
| CommonDialog | `FILEDIALOG`/`PRINTERDIALOG`/`COLORPICKER`/`DATEPICKER` | Per tipo; P2. |
| StatusBar | `STATUSBAR` | Diretto, pannelli limitati; P1. |
| ProgressBar | `PROGRESS` | Diretto; P1. |
| Image | `IMAGE` | Pixel-only; P1. |
| Line / Shape | `LINE` / `CIRCLE` / `PSET` | Non sono controlli; P2. |
| DriveListBox / DirListBox / FileListBox | `FILEEXPLORER` | Adattamento; P2. |
| ListView | `DATALIST` / `TBROWSE` | Adattamento; P2. |
| TreeView | `TREEVIEW` (TUI); `PIXTREEVIEW` è solo emitter | P2; pixel da verificare. |
| RichTextBox | `RICHTEXT` | Pixel-only; P2. |
| UpDown | `NUMERICUPDOWN` / `SPINNER` | Adattamento; P2. |
| SSTab | `TAB` | Adattamento; P2. |
| Slider (non standard VB6) | `SLIDER` | Diretto; P2. |
| Chart | `CHART` | Pixel-only; P2. |
| OLE / Toolbar / CoolBar / ImageList | — | **Non supportati**: rifiuto esplicito. |

Le righe P1/P2 non sono tutte verificate: `PIX*` nell'emitter non implica che il parser lo
accetti (è il caso di O3). Il designer abilita un controllo solo dopo un probe di capability
nel progetto; le voci non provate restano disabilitate con tooltip.

### 4.3 Proprietà

| VB6 | GMBasic | Note |
|---|---|---|
| `Name` | clausola `NAME "…"` / `name=` nello spec | Obbligatoria per gli handler e `CONTROL *`. |
| `Caption` | argomento di creazione / `caption` / `CONTROL SET` | Per Label/Button è l'argomento; per il form `WINDOW SET TITLE`. |
| `Text` | variabile `INTO` / `CONTROL SET "x", "text"` | Per TextBox il valore è la variabile; initial text assegnato prima della creazione. |
| `Visible` / `Enabled` | `VISIBLE n` / `ENABLED n` / `visible=`/`enabled=` | Lo spec base supporta entrambi; test per ogni tipo. |
| `TabIndex` / `TabStop` | `TABINDEX n` / `TABSTOP n` | Ordine globale calcolato (vedi 3.4). |
| `ToolTipText` / `Tag` | `TOOLTIP "…"` / `TAG "…"` | Supportati nello spec base. |
| `Left`/`Top`/`Width`/`Height` | argomenti di creazione / `CONTROL MOVE` | twip / 15. |
| `BackColor`/`ForeColor`/`Font` | `STYLE "bg=#… fg=#… font=…"` | Solo i token supportati dal runtime; verificare. |
| `BorderStyle`/`Locked`/`MaxLength`/`PasswordChar` | `STYLE` o `CONTROL SET` | Da verificare per tipo; altrimenti badge "non mappato". |
| `Index` (control array), `DataSource` | — | Non supportati; errore esplicito. |

### 4.4 Eventi

| VB6 | GMBasic | Note |
|---|---|---|
| `Form_Load` | chiamata a `frmX_Load()` in `frmX_Show()` | Stub generato; se manca, errore di compilazione chiaro. |
| `Form_Unload` | `ON WINDOW CLOSE GOSUB frmX_Unload` | Verificato: serve `GOSUB`; senza, l'emitter disregistra. |
| `Click` / `DblClick` | `ON CLICK` / `ON DBLCLICK` | `Click` verificato su Button; gli altri da testare. |
| `Change` | `ON CHANGE` | Diretto; test. |
| `KeyPress`/`KeyDown`/`KeyUp` | `ON KEYPRESS`/`ON KEYDOWN`/`ON KEYUP` | **Senza argomenti**: `KeyAscii` non arriva; usare i getter di stato. |
| `MouseDown`/`MouseUp`/`MouseMove` | `ON MOUSEDOWN`/… | **Senza** button/x/y; usare `MOUSE.X/Y/BUTTON`. |
| `GotFocus`/`LostFocus` | `ON GOTFOCUS`/`ON LOSTFOCUS` | Diretto; test. |
| `Validate` | `ON VALIDATE` | Diretto; test. |
| `DragOver`/`DragDrop` | `ON DRAGOVER`/`ON DRAGDROP` | Diretto; test. |
| `Form_Resize`/`Activate`/`Deactivate` | `ON WINDOW RESIZE/ACTIVATE/DEACTIVATE GOSUB` | Da verificare sul runtime SDL. |
| `Timer` | — | O6; nessun evento nativo. |

---

## 5. Omissioni da implementare (prerequisiti upstream)

Le voci O1–O9 sono modifiche al repo **GMBasic**; O10 è una modifica al **core di GMBasicIDE**.
Senza queste, l'IDE può funzionare solo con un sottoinsieme dichiarato.

| ID | Omissione | Priorità | Criterio di accettazione |
|---|---|---|---|
| O1 | **Label pixel** | P0 | `gmb_form_add_label(x,y,w,h,text)` / `gmb_control_add_label`; sintassi `LABEL x,y,w,h,"text"` (5 argomenti) distinta dalla TUI a 4; testo disegnato con tema, `NAME`/`STYLE`/`VISIBLE` applicati; test dummy SDL. |
| O2 | **CheckBox pixel completo** | P1 | API con `w,h` (o `_ex`); `NAME`/`TABINDEX`/`VISIBLE`/`ENABLED` applicati al widget restituito, non a `gmb_tui_last_handle()`; test di rendering e proprietà. |
| O3 | **ComboBox/ListBox pixel** | P1 | Il parser accetta la variante pixel (o rende `COMBOBOX`/`LISTBOX` polimorfici con `SCREEN`); `INTO`; runtime `gmb_form_add_dropdown`/`listbox`; test. |
| O4 | **Frame/PictureBox contenitore** | P1 | Contenitore pixel con figli, clipping, offset e `Enabled=False` che disabilita i figli; codegen e designer; test annidato. MVP senza O4: appiattimento esplicito con badge. |
| O5 | **OptionButton pixel** | P1 | Controllo singolo con gruppo (o API radio-group con layout esplicito); semantica VB6; test. Senza O5: rifiuto esplicito. |
| O6 | **Timer** | P1 | Builtin `TIMER()` in ms (o controllo `TIMER`) utilizzabile in espressione; il codegen genera il dispatch a intervallo; test. Senza O6: Timer disabilitato. |
| O7 | **Progetto e scoperta file** | **Fatto 2026-10-24** | Risoluzione allineata (`gmb_inc_resolve`: ricorsione, `importFolders`/`importFiles`, `compiler.searchFolders`, `IMPORT` transitivi, shadowing) **e** CLI `gmb check/build/run --project`: esclusioni (`work/`, `bin/`, `obj/`, `.git/`, …), entry esplicita o unico `SUB Main`, `NAMESPACE` mancante = avviso (`--strict` = errore), `IMPORT` non risolto = errore `file:riga` con i namespace builtin riconosciuti, `--json`, exit 0/2/3/4/5/6. In più `gmb minitests`: **punto unico di verifica della suite** (driver → C → `cc -fsyntax-only`) con `--baseline` per essere rosso solo sulle regressioni; gate `tools/check_minitests.sh` + baseline versionato `native/gmb_compiler_c/tests/minitests_baseline.json`; `tools/c99_compile_errors.sh` è ora solo un alias. Gate: `c99_module_resolve_test.sh` (15) + `gmb_cli_project_test.sh` (14) + `gmb_cli_minitests_test.sh` (31), sample `samples/modules/FolderResolution`. Limite: il link è console-only (`libgmb_runtime`), chart/db/AI richiedono la ricetta di `tools/gmb_c99native_run.sh`. |
| O8 | **Mappa diagnostica C→BASIC** | **Fatto 2026-10-24** | Implementati entrambi: `#line` per statement e `--line-map <json>`. `gmb_resolve_includes_mapped` produce i segmenti (modulo `IMPORT`ato + file di ingresso), l'emitter li usa e `c99_fail` cita `file:riga`. Test: `native/gmb_c99native/tests/c99_line_map_test.sh` (8 asserzioni). Resta solo l'esposizione in `gmb check --json`, che è O9. |
| O9 | **CLI stabile `gmb`** | **P0** | `check/run/build --json`, exit code stabili, stdout dati/stderr progress, `doctor`, versione driver, link completo. |
| O10 | **`.frm` VB6 round-trip** | **P0** (GMBasicIDE) | EOL preservato (CRLF/LF), proprietà sconosciute conservate, blocchi `BeginProperty` non-Font preservati o rifiutati; secondo salvataggio identico; fixture CRLF. |

### 5.1 Priorità per il primo flusso

**P0 reale:** O1, O9, O10 — **O8 è fatto** (2026-10-24). Con questi si può fare una form con
Label/TextBox/Button, compilarla e lanciarla da C99Native, con errori cliccabili. La risoluzione
multi-file non è un prerequisito: esiste già (sezione 1.1).

**P1:** O2, O3, O4, O5, O6 — **O7 è fatto** (risoluzione + CLI di progetto). Completano la
maggior parte delle form VB6.

**P2:** Data/DataGrid, Menu Editor, grafica vettoriale, debug.

---

## 6. Piano di lavoro

### G0 — Contratto toolchain e omissioni C99Native (P0, upstream)

- [ ] **G0.1** CLI `gmb` con `check`, `build`, `run`, `doctor`; `--project <dir>`, `--json`,
      `--out <dir>`, `--backend c99native` (unico valore accettato nell'MVP), `--headless`
      per i test (`SDL_VIDEODRIVER=dummy`).
- [x] **G0.2** Progetto (O7): discovery ricorsiva + settings (parità C#/nativo), CLI
      `gmb check/build/run --project` con esclusioni, entry, `NAMESPACE`/`IMPORT` e `--json`.
      Il link è console-only: la ricetta completa (`tools/gmb_c99native_run.sh`) resta da
      integrare per chart/db/AI (vedi O7).
- [~] **G0.3** Diagnostica strutturata: la parte C→BASIC (O8) è fatta — `#line`, `--line-map`,
      `emissione: file:riga`. Resta il formato JSON `{"file","line","column","severity",
      "code","message"}` e la tabella degli exit code (0 ok, 2 compilazione, 3 parse, 4
      emissione, 5 link, 6 runtime) nel CLI, cioè O9.
- [ ] **G0.4** Driver fresco e dichiarato: `gmb doctor` costruisce/verifica il driver, stampa
      versione/hash e non usa un `tools/gmb_c99native_driver` stantio.
- [ ] **G0.5** Pixel Label (O1); test con `SDL_VIDEODRIVER=dummy` e con finestra reale.
- [x] **G0.6** `#line`/mappa (O8): fatto — `#line` per statement, `--line-map`, errori di
      emissione con `file:riga`; il gate è `native/gmb_c99native/tests/c99_line_map_test.sh`.
      Il test su un errore C reale è `gmb minitests` (l'alias `tools/c99_compile_errors.sh`
      delega lì), che ora stampa il `.bas`.
- [ ] **G0.7** Documentare la matrice C99Native (sezione 1.1) in `Guide/utente/`; niente
      promesse IL.
- [ ] **G0.8** (P1) O2–O6 in ordine: checkbox proprietà, combo/list, frame, option button,
      timer.

**Fallback IDE** (se G0 non è disponibile subito): comandi configurabili + parser diagnostica
tollerante. Non è il target e va marcato nei log; la concatenazione non serve più.
**Accettazione:** un progetto con `Main.bas`, `frmMain.ui.bas`, `frmMain.bas` compila e gira;
un errore in `frmMain.bas` è riportato con file/riga; `gmb doctor` dice cosa manca.

### G1 — Modello progetto e `.frm` (P0)

- [ ] **G1.1** `VbGmbProject` in `src/core/vb_gmb_project.c/.h`: caricamento cartella, manifest
      `.gmbide.json`, discovery `.frm`/`.bas`/`.inc`, esclusioni; associazione
      layout→codice→generato.
- [ ] **G1.2** Riuso di `vb_form_load`/`vb_form_save` per il layout; nessun adapter `.gmbform`.
- [ ] **G1.3** O10: `VbForm.eol`, preservazione proprietà sconosciute, blocchi `BeginProperty`
      non-Font; fixture CRLF; test di secondo salvataggio identico.
- [ ] **G1.4** `VbDocument`: path, testo, dirty, kind, encoding/EOL, save atomico; spostare qui
      la cache `_moduleCodes`/`saveModules` oggi in `VBEditorWindow.m`.
- [ ] **G1.5** Project Explorer GMBasic: cartelle, `.frm` (form), `.bas` (codice), `.inc`,
      `.ui.bas` (generato, read-only); doppio click, dirty marker, Save All.
- [ ] **G1.6** Import di un `.frm` VB6: layout nel designer; codice VB6 in una tab sola lettura
      con banner "non compilato da GMBasic"; nessuna traduzione.
- [ ] **G1.7** (P2) Import/export `.vbp` come elenco di layout, non come progetto compilabile.

**Accettazione:** `vbtool gmb-project <fixture>` elenca form/codice/entry; aprire, modificare e
salvare lascia il `.frm` senza perdite; il codice VB6 importato non viene riscritto.

### G2 — Language mode GMBasic (P0)

- [ ] **G2.1** Generatore della tabella keyword dal **lexer C99Native** + elenco soft keyword
      del parser; registry per i nomi delle procedure/comandi/costanti. Output
      `src/core/vb_gmb_keywords.h` con hash/versione di origine.
- [ ] **G2.2** `VbCodeLanguage` (`VB6`, `GMBASIC`) e `vb_code_highlight(..., language, out)`;
      commenti `'`/`REM`, continuazione `_`, stringhe, numeri, operatori, label, numeri di riga,
      keyword hard/soft; case-insensitive.
- [ ] **G2.3** Outline e procedure combo GMBasic: `SUB`, `FUNCTION`, `PUBLIC CLASS`, `TYPE`,
      `ENUM`, `NAMESPACE`, `GENERATOR`, label; niente `Property Get/Let` VB6.
- [ ] **G2.4** Handler template: `SUB frmX_<control>_<event>()` / `END SUB`; il combo eventi
      usa i nomi GMBasic e il prefisso form; nessun `Private Sub` automatico.
- [ ] **G2.5** In modalità GMBasic disabilitare gutter breakpoint, F8/F10, `Form_Load` VB6 e
      qualunque auto-handler VB6.
- [ ] **G2.6** (P1) Completion: keyword + simboli progetto + registry; se disponibile, usare
      `gmb ide --json`/LSP invece di duplicare l'analisi.
- [ ] **G2.7** Test di sincronia: `make check` fallisce se la tabella è più vecchia del lexer
      C99Native o se il conteggio non coincide.

**Accettazione:** fixture `samples/GMBasic/Language.bas`; snapshot degli span; nessun keyword
VB6 colorato per errore; handler GMBasic inserito dal combo.

### G3 — Check / Run / Build / Output (P0)

- [ ] **G3.1** `VbGmbBuild` in core: specifica comando, cwd, env, timeout, stop, exit code,
      quoting; nessuna stringa di shell costruita a mano.
- [ ] **G3.2** `VBProcessRunner` Cocoa (`NSTask`, pipe, streaming, stop del process group,
      terminazione) e `VBOutputView` in stile VB6 (monospace, sfondo bianco, niente Aqua).
- [ ] **G3.3** Parser diagnostica: JSON del CLI; fallback su `parse error`/`emissione`/`cc`;
      deduplica, ordina, click → `jumpToLine`; nessuna riga inventata.
- [ ] **G3.4** Comandi e stati: **Check** (C99Native, no link, multi-file), **Run**
      (C99Native, finestra SDL), **Build** (C99Native, eseguibile), **Stop**; status bar
      `CHECKING`/`BUILDING`/`RUNNING`/`ERROR`.
- [ ] **G3.5** First-run `gmb doctor`: root GMBasic, driver, cc, cmake, runtime/SDL, licenza;
      dialog Win98 con il comando esatto mancante; nessun crash.
- [ ] **G3.6** (P1) Log in `~/Library/Logs/GMBasicIDE/gmbasic.log`: command line, cwd, exit code,
      prime/ultime righe; nessun segreto.
- [ ] **G3.7** Ambiente Run: `GMBASIC_HEADLESS=0`, `SDL_VIDEODRIVER=cocoa` (default),
      `GMBASIC_DEV_MODE` solo se attivato; test headless con `SDL_VIDEODRIVER=dummy`.

**Accettazione:** un file con errore mostra file/riga e il click salta; Run apre la finestra
SDL e Stop la chiude senza orfani; Build produce un binario o un errore chiaro; l'IDE resta
reattivo.

### G4 — Designer → `.frm` → `.ui.bas` (P0/P1)

- [ ] **G4.1** Il designer in modalità GMBasic carica/salva `.frm` con `vb_form_load/save`;
      nessun sidecar di layout.
- [ ] **G4.2** Tabella di mapping come **dato** in core (tipo VB6 → verbo GMBasic, proprietà,
      stato `ok`/`adattato`/`non supportato`, note). Alimenta designer, properties, codegen e
      documentazione.
- [ ] **G4.3** Codegen deterministico `frmX.ui.bas`: header+hash, `DIM` value vars,
      `SCREEN`, `WINDOW SET TITLE`, controlli in z-order, `frmX_Load`, `ON WINDOW CLOSE GOSUB`,
      loop `WINDOW PUMP`.
- [ ] **G4.4** Stub in `frmX.bas` per gli eventi collegati, nomi `frmX_<control>_<event>`, mai
      sovrascritti; `Main.bas` generato una volta.
- [ ] **G4.5** Conversione twip→pixel: `VB_TWIPS_PER_PIXEL = 15`, interi, nessun drift;
      test di andata/ritorno.
- [ ] **G4.6** Properties panel: mapping di `Name`, `Caption`, `Text`, `Visible`, `Enabled`,
      `TabIndex`, `TabStop`, `ToolTip`, `Tag`, `BackColor`/`ForeColor`/`Font` (se supportati);
      valori non mappati restano nel `.frm` con badge, non vengono cancellati.
- [ ] **G4.7** Controlli: Label/TextBox/Button (P0); CheckBox (O2); ComboBox/ListBox (O3);
      Frame/PictureBox (O4, con appiattimento P0); OptionButton (O5); Timer (O6); Data/DataGrid
      e Menu (P2). I non supportati sono disabilitati nel toolbox con tooltip.
- [ ] **G4.8** Tab order globale stabile (DFS) e z-order; test con form annidata.
- [ ] **G4.9** Badge "adattato"/"non supportato" nel designer; (P2) "Preview" lancia `Run`.
- [ ] **G4.10** (P2) Round-trip inverso: parse di `.ui.bas` → `.frm` per progetti code-first.
- [ ] **G4.11** (P2) Menu Editor limitato a `MENU "File|Edit"`; niente sottomenu VB6 completi.

**Accettazione:** golden `*.ui.bas`; modifica caption/posizione → salvataggio → `Check` verde →
`Run` mostra il controllo nella posizione attesa; un controllo non supportato produce un errore
chiaro e **non** codice invalido; il secondo salvataggio è byte-identico.

### G5 — Debug (P2)

- [ ] **G5.1** In modalità GMBasic gutter breakpoint e F8/F10 sono **disabilitati** con un
      messaggio "debugger GMBasic non disponibile"; non riusare il break mode VB6.
- [ ] **G5.2** Solo se GMBasic espone un DAP reale (`--debug-tcp` è documentato, ma non è stato
      trovato nell'albero attuale): client DAP con breakpoint, step, call stack, locali.
- [ ] **G5.3** Fino ad allora: Run/Stop, output, salto alla riga d'errore; nessuna barra gialla
      finta.

### G6 — UX, documentazione, packaging (P1/P2)

- [ ] **G6.1** Indicatore "GMBASIC — C99Native" nel titolo/status; menu Project/Run
      contestuali; niente voci VB6 senza senso (Menu Editor, Componenti, Data).
- [ ] **G6.2** Dialog impostazioni: root GMBasic, path `gmb`, backend fisso `c99native`,
      dev mode; persistenza in Application Support, non nel progetto.
- [ ] **G6.3** Apri cartella / progetti recenti / drag&drop di una cartella o di un `.bas`;
      (P2) document types in `Info.plist`.
- [ ] **G6.4** Documentazione: sezione "GMBasic mode" in `AGENTS.md`, README con limiti noti,
      guida utente con la tabella di mapping e il flusso `.frm`→`.ui.bas`.
- [ ] **G6.5** Licenza/distribuzione: non incorporare GMBasic; `GMBASIC_DEV_MODE` solo in
      sviluppo; rispettare le edizioni del compilatore.

### G7 — Verifica (P0)

- [ ] **G7.1** Sezioni `vbtool check` (headless, solo core C):
      - `gmb project`: manifest, discovery, associazione form/codice, esclusioni;
      - `gmb form`: `.frm` load/save, EOL, proprietà sconosciute, `BeginProperty`, fixture CRLF;
      - `gmb language`: span (hard/soft keyword, commenti, stringhe, tipi, numeri, label),
        handler, case-insensitive, nessun keyword VB6;
      - `gmb codegen`: `.frm` → `.ui.bas` golden, z-order, tab order, twip, stub, hash,
        idempotenza;
      - `gmb diagnostics`: parse/emissione/cc con e senza mappa, multi-file;
      - `gmb mapping`: ogni controllo/proprietà VB6 ha uno stato; nessun fallback silenzioso;
      - `gmb command`: quoting, env, temp dir, exit code; nessun processo lanciato dal core.
- [ ] **G7.2** Fixture in `samples/GMBasic/`:
      `HelloForm` (Label/TextBox/Button), `MultiFile` (Main+Ui+User), `ImportedVB6`
      (CRLF, Font, codice VB6), `SyntaxError`, `Unsupported` (Timer/OptionButton/control array),
      `Nested` (Frame/PictureBox).
- [ ] **G7.3** Golden file + diff; test di mutazione (rompere parser/codegen e verificare che il
      test fallisca).
- [ ] **G7.4** End-to-end headless: `gmb check`/`gmb run` con `SDL_VIDEODRIVER=dummy` su un
      fixture che fa pochi `WINDOW PUMP` ed esce; exit code e output asseriti.
- [ ] **G7.5** Ispezione visiva manuale con `--window-snapshot` per output pane, error list,
      badge del designer; nessun test GUI automatico (regola del repo).
- [ ] **G7.6** (P2) Performance: apertura 50 file < 200 ms, rigenerazione 50 controlli < 50 ms.

---

## 7. Matrice operazioni (C99Native)

| Operazione | Comando target | Cosa copre | Limiti |
|---|---|---|---|
| **Check** | `gmb check --project <dir> --json` | front-end + emitter C99Native, multi-file, no link | non esegue; errori runtime non coperti. |
| **Run** | `gmb run --project <dir> --json` | build+link+esecuzione, finestra SDL | finestra esterna, non incorporata; debug assente. |
| **Build** | `gmb build --project <dir> --out <dir> --json` | eseguibile nativo | link completo dipende dal runtime GMBasic. |
| **Test headless** | `gmb run --project <dir> --headless` | esecuzione senza finestra (`SDL_VIDEODRIVER=dummy`) | non prova il rendering reale. |
| **Doctor** | `gmb doctor --json` | driver, cc, cmake, runtime, SDL, licenza | non installa dipendenze. |
| **Debug** | — | — | nessun DAP verificato. |

Ambiente Run: cwd = cartella progetto, `GMBASIC_HEADLESS=0`, `SDL_VIDEODRIVER=cocoa` (default),
`GMBASIC_DEV_MODE=1` solo in sviluppo. Per Check/Build: stesso cwd, `GMBASIC_ROOT` esplicito,
niente `GMBASIC_HEADLESS=1`. Output: stdout dati, stderr progress/diagnostica.

Per **Run** il JSON non deve mescolarsi all'output dell'app: il CLI emette un solo evento di
avvio/diagnostica (o li scrive su un fd/file di controllo), poi lascia stdout/stderr del processo
figlio raw. L'IDE distingue la fase di build (diagnostica JSON) dall'esecuzione (output utente).

Nota operativa: il driver scrive in `work/` accanto al sorgente se non si passa un `out.c`;
l'IDE deve passare una directory temporanea per **Check** e una build dir per **Build**, per non
sporcare il progetto. Il CLI di progetto deve fare lo stesso.

---

## 8. Rischi e mitigazioni

| # | Rischio | Impatto | Mitigazione |
|---|---|---|---|
| R1 | Un nome non importato usato come statement diventa una variabile implicita senza errore; e un modulo senza `NAMESPACE` (o non raggiungibile da un `IMPORT`) non entra nel build | medio | La **risoluzione** è allineata (ricorsione + settings + transito, 2026-10-24); resta O7/G0.2: `NAMESPACE` obbligatorio nel generato ed errore sui nomi non risolti. |
| R2 | Label pixel assente; checkbox/combo/list incompleti | alto | O1 P0, O2/O3 P1; designer filtra e dichiara i limiti. |
| R3 | Il chrome GMBasic è SDL, non VB6 | alto | Designer = layout editor + codegen, non preview WYSIWYG; badge. |
| R4 | `.frm` compatibile come layout, non come codice | alto | Documentare; codice VB6 importato in sola lettura; nessuna traduzione. |
| R5 | ~~Errori `cc` senza `#line`~~ — chiuso da O8 | — | `#line` + `--line-map`; resta solo il glue generato (es. la chiamata in `int main`) che non ha una riga BASIC propria. |
| R6 | Driver `tools/` stantio | medio | G0.4 `doctor`; usare il build più recente; versione/hash. |
| R7 | Superficie linguistica enorme e in evoluzione | medio | Tabella generata dal lexer C99Native + registry; hash e test di sincronia; commit pinnato. |
| R8 | Discovery ricorsiva di tutti i `.bas` | medio | Manifest + esclusioni; avvisare sui file estranei; nessun file generato in root. |
| R9 | `app.settings.json`/runtime risolti rispetto alla cwd | medio | cwd = progetto, `GMBASIC_ROOT` esplicito, log della command line. |
| R10 | Licensing/dev mode | medio | Mai `GMBASIC_DEV_MODE` di default; documentare; nessun bypass. |
| R11 | Nomi handler globali in GMBasic | medio | Prefisso `frmX_`; test di collisione tra due form. |
| R12 | Timer/debug promessi e non disponibili | medio | Disabilitare con messaggio; O6/DAP solo quando reali. |
| R13 | Twip/DPI e dimensioni diverse | basso | Conversione fissa 15; test su form VB6 reali; nessun drift. |

---

## 9. Decisioni ancora aperte

1. **Sezione codice del `.frm`**: layout-only con commento (consigliato) o codice GMBasic
   incorporato con mirror `.bas`? Il secondo duplica il codice e rende il `.frm` non-VB6.
2. **Manifest**: `.gmbide.json` (consigliato) o `.vbp` come contenitore? Il `.vbp` con `.bas`
   GMBasic non è compilabile da VB6; usarlo solo per import/export di layout (P2).
3. **Frame/PictureBox**: appiattimento con coordinate assolute (P0) o contenitore pixel reale
   (O4, P1)? Consigliato: appiattimento per la demo, O4 subito dopo.
4. **ComboBox/ListBox**: implementare O3 prima della demo o limitare la demo a
   Label/TextBox/Button/CheckBox? Consigliato: O3 in P1, demo con i quattro controlli base.
5. **Timer**: builtin `TIMER()` (O6) o generazione di un loop con `SLEEP`/`TI`? Consigliato:
   O6, perché `TI`/`TIMER` oggi non sono espressioni utilizzabili.
6. **CLI**: `gmb` upstream (consigliato) o wrapper locale che chiama driver+script? Il wrapper
   non aggiunge progetto né mappa righe; va bene solo come ponte.
7. **Naming handler**: `frmX_<control>_<event>` (consigliato) o namespace per form? Il namespace
   GMBasic esiste ma va verificato sul linker e sul codegen.
8. ~~**Build ponte**~~: chiusa. `IMPORT` multi-file funziona; il generato dichiara `NAMESPACE` e
   chi lo usa emette `IMPORT`. Resta O7 per elenco file/esclusioni/entry.

---

## 10. Primo incremento consigliato (demo end-to-end)

1. **O1 + O9** — Label pixel e CLI minimo (O8 mappa diagnostica è fatto; O7 progetto subito dopo).
2. **G1** — aprire una cartella con `HelloForm.gmbide.json`, `frmMain.frm`, `frmMain.bas`,
   `Main.bas`; Explorer con form/codice/generato.
3. **G2** — language mode GMBasic e highlighting; handler `frmMain_cmdOK_Click` generato.
4. **G3** — Check con errori cliccabili, Run C99Native, Output, Stop.
5. **G4** — designer su `.frm`: Label/TextBox/Button, modifica caption/posizione, `.ui.bas`
   deterministico, stub handler.
6. **G7** — fixture `HelloForm` + test headless; snapshot manuale.

**Demo:** aprire `samples/GMBasic/HelloForm`, cambiare la caption e spostare il bottone nel
designer, scrivere `frmMain_cmdOK_Click`, premere Check (nessun errore), Run (finestra SDL con
il bottone nella posizione attesa), Stop; il `.frm` resta apribile da VB6 come layout e il
secondo salvataggio è identico.

---

## 11. Cose da NON fare

- Non usare IL come default o fallback: il target è C99Native; se manca una feature, si dichiara.
- Non compilare/linkare GMBasic dentro GMBasicIDE: il confine è il processo.
- Non implementare la semantica GMBasic in `vb_code.c`: l'interprete VB6 resta per i progetti
  VB6 e non deve "quasi funzionare" con GMBasic.
- Non tradurre codice VB6 in GMBasic: il `.frm` è compatibile come layout, non come sorgente.
- Non mettere codice GMBasic nel `.frm` per default: il file deve restare un `.frm` VB6 valido.
- Non usare controlli TUI (`LABEL`, `COMBOBOX`, `LISTBOX`) in una form pixel: non si vedono.
- Non mantenere a mano 1285 keyword: generarle dal lexer C99Native e testare la sincronia.
- Non inventare file/riga: se `cc` non ha la mappa, mostrare il messaggio grezzo.
- Non sovrascrivere mai `Main.bas`, `frmX.bas` o un `.ui.bas` modificato a mano senza conferma.
- Non promettere un preview VB6 del runtime GMBasic: la finestra è SDL e il chrome è diverso.
- Non introdurre dipendenze (package manager, framework) per questa integrazione: CLI + C99 +
  Cocoa, come il resto del progetto.

---

## 12. Verifiche già fatte (per non rifarle)

### GMBasicIDE

- `make check` → **0**, tutte le sezioni `PASS` (ultima esecuzione prima di questo piano).
- `vb_form_parse/serialize` sul sample `HelloWorld` è LF, non CRLF; il serializer scrive LF.
- `serialize_control` tratta solo `Font.` come `BeginProperty`; altri blocchi `BeginProperty`
  vengono piatti in proprietà.
- Nessun `NSTask`/spawn nel repo; moduli editabili/salvati via `_moduleCodes`/`saveModules`;
  nessun pannello output/diagnostica.

### GMBasic / C99Native (2026-10-22)

- `tools/gmb_c99native_run.sh /tmp/gmbprobe/ui3.bas c99ui3s` con `DIM txtNome AS STRING`:
  **rc=0**, log `[gmb_gfx] window 320x200 (logical)`, `renderer=software`.
- Stesso form senza `INTO`: `CONTROL 'TEXTBOX' non supportato dall'emitter nativo`.
- `TEXTBOX` con `DIM txtNome` numerico: compila ma passa `double*` a `gmb_form_add_textbox`
  (atteso `gmb_string_t*`); il codegen deve usare `STRING`.
- `LABEL 10,130,120,"Etichetta" INTO lab`: emette `gmb_label_init` (TUI); nessun
  `gmb_form_add_label` nel runtime. In pixel non compare.
- `CHECKBOX` con il driver fresco: `gmb_control_add_checkbox(...)`; la riga successiva applica
  le proprietà a `gmb_tui_last_handle()`, che in grafica non è il checkbox. `NAME`/`TABINDEX`
  non sono garantiti.
- `PIXCOMBOBOX`/`PIXLISTBOX`: `parse error: statement non riconosciuto`, rc=3; i rami
  dell'emitter non sono raggiungibili.
- `ON WINDOW CLOSE GOSUB OnClose`: emette `gmb_window_register_event("CLOSE","OnClose")`;
  senza `GOSUB` emette `gmb_window_unregister_event("CLOSE")`.
- `TIMER` in espressione: emette l'identificatore `TIMER` (C non definito); `TIMER()` dà
  `simbolo non trovato: funzione o subroutine 'TIMER' non definita`, rc=4; `TI` resta `TI`.
- O8 (2026-10-24): ogni statement emette `#line <riga bas> "<file bas>"` prima del suo C
  (misurato: `#line 4 "/…/Ui.bas"` sopra la `gmb_print_string` del modulo). `--line-map`
  scrive gli anchor: `{"c_line": 80, "line": 4, "file": "…/Ui.bas"}`. Errore di emissione con
  `TIMER()` in un modulo → `emissione: …/Ui.bas:5: simbolo non trovato…`, rc=4 (prima: `riga 13`
  del testo unito). `native/gmb_c99native/tests/c99_line_map_test.sh`: 8/8 PASS.
- O8, regressione: lo sweep su tutti i MiniTests dava gli **stessi** 58 CC-FAIL e 51
  DRIVER-FAIL con e senza `#line` (stessi file), quindi le direttive non cambiano la
  compilabilità; gli errori `cc` ora citano il `.bas` (es. `toggle_parity.bas:5:55: error:`).
- O7, risoluzione (2026-10-24): `sample samples/modules/FolderResolution/AllFolders.bas`
  compila e gira su C99Native (8 moduli da 8 cartelle diverse, transito `DEMO_CHAIN → DEMO_DEEP`),
  e l'Orchestrator C# carica gli stessi 8 moduli. `c99_module_resolve_test.sh`: 15/15 PASS.
  `app.settings.json` era ignorato dal C# (`LoadSettings` riceveva il file della entry invece
  della cartella): corretto in `SettingsManager.LoadSettings`.
- O7, CLI (2026-10-24): `build/gmb` (target `gmb_cli`) su `samples/modules/FolderResolution`:
  `gmb check` → rc=0, **7 sorgenti del progetto, 8 moduli, 0 avvisi**; `gmb run` → le 8 righe
  del sample, rc=0; `gmb check` su un progetto con `IMPORT FANTASMA` → **rc=2** con
  `Main.bas:1: IMPORT FANTASMA non risolto`; `--strict` promuove `no-namespace` a errore; due
  `SUB Main` → errore con invito a `--entry`. `gmb_cli_project_test.sh`: **14/14 PASS**.
  Build in tre passi: driver `--no-exe`, `cc -c` con la lista `-I` del runtime/LLMEnv, link
  `libgmb_runtime.a` + `-lm -lpthread` (console).
- O7, regressione: lo sweep su tutti i MiniTests scende da 58 CC-FAIL / 51 DRIVER-FAIL a
  **53 / 47** (nessuna nuova classe di errore, zero `già definita`): la ricorsione e `modules/`
  recuperano 5 casi C e 4 di emissione. Il set è un soprainsieme del precedente (stessi
  `IMPORT` + transito + più candidati), quindi un errore nuovo sarebbe comparso come aumento.
- Verifica della suite, punto UNICO (2026-10-24, dopo O7): `gmb minitests` sostituisce lo
  sweep ad hoc di `tools/c99_compile_errors.sh`. Scoperta ricorsiva e ORDINATA (`readdir` non
  lo è: senza `qsort` due run davano diff inutili), stessa ricetta `-I` di `gmb build`
  (`build_inc()`, unica copia), esclusione dei `' TAG: interactive`/`heavy` come prima
  (`--all`/`--interactive`/`--heavy` per includerli), `--filter`, `--driver`/`--cc` per
  l'A/B, istogramma dei CC-FAIL per categoria (apici → `'*'`, parentesi via, taglio a parola:
  altrimenti lo stesso errore finisce in più categorie).
  **Numero misurato oggi: 1948 file, 1914 eseguiti (7 interactive + 27 heavy esclusi), 1831
  PASS, 36 CC-FAIL, 47 DRIVER-FAIL, 0 crash** (`native/gmb_compiler_c/build/gmb minitests`,
  ~2m30s). Il set coincideva **file per file** con quello del vecchio script (diff vuoto su CC e
  DRIVER), quindi il cambio di strumento non ha cambiato il verdetto: i 16 CC-FAIL in meno
  rispetto a `1a4715122` vengono dai fix atterrati nel frattempo, non dal gate. Gli ultimi due
  numeri (1831/36) sono dopo il fix di `LIBRARY`.
- Exit code del gate: 0 tutti PASS · 2 almeno un FAIL · 1 setup rotto (driver/radice/cc). Con
  un pavimento di 83 file un "verde se 0 FAIL" sarebbe rosso per sempre, quindi `--baseline
  <json>` confronta con un run registrato: rosso solo per le **regressioni** (`REGRESSIONE
  <file>`), e `RISOLTO <file>` per i file che tornano a passare. `--json` è stabile fra run
  (stesso ordine, stessi numeri): è il diff da mettere in CI.
- Gate del gate: `native/gmb_compiler_c/tests/gmb_cli_minitests_test.sh`, **31/31 PASS** su una
  radice sintetica (ricorsione, TAG, `--cc /usr/bin/false` → CC-FAIL, driver ucciso da
  SIGSEGV → `crash` e non CC-FAIL, `--driver`/radice/filtro inesistenti → rc 1, `--baseline`
  identico/riparato/regressione/case diverso, JSON deterministico). `gmb_cli_project_test.sh`
  resta 14/14.
- Baseline registrato (2026-10-24): `native/gmb_compiler_c/tests/minitests_baseline.json`
  (35 KB, 83 FAIL noti) + `tools/check_minitests.sh`. Due scelte non ovvie:
  (a) `--update` considera **rc=2 un successo** — un baseline con FAIL è il caso normale, solo
  rc=1 (driver/radice) o un JSON vuoto lasciano il file com'era, e la scrittura è atomica
  (tmp + `mv`) perché un'interruzione non deve troncare il riferimento del gate;
  (b) il confronto è **case-insensitive**: su macOS la cartella si chiama `minitests`, su un
  checkout Linux `MiniTests`, e senza normalizzazione tutti i file noti sarebbero "nuovi".
  Da solo il pavimento non rende il gate inutilizzabile: si è rossi solo per le regressioni.
- Baseline ripetibile (2026-10-24): gli errori `cc` che puntano al **C generato** citavano
  `/tmp/gmb-minitests.<random>/out.c`, quindi ogni `--update` produceva un diff spurio di una
  riga e il riferimento registrato puntava a un file inesistente. Nel JSON (e
  nell'output leggibile) il path diventa `<out.c>`: la rigenerazione ora è idempotente
  (verificato: `--update` → `git status` pulito per il baseline).
- Bug del gate trovato dal primo run senza argomenti: `"${args[@]}"` con `set -u` e array
  vuoto fa abortire **bash 3.2** (macOS) con `args[@]: unbound variable` — cioè falliva
  proprio l'invocazione nuda della CI, non quelle con `--json`/`--filter` provate prima.
  Corretto con `${args[@]+"${args[@]}"}` e verificato con un run completo (rc=0, 37 CC-FAIL,
  0 nuovi FAIL).
- Caso isolato emerso dal gate, risolto: `constructs/LibraryDirective.bas` non falliva per i
  tipi ma perché la modalità `LIBRARY` emetteva `#include "gmb_host.h"` — header **assente dal
  repo**, copiato dal transpiler `basic2c` di GMTrading (dove `gbmtraderuntime` lo spedisce).
  Vedi la voce dedicata più sotto: emettitore e runner corretti, test PASS sul backend C
  nativo.
- Nota (non O8): `MiniTests/tui/widgets_all_parity.bas`, `statusbar_parity.bas` e
  `MiniTests/controls/keyword_controls.bas` facevano crashare il driver (SIGSEGV/SIGBUS) anche
  con le direttive disattivate: era un difetto del sorgente di allora, non della mappa.
  **Riverificato 2026-10-24: 0 crash sui 1914 file eseguiti** (`driver_crash: 0`).
- Multi-file (riverificato il 2026-10-24): `Main.bas` con `IMPORT UI` che chiama `frmShow`
  definita in un `Ui.bas` con `NAMESPACE UI` **compila**; il C emesso contiene
  `gmb_user_frmShow()` con il corpo. Senza `IMPORT` → `simbolo non trovato: funzione o
  subroutine 'frmShow' non definita`, rc=4. Con `Ui.bas` in `forms/` → stesso errore: la
  ricerca non è ricorsiva. Una chiamata come statement (`frmShow` senza parentesi) senza
  `IMPORT` non dà errore: diventa una variabile implicita (da chiudere in O7).
- `LIBRARY` non compilava, in nessun caso (2026-10-24, risolto). Due cause distinte, entrambe
  invisibili a un gate che guardi solo il verdetto finale:
  (a) **header fantasma**: `gmb_c99_emit.c` emetteva `#include "gmb_host.h"` + quattro
  entry-point host (`<lib>_set_host/_configure/_flush/_query`) che chiamano `gmb_host_set`,
  `gmb_host_set_token`, `gmb_host_serve`, `gmb_host_query` e leggono `gmb_config_t.transport`.
  Nessuno di quei simboli esiste in GMBasic: sono del **contratto host del trading**, spedito da
  `gbmtraderuntime` (GMTrading) e copiato qui dal transpiler `basic2c` che ne è la sorgente
  comune. In GMBasic produceva `fatal error: 'gmb_host.h' file not found` su OGNI file
  `LIBRARY`; l'include era in testa, quindi nemmeno i verdi tipi della libreria venivano a
  galla. Rimosso: l'ABI v1 documentata (`Guide/utente/LIBRARY-gmblib-Guide.md` §2) è
  `<lib>_init`/`<lib>_fini` + ctor/dtor + `<lib>_<Proc>` per i `PUBLIC`, esattamente ciò che
  emette il C# (`C99CodeEmitter.EmitLibraryProgram`), che infatti non ha mai avuto l'host
  glue. Gli hook `ONINIT`/`ONBAR`/`ONTRADE`/`ONDONE` restano (inoltrano solo a `gmb_user_*`,
  quindi non creano dipendenze) e compaiono solo se il sorgente li definisce.
  (b) **runner senza gamba libreria**: `tools/test_minitests_c99native` linkava sempre un
  eseguibile, e una `LIBRARY` per contratto **non ha `main`** → dopo il fix (a) falliva con
  `undefined _main` su un file corretto. Ora `' TAG: library` linka `-shared -fPIC` e verifica
  l'ABI con `nm -gU`: `_<lib>_init`, `_<lib>_fini` e un `_<lib>_<Proc>` per ogni `PUBLIC
  SUB|FUNCTION` del sorgente (nomi letti dal file, non un elenco hardcoded), più l'assenza di
  `_main`. Il verdetto negativo è reale: un probe con un `PUBLIC SUB` ad argomento array
  (legittimamente non esportabile in v1) dà `ABI: simboli non esportati: _probe_Run`.
  Verificato end-to-end: MiniTest **PASS** (`minilib_Add/Mul/Greet`), sample
  `samples/Community/C99/GmbLibMath.bas` → `gmbmath.dylib` con `gmbmath_Add/Mul/Greet/init/fini`
  e nessun `main`, gate `tools/check_minitests.sh` → `RISOLTO`, baseline rigenerato a 83 FAIL
  noti, 0 regressioni, `constructs` 40/41 (resta il gap noto `types_operator`).
  (c) **`gmb build` compilava una libreria come eseguibile** (2026-10-25, risolto): il CLI di
  progetto non aveva una gamba `LIBRARY` — emetteva il C, poi linkava un eseguibile e moriva
  con `_main` non definito su un C corretto. Ora la modalità si legge dall'**artefatto**
  (assenza di `int main`, la stessa sonda di `tools/gmb_c99native_run.sh`), il link è
  `-shared -fPIC -Wl,-undefined,dynamic_lookup` verso `<progetto>/bin/<nome-direttiva>.dylib`,
  `pick_entry` accetta un progetto senza `SUB Main` quando un modulo dichiara `LIBRARY "..."`
  (è quella l'entry), `run` costruisce e non esegue, e `--json` porta `"library": true`.
  Verificato caricando l'artefatto con `dlopen` e chiamando l'ABI: `Add(2,3)=5`,
  `Mul(1.5,4)=6`, `Greet("GMB")="Ciao, GMB"`; 6 asserzioni nuove in
  `gmb_cli_project_test.sh`. Resta rosso, ma **preesistente e non di LIBRARY**, il link
  dell'eseguibile: la ricetta minima di `gmb build` (solo `libgmb_runtime.a`) non copre più
  le SDL che il runtime console trascina con sé (vedi `build → bin/Main eseguibile`).
  (d) **la ricetta di link dell'eseguibile era incompleta** (2026-10-26, risolto): il runtime
  console trascina il motore grafico (`gmb_tui_app` → SDL2/SDL2_ttf/SDL2_mixer, Vulkan via
  MoltenVK) più il chart e zlib, quindi anche un programma con un solo `PRINT` moriva su
  `_SDL_*`/`_vk*`/`gmb_chart_*`. Ora il link di `build` aggiunge `libgmb_chart.a` (statica
  accanto al runtime), le `-L`/`-Wl,-rpath` delle dir esistenti (la cache utente
  `~/.gmbasic/libs/<rid>`, `/usr/lib`) e le `-l` **solo se la libreria c'è davvero** (così su
  Linux, senza MoltenVK, la riga resta valida invece di fermarsi su una `-l` inesistente), più i
  framework Apple. Non si copia l'intero lib-set di `tools/gmb_c99native_run.sh` (db/agent/doc/
  AI): serve ai programmi che usano quei domini. L'`-rpath` non è cosmetico: senza, il link
  riesce e l'avvio muore con `Library not loaded: @rpath/libSDL2.dylib`. Verificato:
  `gmb_cli_project_test.sh` 20/20 PASS, incluso `build → bin/Main eseguibile` e
  `run → stdout del programma` (prima FAIL), `LibraryDirective` ancora PASS e `--out` invariato.
- `tools/gmb_c99native_driver` è del 2026-10-10; il build `native/gmb_compiler_c/build/…` è del
  2026-10-22 e `tools/gmb_c99native_run.sh` preferisce il più recente. Il driver vecchio emetteva
  `gmb_tui_add_checkbox` per il checkbox; quello fresco usa `gmb_control_add_checkbox`. `LABEL`
  resta TUI in entrambi.
- `canonical.registry.json`: 1567 procedure, 115 comandi, 284 costanti; lexer C: 1285 entry.

### Da verificare prima del codegen

- `WINDOW FRAME` e i token `STYLE` per font/border/colore sul path `SCREEN`.
- `WINDOWCLOSED()` e `ON WINDOW CLOSE` con la finestra SDL reale (non solo dummy).
- `PIX*`/`COMBOBOX`/`LISTBOX` dopo O3.
- Visibilità cross-file di `DIM` top-level con il progetto (O7); fallback
  `CONTROL GET/GETS`.
- `Form_Resize`/`Activate`/`Deactivate` con `ON WINDOW … GOSUB`.
- Access key `&` nelle caption e `MOUSE.X/Y` nei handler senza argomenti.
- DAP reale (`--debug-tcp`) prima di promettere debug.

---

## 13. Riferimenti rapidi

- Codice GMBasicIDE: `src/core/vb_parse.c` (`.frm`), `src/core/vb_model.h` (`VbForm`/`VbControl`),
  `src/platform/mac/VBEditorWindow.m` (shell), `src/platform/mac/VBCodeEditor.m` (editor),
  `src/tools/vbtool.c` (harness).
- GMBasic CLI/gate: `native/gmb_compiler_c/src/cli/gmb_cli.c` (`check/build/run/minitests`),
  `native/gmb_compiler_c/tests/gmb_cli_minitests_test.sh`,
  `native/gmb_c99native/src/gmb_c99_emit_controls.c` (controlli),
  `native/gmb_runtime_c/include/gmb_form.h` (API pixel),
  `native/gmb_runtime_c/src/gmb_form.c` (runtime),
  `native/gmb_compiler_c/frontend/include/gmb_fn_lookup.h` (nomi),
  `tools/gmb_c99native_run.sh` (link di riferimento).
- Vault: `Guide/utente/Finestre-Eventi-Guida-Utente.md` (`ON WINDOW`),
  `Guide/utente/Controlli-Pixel-Proprieta-Metodi-Guida-Utente.md` (controlli),
  `Todo/(open) - IDE-TUI-BASIC-e-standalone-e-VSCode-LSP.md` (IDE TUI/LSP).
