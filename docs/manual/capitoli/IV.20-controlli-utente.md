# IV.20 — Controlli utente

## Cosa ottieni da questo capitolo

Il catalogo dei controlli e la regola unica che li governa: **creazione con posizione e
dimensione, eventi per nome, proprietà per aspetto**. Quando hai capito un `BUTTON`, hai capito
anche `DataGrid` e `Chart`.

## 1. Un controllo, end-to-end

```basic
SUB OnClick()
    clicks = clicks + 1
    DRAW TEXT "click: " + STR$(clicks), 60, 130
END SUB

SUB Main()
    WINDOW THEME "light"
    WINDOW SHOW
    BUTTON 60, 50, 200, 44, "Cliccami" ON CLICK OnClick STYLE "rounded radius=8"
    DO WHILE WINDOWCLOSED() = 0
        WINDOW PUMP 16
    LOOP
END SUB
```

La riga del `BUTTON` contiene tutto il modello:

| Pezzo | Significato |
|---|---|
| `60, 50` | posizione `x, y` in pixel |
| `200, 44` | larghezza e altezza |
| `"Cliccami"` | testo |
| `ON CLICK OnClick` | handler del click |
| `STYLE "rounded radius=8"` | aspetto locale |

## 2. Il catalogo

| Famiglia | Controlli | Sample |
|---|---|---|
| base | `BUTTON`, `LABEL`, `EDIT`, `CHECKBOX`, `RADIO` | `ButtonFirst.bas`, `ButtonPropsDemo.bas` |
| testo | `TEXTAREA`, `CODEAREA`, `BIGTEXT` | `CodeAreaDemo.bas`, `BigTextDemo.bas` |
| liste | `LISTBOX`, `COMBOBOX`, `DATALIST`, `DATAGRID` | `DataGridDemo.bas`, `DataListDemo.bas` |
| contenitori | `PANEL`, `SPLITTER`, `TABS` | `BuiltinSplitterContextMenu.bas` |
| dialogo | `ALERTBOX`, `CONFIRMATIONBOX`, `CONTEXTMENU` | `AlertBoxDemo.bas`, `ConfirmationBoxDemo.bas` |
| dati | `CHART`, `CANDLECHART`, `MARKETDEPTH`, `WATCHLIST`, `ORDERTICKET` | `ChartControlDemo.bas`, `CandleChartDemo.bas` |
| compositi | dashboard, cruscotti | `DashboardDemo.bas`, `ControlCenter.bas` |

Sono 45 sample nella cartella `shared/samples/Controls/`: uno per controllo, più quelli di
proprietà (`ControlPropertiesCoverage.bas`) e di tema (`ControlThemeDemo.bas`).

## 3. Proprietà e metodi

Ogni controllo ha proprietà (aspetto, stato) e metodi (azioni). Le proprietà si possono leggere
e scrivere dopo la creazione, ed è così che si aggiorna un'interfaccia senza ricrearla:

```basic
' creazione
BUTTON 60, 50, 200, 44, "Cliccami" ON CLICK OnClick

' aggiornamento: testo e stato cambiano, il controllo resta
```

Il pattern corretto è: crei una volta, aggiorni molte. Ricreare il controllo a ogni cambio di
dato è la causa più comune di interfacce che sfarfallano.

## 4. Controlli composti

Un controllo utente può essere composto da altri controlli: un pannello con dentro una griglia e
due pulsanti è un controllo a sé, riusabile. Le guide `Controlli-Utente-Base-Control`,
`Controlli-Utente-Composizione` e `Controlli-Utente-Struttura` descrivono le tre parti:
la classe base, la composizione, la struttura dell'albero.

La conseguenza utile: gli eventi dei figli si possono propagare al contenitore, così chi usa il
controllo composto non deve conoscere i figli.

## 5. Controlli specializzati

Tre famiglie meritano una nota perché non sono "un pulsante in più":

- **grafici** (`CHART`, `CANDLECHART`) — serie di dati, assi, legenda, aggiornamento in tempo
  reale; guida `Controls-CHART-Guida-Utente`;
- **trading** (`MARKETDEPTH`, `WATCHLIST`, `ORDERTICKET`) — profondità di mercato, lista
  prezzi, ticket d'ordine; guide dedicate;
- **griglie** (`DATAGRID`) — colonne, ordinamento, editing, selezione; guida
  `DataGrid-Controllo-Guida-Utente`.

## 6. Perché il libro non mostra un output

Un controllo vive in una finestra: il suo output è un pixel, non una riga di testo. Questi sample
sono interattivi (`' TAG: interactive`) e nei runner vengono saltati — la stessa ragione del
capitolo IV.19. Il codice citato è quello reale dei sample, non una ricostruzione.

## 7. Errori frequenti

| Sintomo | Causa | Correzione |
|---|---|---|
| il controllo non appare | creato prima di `WINDOW SHOW` | mostra la finestra, poi crea, oppure ridisegna |
| il click non fa nulla | handler non collegato o SUB fuori dal GPM | `ON CLICK` con una SUB raggiungibile |
| il testo non si aggiorna | ricreazione continua del controllo | aggiorna la proprietà |
| la finestra si blocca durante un handler | lavoro lungo sul thread della GUI | sposta il lavoro in un task GPM |
| il controllo ignora il tema | `STYLE` che fissa il colore | togli quella proprietà |

## 8. Dove andare adesso

- **IV.19** — la finestra che contiene i controlli.
- **IV.18** — temi e stili.
- **IV.21** — quando serve disegnare, non solo comporre.













<!-- BEGIN DOMAIN TABLE -->
## Tavola sinottica del dominio

Ogni riga riporta la firma canonica (dal registry) e il primo uso reale trovato
nel corpus di test e sample `.bas`/`.inc`: dove il corpus non usa il simbolo è dichiarato.

- **CLIP.GET()** — `PRINT "-- CLIP.GET --"` (shared/samples/Console/ClipboardUncoveredCoverage.bas:30)
- **CONTROLS.CHANGED()** — `PRINT "CH0="; CONTROLS.CHANGED()` (shared/minitests/controls/clicked_optional_handle.bas:17)
- **CONTROLS.CLICKED()** — `a = CONTROLS.CLICKED()` (shared/minitests/controls/clicked_optional_handle.bas:14)
- **CONTROLS.CTRLGETPROP(x1, x2)** — `posizione = CTRLGETPROP("btn1", "Left")` (shared/samples/Controls/ControlPropertiesCoverage.bas:22)
- **CONTROLS.CTRLGETPROPS(x1, x2)** — `didascalia = CTRLGETPROPS("btn1", "Caption")` (shared/samples/Controls/ControlPropertiesCoverage.bas:23)
- **CONTROLS.DOUBLECLICKED()** — `PRINT "DBL0="; CONTROLS.DOUBLECLICKED()` (shared/minitests/controls/clicked_optional_handle.bas:18)
- **CONTROLS.GOTFOCUSED()** — `PRINT "gotfocus=";  GOTFOCUSED(hChk)` (shared/samples/graphics/EventModelDemo.bas:46)
- **CONTROLS.KEYPRESSED()** — `PRINT "key=";       KEYPRESSED()` (shared/samples/graphics/EventModelDemo.bas:51)
- **CONTROLS.LOSTFOCUSED()** — `PRINT "lostfocus="; LOSTFOCUSED(hChk)` (shared/samples/graphics/EventModelDemo.bas:47)
- **CONTROLS.MOUSECLICKED()** — `PRINT "mouse=";     MOUSECLICKED()` (shared/samples/graphics/EventModelDemo.bas:48)
- **CONTROLS.MOUSEDBLCLICKED()** — `PRINT "mousedbl=";  MOUSEDBLCLICKED()` (shared/samples/graphics/EventModelDemo.bas:49)
- **CONTROLS.MOUSEPRESSED()** — `PRINT "pressed=";   MOUSEPRESSED()` (shared/samples/graphics/EventModelDemo.bas:50)
- **UI.CLICK(x1)** — `PRINT "click: "; UI.CLICK("gmb-elemento-inesistente-xyz")` (shared/samples/UI/UiAutomation.bas:22)
- **UI.FIND(x1)** — `PRINT "find: "; UI.FIND("")` (shared/samples/UI/UiAutomation.bas:19)
- **UI.FINDALL(x1)** — `PRINT "findall: "; UI.FINDALL("")` (shared/samples/UI/UiAutomation.bas:20)
- **UI.TYPE(x1, x2)** — `PRINT "type: "; UI.TYPE("", "")` (shared/samples/UI/UiAutomation.bas:23)
<!-- END DOMAIN TABLE -->

<!-- BEGIN SURFACE (generato da tools/sh/manual_surface.sh — non modificare a mano) -->

## Riferimento completo: comandi, funzioni, parametri

> Sezione generata dalle guide del progetto per questo capitolo: contiene **tutte**
> le superfici documentate, non una selezione. La prosa dei capitoli precedenti le
> spiega; qui stanno i dettagli — parametri, varianti, esempi d'uso.

### Controlli-Pixel-Proprieta-Metodi-Guida-Utente

*Fonte: `Controlli-Pixel-Proprieta-Metodi-Guida-Utente`*


## 1. Creare un controllo

<CONTROLLO> <x>, <y> [, <argomenti...>] INTO <variabile> [ <clausole> ]


NAME "identificatore"     ' nome logico del controllo (per CONTROL ... e per gli eventi)
TAG "etichetta"           ' stringa libera dell'applicazione
TOOLTIP "suggerimento"    ' testo di aiuto
VISIBLE n                 ' 1 = visibile (default), 0 = nascosto
ENABLED n                 ' 1 = attivo (default), 0 = disabilitato
TABINDEX n                ' ordine di tabulazione
TABSTOP n                 ' 1 = il Tab si ferma qui (default), 0 = salta
FOCUS                     ' assegna il focus al controllo (SetFocus alla creazione)
STYLE "spec"              ' stile CSS-like (bg/fg/border/rounded/radius/emboss/...)
ON <EVENTO> <SUB>         ' collega un gestore di evento (ripetibile)
ID <variabile>            ' riceve l'handle intero del controllo (widget=i, bottone=1000+i)


### Verbi canonici (namespace `CONTROLS`)

IMPORT CONTROLS
IMPORT WINDOWS
SCREEN 12
DIM ok = 0
DIM hChk = -1
DIM hBtn = -1
CHECKBOX 40, 120, "Attiva" INTO ok ID hChk NAME "chkAttiva" TOOLTIP "on/off" TAG "grp" TABINDEX 1 ON CHANGE OnAttiva
BUTTON 40, 240, 150, 40, "Salva" ID hBtn ON CLICK OnSalva NAME "btnSalva" STYLE "rounded radius=10 emboss"
WINDOW PUMP 0


### Proprietà: significato dei parametri
| Proprietà | Tipo | Default | Descrizione |
|---|---|---|---|
| `NAME` | stringa | `""` | Nome logico, usato da `CONTROL ...` e per gli eventi. |
| `TAG` | stringa | `""` | Valore applicativo libero. |
| `TOOLTIP` | stringa | `""` | Testo di suggerimento. |
| `VISIBLE` | numero | `1` | Se `0`, il controllo non viene disegnato né riceve input. |
| `ENABLED` | numero | `1` | Se `0`, il controllo appare disabilitato e non risponde. |
| `TABINDEX` | numero | `0` | Ordine di tabulazione. |
| `TABSTOP` | numero | `1` | Se `0`, il Tab salta il controllo. |
| `FOCUS` | (nessuno) | — | Imposta il focus al controllo (equivale a `CONTROL FOCUS`). |
| `ID <var>` | numero | — | Scrive nell'`<var>` l'handle univoco del controllo (`widget=i`, `bottone=1000+i`). |

## 2. Eventi (`ON <EVENTO> <SUB>`)

### Eventi comuni (tutti i controlli)

### Eventi specifici
| Evento | Controllo | Quando |
|---|---|---|
| `SCROLL` | TRACK | durante il trascinamento |
| `DROPDOWN` | DROPDOWN | all'apertura dell'elenco |
| `UPCLICK` / `DOWNCLICK` | SPINNER | clic sui pulsanti `+` / `−` |
| `NODECLICK` | TREEVIEW | clic su un nodo |
| `EXPAND` / `COLLAPSE` | TREEVIEW | espansione/compressione di un ramo |
| `SELCHANGE` | LISTBOX, GRID | cambio di selezione |
| `ROWCOLCHANGE` | GRID | cambio riga/colonna |

SPINNER 40, 350, 150, 0, 20, 1 INTO qta NAME "spnQta" ON UPCLICK OnQta ON DOWNCLICK OnQta
TREEVIEW 340, 430, 240, 150, "a|a/f1|a/f2" INTO nodo NAME "tre" ON EXPAND OnEspandi ON COLLAPSE OnComprimi


### Due sintassi equivalenti

### Forme dell'azione
| Forma | Significato |
|---|---|
| `OnSub` / `OnSub()` / `CALL OnSub()` | invoca la `SUB OnSub` (callback) |
| `GOSUB OnSub` | come `CALL OnSub` (il bersaglio **deve essere una SUB**) |
| `GOTO OnSub` | come `GOSUB` (per gli eventi non c'è ritorno) |
| `STOP` | rimuove il gestore (deregistra l'evento) |

BUTTON 40, 40, 120, 36, "OK" ID h1 ONCLICK OnOk()
CHECK 40, 90, "Attiva" INTO ok ONCHANGE GOSUB OnCambia
CHECK 40, 130, "Off" INTO off2 ONCLICK STOP       ' nessun gestore


### Polling di stato (`CLICKED`/`CHANGED`/…)
| Funzione | Parametri | Ritorno | Significato |
|---|---|---|---|
| `CLICKED(h)` | `h` = handle del controllo | `1`/`0` | Vero se l'**ultimo evento** del controllo `h` è `CLICK`. |
| `DOUBLECLICKED(h)` | `h` = handle | `1`/`0` | Ultimo evento = doppio click. |
| `CHANGED(h)` | `h` = handle | `1`/`0` | Ultimo evento = cambio valore. |
| `GOTFOCUSED(h)` | `h` = handle | `1`/`0` | Ultimo evento = acquisizione focus. |
| `LOSTFOCUSED(h)` | `h` = handle | `1`/`0` | Ultimo evento = perdita focus. |
| `MOUSECLICKED()` | — | `1`/`0` | Ultimo evento **mouse globale** = click. |
| `MOUSEDBLCLICKED()` | — | `1`/`0` | Ultimo evento mouse globale = doppio click. |
| `MOUSEPRESSED()` | — | `1`/`0` | Ultimo evento mouse globale = pressione (`MOUSEDOWN`). |
| `KEYPRESSED()` | — | `1`/`0` | Un `KEYPRESS` globale è avvenuto (**sticky**, non azzerato dal rilascio). |

DIM h = -1
CHECK 40, 40, "A" INTO a ID h
' ... nel loop:
IF CLICKED(h) THEN PRINT "cliccato"


### Eventi raggruppati (`ON <EVENTO> CONTROL <id> <azione>`)
| Parte | Descrizione |
|---|---|
| `<EVENTO>` | nome dell'evento (CLICK, CHANGE, GOTFOCUS, …) — vedi tabelle sopra. |
| `CONTROL` | parola chiave fissa. |
| `<id>` | espressione numerica = handle del controllo (`ID h`). |
| `<azione>` | `CALL sub()` / `sub` / `GOSUB label` / `GOTO label` / `STOP`. |

DIM h = -1
BUTTON 40, 40, 120, 36, "OK" ID h
ON CLICK CONTROL h CALL OnOk()     ' sovrascrive l'eventuale ONCLICK inline
ON CHANGE CONTROL h STOP           ' rimuove il gestore CHANGE


### Eventi finestra (`ON WINDOW <EVENTO> [OF <handle|nome>] <azione>`)
| Parametro | Descrizione |
|---|---|
| `<handle>` | _espressione numerica_: `0` = finestra SDL; `id` restituito da `WINDOW DEFINE` per le finestre virtuali. |
| `<nome>` | in alternativa, il nome della finestra virtuale (`WINDOW DEFINE w1 …`). |
| `<azione>` | `CALL sub()` / `sub` / `GOSUB label` / `GOTO label` / `STOP`. |

ON WINDOW CLOSE OF 1 GOSUB OnChiudi        ' alla chiusura della finestra 1
ON WINDOW MINIMIZE OF win1 CALL OnMin()     ' minimizzazione
ON WINDOW CLOSE OF 0 GOSUB OnAppClose       ' finestra SDL principale


### `FIRE EVENT` — innesco programmatico di un evento (COMANDO)

FIRE EVENT <handle>, "<EVENTO>"

| Parametro | Descrizione |
|---|---|
| `<handle>` | espressione numerica: handle del controllo (`ID h`). |
| `"<EVENTO>"` | nome evento (`"CLICK"`, `"CHANGE"`, `"GOTFOCUS"`, …). |

DIM h = -1
CHECK 40, 40, "A" INTO a ID h
ON CHANGE CONTROL h CALL OnChg
FIRE EVENT h, "CHANGE"      ' esegue OnChg come se l'utente avesse cambiato il valore


## 3. `TEXTBOX` — campo di testo con cursore

TEXTBOX <x>, <y>, <w>, <h> INTO <variabile$> [ <clausole> ]


### Parametri
| Parametro | Tipo | Descrizione |
|---|---|---|
| `x`,`y` | numero | posizione (in pixel logici) |
| `w`,`h` | numero | larghezza/altezza |
| `INTO var$` | stringa | variabile che contiene/riceve il testo |

DIM nome AS STRING
TEXTBOX 600, 430, 240, 30 INTO nome NAME "txtNome" TOOLTIP "Scrivi un nome" TABINDEX 8 ON CHANGE OnNome ON VALIDATE OnNome


## 4. Metodi a runtime: `CONTROL <SUB> ...`

### `CONTROL SET "nome", "proprietà", valore [, stringa$]`

CONTROL SET "chkAttiva", "value", 1
CONTROL SET "btnSalva", "caption", 0, "Invia"
CONTROL SET "btnMostra", "visible", 0


### `CONTROL GET "nome", "proprietà" INTO <var>`

CONTROL GET "btnReset", "left" INTO lettura
CONTROL GET "btnMostra", "visible" INTO stato


### `CONTROL GETS "nome", "proprietà" INTO <var$>`

CONTROL GETS "btnReset", "caption" INTO etichetta
CONTROL GETS "txtNome", "tooltip" INTO aiuto


### `CONTROL MOVE "nome", x, y [, w, h]`

CONTROL MOVE "btnMostra", 600, 480, 240, 40
CONTROL MOVE "chkAttiva", 50, 130


### `CONTROL FOCUS "nome"`

CONTROL FOCUS "txtNome"


### `CONTROL ZORDER "nome", 1|0`

CONTROL ZORDER "btnReset", 1


### Valori di ritorno

## 5. Esempio completo

SCREEN 12, 900, 620, 32
DIM attivo = 1
DIM nome AS STRING
DIM lettura = 0
DIM etichetta AS STRING

SUB Main()
    WINDOW THEME "Monokai"
    CHECK 40, 120, "Attiva" INTO attivo NAME "chkAttiva" TABINDEX 1 ON CHANGE OnAttiva
    TEXTBOX 600, 430, 240, 30 INTO nome NAME "txtNome" TABINDEX 8 ON VALIDATE OnNome
    BUTTON 40, 30, 150, 40, "Ricomincia" ON CLICK OnReset NAME "btnReset"
    GO SUB EventLoop()
END SUB

SUB OnAttiva()
    PRINT "Attiva = "; attivo


### Nota: richiede `IMPORT WINDOWS`

## 5b. `WINDOW PUMP` / `WINDOWCLOSED()` / `WINDOW CLOSE` — il ciclo della finestra (namespace WINDOWS)

SCREEN 12
DIM stato = 0
CHECK 40, 30, "Attiva" INTO stato NAME "chk"
DO WHILE WINDOWCLOSED() = 0        ' 1 = finestra chiusa (ESC/X), 0 = aperta
    WINDOW PUMP 16                ' un frame: input + disegno; 16 = ms di park GPM
LOOP
WINDOW CLOSE                      ' richiede la chiusura (esce dal loop)


### Parametri e valori
| Funzione | Parametri | Ritorno | Descrizione |
|---|---|---|---|
| `WINDOW PUMP ms` (comando) | `ms` (numero) = millisecondi di attesa/park per frame (tipico 16 ≈ 60fps). Opzionale (default 16) | — | Esegue **un frame**: drena l'input (mouse/tastiera), dispatcha gli eventi, ridisegna. Va chiamata nel loop. |
| `WINDOWCLOSED()` (funzione) | nessuno | numero: `1` se la finestra è chiusa (ESC/X), `0` altrimenti | Condizione di uscita del loop. |
| `WINDOW CLOSE` | nessuno | — | Richiede la chiusura della finestra (la prossima `FORM_CLOSED()` ritorna 1). |

## 6. Famiglia unica e parità tra i 4 backend (verificata)
| Minitest | Copre |
|---|---|
| `control_id.bas` / `control_tui_id.bas` / `control_tui_id_multi.bas` | clausola `ID <var>` (handle del controllo) |
| `control_props.bas` | `NAME` `TAG` `TOOLTIP` `VISIBLE` `ENABLED` `TABINDEX` `TABSTOP` |
| `control_style_focus.bas` | `FOCUS` e `STYLE "spec"` |
| `control_events_forms.bas` | eventi inline `ONCLICK`/`ONCHANGE` + forme `CALL`/`GOSUB`/`GOTO`/`STOP` |
| `control_events_grouped.bas` | eventi raggruppati `ON <EV> CONTROL <id> <azione>` |
| `control_poll.bas` | polling `CLICKED`/`DOUBLECLICKED`/`CHANGED`/`GOTFOCUSED`/`LOSTFOCUSED` + `MOUSECLICKED`/`MOUSEDBLCLICKED`/`MOUSEPRESSED`/`KEYPRESSED` |
| `control_fireevent.bas`, `control_event_fire_e2e.bas` | `FIRE EVENT h, "EV"` (esegue DAVVERO la SUB gestore) |
| `control_setget_tui.bas`, `control_methods_perleg.bas` | `CONTROL SET/GET/GETS/MOVE/FOCUS/ZORDER` (anche sui controlli testuali) |
| `window_events.bas` | `ON WINDOW <EV>` e `WINDOW PUMP`/`WINDOW CLOSE`/`WINDOWCLOSED()` |

## Colori in formato Clipper

LISTBOX 5, 2, 30, 5, "a|b|c" INTO v COLOR "W+/B" COLORFOCUS "GR+/B" COLORSEL "N/W" COLORERROR "W+/R"

| Clausola | Significato |
|---|---|
| `COLOR "fg/bg"` | colore normale (testo/sfondo) |
| `COLORFOCUS "fg/bg"` | colore quando il controllo ha il focus |
| `COLORSEL "fg/bg"` | colore della voce selezionata |
| `COLORERROR "fg/bg"` | colore in errore |

LABEL 3, 2, 20, "titolo" STYLE "color=GR+/N"
CONTROL SET "lbl", "color", 0, "W+/B"     ' equivalente via CONTROL SET


## Immissione campo: `@ ... GET` + `READ` (Clipper)

@ row, col GET variabile [PICTURE "pic"] [WHEN <expr>] [VALID <expr>] [RANGE lo, hi]
       [MESSAGE "msg"] [ERROR "msg"] [COLOR "fg/bg"] [TYPE "..."] [SIZE w, h]
       [CAPTION "testo"] [STEP n] [MIN n] [MAX n]
READ

| Clausola | Significato |
|---|---|
| `PICTURE "pic"` | maschera di formato (es. `@!` maiuscolo) |
| `WHEN <expr>` | **pre**: se `.F.` il campo viene **saltato** (nessuna immissione) |
| `VALID <expr>` | **post**: se `.F.` l'uscita è impedita (re-focus del campo) |
| `RANGE lo, hi` | limita il valore numerico all'intervallo |
| `MESSAGE "msg"` | messaggio mostrato sul campo attivo |
| `ERROR "msg"` | messaggio mostrato se VALID fallisce |
| `COLOR "fg/bg"` | colore del campo (sintassi Clipper) |
| `TYPE "..."`/`SIZE w,h`/`CAPTION`/`STEP`/`MIN`/`MAX` | controllo tipizzato (Spinner, ComboBox, CheckBox, DatePicker, ListBox, …) |

## Controlli solo-grafica (pixel)
| Verbo | Firma | Descrizione |
|---|---|---|
| `CANVAS` | `CANVAS x, y, w, h` | superficie di disegno (rettangolo con bordo) |
| `IMAGE` | `IMAGE x, y, w, h, "path"` | immagine (placeholder con diagonale; `path` come etichetta) |
| `IMAGEBUTTON` | `IMAGEBUTTON x, y, w, h, "label"` | pulsante con icona + etichetta |
| `QR` | `QR x, y, w, h, "testo"` | codice QR generato dal testo |
| `RICHTEXT` | `RICHTEXT x, y, w, h, "riga1\|riga2"` | testo ricco multi-riga (`\|` separa le righe) |
| `HALFBLOCK` | `HALFBLOCK x, y, w, h [, "label"]` | mosaico di blocchi alternati |
| `TILING` | `TILING x, y, w, h [, "label"]` | pattern a piastrelle |

SCREEN 1
IMPORT CONTROLS
CANVAS 0, 0, 320, 200
IMAGE 8, 8, 64, 64, "logo.png"
IMAGEBUTTON 80, 8, 96, 32, "Salva"
QR 80, 48, 48, 48, "https://esempio"
RICHTEXT 8, 80, 200, 60, "Titolo|riga 1|riga 2"
TILING 8, 148, 120, 40


### CandleChart-Controllo-Guida-Utente

*Fonte: `CandleChart-Controllo-Guida-Utente`*


## Funzioni

## Geometria / mapping

## Esempio

IMPORT CONTROLS
IMPORT CustomControls

CLASS MyChart INHERITS Control
  ' campo dati + metodi come in CandleChartUC.bas
END CLASS

SUB Main()
  DIM ch AS MyChart = NEW MyChart()
  CONTROL ADD ch AT 2, 2 SIZE 78, 20
  ch.SetOrigin(2, 2, 78, 20)
  ch.AddCandle(100, 105, 98, 103)
  ' ... altre candele ...
  ch.Fit()
  ch.SetOverlay(3, 5)                 ' SMA(5)+EMA(5)
  ch.AddTrend(2, 96, 30, 108)         ' trendline (coordinate dati)


## Disponibilità sui 4 backend
| Backend | Esito |
|---|---|
| IL | ✅ |
| PCode (VM) | ✅ |
| VM/C | ✅ |
| C99Native | ✅ |

## Note di implementazione (2026-10-09)

## Riferimenti

### ClipperLib-Controls-Reference

*Fonte: `ClipperLib-Controls-Reference`*


## Namespaces

## Base Class

### `Control` (abstract)
| Property | Type | Default | Description |
|---|---|---|---|
| `Id` | `string` | `""` | Unique identifier for event routing |
| `X` | `int` | `0` | Column position (left) |
| `Y` | `int` | `0` | Row position (top) |
| `Width` | `int` | `0` | Width in terminal columns |
| `Height` | `int` | `0` | Height in terminal rows |
| `Focused` | `bool` | `false` (`virtual`) | Whether control has keyboard focus |
| `Visible` | `bool` | `true` | Whether control is rendered |
| `Enabled` | `bool` | `true` | Whether control accepts input |
| `TabStop` | `bool` | `true` | Whether Tab/Shift+Tab can focus this control |
| `TabIndex` | `int` | `0` | Tab order index |
| `Style` | `ControlStyle` | `ControlStyle.Default()` | Visual style (colors, fill) |
| `Uppercase` | `bool` | `false` | Auto-convert text to uppercase |
| `HasError` | `bool` | `false` | Error state — activates error colors |
| Property | Type | Default | Description |
|---|---|---|---|
| `BlinkOn` | `bool` | `true` | Global cursor blink toggle |
| Method | Returns | Description |
|---|---|---|
| `Draw(ScreenBuffer)` | `abstract void` | Render control to buffer |
| `HandleKey(int key)` | `abstract bool` | Process keyboard event |
| `HandleClick(x, y)` | `virtual bool` | Legacy click handler |
| `HandleMouse(MouseAction)` | `virtual bool` | Full mouse handler (click, scroll, drag) |
| `HandleInput(int key)` | `bool` | Unified entry: routes mouse via `MouseAction.FromInkeyCode`, keyboard via `HandleKey` |
| `Navigate(UiEvent)` | `virtual bool` | Unified event handler (crossterm-style) |
| `WithStyle(ControlStyle)` | `Control` | Set style |
| `WithColors(fg, bg)` | `Control` | Set normal colors |
| `WithFocusColors(fg, bg)` | `Control` | Set focus colors |
| `WithErrorColors(fg, bg)` | `Control` | Set error colors |
| `WithSelectedColors(fg, bg)` | `Control` | Set selection colors |
| `WithFillChar(char)` | `Control` | Set fill character |
| `WithUppercase(bool)` | `Control` | Enable/disable uppercase |
| Property | Type | Description |
|---|---|---|
| `Rect` | `ScreenRect` | Returns `new(X, Y, Width, max(1, Height))` |

var input = new TextInput { X = 2, Y = 5, Width = 30, Height = 1 }
    .WithColors(CellColor.White, CellColor.Blue)
    .WithFocusColors(CellColor.Black, CellColor.Yellow);


### `ControlStyle`
| Property | Type | Default | Role |
|---|---|---|---|
| `Fg` | `CellColor?` | — | Normal text color |
| `Bg` | `CellColor?` | — | Normal background color |
| `FgFocus` | `CellColor?` | — | Focused text color |
| `BgFocus` | `CellColor?` | — | Focused background color |
| `FgError` | `CellColor?` | — | Error text color |
| `BgError` | `CellColor?` | — | Error background color |
| `SelFg` | `CellColor?` | — | Selected/highlighted text color |
| `SelBg` | `CellColor?` | — | Selected/highlighted background color |
| `FillChar` | `char` | `' '` | Background fill character |
| Method | Returns | Description |
|---|---|---|
| `Default()` | `ControlStyle` | Default style (or from active Theme) |
| `FromTheme(Theme)` | `ControlStyle` | Build style from a Theme palette |
| `SetTheme(Theme)` | `void` | Set global theme |
| Method | Returns | Description |
|---|---|---|
| `Clone()` | `ControlStyle` | Deep copy |
| `WithColors(fg, bg)` | `ControlStyle` | Set normal colors |
| `WithFocusColors(fg, bg)` | `ControlStyle` | Set focus colors |
| `WithErrorColors(fg, bg)` | `ControlStyle` | Set error colors |
| `WithSelectedColors(fg, bg)` | `ControlStyle` | Set selection colors |
| `WithFill(char)` | `ControlStyle` | Set fill character |
| `Effective(focused, error, selected)` | `(CellColor, CellColor)` | Resolve effective colors for state |

var style = new ControlStyle()
    .WithColors(CellColor.White, CellColor.Blue)
    .WithFocusColors(CellColor.Black, CellColor.Cyan)
    .WithErrorColors(CellColor.White, CellColor.Red)
    .WithFill('░');

var (fg, bg) = style.Effective(focused: true);


## Input Controls

### `TextInput`
| Property | Type | Default | Description |
|---|---|---|---|
| `Text` | `string` | `""` | Text content |
| `Cursor` | `int` | `0` | Cursor position within text |
| `MaxLength` | `int` | `50` | Maximum characters allowed |
| `Picture` | `string?` | `null` | PICTURE format template (Clipper-compatible) |
| `Validator` | `Func<string, bool>?` | `null` | Validation function |
| `PasswordMode` | `bool` | `false` | Mask input with `*` |
| `ReadOnly` | `bool` | `false` | Disable editing |
| Property | Description |
|---|---|
| `IsValid` | Returns `Validator == null \|\| Validator(Text)` |
| Method | Returns | Description |
|---|---|---|
| `WithText(string)` | `TextInput` | Set text, cursor at end |
| `WithMaxLength(int)` | `TextInput` | Set max length |
| `WithPasswordMode(bool)` | `TextInput` | Toggle password mode |
| `WithValidator(Func<string,bool>?)` | `TextInput` | Set validator |
| Key | Action |
|---|---|
| Printable (32–126) | Insert character at cursor |
| `BS` | Delete character before cursor |
| `DEL` | Delete character at cursor |
| `LEFT` | Move cursor left |
| `RIGHT` | Move cursor right |
| `HOME` | Move cursor to start |
| `END` | Move cursor to end |

var input = new TextInput
{
    X = 5, Y = 8, Width = 30, Height = 1
}
.WithText("hello")
.WithMaxLength(100)
.WithValidator(s => s.Length >= 3);


### `TextArea`
| Property | Type | Default | Description |
|---|---|---|---|
| `Lines` | `List<string>` | `[""]` | Text lines |
| `CursorLine` | `int` | `0` | Current line index |
| `CursorCol` | `int` | `0` | Column within current line |
| `ScrollOffset` | `int` | `0` | Vertical scroll offset |
| Method | Returns | Description |
|---|---|---|
| `WithLines(List<string>)` | `TextArea` | Set lines, reset cursor |
| Key | Action |
|---|---|
| Printable | Insert character at cursor |
| `ENTER` | Split line at cursor |
| `BS` | Delete char before cursor; join with previous line at start |
| `UP` | Move cursor up |
| `DOWN` | Move cursor down |
| `LEFT` | Move cursor left |
| `RIGHT` | Move cursor right |
| `HOME` | Move to start of line |
| `END` | Move to end of line |

var editor = new TextArea
{
    X = 2, Y = 3, Width = 60, Height = 20
}
.WithLines(["First line", "Second line", "Third line"]);


### `Button`
| Property | Type | Default | Description |
|---|---|---|---|
| `Label` | `string` | `"OK"` | Button text |
| `OnClick` | `Action?` | `null` | Click callback |
| Method | Returns | Description |
|---|---|---|
| `WithLabel(string)` | `Button` | Set label |
| `WithOnClick(Action)` | `Button` | Set click handler |
| Key | Action |
|---|---|
| `ENTER` or `SPACE` | Invoke `OnClick` |

var btn = new Button { X = 10, Y = 15 }
    .WithLabel("Save")
    .WithOnClick(() => Console.Beep());


### `CheckBox`
| Property | Type | Default | Description |
|---|---|---|---|
| `Checked` | `bool` | `false` | Checked state |
| `Label` | `string` | `""` | Checkbox label |
| Method | Returns | Description |
|---|---|---|
| `WithChecked(bool)` | `CheckBox` | Set checked state |
| `WithLabel(string)` | `CheckBox` | Set label |
| Key | Action |
|---|---|
| `SPACE` or `ENTER` | Toggle checked state |

var cb = new CheckBox { X = 2, Y = 4, Width = 20, Height = 1 }
    .WithLabel("Enable feature")
    .WithChecked(true);


### `RadioGroup`
| Property | Type | Default | Description |
|---|---|---|---|
| `Options` | `List<string>` | `[]` | Radio options |
| `Selected` | `int` | `0` | Selected index |
| Method | Returns | Description |
|---|---|---|
| `WithOptions(List<string>)` | `RadioGroup` | Set options |
| Key | Action |
|---|---|
| `UP` | Select previous option |
| `DOWN` | Select next option |

var rg = new RadioGroup { X = 2, Y = 5, Width = 15, Height = 3 }
    .WithOptions(["Small", "Medium", "Large"]);


### `ComboBox`
| Property | Type | Default | Description |
|---|---|---|---|
| `Items` | `List<string>` | `[]` | Drop-down items |
| `Selected` | `int?` | `null` | Selected index |
| `Open` | `bool` | `false` | Drop-down open state |
| `Highlight` | `int` | `0` | Highlighted item in open list |
| `DropDownHeight` | `int` | `5` | Visible items in drop-down |
| Method | Returns | Description |
|---|---|---|
| `WithItems(List<string>)` | `ComboBox` | Set items |
| Key | Action |
|---|---|
| `DOWN` | Open drop-down and move to next |
| `ENTER` | Open drop-down |
| Key | Action |
|---|---|
| `UP` / `DOWN` | Navigate items |
| `ENTER` | Select highlighted item |
| `ESC` | Close without selecting |
| Printable | Filter items by typed text |

var combo = new ComboBox { X = 2, Y = 3, Width = 20, Height = 1 }
    .WithItems(["Option A", "Option B", "Option C"]);


### `NumericUpDown`
| Property | Type | Default | Description |
|---|---|---|---|
| `Value` | `int` | `0` | Current value |
| `Min` | `int` | `int.MinValue` | Minimum value |
| `Max` | `int` | `int.MaxValue` | Maximum value |
| `Step` | `int` | `1` | Step increment |
| Method | Returns | Description |
|---|---|---|
| `WithRange(int, int)` | `NumericUpDown` | Set min/max range |
| `WithValue(int)` | `NumericUpDown` | Set value (clamped) |
| Key | Action |
|---|---|
| `UP` | Increment by step |
| `DOWN` | Decrement by step |
| `PGUP` | Add 10 |
| `PGDN` | Subtract 10 |
| Digits (0–9) | Direct entry |

var nud = new NumericUpDown { X = 2, Y = 4, Width = 8, Height = 1 }
    .WithRange(0, 100)
    .WithValue(42);


### `Slider`
| Property | Type | Default | Description |
|---|---|---|---|
| `Min` | `int` | `0` | Minimum value |
| `Max` | `int` | `100` | Maximum value |
| `Value` | `int` | `0` | Current value |
| `Step` | `int` | `1` | Step increment |
| Method | Returns | Description |
|---|---|---|
| `WithRange(int, int)` | `Slider` | Set min/max |
| `WithValue(int)` | `Slider` | Set value (clamped) |
| Key | Action |
|---|---|
| `LEFT` / `UP` | Decrease value |
| `RIGHT` / `DOWN` | Increase value |
| `HOME` | Set to minimum |
| `END` | Set to maximum |

var slider = new Slider { X = 2, Y = 6, Width = 20, Height = 1 }
    .WithRange(0, 50)
    .WithValue(25);


### `StarRating`
| Property | Type | Default | Description |
|---|---|---|---|
| `Value` | `int` | `0` | Current rating |
| `Max` | `int` | `5` | Maximum stars |
| Method | Returns | Description |
|---|---|---|
| `WithMax(int)` | `StarRating` | Set max stars |
| `WithValue(int)` | `StarRating` | Set value |
| Key | Action |
|---|---|
| `LEFT` / `UP` | Decrease |
| `RIGHT` / `DOWN` | Increase |

var rating = new StarRating { X = 2, Y = 3, Width = 10, Height = 1 }
    .WithMax(10)
    .WithValue(7);


### `Spinner`
| Property | Type | Default | Description |
|---|---|---|---|
| `Frames` | `List<string>` | `["|","/","-","\\"]` | Animation frames |
| `Index` | `int` | `0` | Current frame index |
| Method | Returns | Description |
|---|---|---|
| `WithFrames(List<string>)` | `Spinner` | Set custom frames |
| `WithPreset(string)` | `Spinner` | Set preset by name |
| Method | Description |
|---|---|
| `Tick()` | Advance to next frame |
| `SetPreset(string)` | Load frame set by name |

var spinner = new Spinner { X = 2, Y = 5, Width = 3, Height = 1 }
    .WithPreset("braille");

// In update loop:
spinner.Tick();


### `DatePicker`
| Property | Type | Default | Description |
|---|---|---|---|
| `Day` | `int` | `1` | Day |
| `Month` | `int` | `1` | Month |
| `Year` | `int` | `2025` | Year |
| `Open` | `bool` | `false` | Calendar popup state |
| `PopupHeight` | `int` | `10` | Height when open |
| `MinYear` | `int` | `0` | Minimum selectable year |
| `MaxYear` | `int` | `0` | Maximum selectable year |
| Event | Description |
|---|---|
| `OnDateSelected` | `event Action<int, int, int>` — day, month, year |
| Method | Returns | Description |
|---|---|---|
| `WithDate(d, m, y)` | `DatePicker` | Set date |
| `WithYearRange(min, max)` | `DatePicker` | Set year range |
| Key | Action |
|---|---|
| `ENTER` or `SPACE` | Open calendar popup |
| Key | Action |
|---|---|
| `LEFT` / `RIGHT` | Previous/next day |
| `UP` / `DOWN` | Previous/next week |
| `PGUP` | Previous month |
| `PGDN` | Next month |
| `HOME` | First of month |
| `END` | Last of month |
| `ENTER` | Confirm selection |
| `ESC` | Cancel |

var dp = new DatePicker { X = 2, Y = 3, Width = 12, Height = 1 }
    .WithDate(15, 6, 2026)
    .WithYearRange(2000, 2030);
dp.OnDateSelected += (d, m, y) => Console.WriteLine($"{d}/{m}/{y}");


### `ColorPicker`
| Property | Type | Default | Description |
|---|---|---|---|
| `Palette` | `List<CellColor>` | `[]` | Color palette |
| `Columns` | `int` | `16` | Display columns |
| `SelectedIndex` | `int` | `0` | Selected color index |
| `TopRow` | `int` | `0` | Scroll offset |
| Method | Returns | Description |
|---|---|---|
| `WithColumns(int)` | `ColorPicker` | Set columns |
| `WithPalette(List<CellColor>)` | `ColorPicker` | Set palette |
| Key | Action |
|---|---|
| `UP` / `DOWN` / `LEFT` / `RIGHT` | Navigate palette |

var cp = new ColorPicker { X = 2, Y = 2, Width = 32, Height = 8 }
    .WithColumns(8);


### `ToggleButton`
| Property | Type | Default | Description |
|---|---|---|---|
| `Caption` | `string` | `""` | Button caption |
| `Pressed` | `bool` | `false` | Toggle state |
| Method | Returns | Description |
|---|---|---|
| `WithCaption(string)` | `ToggleButton` | Set caption |
| `WithPressed(bool)` | `ToggleButton` | Set state |
| Key | Action |
|---|---|
| `SPACE` or `ENTER` | Toggle pressed state |

var tb = new ToggleButton { X = 2, Y = 3, Width = 16, Height = 1 }
    .WithCaption("Dark Mode")
    .WithPressed(true);


### `ImageButton`
| Property | Type | Default | Description |
|---|---|---|---|
| `Icon` | `string` | `""` | Icon character(s) |
| `Label` | `string` | `""` | Button label |
| `OnClick` | `Action?` | `null` | Click callback |
| Method | Returns | Description |
|---|---|---|
| `WithIcon(string)` | `ImageButton` | Set icon |
| `WithLabel(string)` | `ImageButton` | Set label |
| `WithOnClick(Action)` | `ImageButton` | Set click handler |

var ibtn = new ImageButton { X = 2, Y = 5, Width = 12, Height = 1 }
    .WithIcon("🔍")
    .WithLabel("Search")
    .WithOnClick(() => Console.Beep());


### `Form`

public class FormField
{
    public string Label { get; set; } = "";
    public string Value { get; set; } = "";
    public bool Editing { get; set; }
}

| Property | Type | Default | Description |
|---|---|---|---|
| `Fields` | `List<FormField>` | `[]` | Form fields |
| `SelectedIndex` | `int` | `0` | Selected field index |
| Method | Returns | Description |
|---|---|---|
| `WithFields(List<FormField>)` | `Form` | Set fields |
| Key | Action |
|---|---|
| `UP` / `DOWN` | Navigate between fields |
| `ENTER` | Edit selected field value |
| `TAB` | Confirm edit and move to next |

var form = new Form { X = 2, Y = 2, Width = 40, Height = 4 }
    .WithFields([
        new FormField { Label = "Name", Value = "John" },
        new FormField { Label = "Email", Value = "john@example.com" }
    ]);


## List Controls

### `ListBox`
| Property | Type | Default | Description |
|---|---|---|---|
| `Items` | `List<string>` | `[]` | List items |
| `Selected` | `int?` | `null` | Selected index |
| `Top` | `int` | `0` | Scroll offset |
| `MultiSelect` | `bool` | `false` | Enable checkboxes |
| `Checked` | `HashSet<int>` | `[]` | Checked indices |
| Method | Returns | Description |
|---|---|---|
| `WithItems(List)` | `ListBox` | Set items |
| `WithMultiSelect(bool)` | `ListBox` | Toggle multi-select |
| Key | Action |
|---|---|
| `UP` / `DOWN` | Navigate items |
| `PGUP` / `PGDN` | Page scroll |
| `HOME` / `END` | First/last item |
| `SPACE` | Toggle checkbox (multi-select) |

var lb = new ListBox { X = 2, Y = 2, Width = 20, Height = 6 }
    .WithItems(["Apples", "Oranges", "Bananas", "Grapes"])
    .WithMultiSelect(true);


### `ReorderListBox`
| Property | Type | Default | Description |
|---|---|---|---|
| `Items` | `List<string>` | `[]` | List items |
| `Selected` | `int?` | `null` | Selected index |
| `Top` | `int` | `0` | Scroll offset |
| `MultiSelect` | `bool` | `false` | Enable checkboxes |
| `Checked` | `List<bool>` | `[]` | Checked states |
| `CtrlHeld` | `bool` | `false` | Ctrl held for reorder |
| Method | Returns | Description |
|---|---|---|
| `WithItems(List)` | `ReorderListBox` | Set items |
| `WithMultiSelect(bool)` | `ReorderListBox` | Toggle multi-select |
| Key | Action |
|---|---|
| `Ctrl+UP` | Move selected item up |
| `Ctrl+DOWN` | Move selected item down |

var rlb = new ReorderListBox { X = 2, Y = 2, Width = 20, Height = 6 }
    .WithItems(["Item A", "Item B", "Item C"])
    .WithMultiSelect(true);


### `DataList`
| Property | Type | Default | Description |
|---|---|---|---|
| `Items` | `List<string>` | `[]` | List items |
| `SelectedIndex` | `int` | `0` | Selected index |
| `ScrollOffset` | `int` | `0` | Scroll offset |
| `MultiSelect` | `bool` | `false` | Enable multi-select |
| `SelectedItems` | `HashSet<int>` | `[]` | Selected indices |
| `Searchable` | `bool` | `false` | Enable incremental search |
| `SearchBuffer` | `string` | `""` | Accumulated search text |
| Method | Returns | Description |
|---|---|---|
| `WithItems(List)` | `DataList` | Set items |
| `WithMultiSelect(bool)` | `DataList` | Toggle multi-select |
| `WithSearchable(bool)` | `DataList` | Toggle search |
| Key | Action |
|---|---|
| `UP` / `DOWN` | Navigate |
| `PGUP` / `PGDN` | Page scroll |
| `HOME` / `END` | First/last |
| `SPACE` | Toggle selection (multi-select) |
| Printable (searchable) | Incremental search |

var dl = new DataList { X = 2, Y = 2, Width = 20, Height = 6 }
    .WithItems(["Alpha", "Beta", "Gamma", "Delta"])
    .WithMultiSelect(true)
    .WithSearchable(true);


### `Tab`
| Property | Type | Default | Description |
|---|---|---|---|
| `Tabs` | `List<string>` | `[]` | Tab titles |
| `Active` | `int` | `0` | Active tab index |
| Method | Returns | Description |
|---|---|---|
| `WithTabs(List)` | `Tab` | Set tabs |
| Key | Action |
|---|---|
| `LEFT` / `RIGHT` | Switch tab |
| `HOME` | First tab |
| `END` | Last tab |

var tab = new Tab { X = 2, Y = 1, Width = 40, Height = 1 }
    .WithTabs(["General", "Advanced", "About"]);


### `Menu`

public class MenuItem
{
    public string Label { get; set; } = "";
    public List<MenuItem> SubItems { get; set; } = [];
    public bool Separator { get; set; }
    public Action? Action { get; set; }
}

| Property | Type | Default | Description |
|---|---|---|---|
| `Items` | `List<MenuItem>` | `[]` | Menu items |
| `SelectedIndex` | `int` | `0` | Selected index |
| `IsOpen` | `bool` | `false` | Menu open state |
| Method | Returns | Description |
|---|---|---|
| `WithItems(List<MenuItem>)` | `Menu` | Set menu items |
| Key | Action |
|---|---|
| `UP` / `DOWN` | Navigate items |
| `ENTER` | Activate item or open submenu |
| `ESC` | Close menu |

var menu = new Menu { X = 2, Y = 1, Width = 15 }
    .WithItems([
        new MenuItem { Label = "New", Action = () => { } },
        new MenuItem { Separator = true },
        new MenuItem { Label = "Exit", Action = () => Environment.Exit(0) }
    ]);


### `TreeView`

public class TreeNode
{
    public string Label { get; set; } = "";
    public int Depth { get; set; }
    public bool Expanded { get; set; } = true;
    public bool HasChildren { get; set; }
}

| Property | Type | Default | Description |
|---|---|---|---|
| `Nodes` | `List<TreeNode>` | `[]` | All tree nodes (flat list) |
| `SelectedIndex` | `int` | `0` | Selected index in flat list |
| `ScrollOffset` | `int` | `0` | Scroll offset |
| Method | Returns | Description |
|---|---|---|
| `WithNodes(List<TreeNode>)` | `TreeView` | Set nodes |
| Key | Action |
|---|---|
| `UP` / `DOWN` | Navigate |
| `LEFT` | Collapse node |
| `RIGHT` | Expand node |
| `HOME` / `END` | First/last |

var tv = new TreeView { X = 2, Y = 2, Width = 30, Height = 8 }
    .WithNodes([
        new TreeNode { Label = "Root", Depth = 0, HasChildren = true, Expanded = true },
        new TreeNode { Label = "Child 1", Depth = 1 },
        new TreeNode { Label = "Child 2", Depth = 1, HasChildren = true, Expanded = false },
        new TreeNode { Label = "Grandchild", Depth = 2 },
    ]);


## Data Controls

### `TBrowse` (in `ClipperLib.TBrowse`)

public class BrowseColumn
{
    public string Header { get; set; }
    public int Width { get; set; }
    public BrowseAlign Align { get; set; } = BrowseAlign.Left;
    public bool Editable { get; set; }
    public bool Visible { get; set; } = true;
    public BrowseDataType DataType { get; set; } = BrowseDataType.Text;
    public List<string>? Choices { get; set; }
}

| Property | Type | Default | Description |
|---|---|---|---|
| `Columns` | `List<BrowseColumn>` | `[]` | Column definitions |
| `DataSource` | `IBrowseDataSource` | — | Data source |
| `FrozenCols` | `int` | `0` | Frozen left columns |
| `CurrentRow` | `int` | `0` | Current row |
| `CurrentCol` | `int` | `0` | Current column |
| `TopRow` | `int` | `0` | Top visible row |
| `LeftCol` | `int` | `0` | Left visible column |
| `ShowHeader` | `bool` | `true` | Show header row |
| `ShowBorder` | `bool` | `true` | Show border |
| `HighlightRow` | `bool` | `true` | Highlight entire row |
| `MouseEnabled` | `bool` | `false` | Mouse navigation |
| `HeaderColors` | `BrowseColorPair` | — | Header color pair |
| `RowColors` | `BrowseColorPair` | — | Normal row colors |
| `HighlightColors` | `BrowseColorPair` | — | Highlighted row colors |
| Method | Returns | Description |
|---|---|---|
| `MoveUp()` | `bool` | Move up one row |
| `MoveDown()` | `bool` | Move down one row |
| `MoveLeft()` | `bool` | Move left one column |
| `MoveRight()` | `bool` | Move right one column |
| `PageUp()` | `bool` | Move up one page |
| `PageDown()` | `bool` | Move down one page |
| `GoTop()` | `void` | Go to first row |
| `GoBottom()` | `void` | Go to last row |
| `HandleKey(int)` | `BrowseAction` | Process key, returns `BrowseAction` |

public interface IBrowseDataSource
{
    string GetValue(int row, int col);
    void SetValue(int row, int col, string val);
    int RowCount { get; }
    int Skip(int delta);
    void GoTop();
    void GoBottom();
    object? Bookmark(bool create);
    void GoTo(object? bookmark);
}


var data = new List<List<string>>
{
    ["Alice", "30", "Engineer"],
    ["Bob", "25", "Designer"],
    ["Charlie", "35", "Manager"]
};

var browse = new TBrowse
{
    Area = new ScreenRect(2, 2, 40, 10),
    Columns =
    [
        new BrowseColumn("Name", 15) { Editable = true },
        new BrowseColumn("Age", 6) { Align = BrowseAlign.Right },
        new BrowseColumn("Role", 15),
    ],


### `Grid`
| Property | Type | Default | Description |
|---|---|---|---|
| `Cols` | `int` | `0` | Number of columns |
| `Cells` | `List<string>` | `[]` | Cell values (row-major) |
| `CursorRow` | `int` | `0` | Row cursor |
| `CursorCol` | `int` | `0` | Column cursor |
| `Selected` | `(int, int)?` | `null` | Selected cell |
| Method | Returns | Description |
|---|---|---|
| `WithCols(int)` | `Grid` | Set columns |
| `WithCells(List<string>)` | `Grid` | Set cells |
| Key | Action |
|---|---|
| `UP` / `DOWN` / `LEFT` / `RIGHT` | Navigate cells |

var grid = new Grid { X = 2, Y = 2, Width = 40, Height = 6 }
    .WithCols(3)
    .WithCells(["A1", "B1", "C1", "A2", "B2", "C2"]);


### `PropertyGrid`
| Property | Type | Default | Description |
|---|---|---|---|
| `Items` | `List<(string name, string value)>` | `[]` | Property list |
| `SelectedIndex` | `int` | `0` | Selected property |
| `ScrollOffset` | `int` | `0` | Scroll offset |
| Method | Returns | Description |
|---|---|---|
| `WithItems(List<(string,string)>)` | `PropertyGrid` | Set items |
| Key | Action |
|---|---|
| `UP` / `DOWN` | Navigate |
| `ENTER` | Start editing value |
| `ENTER` (editing) | Confirm edit |
| `ESC` (editing) | Cancel edit |

var pg = new PropertyGrid { X = 2, Y = 2, Width = 40, Height = 6 }
    .WithItems([
        ("Name", "MyApp"),
        ("Version", "1.0.0"),
        ("Author", "Developer")
    ]);


### `HexView`

public class HexViewConfig
{
    public int BytesPerGroup { get; set; } = 2;
    public string GroupSeparator { get; set; } = " ";
    public bool ShowAddress { get; set; } = true;
    public AddressFormat AddressFormat { get; set; } = AddressFormat.Hex;
    public int StartAddr { get; set; }
    public bool ShowAscii { get; set; } = true;
}

| Property | Type | Default | Description |
|---|---|---|---|
| `Data` | `byte[]` | `[]` | Data to display |
| `BytesPerRow` | `int` | `16` | Bytes per line |
| `Offset` | `int` | `0` | Scroll offset |
| `Config` | `HexViewConfig` | `new()` | Display configuration |
| `State` | `DisplayState` | `Enabled` | Disabled/Enabled/Editable |
| `EditingIndex` | `int?` | `null` | Currently editing byte |
| `EditingHighNibble` | `bool` | `false` | Nibble being edited |
| Method | Returns | Description |
|---|---|---|
| `WithData(byte[])` | `HexView` | Set data |
| `WithConfig(HexViewConfig)` | `HexView` | Set config |
| Key | Action |
|---|---|
| `UP` / `DOWN` | Scroll |
| `LEFT` / `RIGHT` | Navigate bytes |
| `HOME` / `END` | First/last byte |
| Hex digit (Editable) | Edit nibble |

var hex = new HexView { X = 2, Y = 2, Width = 60, Height = 10 }
    .WithData(File.ReadAllBytes("data.bin"))
    .WithConfig(new HexViewConfig { BytesPerGroup = 4, ShowAscii = true });


## Navigation & Display Controls

### `Label`
| Property | Type | Default | Description |
|---|---|---|---|
| `Text` | `string` | `""` | Label text |
| `Align` | `Alignment` | `Left` | Horizontal alignment |
| Method | Returns | Description |
|---|---|---|
| `WithText(string)` | `Label` | Set text |
| `WithAlign(Alignment)` | `Label` | Set alignment |

var label = new Label { X = 2, Y = 1, Width = 20, Height = 1 }
    .WithText("Welcome to ClipperLib")
    .WithAlign(Alignment.Center);


### `HLine` (Horizontal Line)
| Property | Type | Default | Description |
|---|---|---|---|
| `Label` | `string` | `""` | Centered label text |
| Method | Returns | Description |
|---|---|---|
| `WithLabel(string)` | `HLine` | Set label |

var hline = new HLine { X = 2, Y = 4, Width = 40, Height = 1 }
    .WithLabel("Section Break");


### `VLine` (Vertical Line)

var vline = new VLine { X = 20, Y = 2, Width = 1, Height = 10 };


### `ProgressBar`
| Property | Type | Default | Description |
|---|---|---|---|
| `Min` | `int` | `0` | Minimum value |
| `Max` | `int` | `100` | Maximum value |
| `Value` | `int` | `0` | Current value |
| `ShowPercent` | `bool` | `true` | Show percentage text |
| Method | Returns | Description |
|---|---|---|
| `WithRange(int, int)` | `ProgressBar` | Set range |
| `WithValue(int)` | `ProgressBar` | Set value (clamped) |

var pb = new ProgressBar { X = 2, Y = 8, Width = 30, Height = 1 }
    .WithRange(0, 100)
    .WithValue(67);

// Output: [████████████████████····] 67%


### `StatusBar`
| Property | Type | Default | Description |
|---|---|---|---|
| `Text` | `string` | `""` | Status text |
| Method | Returns | Description |
|---|---|---|
| `WithText(string)` | `StatusBar` | Set text |

var sb = new StatusBar { X = 0, Y = 24, Width = 80 }
    .WithText("Ready | Line: 42 | Col: 7");


### `Panel`
| Property | Type | Default | Description |
|---|---|---|---|
| `Title` | `string?` | `null` | Panel title |
| `Border` | `PanelBorder` | `Single` | Border style |
| `Draggable` | `bool` | `true` | Can be dragged by title bar |
| `IsDragging` | `bool` | `false` | Drag state |
| `DragOffsetX/Y` | `int` | `0` | Mouse offset during drag |
| Method | Returns | Description |
|---|---|---|
| `WithTitle(string?)` | `Panel` | Set title |
| `WithBorder(PanelBorder)` | `Panel` | Set border style |
| Property | Description |
|---|---|
| `InnerArea` | `ScreenRect` inside the border |

var panel = new Panel { X = 2, Y = 2, Width = 40, Height = 12 }
    .WithTitle("Settings Panel")
    .WithBorder(PanelBorder.Rounded);

// Use panel.InnerArea to layout child controls


### `Window`
| Property | Type | Default | Description |
|---|---|---|---|
| `Title` | `string` | `""` | Window title |
| `Border` | `PanelBorder` | `Single` | Border style |
| `Closable` | `bool` | `true` | Show close button (×) |
| `Draggable` | `bool` | `true` | Can be dragged by title bar |
| `IsDragging` | `bool` | `false` | Drag state |
| Method | Returns | Description |
|---|---|---|
| `WithTitle(string)` | `Window` | Set title |
| `WithBorder(PanelBorder)` | `Window` | Set border style |
| `WithClosable(bool)` | `Window` | Toggle close button |

var win = new Window { X = 5, Y = 3, Width = 50, Height = 20 }
    .WithTitle("Document Editor")
    .WithBorder(PanelBorder.Double);


### `ScrollView`
| Property | Type | Default | Description |
|---|---|---|---|
| `Content` | `List<string>` | `[]` | Content lines |
| `Offset` | `int` | `0` | Scroll offset |
| Method | Returns | Description |
|---|---|---|
| `WithContent(List<string>)` | `ScrollView` | Set content |
| Method | Description |
|---|---|
| `Push(string line)` | Add line to content |
| `ScrollFraction` | `double` — current scroll position (0–1) |
| Key | Action |
|---|---|
| `UP` / `DOWN` | Scroll |
| `PGUP` / `PGDN` | Page scroll |
| `HOME` / `END` | Top/bottom |

var sv = new ScrollView { X = 2, Y = 2, Width = 40, Height = 10 }
    .WithContent(File.ReadAllLines("log.txt").ToList());


### `LogView`

public class LogRecord
{
    public LogLevel Level { get; set; }
    public string Target { get; set; } = "";
    public string Message { get; set; } = "";
}

| Property | Type | Default | Description |
|---|---|---|---|
| `Records` | `List<LogRecord>` | `[]` | All records |
| `Capacity` | `int` | `1000` | Max records (oldest trimmed) |
| `MinLevel` | `LogLevel` | `Trace` | Minimum level to display |
| `Offset` | `int` | `0` | Scroll offset |
| Method | Returns | Description |
|---|---|---|
| `WithMinLevel(LogLevel)` | `LogView` | Set minimum level |
| `WithCapacity(int)` | `LogView` | Set capacity |
| Method | Description |
|---|---|
| `Push(LogLevel, target, message)` | Add log record |
| `ScrollFraction` | `double` — current scroll position |

var logview = new LogView { X = 2, Y = 2, Width = 60, Height = 15 }
    .WithMinLevel(LogLevel.Info)
    .WithCapacity(500);

logview.Push(LogLevel.Info, "System", "Application started");
logview.Push(LogLevel.Error, "Database", "Connection failed");


### `Dashboard`

public class Metric
{
    public string Label { get; set; } = "";
    public float Value { get; set; }
    public float Max { get; set; } = 100;
    public string Unit { get; set; } = "";
}

| Property | Type | Default | Description |
|---|---|---|---|
| `Metrics` | `List<Metric>` | `[]` | Metrics to display |
| `SelectedIndex` | `int` | `0` | Selected metric |
| `BarWidth` | `int` | `10` | Bar segment width |
| Method | Returns | Description |
|---|---|---|
| `WithMetrics(List<Metric>)` | `Dashboard` | Set metrics |

var dash = new Dashboard { X = 2, Y = 2, Width = 30, Height = 5 }
    .WithMetrics([
        new Metric { Label = "CPU", Value = 45, Max = 100, Unit = "%" },
        new Metric { Label = "RAM", Value = 6.2f, Max = 16, Unit = "GB" }
    ]);


### `ScrollBar`
| Property | Type | Default | Description |
|---|---|---|---|
| `Orientation` | `Orientation` | `Vertical` | `Vertical` or `Horizontal` |
| `Min` | `int` | `0` | Minimum position |
| `Max` | `int` | `100` | Maximum position |
| `Position` | `int` | `0` | Current position |
| `Page` | `int` | `10` | Page size (thumb proportion) |
| Method | Returns | Description |
|---|---|---|
| `WithRange(int, int)` | `ScrollBar` | Set range |
| `WithPage(int)` | `ScrollBar` | Set page size |
| `WithPosition(int)` | `ScrollBar` | Set position (clamped) |

var scrollbar = new ScrollBar { X = 40, Y = 2, Width = 1, Height = 10 }
    .WithRange(0, 200)
    .WithPage(10)
    .WithPosition(50);


## Dialog Controls

### `AlertBox`
| Property | Type | Default | Description |
|---|---|---|---|
| `Message` | `string` | `""` | Alert message |
| `Title` | `string` | `""` | Dialog title |
| `OKButton` | `Button` | `"OK"` | OK button |
| `Border` | `PanelBorder` | `Single` | Border style |
| `Draggable` | `bool` | `true` | Drag support |
| `IsDragging` | `bool` | `false` | Drag state |
| Method | Description |
|---|---|
| `Centered(termW, termH, title, message)` | Create centered alert box |
| Key | Action |
|---|---|
| `ENTER` / `ESC` / `SPACE` | Dismiss |

var alert = AlertBox.Centered(80, 24, "Notice", "Operation completed successfully.");


### `ConfirmationBox`
| Property | Type | Default | Description |
|---|---|---|---|
| `Message` | `string` | `""` | Confirmation message |
| `Title` | `string?` | `null` | Dialog title |
| `Border` | `PanelBorder` | `Double` | Border style |
| `ConfirmButton` | `Button` | `"OK"` | Confirm button |
| `CancelButton` | `Button` | `"Cancel"` | Cancel button |
| `RetryButton` | `Button?` | `null` | Optional retry button |
| `FocusedChoice` | `ConfirmationChoice` | `Confirm` | Currently focused button |
| `Draggable` | `bool` | `true` | Drag support |
| Method | Returns | Description |
|---|---|---|
| `WithTitle(string?)` | `ConfirmationBox` | Set title |
| `WithMessage(string)` | `ConfirmationBox` | Set message |
| `WithButtons(confirm, cancel, retry?)` | `ConfirmationBox` | Set button labels |
| Method | Description |
|---|---|
| `CenteredConfirmCancel(w, h, title, msg)` | Full-params centered |
| `CenteredConfirmCancel(termW, termH, title, msg)` | Terminal-aware centered |
| Key | Action |
|---|---|
| `TAB` / `LEFT` / `RIGHT` | Cycle buttons |
| `ENTER` | Activate focused button |

var confirm = ConfirmationBox.CenteredConfirmCancel(80, 24, "Save?", "Save changes before closing?");


### `FileDialog`

public class FileEntry
{
    public string Name { get; set; } = "";
    public string FullPath { get; set; } = "";
    public bool IsDir { get; set; }
}

| Property | Type | Default | Description |
|---|---|---|---|
| `Entries` | `List<FileEntry>` | `[]` | File entries |
| `SelectedIndex` | `int` | `0` | Selected index |
| `ScrollOffset` | `int` | `0` | Scroll offset |
| Method | Returns | Description |
|---|---|---|
| `WithEntries(List<FileEntry>)` | `FileDialog` | Set entries |

var fd = new FileDialog { X = 10, Y = 5, Width = 40 }
    .WithEntries([
        new FileEntry { Name = "Documents", FullPath = "/home/user/docs", IsDir = true },
        new FileEntry { Name = "readme.txt", FullPath = "/home/user/readme.txt", IsDir = false }
    ]);


### `FileExplorer`
| Property | Type | Default | Description |
|---|---|---|---|
| `CurrentPath` | `string` | `""` | Current directory |
| `ShowHidden` | `bool` | `false` | Show hidden files |
| `ShowIcons` | `bool` | `true` | Show file type icons |
| `LastAction` | `FileExplorerAction` | `None` | Last user action |
| `SelectedPath` | `string?` | `null` | Selected file path |
| Method | Returns | Description |
|---|---|---|

### ContextMenu-Controllo-Guida-Utente

*Fonte: `ContextMenu-Controllo-Guida-Utente`*


## Sintassi

IMPORT CONTROLS
IMPORT CustomControls

CLASS ContextMenu INHERITS Control
  SUB Configure(width AS INTEGER)
  SUB SetTheme(bg, fg, selBg, selFg, dis, borderCol)   ' DOUBLE
  SUB SetItem(txt AS STRING)
  SUB SetSeparator(txt AS STRING)
  SUB SetEnabled(i AS INTEGER, e AS INTEGER)
  FUNCTION Count() AS INTEGER
  FUNCTION ItemText(i AS INTEGER) AS STRING
  FUNCTION IsOpen() AS INTEGER
  FUNCTION Sel() AS INTEGER
  FUNCTION SelText() AS STRING
  FUNCTION Chosen() AS INTEGER          ' indice scelto (-1 nessuno)
  FUNCTION ChosenText() AS STRING


## Modello

## Esempio

IMPORT CONTROLS
IMPORT CustomControls

SUB Main()
  DIM m AS ContextMenu = NEW ContextMenu()
  CONTROL ADD m AT 1, 1 SIZE 24, 12
  m.Configure(18)
  m.SetItem("Nuovo")
  m.SetItem("Apri")
  m.SetSeparator("---")
  m.SetItem("Esporta")
  m.SetItem("Elimina")
  m.SetEnabled(4, 0)                 ' "Elimina" disabilitata
  m.Popup(2, 2)
  m.MoveSel(1)                       ' -> "Apri"
  PRINT "sel="; m.SelText()


## Disponibilità sui 4 backend
| Backend | Esito |
|---|---|
| IL | ✅ |
| PCode (VM) | ✅ |
| VM/C | ✅ |
| C99Native | ✅ |

## Riferimenti

### Controlli-Utente-Base-Control

*Fonte: `Controlli-Utente-Base-Control`*


## Sintassi

CLASS <nome> INHERITS Control
  SUB Paint(g AS ANY)
    ' ...
  END SUB
END CLASS


## Stato

### Esempio (proprietà)

CLASS Panel INHERITS Control
  SUB Paint(g AS ANY)
  END SUB
END CLASS

SUB Main()
  DIM p AS Panel = NEW Panel()
  CONTROL ADD p AT 2, 2 SIZE 40, 12
  PRINT p.X; p.Y; p.Width; p.Height   ' lettura (CONTROL GET)
  p.X = 5                              ' scrittura (CONTROL SET)
END SUB


### Esempio (ForeColor / BackColor)

CLASS Panel INHERITS Control
  SUB Paint(g AS ANY)
  END SUB
END CLASS

SUB Main()
  DIM p AS Panel = NEW Panel()
  CONTROL ADD p AT 2, 2 SIZE 40, 12
  p.ForeColor = "#E0E0E0"          ' colore primo piano (hex o Clipper "W+/B")
  p.BackColor = "#101820"          ' sfondo
  PRINT p.ForeColor; p.BackColor   ' "#E0E0E0"  "#101820"
END SUB


### Proprietà comuni della base `Control`
| Proprietà | Tipo | Valori / parametri | Note |
|---|---|---|---|
| `X`, `Y` | numerico | coordinate | `Left`/`Top` |
| `Width`, `Height` | numerico | dimensioni | |
| `Visible`, `Enabled` | 0/1 | mostra/nasconde, abilita/disabilita | |
| `TabIndex` | numerico | ordine Tab | |
| `TabStop` | 0/1 | incluso nel ciclo Tab | |
| `Name` | stringa | nome del controllo | |
| `Tag` | stringa | dato applicativo | |
| `ToolTipText` | stringa | tooltip | alias `Tooltip` |
| `FontName` | stringa | famiglia/carattere | superficie canonica |
| `FontSize` | numerico | punti | |
| `ForeColor`, `BackColor` | stringa | `#RRGGBB` o colore Clipper | |
| `Appearance` | numerico | `0` = Flat, `1` = 3D | |
| `MousePointer` | numerico | codice cursore VB6 | |
| `RightToLeft` | 0/1 | direzione testo | |
| `Locked` | 0/1 | controllo bloccato | |
| `StartUpPosition` | numerico | `0` Manual, `1` Center Owner, `2` Center Screen, `3` Windows Default | |
| `ShowInTaskbar` | 0/1 | visibilità nella taskbar | |
| `WhatsThisHelpID` | numerico | ID guida contestuale | |
| `WordWrap` | 0/1 | a capo automatico | |
| `AutoSize` | 0/1 | ridimensiona al contenuto | |
| `BackStyle` | numerico | `0` Transparent, `1` Opaque | |
| `BorderStyle` | numerico | `0` None, `1` Fixed Single | |
| `UseMnemonic` | 0/1 | interpreta `&` nel testo | |
| `CausesValidation` | 0/1 | valida al cambio fuoco | |

CLASS Panel INHERITS Control
  SUB Paint(g AS ANY)
    CONTROL DRAW g, 0
  END SUB
END CLASS

SUB Main()
  DIM p AS Panel = NEW Panel()
  CONTROL ADD p AT 2, 2 SIZE 40, 12
  p.FontName = "Arial"
  p.FontSize = 10
  p.Appearance = 1
  p.StartUpPosition = 2
  PRINT p.FontName; " "; p.FontSize; " "; p.Appearance; " "; p.StartUpPosition
  CONTROL REMOVE p
END SUB


### ListBox / ComboBox: `List`, `ListIndex`, `ListCount`, `Items`

LISTBOX 2, 2, 20, 5, "A|B|C" INTO sel ID h NAME "L1"
CONTROL SET "L1", "items", 0, "X|Y|Z"
CONTROL GET "L1", "listcount" INTO n
CONTROL GETS "L1", "items" INTO items
CONTROL SET "L1", "listindex", 1
CONTROL GET "L1", "listindex" INTO sel


### Esempio (eventi)

CLASS Panel INHERITS Control
  SUB Change()
    ticks = ticks + 1     ' invocato quando l'evento CHANGE del controllo scatta
  END SUB
  SUB Paint(g AS ANY)
  END SUB
END CLASS

SUB Main()
  DIM p AS Panel = NEW Panel()
  CONTROL ADD p AT 2, 2 SIZE 40, 12 INTO h
  FIRE EVENT h, "CHANGE"   ' → Panel.Change come task GPM
END SUB


### Esempio (zucchero metodi)

CLASS Panel INHERITS Control
  SUB Paint(g AS ANY)
    CONTROL DRAW g, 0
  END SUB
END CLASS

SUB Main()
  DIM p AS Panel = NEW Panel()
  CONTROL ADD p AT 2, 2 SIZE 40, 12
  p.Move(5, 5)        ' ≡ CONTROL MOVE "p", 5, 5
  p.SetFocus()        ' ≡ CONTROL FOCUS "p"
  p.ZOrder(1)         ' ≡ CONTROL ZORDER "p", 1
  p.Invalidate()      ' marca da ridisegnare
  p.Refresh()         ' repaint
END SUB


## Disponibilità sui 4 backend del protocollo 4.1
| Backend | Stato |
|---|---|
| IL | ✅ |
| PCodeVM | ✅ |
| VM/C | ✅ |
| C99Native | ✅ |

## Riferimenti

### Controlli-Utente-Composizione

*Fonte: `Controlli-Utente-Composizione`*


## Sintassi

CONTROL ADD figlio AT x, y SIZE w, h PARENT padre


## Semantica

## Esempio

IMPORT CONTROLS

CLASS Panel
  SUB Paint(g AS ANY)
    CONTROL DRAW g, 0                 ' clear
    CONTROL DRAW g, 2, 0, 0, 40, 40   ' diagonale locale
  END SUB
END CLASS

SUB Main()
  DIM outer AS Panel = NEW Panel()
  DIM inner AS Panel = NEW Panel()
  CONTROL ADD outer AT 4, 2 SIZE 60, 20
  CONTROL ADD inner AT 2, 2 SIZE 30, 10 PARENT outer   ' coordinate relative a outer
END SUB


## Disponibilità sui 4 backend
| Backend | Stato |
|---|---|
| IL | ✅ |
| PCodeVM | ✅ |
| VM/C | ✅ |
| C99Native | ✅ |

## Riferimenti

### Controlli-Utente-Struttura

*Fonte: `Controlli-Utente-Struttura`*


## 1. Due mondi distinti
| | Namespace | Dove vive | Cos'è |
|---|---|---|---|
| **Built-in** | `CONTROLS` | `GMBasic/native/gmb_runtime_c` (runtime C condiviso) + `StdLib.Controls.cs` | controlli nativi (CHECKBOX, LISTBOX, TBROWSE, ALERTBOX, …): verbi/TokenKind del linguaggio, 5 emitter |
| **BASIC (libreria)** | **`CustomControls`** | **`shared/Controls/`** (sorgenti `.bas`) | controlli scritti interamente in BASIC come `PUBLIC CLASS … INHERITS Control` + `Paint`: nessun verbo/TokenKind nuovo, parità 4 backend **per costruzione** |

## 2. Layout

GMBasic/
  Controls/                       ← LIBRERIA (solo controlli)
    README.md
    CandleChart.bas               ← PUBLIC CLASS CandleChart INHERITS Control  (NAMESPACE CustomControls)
    Depth.bas                     ← MarketDepth / DOM
    OrderTicket.bas
    Watchlist.bas
    Splitter.bas
    ContextMenu.bas
    Segmented.bas
    DataGrid.bas                  ← + PUBLIC CLASS PnlGrid INHERITS DataGrid
    TradingMinors.bas             ← Ticker / TreeGroup / EconCalendar / Toolbar
  shared/samples/Controls/     ← ESEMPI (demo d'applicazione, importano CustomControls)
    CandleChartDemo.bas
    DepthDemo.bas
    OrderTicketDemo.bas


## 3. Risoluzione di `Controls/` (come `include/`)

## 4. Uso

IMPORT CONTROLS         ' base Control + primitive GraphicsContext
IMPORT CustomControls   ' i controlli di shared/Controls/

SUB Main()
  DIM s AS Segmented = NEW Segmented()
  CONTROL ADD s AT 2, 2 SIZE 40, 3
  s.SetOrigin(2, 2, 40, 3)
  s.Configure(0)
  s.AddItem("1m") : s.AddItem("5m") : s.AddItem("15m") : s.AddItem("1h")
  s.SetSelected(0)
  PRINT s.SelectedText()
END SUB


## 5. Controlli della libreria
| File (`Controls/`) | Classe | Dominio |
|---|---|---|
| `CandleChart.bas` | `CandleChart` | chart a candele interattivo (zoom/pan/crosshair/disegni/overlay) |
| `Depth.bas` | `MarketDepth` | price ladder / DOM (best bid/ask, click-to-trade, SL/TP) |
| `OrderTicket.bas` | `OrderTicket` | order ticket (BUY/SELL, qty, price) |
| `Watchlist.bas` | `Watchlist` | tabella simboli/prezzi (tick, variazione) |
| `Splitter.bas` | `Splitter` | divisore a due pannelli trascinabile |
| `ContextMenu.bas` | `ContextMenu` | menu contestuale (popup chiuso di default) |
| `Segmented.bas` | `Segmented` | segmented / toggle-group (timeframe) |
| `DataGrid.bas` | `DataGrid`, `PnlGrid` | grid personalizzabile (colonne/filtro/sort/edit) |
| `TradingMinors.bas` | `Ticker`, `TreeGroup`, `EconCalendar`, `Toolbar` | controlli minori di trading |

## 6. Aggiungere un controllo nuovo

## 7. Riferimenti

### Controlli-Utente-UserControl

*Fonte: `Controlli-Utente-UserControl`*


## Perché

## Sintassi

IMPORT CONTROLS

CLASS MioControllo
  SUB Paint(g AS ANY)
    ' disegno del controllo (coordinate locali 0..Width / 0..Height)
  END SUB
END CLASS

SUB Main()
  DIM c AS MioControllo = NEW MioControllo()
  CONTROL ADD c AT x, y SIZE w, h              ' contenitore = window corrente
  CONTROL ADD c AT x, y SIZE w, h PARENT "W"   ' contenitore = window "W"
  ...
  CONTROL REMOVE c                              ' Unload / deregistra
END SUB


## Metodo `Paint`
| Metodo | Effetto |
|---|---|
| `g.Clear` | riempie il controllo con il colore di sfondo |
| `g.SetColor rgb` | imposta il colore corrente (ARGB/indice) |
| `g.Line x1,y1,x2,y2` | linea |
| `g.Rect x,y,w,h` | rettangolo (bordo) |
| `g.FillRect x,y,w,h` | rettangolo pieno |
| `g.Circle cx,cy,r` | cerchio (bordo) |
| `g.FillCircle cx,cy,r` | cerchio pieno |
| `g.Text s,x,y` | testo |
| `g.Plot x,y` | punto |

## Rendering TEXT vs GRAPHICS

## Stato

## Esempi

### Controls-CHART-Guida-Utente

*Fonte: `Controls-CHART-Guida-Utente`*


## Sintassi

CHART x, y, w, h [, "serie"]

| Parametro | Tipo | Descrizione |
|---|---|---|
| `x`, `y` | intero | posizione (celle) del controllo nella superficie |
| `w`, `h` | intero | larghezza/altezza del controllo (celle) |
| `serie` | stringa (opz.) | dati della serie (vedi sotto). Se omessa/vuota → **sinusoide demo** |

## Disambiguazione (importante)

CHART SET/GET/GETS/PLOT/FINANCIAL/DRAW/SHOW/SAVE/CLEAR/CLOSE ...  → COMANDO (handle)
CHART x, y, w, h [, "serie"]                                      → CONTROLLO a celle


## Requisiti

## Esempi

IMPORT CHART
SUB Main()
  ' controllo con valori (x = indice)
  CHART 2, 2, 60, 14, "10;20;15;30;25;40;35;50;45;60"
  ' controllo con coppie x,y
  CHART 2, 18, 60, 14, "1,5;2,9;3,7;4,12;5,10;6,15"
  ' controllo demo (csv vuoto)
  CHART 2, 34, 60, 14
END SUB


## Disponibilità sui 4 backend
| Backend | Stato |
|---|---|
| IL | ✅ |
| PCodeVM | ✅ |
| VM/C | ✅ |
| C99Native | ✅ |

## Polimorfismo TEXT / GRAPHICS

## Riferimenti

### DataGrid-Controllo-Guida-Utente

*Fonte: `DataGrid-Controllo-Guida-Utente`*


## Funzioni

## Personalizzazione

### 1) Colori per colonna (configurazione, senza sottoclasse)

g.SetColumnColors(1, 16776960, -1)   ' colonna 1: foreground giallo, sfondo invariato (-1)


### 2) Hook override-abili (sottoclasse)

CLASS PnlGrid INHERITS DataGrid
  ' Il valore della cella è PASSATO come parametro (value) così l'override non deve
  ' chiamare metodi della base (l'emitter IL non consente, da un metodo di sottoclasse,
  ' di invocare metodi della classe base).
  PUBLIC OVERRIDE FUNCTION CellFg(vr AS INTEGER, c AS INTEGER, value AS STRING) AS DOUBLE
    IF c = 2 THEN
      IF VAL(value) < 0 THEN RETURN 16711680    ' rosso per i negativi
      RETURN 65280
    END IF
    RETURN 16777215
  END FUNCTION
  PUBLIC OVERRIDE FUNCTION CellText(vr AS INTEGER, c AS INTEGER, value AS STRING) AS STRING
    RETURN value
  END FUNCTION
  PUBLIC OVERRIDE FUNCTION CellBg(vr AS INTEGER, c AS INTEGER, value AS STRING) AS DOUBLE
    RETURN -1


## Esempio

IMPORT CONTROLS
IMPORT CustomControls

SUB Main()
  DIM g AS DataGrid = NEW DataGrid()
  CONTROL ADD g AT 1, 1 SIZE 46, 12
  g.Configure(3, 3)
  g.SetColumn(0, "Sym", 6, 0, 0, 0)
  g.SetColumn(1, "Val", 6, 2, 2, 1)
  g.SetColumn(2, "Qty", 5, 2, 1, 0)
  g.SetColumnColors(1, 16776960, -1)
  g.SetCell(0, 0, "b") : g.SetCell(0, 1, "3")
  g.SetCell(1, 0, "a") : g.SetCell(1, 1, "1")
  g.SetCell(2, 0, "c") : g.SetCell(2, 1, "2")
  g.SortBy(1, 1)                 ' Val decrescente
  PRINT g.GetCell(0, 0)          ' b


## Disponibilità sui 4 backend
| Backend | Esito |
|---|---|
| IL | ✅ |
| PCode (VM) | ✅ |
| VM/C | ✅ |
| C99Native | ✅ |

## Note di implementazione (2026-10-08)

## Riferimenti

### MarketDepth-Controllo-Guida-Utente

*Fonte: `MarketDepth-Controllo-Guida-Utente`*


## Sintassi

IMPORT CONTROLS
IMPORT CustomControls

CLASS MarketDepth INHERITS Control
  ' stato privato: righe del ladder, top price, step, best bid/ask,
  ' selezione, lato, ultimo prezzo, stop/target.
  SUB Configure(rows AS INTEGER, topPx AS DOUBLE, step AS DOUBLE)
  SUB SetQuote(bid AS DOUBLE, ask AS DOUBLE)
  SUB DragStop(price AS DOUBLE)
  SUB DragTarget(price AS DOUBLE)
  FUNCTION Rows() AS INTEGER
  FUNCTION PriceAt(r AS INTEGER) AS DOUBLE
  FUNCTION RowAt(price AS DOUBLE) AS INTEGER
  FUNCTION Hit(lx AS INTEGER, ly AS INTEGER) AS INTEGER
  FUNCTION SelectedRow() AS INTEGER
  FUNCTION Side() AS INTEGER


## Modello

## Eventi

SUB MouseDown()
  DIM lx AS INTEGER, ly AS INTEGER
  lx = MOUSE.X() - mOriginX
  ly = MOUSE.Y() - mOriginY
  DIM r AS INTEGER
  r = Hit(lx, ly)        ' click-to-trade sull'riga sotto il mouse
END SUB


## Esempio

IMPORT CONTROLS
IMPORT CustomControls

SUB Main()
  DIM d AS MarketDepth = NEW MarketDepth()
  CONTROL ADD d AT 2, 2 SIZE 30, 12
  d.Configure(10, 1010.0, 1.0)      ' 10 righe: 1010, 1009, …
  d.SetQuote(1003.0, 1004.0)        ' best bid / best ask
  d.Hit(5, 6)                       ' riga price 1004 = ask → BUY
  PRINT "side="; d.Side(); " px="; d.LastPrice()
  d.DragStop(996.0)
  d.DragTarget(1012.0)
  PRINT "sl="; d.Stop(); " tp="; d.Target()
END SUB


## Paint (zucchero GraphicsContext)

## Disponibilità sui 4 backend
| Backend | Esito |
|---|---|
| IL | ✅ |
| PCode (VM) | ✅ |
| VM/C | ✅ |
| C99Native | ✅ |

## Riferimenti

### OrderTicket-Controllo-Guida-Utente

*Fonte: `OrderTicket-Controllo-Guida-Utente`*


## Import

IMPORT CONTROLS         ' base Control + primitive GraphicsContext
IMPORT CustomControls   ' la libreria dei controlli BASIC (Controls/)


## Funzioni

## Esempio

IMPORT CONTROLS
IMPORT CustomControls

SUB Main()
  DIM t AS OrderTicket = NEW OrderTicket()
  CONTROL ADD t AT 2, 2 SIZE 38, 12
  t.SetOrigin(2, 2)
  t.SetQty(5)
  t.SetPrice(123.5)
  PRINT t.Hit(6, 9)          ' click su BUY  -> 0
  PRINT t.Side()             ' 0 (BUY)
  PRINT t.Hit(20, 9)         ' click su SELL -> 1
  PRINT t.Side()             ' 1 (SELL)
END SUB


## Disponibilità sui 4 backend
| Backend | Esito |
|---|---|
| IL | ✅ |
| PCode (VM) | ✅ |
| VM/C | ✅ |
| C99Native | ✅ |

## Riferimenti

### PANEL-Guida-Utente

*Fonte: `PANEL-Guida-Utente`*


## Sintassi (comando — senza parentesi)

PANEL x, y, w, h [, "Titolo"]

| parametro | significato |
|---|---|
| `x`, `y` | posizione in **pixel** su schermo grafico, in **celle** in TUI/SDLConsole (dipende dal backend) |
| `w`, `h` | larghezza e altezza nella stessa unità |
| `"Titolo"` | testo del titolo, disegnato in alto a sinistra nel colore secondario del tema (opzionale) |

## Semantica

## Esempio

IMPORT CONTROLS
IMPORT GRAPHICS
SCREEN 12, 800, 500, 32
WINDOW THEME "soft"
PANEL 32, 32, 320, 200, "MONITOR"
PROGRESS 48, 72, 288, 40
SLIDER   48, 128, 288, 0, 100, 40 INTO v
PANEL 368, 32, 400, 200, "ACTIVITY"
LOGVIEW 384, 72, 368, 144, "started|ready"


## Parità 4 backend
| backend | stato |
|---|---|
| IL | ✅ |
| PCodeVM | ✅ |
| VM/C | ✅ |
| C99Native | ✅ |

## Nota su `STYLE "primary"`

### SPLITTER-CONTEXTMENU-Builtin-Guida-Utente

*Fonte: `SPLITTER-CONTEXTMENU-Builtin-Guida-Utente`*


## Sintassi (comandi, senza parentesi)

SPLITTER <row>, <col>, <width>, <height>, <orient> INTO <var>
CONTEXTMENU <row>, <col>, <width>, <height>, <items()> INTO <var>


## Esempio

IMPORT CONTROLS
SCREEN 0

SUB Main()
  DIM sp AS INTEGER
  SPLITTER 2, 2, 40, 10, 0 INTO sp          ' verticale

  DIM cm AS INTEGER
  CONTEXTMENU 2, 13, 20, 6, ["Nuovo", "Apri", "--", "Esporta"] INTO cm

  PRINT "ok"
END SUB


## Proprietà (`CONTROL <name> SET/GET`)

## Disponibilità sui 4 backend
| Backend | Esito |
|---|---|
| IL | ✅ |
| PCode (VM) | ✅ |
| VM/C | ✅ |
| C99Native | ✅ |

## Implementazione

## `INTO` = handle (fix 2026-10-09)

## Riferimenti

### Segmented-Controllo-Guida-Utente

*Fonte: `Segmented-Controllo-Guida-Utente`*


## Funzioni

## Esempio

IMPORT CONTROLS
IMPORT CustomControls

SUB Main()
  DIM s AS Segmented = NEW Segmented()
  CONTROL ADD s AT 2, 2 SIZE 40, 3
  s.SetOrigin(2, 2, 40, 3)
  s.Configure(0)
  s.AddItem("1m") : s.AddItem("5m") : s.AddItem("15m") : s.AddItem("1h")
  s.SetSelected(0)
  s.Hit(25, 1)                       ' click sul 3° segmento
  PRINT s.SelectedText()             ' "15m"
END SUB


## Disponibilità sui 4 backend
| Backend | Esito |
|---|---|
| IL | ✅ |
| PCode (VM) | ✅ |
| VM/C | ✅ |
| C99Native | ✅ |

## Note di implementazione (2026-10-09)

## Riferimenti

### Splitter-Controllo-Guida-Utente

*Fonte: `Splitter-Controllo-Guida-Utente`*


## Sintassi

IMPORT CONTROLS
IMPORT CustomControls

CLASS Splitter INHERITS Control
  SUB Configure(orient AS INTEGER, total AS INTEGER, thick AS INTEGER, cross AS INTEGER)
  SUB SetTheme(bgA AS DOUBLE, bgB AS DOUBLE, divCol AS DOUBLE, divHot AS DOUBLE, borderCol AS DOUBLE)
  SUB SetPos(p AS INTEGER)          ' sposta la barra (con clamp ai minimi)
  FUNCTION Pos() AS INTEGER
  FUNCTION Thick() AS INTEGER
  FUNCTION Orient() AS INTEGER      ' 0 = verticale (A|B), 1 = orizzontale (A/B)
  FUNCTION Dragging() AS INTEGER
  FUNCTION PaneAX() AS INTEGER      ' rettangolo pannello A (celle locali)
  FUNCTION PaneAY() AS INTEGER
  FUNCTION PaneAW() AS INTEGER
  FUNCTION PaneAH() AS INTEGER
  FUNCTION PaneBX() AS INTEGER      ' rettangolo pannello B


## Modello

## Esempio

IMPORT CONTROLS
IMPORT CustomControls

SUB Main()
  DIM s AS Splitter = NEW Splitter()
  CONTROL ADD s AT 1, 1 SIZE 40, 10
  s.Configure(0, 40, 1, 10)     ' verticale, 40 largo, barra 1, alto 10
  s.SetPos(15)                  ' barra a x=15
  PRINT "A w="; s.PaneAW(); " B x="; s.PaneBX()
  s.MousePress(15, 5)           ' preme sulla barra
  s.MouseDrag(25, 5)            ' trascina
  s.MouseRelease()
  PRINT "pos="; s.Pos()
END SUB


## Disponibilità sui 4 backend
| Backend | Esito |
|---|---|
| IL | ✅ |
| PCode (VM) | ✅ |
| VM/C | ✅ |
| C99Native | ✅ |

## Note

## Riferimenti

### Trading-Minori-Controlli-Guida-Utente

*Fonte: `Trading-Minori-Controlli-Guida-Utente`*


## Ticker (flash/toast)

## TreeGroup (Tree-grid posizioni)

## EconCalendar (calendario economico)

## Toolbar (icone)

## Disponibilità sui 4 backend
| Backend | Esito |
|---|---|
| IL | ✅ |
| PCode (VM) | ✅ |
| VM/C | ✅ |
| C99Native | ✅ |

## Riferimenti

### Watchlist-Controllo-Guida-Utente

*Fonte: `Watchlist-Controllo-Guida-Utente`*


## Sintassi

IMPORT CONTROLS
IMPORT CustomControls

CLASS Watchlist INHERITS Control
  SUB Append(sym AS STRING, px AS DOUBLE)      ' aggiunge una riga
  FUNCTION Count() AS INTEGER
  FUNCTION SymbolOf(i AS INTEGER) AS STRING
  FUNCTION PriceOf(i AS INTEGER) AS DOUBLE
  FUNCTION ChangePct(i AS INTEGER) AS DOUBLE   ' variazione % vs prezzo precedente
  FUNCTION IndexOf(sym AS STRING) AS INTEGER   ' -1 se assente
  SUB ApplyTick(sym AS STRING, px AS DOUBLE)   ' tick di mercato
  SUB RemoveRow(i AS INTEGER)                   ' rimuove una riga
  FUNCTION Hit(lx AS INTEGER, ly AS INTEGER) AS INTEGER
  FUNCTION SelectedRow() AS INTEGER
  SUB Paint(g AS ANY)
END CLASS


## Modello

## Eventi

SUB MouseDown()
  DIM lx AS INTEGER, ly AS INTEGER
  lx = MOUSE.X() - mOriginX
  ly = MOUSE.Y() - mOriginY
  DIM r AS INTEGER
  r = Hit(lx, ly)
END SUB


## Esempio

IMPORT CONTROLS
IMPORT CustomControls

SUB Main()
  DIM w AS Watchlist = NEW Watchlist()
  CONTROL ADD w AT 2, 2 SIZE 30, 12
  w.Append("AAPL", 100.0)
  w.Append("MSFT", 250.0)
  w.ApplyTick("AAPL", 102.0)     ' +2.0%
  w.ApplyTick("MSFT", 245.0)     ' -2.0%
  PRINT "n="; w.Count()
  PRINT "AAPL chg%="; w.ChangePct(0)
  PRINT "sel="; w.Hit(5, 1); "/"; w.SelectedRow()
END SUB


## Paint (zucchero GraphicsContext)

## Ordinamento (rimandato)

## Disponibilità sui 4 backend
| Backend | Esito |
|---|---|
| IL | ✅ |
| PCode (VM) | ✅ |
| VM/C | ✅ |
| C99Native | ✅ |

## Riferimenti

### Clipper-Controls-Reference

*Fonte: `Clipper-Controls-Reference`*


## Standalone Controls (comando indipendente)

### ALERT / CONFIRM

ALERT "Operazione completata"
CONFIRM "Vuoi uscire?" TO risposta    ' → BOOLEAN


### CHECKBOX

CHECKBOX var, row, col, "caption"     ' → BOOLEAN


### LISTBOX

LISTBOX var, row, col, w, h, items()  ' → STRING (label selezionata)


### COMBOBOX

COMBOBOX var, row, col, w, items()    ' → STRING


### BUTTON

BUTTON row, col, "caption" CALL sub   ' esegue sub solo su click


### MENU (flat + nested + separatori)

' Flat
MENU var, row, col, ["Alpha", "Beta"]       ' → INTEGER (indice)

' Nested con separatori
MENU var, row, col, [["File", "Open", "Save", "---", "Exit"], ["Edit", "Cut", "Copy"], "Help"]
                                             ' → STRING (label)


### STATUSBAR

STATUSBAR " Ready | F1=Help | F10=Quit"    ' display only, fondo schermo


### FILEDIALOG

FILEDIALOG var, "*.bas", "Apri file"        ' → STRING (path) o ""


### PRINTERDIALOG

PRINTERDIALOG printer, copies, duplex, "Setup"
    ' printer → STRING, copies → INTEGER, duplex → BOOLEAN


### TOGGLE

TOGGLE var, row, col, "caption"            ' → BOOLEAN


### RADIO

RADIO var, row, col, items()          ' → STRING (label)


### SLIDER

SLIDER var, row, col, len, min, max, step  ' → INTEGER


### TAB

TAB var, row, col, w, h, tabs()            ' → INTEGER (indice)


### GRID

GRID var, row, col, w, h, headers(), data() ' → INTEGER (cell index)


### DATEPICKER

DATEPICKER var, row, col                  ' → STRING "YYYY-MM-DD"


### COLORPICKER

COLORPICKER var, row, col, cols           ' → INTEGER (0-255)


### TREEVIEW

TREEVIEW var, row, col, w, h, tree()      ' → STRING (label)
' tree() usa indentazione per depth, "+" suffix = hasChildren


### SPINNER

SPINNER row, col, "preset"                ' display only
' preset: line, dots, bar, braille, boxes, circle, clock


### TBROWSE (griglia dati Clipper 5.3)

TBROWSE var, row, col, w, h, columns(), data() [, options()]

' columns: [[header, width, type, align, editable, visible], ...]
' options: [[key, val], ...]  — freeze, footer, colors, border, header, highlight, mouse, autolite, colsep, headsep, footsep
' Tasti: F5=sort, /=search, Enter=select/edit, Esc=cancel
' → INTEGER (row index, -1 cancel)


## @...GET TYPE — Form Controls inline

@ row, col GET var TYPE "N" MIN 0 MAX 100 STEP 1    ' numeric spinner
@ row, col GET var TYPE "D" CAPTION "Data:"          ' datepicker inline
@ row, col GET var TYPE "K" CAPTION "Attivo:"        ' checkbox
@ row, col GET var TYPE "R" CAPTION "Opzione:"       ' radio button
@ row, col GET var TYPE "C" ITEMS ["A","B","C"]      ' combobox
@ row, col GET var TYPE "L" ITEMS ["X","Y","Z"]      ' listbox
@ row, col GET var TYPE "P"                          ' progress bar
@ row, col GET var TYPE "M"                          ' memo (multiline)
@ row, col GET var                                   ' text input (default)


### Alias corti
| Alias | Tipo | FormControl | Ritorno |
|-------|------|-------------|---------|
| `"N"` `"spinner"` `"numeric"` | Numeric | FormNumeric | DOUBLE |
| `"C"` `"combobox"` | Combo | FormComboBox | STRING |
| `"L"` `"listbox"` | List | FormListBox | STRING |
| `"K"` `"checkbox"` | Check | FormCheckBox | BOOLEAN |
| `"R"` `"radio"` | Radio | FormRadioButton | BOOLEAN |
| `"D"` `"datepicker"` | Date | FormDatePicker | STRING |
| `"P"` `"progressbar"` | Progress | FormProgressBar | INTEGER |
| `"M"` `"memo"` | Memo | FormMemo | STRING |
| default | Text | FormInput | STRING |

## WINDOW Management

WINDOW.DEFINE("nome", top, left, bottom, right)
WINDOW.DEFINE("nome", top, left, bottom, right, "Titolo")
WINDOW.DEFINE("nome", top, left, bottom, right, "Titolo", TRUE)  ' double border
WINDOW.ACTIVATE("nome")
WINDOW.DEACTIVATE("nome")
WINDOW.CLOSE("nome")
WINDOW.MINIMIZE("nome")
WINDOW.RESTORE("nome")
WINDOW.RELEASE("nome")


## Events

' ON WINDOW (9 eventi)
ON WINDOW CLOSE GOSUB Handler
ON WINDOW RESIZE GOSUB Handler
ON WINDOW QUIT GOSUB Handler
ON WINDOW MOVE GOSUB Handler
ON WINDOW FOCUS GOSUB Handler
ON WINDOW MINIMIZE GOSUB Handler
ON WINDOW RESTORE GOSUB Handler
ON WINDOW OPEN GOSUB Handler
ON WINDOW PASTE GOSUB Handler

' ON MOUSE (6 eventi)
ON MOUSE CLICK GOSUB Handler
ON MOUSE DBLCLICK GOSUB Handler
ON MOUSE MOVE CALL Handler
ON MOUSE DOWN GOSUB Handler


## @...SAY / @...GET / BOX / SCROLL

@ row, col SAY expr
@ row, col SAY expr PICTURE "999.99"
@ row1, col1, row2, col2 BOX
@ row1, col1, row2, col2 BOX SINGLE
@ row1, col1, row2, col2 BOX DOUBLE
SCROLL top, left, bottom, right, n
CLEAR
CLEAR top, left, bottom, right


## Gates

<!-- END SURFACE -->
