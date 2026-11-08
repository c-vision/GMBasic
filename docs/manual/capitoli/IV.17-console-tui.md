# IV.17 — Console e TUI

## Cosa ottieni da questo capitolo

Il controllo del terminale: dove stampa il cursore, di che colore, come si legge una cella dello
schermo, come si gestisce l'input e come si disegna una tabella navigabile. È la base di ogni
programma che non apre una finestra.

## 1. Lo schermo è una griglia

`CLS` pulisce e riporta il cursore a `(1,1)`; `LOCATE` lo sposta. Le coordinate sono
**1-based**, riga poi colonna:

```basic
CLS
LOCATE 5, 10
PRINT "Ciao, mondo colorato!"
```

`CLS` riempie con il colore di sfondo corrente, non con il nero: se hai impostato uno sfondo,
pulire non lo cancella.

## 2. Colori a 16 tinte

```basic
COLOR 14, 1     ' testo giallo su sfondo blu scuro
COLOR 12        ' solo testo rosso
COLOR , 7       ' solo sfondo grigio, testo invariato
COLOR           ' ripristina i default
```

La virgola iniziale in `COLOR , 7` salta il primo parametro: è la forma per cambiare solo lo
sfondo. Valori fuori da 0-15 sono un errore, non un colore approssimato.

## 3. Leggere lo schermo

```basic
PRINT "SCREENS="; SCREENS
PRINT "CELLSTR(1,1)='"; CELLSTR(1,1); "'"
```

`SCREENS` dice quale schermo è attivo, `CELLSTR(riga, colonna)` restituisce il carattere in una
cella. Sono le funzioni che servono per verificare un output senza leggere l'intero flusso: un
test può controllare *una* cella.

Eseguito (`shared/samples/Console/ConsoleBasics.bas`, backend C99Native):

```
Ciao, mondo colorato!
Questo testo è rosso su sfondo blu scuro.
Testo rosso su sfondo grigio.
Colori ripristinati.
SCREENS= 0
CELLSTR(1,1)=''
TEXTWIDTH('ABC')>0=-1
TEXTHEIGHT>0= 0
Test terminato.
```

Due letture da fare con attenzione: `SCREENS= 0` dice che non c'è una finestra grafica attiva
(siamo in modalità console), e `CELLSTR(1,1)=''` è vuota perché la cella in alto a sinistra è il
bordo del riquadro, non testo. `TEXTHEIGHT>0= 0` è un valore sospetto — sul backend nativo la
metrica verticale del font risulta zero — e non va usato come misura affidabile.

## 4. Input da tastiera

L'input da console si legge con le funzioni di tastiera (`INKEY`, `GETKEY`) e con `ON KEY` per i
tasti speciali; l'input riga per riga con `INPUT` e `LINE INPUT`. I sample
`Console/ConsoleKeyboardInput.bas` e `Console/ConsoleKeyboardSemantic.bas` coprono il caso
completo, `Console/ConsoleOnKeyClipboard.bas` quello degli handler.

Regola: `INPUT` è per l'utente che digita, non per un test — in un MiniTest l'input va
simulato o evitato, altrimenti il file diventa interattivo (vedi III.15).

## 5. Scorrimento e salvataggio

- `Console/ConsoleScrollMode.bas` — come si comporta l'output quando supera l'ultima riga;
- `Console/ConsoleSaveRestoreScreen.bas` — salvare lo schermo e riportarlo com'era;
- `Console/ConsoleBreakTrap.bas` — intercettare l'interruzione.

Il salvataggio dello schermo è ciò che rende possibile un'applicazione TUI che apre un pannello e
poi torna all'immagine precedente invece di ridisegnare tutto.

## 6. Il mondo Clipper: `SET` e le funzioni `CONSOLE`

```basic
SET WRAP ON
SET DELIMITERS TO ",;"
SET DECIMALS 3
PRINT "3.14159 -> "; 3.14159
SET FIXED ON
PRINT "3.14159 -> "; 3.14159
```

`SET` è lo stato **persistente della sessione**: `SET FIXED ON` cambia come si stampano tutti i
numeri, non solo quello successivo. Per ripristinare, `SET FIXED OFF`.

Le funzioni di lettura sono nel namespace `CONSOLE`:

| Funzione | Ritorna |
|---|---|
| `CONSOLE.GETCURSORPOS()` | `[col, riga]` |
| `CONSOLE.GETTEXTMETRICS()` | `[larghezza, altezza]` della finestra |
| `CONSOLE.GETPALETTECOLOR(i)` | `[r, g, b]` del colore `i` |

Il sample `Console/ConsoleSetAndGet.bas` le usa tutte e tre, più `WINDOW SET TITLE`. Attenzione al
tipo: `DIM pos(1) AS INTEGER` e poi `pos = CONSOLE.GETCURSORPOS()` — l'array riceve i valori,
non li contiene già.

## 7. Tabelle in terminale: TBROWSE

Per una lista navigabile (righe, colonne, ordinamento, filtro, editing) esiste `TBROWSE`, di
stirpe Clipper. Il dettaglio delle colonne, del sort e del filtro è nella reference
`TBrowse-Reference` e nella guida `TBROWSE-Sort-Filter-Guida-Utente`; qui basta la collocazione:
`TBROWSE` è il controllo TUI, non grafico, e convive con `SET`/`COLOR`.

## 8. Errori frequenti

| Sintomo | Causa | Correzione |
|---|---|---|
| il testo appare in posizione sbagliata | coordinate 0-based per abitudine | `LOCATE` è 1-based |
| `CLS` lascia bande colorate | sfondo impostato prima di pulire | `COLOR` prima di `CLS` |
| nessun colore nel terminale | terminale senza supporto a 16 colori | prova su un terminale VGA/xterm |
| `INPUT` blocca il programma | il sample è interattivo | marcalo `' TAG: interactive` |
| i numeri hanno decimali inattesi | `SET DECIMALS`/`SET FIXED` attivi da prima | ripristina a fine sezione |

## 9. Dove andare adesso

- **IV.18** — colori, stili e temi, dal terminale alla finestra.
- **IV.19** — quando la console non basta: finestre ed eventi.
- **II.7** — cicli e `DO WHILE` per un menu TUI.
- **VI.38 §7bis** — le opzioni del registro che regolano console e font (`GMB_CONSOLE`,
  `GMB_CONSOLE_SIZE`, `GMB_CONSOLE_FONT`, `GMB_SDLCONSOLE`, `GMB_FONT_PATH`) e le impostazioni
  `consoleConfiguration` che le sostituiscono.














<!-- BEGIN SURFACE (generato da tools/sh/manual_surface.sh — non modificare a mano) -->

## Riferimento completo: comandi, funzioni, parametri

> Sezione generata dalle guide del progetto per questo capitolo: contiene **tutte**
> le superfici documentate, non una selezione. La prosa dei capitoli precedenti le
> spiega; qui stanno i dettagli — parametri, varianti, esempi d'uso.

### Console-Architecture

*Fonte: `Console-Architecture`*


## Overview

## 4 Modalità Operative
| # | Avvio | Console | Classe | Buffer | Uso |
|---|-------|---------|--------|--------|-----|
| 1 | `-c -s` flags | SystemConsole | `SystemConsole` | Nessuno | Standard .NET `System.Console` |
| 2 | `-c` flag | StandardConsole | `StandardConsole` | Proprio | Standard .NET con buffer caratteri |
| 3 | Nessun flag / `SCREEN 0` | Simulata SDL2 | `SdlConsole` | Proprio | REPL simulata, SCREEN=0 |
| 4 | `SCREEN != 0` | Mista | `GraphicsManager` + `SdlConsole` | Separato | Schermo grafico + REPL nascosta/visibile |

## Comportamento dei Comandi

### Console Standard (`-c` / `-c -s`)

### Console Simulata (nessun flag / SCREEN 0)

### Modalità Grafica (SCREEN != 0)

## Bug Noti (runtime legacy C#, D1)

## Classi Chiave

## Riferimenti

### Clipper-TUI-Manual

*Fonte: `Clipper-TUI-Manual`*


## 1. Architettura

                   ┌──────────────────────────┐
                   │     EventDispatcher       │
                   │  (ON KEY / ON WINDOW /    │
                   │   ON MOUSE / CTRL+C)      │
                   └───────────┬──────────────┘
                               │
         ┌─────────────────────┼─────────────────────┐
         │                     │                     │
    ┌────▼────┐          ┌────▼────┐          ┌─────▼─────┐
    │ MODALE  │          │@...SAY  │          │STANDALONE │
    │ READ    │          │@...GET  │          │CONTROLS   │
    │ GET     │          │BOX      │          │           │
    │ FORM    │          │SCROLL   │          │ALERT      │
    │         │          │CLEAR    │          │CONFIRM    │
    └─────────┘          └─────────┘          │CHECKBOX   │
                                              │LISTBOX    │


## 2. @...SAY / @...GET / BOX / SCROLL / CLEAR

### 2.1 @...SAY

@ row, col SAY expression
@ row, col SAY expression PICTURE "999.99"


### 2.2 @...GET

@ row, col GET variable
@ row, col GET variable PICTURE "@!"
@ row, col GET variable VALID condition
@ row, col GET variable RANGE min, max
@ row, col GET variable COLOR "W/B"
@ row, col GET variable TYPE "N" MIN 0 MAX 100 STEP 1


#### TYPE — Form Controls inline
| Alias | Nome completo | FormControl | Ritorno | Parametri |
|-------|--------------|-------------|---------|-----------|
| `"N"` | `"spinner"` `"numeric"` | FormNumeric | DOUBLE | MIN, MAX, STEP |
| `"C"` | `"combobox"` | FormComboBox | STRING | ITEMS |
| `"L"` | `"listbox"` | FormListBox | STRING | ITEMS, HEIGHT |
| `"K"` | `"checkbox"` | FormCheckBox | BOOLEAN | CAPTION |
| `"R"` | `"radio"` | FormRadioButton | BOOLEAN | CAPTION |
| `"D"` | `"datepicker"` | FormDatePicker | STRING "YYYY-MM-DD" | CAPTION |
| `"P"` | `"progressbar"` | FormProgressBar | INTEGER | MIN, MAX |
| `"M"` | `"memo"` | FormMemo | STRING | HEIGHT |
| *(default)* | — | FormInput | STRING | — |

### 2.3 BOX

@ top, left, bottom, right BOX              ' single border
@ top, left, bottom, right BOX SINGLE       ' esplicito
@ top, left, bottom, right BOX DOUBLE       ' doppio bordo


### 2.4 SCROLL

SCROLL top, left, bottom, right, n    ' n > 0 scroll up, n < 0 scroll down


### 2.5 CLEAR

CLEAR                       ' tutto lo schermo
CLEAR top, left, bot, right ' regione


## 3. Standalone Controls

### 3.1 ALERT / CONFIRM

ALERT "Operazione completata"                      ' popup OK
CONFIRM "Vuoi uscire?" TO risposta                 ' → BOOLEAN


### 3.2 CHECKBOX

CHECKBOX variable, row, col, "caption"             ' → BOOLEAN


### 3.3 LISTBOX / COMBOBOX

LISTBOX var, row, col, width, height, items()      ' → STRING (label)
COMBOBOX var, row, col, width, items()             ' → STRING (label)


### 3.4 BUTTON

BUTTON row, col, "caption" CALL SubName            ' SUB eseguita solo su click
BUTTON row, col, "caption" CALL SubName(arg)       ' con argomenti


### 3.5 MENU

' Flat — ritorna INTEGER (indice)
MENU var, row, col, ["Alpha", "Beta", "Gamma"]

' Nested con separatori — ritorna STRING (label)
MENU var, row, col, [["File", "Open", "Save", "---", "Exit"], _
                     ["Edit", "Cut", "Copy", "Paste"], _
                     "Help"]


### 3.6 STATUSBAR

STATUSBAR " Ready | F1=Help | F10=Quit"


### 3.7 FILEDIALOG

FILEDIALOG var, "*.bas", "Select a file"           ' → STRING (path) o ""


### 3.8 PRINTERDIALOG

PRINTERDIALOG printerVar, copiesVar, duplexVar [, "Titolo"]
' printerVar → STRING (nome stampante o "")
' copiesVar  → INTEGER (1-999)
' duplexVar  → BOOLEAN


### 3.9 TOGGLE

TOGGLE var, row, col, "caption"                    ' → BOOLEAN


### 3.10 RADIO

RADIO var, row, col, items()                  ' → STRING (label)


### 3.11 SLIDER

SLIDER var, row, col, length, min, max, step       ' → INTEGER


### 3.12 TAB

TAB var, row, col, width, height, tabs()           ' → INTEGER (indice 0-based)


### 3.13 GRID

GRID var, row, col, width, height, headers(), cells()
' → INTEGER (indice cella = row * cols + col, -1 su cancel)


### 3.14 DATEPICKER

DATEPICKER var, row, col                           ' → STRING "YYYY-MM-DD"


### 3.15 COLORPICKER

COLORPICKER var, row, col, columns                 ' → INTEGER (0-255)


### 3.16 TREEVIEW

TREEVIEW var, row, col, width, height, tree()      ' → STRING (label)


["Root +",                  ' "+" = ha figli
 "  Child 1",               ' 2 spazi = depth 1
 "  Child 2 +",
 "    Grandchild",          ' 4 spazi = depth 2
 "Sibling"]                 ' nessuno spazio = depth 0


### 3.17 SPINNER

SPINNER row, col, "preset"                         ' display only


### 3.18 SET commands

SET BELL ON/OFF          ' beep su errori (default ON)
SET CONFIRM ON/OFF       ' conferma prima di sovrascrivere file
SET CURSOR ON/OFF        ' visibilità cursore
SET THEME TO "Midnight"  ' tema (Midnight, Nord, Dracula, Turbo)
SET BOLD ON/OFF          ' testo in grassetto
SET INVERSE ON/OFF       ' colori invertiti
SET UNDERLINE ON/OFF     ' testo sottolineato
SET BLINK ON/OFF         ' testo lampeggiante


### 3.19 PICTURE & TRANSFORM

' @...SAY con template
@ 5, 10 SAY 1234.56 PICTURE "999,999.99"     ' → " 1,234.56"
@ 5, 10 SAY "hello" PICTURE "@!"              ' → "HELLO"
@ 5, 10 SAY 15081990 PICTURE "@R 99/99/9999" ' → "15/08/1990"

' TRANSFORM() — applica PICTURE come funzione
DIM x = TRANSFORM(1234.56, "***,***.99")      ' → "**1,234.56"
DIM y = TRANSFORM(-100, "@(")                 ' → "(100)"


### 3.20 TrueColor & COLOR

' ANSI 0-15 (backward compat)
COLOR 14, 1                    ' giallo su blu

' TrueColor hex
COLOR "#FF8000", "#000040"     ' arancio su blu scuro

' RGB array
COLOR [255, 128, 0], [0, 0, 64]

' Funzioni inline
PRINT FGRGB("testo rosso", 255, 0, 0)
PRINT BGRGB("sfondo giallo", 255, 255, 0)


### 3.21 SET THEME & Stili

SET THEME TO "Midnight"   ' blu scuro, testo bianco, accenti azzurri
SET THEME TO "Nord"       ' grigio-blu, testo chiaro
SET THEME TO "Dracula"    ' viola scuro, accenti rosa
SET THEME TO "Turbo"      ' blu, testo giallo, accenti ciano


## 4. TBROWSE — Griglia Dati Clipper 5.3

### 4.1 Sintassi

TBROWSE var, row, col, width, height, columns(), data() [, options()]
' → INTEGER: indice riga selezionata (0-based), -1 = cancel


### 4.2 Colonne

columns: [[header, width, type, align, editable, visible, choices...], ...]

| Parametro | Default | Valori |
|-----------|---------|--------|
| header | *(obbligatorio)* | STRING |
| width | *(obbligatorio)* | INTEGER |
| type | `"C"` | `C`=text, `N`=int, `F`=float, `L`=bool, `D`=date, `X`=choice |
| align | `"L"` | `L`=left, `C`=center, `R`=right |
| editable | FALSE | TRUE/FALSE |
| visible | TRUE | TRUE/FALSE |
| choices | — | array STRING (per type `X`) |

### 4.3 Dati — 3 modalità

TBROWSE sel, 3, 2, 70, 15, cols(), [["Alice","30"],["Bob","25"]]


ISAM.USE("clienti.isdb") AS Clienti
TBROWSE sel, 3, 2, 70, 15, , 1          ' area 1
TBROWSE sel, 3, 2, 70, 15, , "Clienti"  ' alias


### 4.4 Options

options: [[key, value], [key, value], ...]

| Key | Tipo | Default | Descrizione |
|-----|------|---------|-------------|
| `freeze` | INTEGER | 0 | Colonne bloccate a sinistra |
| `footer` | STRING | — | Testo footer |
| `colors` / `colorspec` | STRING | — | `"W/B,N/G,B+/W"` (header, row, highlight) |
| `border` / `showborder` | BOOLEAN | TRUE | Bordo `┌─┐│└─┘` |
| `header` / `showheader` | BOOLEAN | TRUE | Header colonne |
| `highlight` / `highlightrow` | BOOLEAN | TRUE | Evidenzia riga corrente |
| `mouse` / `mouseenabled` | BOOLEAN | TRUE | Click mouse |
| `autolite` | BOOLEAN | TRUE | Auto-refresh rendering |
| `colsep` | STRING | `"│"` | Separatore colonne |
| `headsep` | STRING | `"─"` | Separatore header |
| `footsep` | STRING | `"─"` | Separatore footer |
| `lock` | ARRAY | — | `[[row, col], ...]` — celle bloccate (non editabili) |
| `bookmark` | INTEGER | 0 | Riga iniziale all'apertura |
| `validate` | CLOSURE | — | `FN(row, col) => condition` — validatore pre-edit |
| `onkey` | ARRAY | — | `[keyCode, closure]` — handler tasto custom |

### 4.5 Tasti
| Tasto | Azione |
|-------|--------|
| ↑ ↓ | Riga su/giù |
| ← → | Colonna sinistra/destra |
| PgUp / PgDn | Pagina su/giù |
| Home / End | Prima/ultima riga |
| Enter | Seleziona riga (se cella non editable) / Edita cella |
| Esc | Annulla (ritorna -1) |
| **F5** | **Sort per colonna corrente (toggle asc/desc)** |
| **/** o **Ctrl+F** | **Search incrementale (filtro su tutte le colonne)** |

### 4.6 ColorSpec (formato Clipper)

"headerFg/headerBg, rowFg/rowBg, highlightFg/highlightBg"


### 4.7 Callback

' Validator: controlla prima di accettare l'edit
DIM v = FN(row, col) => row >= 0 AND col >= 0

' Key handler: F2 custom
DIM h = FN() => ALERT "F2 pressed!"

TBROWSE sel, 3, 2, 70, 15, cols(), data(), _
    [["validate", v], ["onkey", [60, h]]]


## 5. WINDOW Management

' Definizione
WINDOW.DEFINE("name", top, left, bottom, right)
WINDOW.DEFINE("name", top, left, bottom, right, "Title")
WINDOW.DEFINE("name", top, left, bottom, right, "Title", TRUE)  ' double border

' Operazioni
WINDOW.ACTIVATE("name")      ' port in primo piano
WINDOW.DEACTIVATE("name")    ' minimizza
WINDOW.CLOSE("name")         ' chiudi
WINDOW.MINIMIZE("name")      ' minimizza a icona
WINDOW.RESTORE("name")       ' ripristina
WINDOW.RELEASE("name")       ' chiudi e distruggi


## 6. Event System

### 6.1 ON KEY

ON KEY(KEY.F1) GOSUB HelpHandler
ON KEY(KEY.CTRL_C) GOSUB ExitHandler
ON KEY(KEY.MAKE(KEY.ALT, KEY.VK_X)) CALL CustomHandler
ON KEY STOP                  ' rimuovi tutti


### 6.2 ON WINDOW (9 eventi)

ON WINDOW OPEN GOSUB Handler      ' finestra creata
ON WINDOW CLOSE GOSUB Handler     ' chiusa
ON WINDOW RESIZE GOSUB Handler    ' ridimensionata
ON WINDOW MOVE GOSUB Handler      ' spostata
ON WINDOW FOCUS GOSUB Handler     ' ricevuto focus
ON WINDOW MINIMIZE GOSUB Handler  ' minimizzata
ON WINDOW RESTORE GOSUB Handler   ' ripristinata
ON WINDOW QUIT GOSUB Handler      ' cmd+Q / uscita
ON WINDOW PASTE GOSUB Handler     ' Ctrl+V / incolla


### 6.3 ON MOUSE (6 eventi)

ON MOUSE CLICK GOSUB Handler
ON MOUSE DBLCLICK GOSUB Handler
ON MOUSE MOVE CALL Handler
ON MOUSE DOWN GOSUB Handler
ON MOUSE UP GOSUB Handler
ON MOUSE SCROLL CALL Handler


### 6.4 Class Events (RAISE EVENT / ADD HANDLER)

CLASS Counter
    EVENT Changed(value AS INTEGER)
    
    SUB Increment()
        SELF._value = SELF._value + 1
        RAISE EVENT Changed(SELF._value)
    END SUB
END CLASS

ADD HANDLER obj.Changed, MyHandler
REMOVE HANDLER obj.Changed, MyHandler


## 7. Funzionalità GMBasic (unificato)
| Feature | GMBasic unificato |
|---------|------------------|
| Rendering | SDL2 + TUI (CommonGraphics — community) |
| WINDOW (SDL events) | ✅ |
| ON WINDOW da SDL | ✅ |
| ON MOUSE da SDL | ✅ |
| ON MOUSE da SGR | ✅ |
| Tutti i controlli standalone | ✅ |
| @...GET TYPE | ✅ |
| TBROWSE + RDD | ✅ |
| Eventi di classe | ✅ |
| ON KEY | ✅ |

## 8. Sample Files
| Sample | Contenuto |
|--------|-----------|
| `ClipperWindow.bas` | WINDOW.DEFINE/ACTIVATE/CLOSE |
| `ClipperEvents.bas` | ON WINDOW + ON MOUSE + ON KEY |
| `ClipperFullDemo.bas` | @...SAY/GET + BOX full demo |
| `ClipperDialogs.bas` | ALERT + CONFIRM |
| `ClipperControls.bas` | @...GET TYPE controls |
| `ClipperControlsStandalone.bas` | CHECKBOX/LISTBOX/COMBOBOX standalone |
| `ClipperButton.bas` | BUTTON (click-only action) |
| `ClipperMenu.bas` | MENU standalone flat |
| `ClipperMenuNested.bas` | MENU con submenus e separatori |
| `ClipperFullSuite.bas` | Demo completa: 12 controlli in sequenza |
| `ClipperAdvanced.bas` | @...GET inline, GRID, TREEVIEW, BUTTON, FILEDIALOG, TBROWSE |
| `ClipperTBrowse.bas` | TBROWSE con sort, search, lock, bookmark, stili |
| `ClipperPCodeTest.bas` | Test VM/C (P-Code): 20 controlli TUI |
| `ClassesEvents.bas` | RAISE EVENT / ADD HANDLER / REMOVE HANDLER |

## 9. Gates
| Gate | GMBasic (unificato) |
|------|---------------------|
| AST compile | **352/352** |
| xUnit AI | 76/76 |
| xUnit SQLite | 15/15 |
| xUnit VectorDB | 11/11 |

## 10. Statement `WINDOW` (finestre TUI) — riferimento completo
| Forma | Effetto |
|---|---|
| `WINDOW DEFINE <nome> FROM r,c TO r,c [TITLE "t"] [DOUBLE\|SINGLE\|ROUNDED] [SHADOW] [PIXEL] [DESKTOP n]` | crea la finestra (rettangolo + titolo + stile) |
| `WINDOW THEME <n>` | tema globale finestre (default `turbovision`; `clipper` = legacy DOS) |
| `WINDOW DESKTOP <n>` | desktop virtuale globale |
| `WINDOW FRAME DOUBLE\|SINGLE\|ROUNDED` | cornice globale |
| `WINDOW SHADOW ON\|OFF` | ombreggiatura globale |
| `WINDOW SELECT <nome>` | attiva la finestra |
| `WINDOW SET TITLE "t"` | titolo della finestra attiva |
| `WINDOW <nome> PRINT <expr>` | aggiunge una riga al contenuto |
| `WINDOW <nome> TEXT <expr>` | imposta il contenuto |
| `WINDOW <nome> CLEAR` | svuota il contenuto |
| `WINDOW <nome> SCROLL <n>` | scroll del contenuto |
| `WINDOW <nome> EDIT ON\|OFF` | modalità edit |
| `WINDOW <nome> DESKTOP <n>` | sposta la finestra su un desktop |
| `WINDOW <nome> FRAME DOUBLE\|SINGLE\|ROUNDED` | cornice della finestra |
| `WINDOW <nome> SHADOW ON\|OFF` | ombra della finestra |
| `WINDOW <nome> ACTIVATE\|DEACTIVATE\|RELEASE\|CLOSE\|MINIMIZE\|RESTORE` | azione sulla finestra |

## 11. Statement `MOUSE` — riferimento completo
| Forma | Effetto |
|---|---|
| `MOUSE ON\|OFF` | abilita/disabilita il driver |
| `MOUSE SHOW\|HIDE` | visibilità cursore |
| `MOUSE SET x, y` | posizione (clampata se `LIMIT` attivo) |
| `MOUSE LIMIT x1, y1, x2, y2` | limita il movimento al rettangolo |
| `MOUSE UNLIMIT` | rimuove i limiti |
| `MOUSE CURSOR id` | tipo cursore (0=arrow, 1=ibeam, 2=wait, 3=cross, …) |
| `MOUSE WAIT` | punto di sync (headless: ritorna subito) |

## 12. Widget/controlli TUI — arità canoniche
| Comando | Forma | INTO |
|---|---|---|
| `STATUSBAR "testo"` | — | nessuno |
| `CHECKBOX r, c, "etichetta"` | INTO n | -1/0 |
| `TOGGLE r, c, "etichetta"` | INTO n | -1/0 |
| `RADIO r, c, [opzioni]` | INTO s | etichetta |
| `SLIDER r, c, len, min, max, step` | INTO n | valore |
| `TAB r, c, w, h, [tabs]` | INTO n | indice |
| `LISTBOX r, c, w, h, [voci]` | INTO s | voce |
| `COMBOBOX r, c, w, [voci]` | INTO s | voce |
| `MENU r, c, [voci]` (anche annidato `[[..],[..]]`) | INTO s | voce |
| `DATEPICKER r, c` | INTO s | data |
| `COLORPICKER r, c, colonne` | INTO n | indice |
| `GRID r, c, w, h, [headers], [celle]` | INTO n | indice |
| `TREEVIEW r, c, w, h, [nodi]` | INTO s | nodo |
| `TBROWSE r, c, w, h, [cols], [celle]` | INTO s | riga |
| `FILEDIALOG "mask", "titolo"` | INTO s | path |
| `SPINNER r, c, preset` | (INTO opz.) | indice |
| `PRINTERDIALOG` | INTO ps, cp, dx | stampante/copie/duplex |

### Console-Architecture-Details

*Fonte: `Console-Architecture-Details`*


## Panoramica

## Classi coinvolte

## Modo 1: SystemConsole (`-c -s`)
| Proprietà | Valore |
|-----------|--------|
| Flag | `-c -s` |
| SDL2 | Mai attivato |
| Classe | `SystemConsole` |
| Buffer | Nessuno — chiama direttamente `System.Console` |
| Destinazione testi | `System.Console` |

EntryPoint.Execute(args)
  → ArgsParser.Parse(args) → UseSystemConsole=true
  → new SystemConsole(config)
  → PRINT/LOCATE/CLS → System.Console metodi diretti


## Modo 2: StandardConsole (`-c`)
| Proprietà | Valore |
|-----------|--------|
| Flag | `-c` (senza `-s`) |
| SDL2 | Mai attivato |
| Classe | `StandardConsole` |
| Buffer | Proprio buffer di caratteri |
| Destinazione testi | `System.Console` (via buffer) |

EntryPoint.Execute(args)
  → ArgsParser.Parse(args) → UseSystemConsole=false
  → new StandardConsole(config, keyboard, clipboard)
  → PRINT/LOCATE/CLS → buffer → System.Console


## Modo 3: SdlConsole — REPL simulata (nessun flag, SCREEN 0)
| Proprietà | Valore |
|-----------|--------|
| Flag | Nessuno (o SCREEN 0 a runtime) |
| SDL2 | Attivato per la console simulata |
| Classe | `SdlConsole` |
| Buffer | Proprio buffer caratteri |
| Destinazione testi | Finestra SDL2 simulata |

EntryPoint.Execute(args)
  → ArgsParser.Parse(args) → UseSdl=true (default)
  → new SdlConsole(clipboard, config)
  → SDL_Init(VIDEO | TIMER | AUDIO | JOYSTICK)
  → Finestra REPL testuale con font bitmap (HackNF)


## Modo 4: GraphicsManager (SCREEN != 0)
| Proprietà | Valore |
|-----------|--------|
| Flag | Nessuno (comando `SCREEN <mode>` a runtime) |
| SDL2 | Attivato per REPL + finestra grafica |
| REPL | `SdlConsole` (rimane aperta, nascosta o visibile) |
| Grafica | `GraphicsManager` (finestra separata o stessa) |

Programma BASIC esegue SCREEN 13  (es. VGA 320x200)
  → GraphicsManager.Initialize(mode)
  → Nuova finestra/schermo grafico SDL2
  → REPL SdlConsole rimane in background


## Tabella riassuntiva
| Comando | Modo 1 (SystemConsole) | Modo 2 (StandardConsole) | Modo 3 (SdlConsole) | Modo 4 (Grafica) |
|---------|:---------------------:|:------------------------:|:-------------------:|:-----------------:|
| PRINT | System.Console | Buffer→Console | SdlConsole | Finestra attiva |
| CPRINT | System.Console | Buffer→Console | SdlConsole | **SdlConsole** |
| CLOCATE | System.Console | Buffer→Console | SdlConsole | **SdlConsole** |
| CCLS | System.Console | Buffer→Console | SdlConsole | **SdlConsole** |
| LOCATE | System.Console | Buffer→Console | SdlConsole | Finestra attiva |
| CLS | System.Console | Buffer→Console | SdlConsole | Finestra attiva |
| INPUT | System.Console | Buffer→Console | SdlConsole | Finestra attiva |
| WAITKEY | System.Console | Buffer→Console | SdlConsole | Finestra attiva |
| INKEY$ | System.Console | Buffer→Console | SdlConsole | Finestra attiva |
| DRAW/PSET/etc | ❌ Errore | ❌ Errore | ❌ Errore | GraphicsManager |
| SCREEN mode | ❌ Errore | ❌ Errore | ✅ Apre grafica | ✅ Cambia modo |
| SYNC/TICK | ❌ Errore | ❌ Errore | ❌ Errore | GraphicsManager |
| SPRITE/SPRITEDEF | ❌ Errore | ❌ Errore | ❌ Errore | GraphicsManager |
| TILEMAP | ❌ Errore | ❌ Errore | ❌ Errore | GraphicsManager |
| MOUSE | ❌ Errore | ❌ Errore | Eventi REPL | GraphicsManager |
| JOY/JOYCOUNT | ❌ Errore | ❌ Errore | ❌ Errore | ✅ Joystick SDL |

## Note implementative

## Relazioni

### PetsciiControlChars

*Fonte: `PetsciiControlChars`*


## Descrizione

## API

PETSCII$("text with {codes}") → string


## Codici supportati
| Categoria | Codici |
|-----------|--------|
| **Screen** | `{clr}` `{home}` `{up}` `{down}` `{left}` `{right}` `{del}` `{inst}` `{rvson}` `{rvsoff}` `{stop}` `{return}` |
| **Colors** | `{blk}` `{wht}` `{red}` `{cyn}` `{pur}` `{grn}` `{blu}` `{yel}` `{orng}` `{brn}` `{lblu}` `{lred}` `{gry1}` `{gry2}` `{lgrn}` `{lblu2}` |
| **Function keys** | `{f1}`..`{f8}` |
| **Numeric** | `{N up}` `{N down}` `{N left}` `{N right}` `{N del}` `{N inst}` (es. `{5 down}`) |
| **Altri** | `{bell}` `{tab}` `{esc}` `{shift}` `{unshift}` |

## Implementazione

## Sample

## Relazioni

### TBROWSE-Sort-Filter-Guida-Utente

*Fonte: `TBROWSE-Sort-Filter-Guida-Utente`*


## Proprietà
| Proprietà | Tipo | Significato |
|---|---|---|
| `sort` | numero | colonna di ordinamento (**0-based**); `-1` = nessun ordinamento |
| `sortdir` | numero | `0` = crescente, `1` = decrescente |
| `sortkeys` | stringa | **ordinamento multi-colonna**: `"col[:asc\|desc][,col[:asc\|desc]]..."` (fino a 4 chiavi) |
| `filter` | stringa | testo di filtro (case-insensitive); `""` = nessun filtro |
| `csv` | stringa (sola lettura) | export **CSV** della vista corrente (header + righe, quoting RFC4180) |

CONTROL SET "<nome>", "sort", <col>, ""
CONTROL SET "<nome>", "sortdir", <0|1>, ""
CONTROL GET "<nome>", "sort" INTO <num>
CONTROL GET "<nome>", "sortdir" INTO <num>
CONTROL SET "<nome>", "filter", 0, "<testo>"
CONTROL GETS "<nome>", "filter" INTO <str>
' multi-colonna + CSV (2026-10-09)
CONTROL SET "<nome>", "sortkeys", 0, "1:asc,2:desc"
CONTROL GETS "<nome>", "sortkeys" INTO <str>
CONTROL GETS "<nome>", "csv" INTO <str>


## Esempio

IMPORT CONTROLS
SCREEN 0

SUB Main()
  DIM hd(1) AS STRING
  hd(0) = "Simbolo" : hd(1) = "Prezzo"
  DIM dt(2, 1) AS STRING
  dt(0, 0) = "MSFT" : dt(0, 1) = "250"
  dt(1, 0) = "AAPL" : dt(1, 1) = "100"
  dt(2, 0) = "NVDA" : dt(2, 1) = "500"
  DIM sel AS INTEGER
  TBROWSE 2, 2, 40, 8, hd, dt INTO sel NAME "book"
  CONTROL SET "book", "sort", 1, ""      ' colonna Prezzo
  CONTROL SET "book", "sortdir", 0, ""   ' crescente
  CONTROL SET "book", "filter", 0, "A"   ' solo righe con "A"
  PRINT "book-sortfilter-ok"


## Disponibilità sui 4 backend
| Backend | Esito |
|---|---|
| IL | ✅ |
| PCode (VM) | ✅ |
| VM/C | ✅ |
| C99Native | ✅ |

## Note di implementazione (2026-10-08, agg. 2026-10-09)

## Riferimenti

### TBrowse-Reference

*Fonte: `TBrowse-Reference`*


## Sintassi completa

TBROWSE var, row, col, w, h, columns(), data() [, options()]


### Colonne

[[header, width, type, align, editable, visible, choices...], ...]

| Campo | Default | Valori |
|-------|---------|--------|
| header | obbligatorio | STRING |
| width | obbligatorio | INTEGER |
| type | `"C"` | `C`=text, `N`=int, `F`=float, `L`=bool, `D`=date, `X`=choice |
| align | `"L"` | `L`=left, `C`=center, `R`=right |
| editable | FALSE | TRUE/FALSE |
| visible | TRUE | TRUE/FALSE |
| choices | — | array di STRING (per type `X`) |

### Options (7° parametro opzionale)

[[key, value], [key, value], ...]

| Key | Tipo | Default | Descrizione |
|-----|------|---------|-------------|
| `freeze` | INTEGER | 0 | Colonne bloccate a sinistra |
| `footer` | STRING | — | Testo footer |
| `colors`/`colorspec` | STRING | — | `"W/B,N/G,B+/W"` (header, row, highlight) |
| `border`/`showborder` | BOOLEAN | TRUE | Bordo tabella |
| `header`/`showheader` | BOOLEAN | TRUE | Header colonne |
| `highlight`/`highlightrow` | BOOLEAN | TRUE | Evidenzia riga corrente |
| `mouse`/`mouseenabled` | BOOLEAN | TRUE | Click mouse |
| `autolite` | BOOLEAN | TRUE | Auto-refresh |
| `colsep` | STRING | `"│"` | Separatore colonne |
| `headsep` | STRING | `"─"` | Separatore header |
| `footsep` | STRING | `"─"` | Separatore footer |

### Tasti
| Tasto | Azione |
|-------|--------|
| ↑↓←→ | Navigazione celle |
| PgUp/PgDn | Pagina su/giù |
| Home/End | Inizio/fine |
| Enter | Seleziona riga / Edita cella |
| Esc | Annulla (-1) |
| **F5** | **Sort per colonna corrente (toggle asc/desc)** |
| **/** o **Ctrl+F** | **Search incrementale** |
| Backspace | Cancella carattere in search |

### Valore di ritorno

### Esempio completo

TBROWSE sel, 3, 2, 70, 15, _
    [["Name", 20, "C", "L", TRUE, TRUE], _
     ["Age",   5, "N", "R", TRUE, TRUE], _
     ["Score", 6, "F", "R", FALSE, TRUE]], _
    [["Alice", "30", "95.5"], ["Bob", "25", "82.0"]], _
    [["freeze", 1], _
     ["footer", "F5=Sort /=Search Enter=Select Esc=Cancel"], _
     ["colors", "W/B,B/W,N/G"]]

IF sel >= 0 THEN PRINT "Row: "; sel ELSE PRINT "Cancelled"


### TEXTMODE-Guida-Utente

*Fonte: `TEXTMODE-Guida-Utente`*


## 1. Scopo

## 2. Sintassi

TEXTMODE <modo>


### Parametro
| Parametro | Tipo | Descrizione |
|---|---|---|
| `<modo>` | numerico (o costante) | Modalità console. Accetta un **intero** (`0`..`3`) oppure una **costante** (`%SDL`, `%C64`, `%ANSI`, `%SYSTEM`). Può essere un'espressione/variabile, non solo un letterale. |

### Valori di `<modo>`
| Valore | Costante | Modalità | Descrizione |
|---:|---|---|---|
| `0` | `%SDL` | SDL (default) | Console grafica TUI del motore C (`consoleConfiguration[0]`). |
| `1` | `%C64` | C64 | Emulazione Commodore 64: font `C64_Pro_Mono-STYLE.otf`, palette **c64Classic** (VICE), remap **PETSCII**, completamento verticale dei box-char. |
| `2` | `%ANSI` | ANSI | Terminale standard con sequenze di escape ANSI. |
| `3` | `%SYSTEM` | SYSTEM | Console di sistema (host). |

## 3. Costanti globali

PRINT %SDL      ' 0
PRINT %C64      ' 1
PRINT %ANSI     ' 2
PRINT %SYSTEM   ' 3

| Costante | Valore |
|---|---:|
| `%SDL` | `0` |
| `%C64` | `1` |
| `%ANSI` | `2` |
| `%SYSTEM` | `3` |

## 4. Semantica ed effetti

### Interazione con `SCREEN 0`

SCREEN 1            ' apre la finestra grafica
'TEXTMODE...        ' (la console resta selezionabile)
SCREEN 0            ' chiude la finestra grafica, torna alla console


## 5. Configurazione e avvio

## 6. Esempi

' Selezione diretta con costanti
TEXTMODE %C64
TEXTMODE %ANSI
TEXTMODE %SDL

' Selezione con interi / variabile
TEXTMODE 3
DIM m AS INTEGER = %C64
TEXTMODE m

' Scelta condizionale
SELECT CASE %SDL
  CASE %SDL   : TEXTMODE %SDL
  CASE %C64   : TEXTMODE %C64
END SELECT


## 6bis. Polimorfismo (come si colloca `TEXTMODE`)

## 7. Note di implementazione (parità 4 backend)
| Backend | Implementazione |
|---|---|
| IL | `IGraphicsService.TextMode(int)` → `CGfxGraphicsService` (P/Invoke `gmb_gfx_console_select`) in modalità C; no-op headless. |
| PCodeVM | `SysCall.TEXTMODE` → `PCodeGraphics.TextMode`. |
| VM/C | syscall `TEXTMODE` (codice 381): no-op (headless). |
| C99Native | parser/emitter nativi (`net_sub` 47) → `gmb_console_select(...)`. |

## 8. Errori comuni
| Sintomo | Causa / Soluzione |
|---|---|
| Nessun effetto grafico | In headless (`GMBASIC_HEADLESS=1`) `TEXTMODE` è no-op. Verificare l'ambiente. |
| Font C64 non applicato | `consoleConfiguration[1]` assente o `fontPath` non risolvibile; verificare i settings. |
| Output invariato dopo `TEXTMODE` | Nessuna superficie grafica attiva (`SCREEN 0`): ridisegno non necessario. |

<!-- END SURFACE -->


<!-- BEGIN DOMAIN TABLE -->
## Tavola sinottica del dominio

Ogni riga riporta la firma canonica (dal registry) e il primo uso reale trovato
nel corpus di test e sample `.bas`/`.inc`: dove il corpus non usa il simbolo è dichiarato.

- **CONSOLE.ALERT(x1)** — `PRINT "alert="; CONSOLE.ALERT("a")` (shared/minitests/misc/console_domain.bas:27)
- **CONSOLE.BGRGB(x1, x2, x3, x4)** — `PRINT "bg="; CONSOLE.BGRGB("Y", 10, 20, 30)` (shared/minitests/misc/console_domain.bas:17)
- **CONSOLE.CELL(x1, x2, x3)** — `LOCATE CONSOLE.CELL(20, 1, 0), 1` (shared/samples/graphics/GraphicsLegacySyntax.bas:105)
- **CONSOLE.CELLSTR(x1, x2)** — `DIM C$ = CONSOLE.CELLSTR(20, 8) ' Accesso qualificato alla funzione` (shared/samples/graphics/GraphicsLegacySyntax.bas:112)
- **CONSOLE.CONFIRM(x1)** — `PRINT "confirm="; CONSOLE.CONFIRM("c")` (shared/minitests/misc/console_domain.bas:28)
- **CONSOLE.FGRGB(x1, x2, x3, x4)** — `PRINT "fg="; CONSOLE.FGRGB("X", 255, 128, 0)` (shared/minitests/misc/console_domain.bas:16)
- **CONSOLE.GETCURSORPOS()** — `pos = CONSOLE.GETCURSORPOS()` (shared/samples/Console/ConsoleSetAndGet.bas:58)
- **CONSOLE.GETPALETTECOLOR(x1)** — `pal = CONSOLE.GETPALETTECOLOR(1)` (shared/samples/Console/ConsoleSetAndGet.bas:66)
- **CONSOLE.GETTEXTMETRICS()** — `met = CONSOLE.GETTEXTMETRICS()` (shared/samples/Console/ConsoleSetAndGet.bas:62)
- **CONSOLE.INKEY()** — `PRINT "inkey=["; CONSOLE.INKEY(); "]"` (shared/minitests/misc/console_domain.bas:18)
- **CONSOLE.MESSAGE()** — `PRINT "=== CONSOLE.MESSAGE ==="` (shared/samples/Console/ConsoleMessagePoint.bas:11)
- **CONSOLE.POINT(x1, x2)** — `PRINT "=== CONSOLE.POINT ==="` (shared/samples/Console/ConsoleMessagePoint.bas:18)
<!-- END DOMAIN TABLE -->
