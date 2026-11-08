# IV.19 — Finestre ed eventi

## Cosa ottieni da questo capitolo

Una finestra che si apre, riceve eventi (tastiera, mouse, chiusura, trascinamento) e si chiude
senza bloccare il resto del programma. Il punto delicato non è disegnare: è **chi** esegue il
codice che reagisce a un evento.

## 1. Il ciclo minimo

```basic
SCREEN 12, 640, 320, 32

SUB Main()
    WINDOW THEME "light"
    WINDOW SHOW
    DO WHILE WINDOWCLOSED() = 0
        WINDOW PUMP 16
    LOOP
END SUB
```

- `SCREEN` sceglie modalità e risoluzione (qui 640×320×32);
- `WINDOW SHOW` rende visibile la finestra;
- `WINDOW PUMP 16` **processa** gli eventi per al massimo 16 ms e ritorna;
- `WINDOWCLOSED()` dice se l'utente ha chiuso.

Il ciclo è `DO WHILE`, non un `WAIT`: l'event loop è tuo, quindi puoi fare altro fra un pump e
l'altro.

## 2. Eventi: chi esegue l'handler

Questo è il punto in cui un programma grafico si rompe o funziona. La regola del progetto è che
**il GPM è l'unico esecutore di codice BASIC**: l'evento viene letto dal runtime della finestra,
lo stato viene salvato, e l'handler viene **dispatchato come task GPM**. Non è un dettaglio
implementativo: se chiami una SUB direttamente dal thread della GUI, quella SUB gira fuori dal
GPM e perde il contesto (variabili, scope, stack).

Conseguenza pratica: un handler può chiamare `SLEEP`, `WAIT`, un'altra SUB bloccante senza
congelare la finestra, perché non è quel thread a eseguirlo. Sui backend nativi (VM/C,
C99Native) il runtime C invoca la callback in modo sincrono, ma il contratto è lo stesso: un
evento per volta e `SLEEP 0` fra le azioni che innalzano eventi.

## 3. `ON WINDOW`: sintassi e parametri

```basic
ON WINDOW <evento> GOSUB <handler>
ON WINDOW <evento> OF <handle> GOSUB <handler>
ON WINDOW <evento> OF <handle> GOTO <label>
ON WINDOW <evento> OF <handle> CALL <procedura>()
ON WINDOW <evento> OF <handle> STOP
```

| Campo | Tipo | Obbligatorio | Descrizione |
|---|---|---|---|
| `<evento>` | nome | sì | uno dei nove nomi di §4, case-insensitive |
| `OF <handle>` | espressione intera | no | limita la registrazione a una finestra; assente = ogni finestra (wildcard `-1`) |
| `GOSUB <handler>` | nome | una delle azioni | esegue SUB, label locale o label di modulo come task |
| `GOTO <label>` | nome | una delle azioni | come `GOSUB`: non c'è ritorno nel codice interrotto |
| `CALL <procedura>()` | chiamata | una delle azioni | procedura senza parametri |
| `STOP` | parola chiave | una delle azioni | deregistra la coppia (evento, handle) |

`OF` è valutato **quando l'istruzione viene eseguita**: il valore è un'istantanea, non un
riferimento. Se la variabile cambia dopo, la registrazione resta sull'handle letto allora.

`STOP` non termina il programma e non tocca le altre registrazioni: toglie solo la callback
della coppia indicata. Senza `OF` toglie la registrazione wildcard.

## 4. I nove eventi

| Evento | Innalzato da | Handle | Note |
|---|---|---|---|
| `OPEN` | `WINDOW DEFINE` che crea la finestra | finestra creata | non a ogni `DEFINE` della stessa finestra |
| `CLOSE` | `WINDOW nome CLOSE`, pulsante di chiusura, richiesta SDL | finestra chiusa | dopo l'evento l'handle non è più valido |
| `MINIMIZE` | `WINDOW nome MINIMIZE`, pulsante di riduzione | finestra minimizzata | azzera il focus se era focalizzata |
| `RESTORE` | `WINDOW nome RESTORE` | finestra ripristinata | no-op senza evento se non è minimizzata |
| `FOCUS` | `WINDOW nome ACTIVATE`, click, `Raise` | finestra attivata | innalzato anche se era già attiva |
| `MOVE` | trascinamento della barra del titolo | finestra spostata | TUI |
| `RESIZE` | ridimensionamento dal bordo, massimizza/ripristina | finestra ridimensionata | TUI; in grafica anche `OF 0` |
| `QUIT` | — | — | riservato: nessuna sorgente attuale |
| `PASTE` | incolla dal front-end | `0` o wildcard | dipende dal backend, vedi §8 |

`FOCUS` è il nome canonico dell'attivazione. `ACTIVATE` e `DEACTIVATE` sono stati interni
della finestra, non tipi di evento del linguaggio: `ON WINDOW ACTIVATE ...` non è valido.

## 5. Handle e tabella di registrazione

| Valore | Significato |
|---|---|
| `0` | finestra SDL dello screen grafico |
| `1 … N` | finestre TUI/PIXEL, nell'ordine di `WINDOW DEFINE` |
| `-1` | wildcard interna: è la forma senza `OF`, non si scrive nel programma |

L'handle è deterministico (ordine di creazione) ma non è un oggetto del linguaggio: non
esiste una funzione che lo restituisca. Per un handler che deve sapere quale finestra ha
prodotto l'evento si registra con `OF`, un handler per finestra.

Per ogni coppia (evento, handle) esiste **una sola registrazione**: una seconda
`ON WINDOW` con la stessa coppia sostituisce la precedente. Il runtime C sovrascrive la
callback, `EventDispatcher.RegisterWindow` rimuove la voce esistente e il dispatcher nativo
riusa lo slot. Due finestre con la stessa logica si gestiscono con un handler senza `OF`.

## 6. Handler: SUB, label e visibilità

| Forma | Esempio | Esecuzione |
|---|---|---|
| SUB senza parametri | `ON WINDOW CLOSE OF 1 GOSUB Chiudi` | corpo della SUB |
| procedura | `ON WINDOW CLOSE OF 1 CALL Chiudi()` | procedura chiamata |
| label locale | `ON WINDOW CLOSE OF 1 GOSUB OnClose` | blocco della label dentro la procedura corrente |
| label di modulo | `ON WINDOW CLOSE GOTO OnClose` | blocco della label fuori dalle procedure |

Una label usata come handler diventa una funzione sintetica senza parametri: C99Native emette
`gmb_user_lfnev_<fn>_<label>`, IL un metodo `Proc_<id>`, VM/C un task all'indirizzo della
label. Il blocco vede i **globali** di modulo; le variabili che nella procedura erano locali
sono **fresche** (azzerate), perché l'evento arriva senza frame del chiamante.

Gli handler non ricevono parametri. Per distinguere la finestra serve `OF`, oppure una
variabile globale che il programma aggiorna prima di provocare l'evento.

## 7. Contratto GPM e test deterministici

Sui backend managed (IL, PCodeVM) l'handler è un task: l'evento viene accodato e il corpo
gira quando lo scheduler gli dà il turno. Un evento per volta, con `SLEEP 0` fra le azioni:

```basic
WINDOW doc MINIMIZE
SLEEP 0
WINDOW doc RESTORE
SLEEP 0
```

Dentro un handler `PRINT` scrive nel buffer della finestra attiva: prima di stampare sulla
console serve `WINDOW SELECT 0`. Le finestre producono frame ANSI, quindi un MiniTest con
finestre si verifica sull'**exit code** e su un marker finale, non sul confronto riga per riga.

## 8. Esempio completo e limiti dichiarati

Il sample `shared/samples/windows/WindowEventsComplete.bas` usa ogni forma: `OPEN`, `CLOSE`,
`MINIMIZE`, `RESTORE`, `FOCUS`, `OF` con handle in variabile, forma senza `OF`, `CALL`,
`GOSUB`, `GOTO` su label locale e `STOP`.

```basic
IMPORT WINDOWS
SCREEN 0
DIM openCount = 0
DIM doc = 1

SUB OnOpenDoc()
    openCount = openCount + 1
END SUB

FUNCTION Main() AS INTEGER
    ON WINDOW OPEN OF doc GOSUB OnOpenDoc
    WINDOW DEFINE doc FROM 0, 0 TO 10, 32 TITLE "doc"
    SLEEP 0
    WINDOW SELECT 0
    PRINT "open="; openCount
    RETURN 0
END FUNCTION
```

I limiti sono di **sorgente dell'evento**, non di semantica:

| Evento | Limite |
|---|---|
| `QUIT` | il parser lo accetta; nessuno dei quattro backend lo innalza |
| `PASTE` | IL/TUI wildcard, PCodeVM text-input SDL con handle `0`, backend nativi nessuna sorgente |
| `OF 0` | solo grafica SDL (IL, PCodeVM): in TUI non esiste una finestra SDL |
| `RESTORE` | su finestra visibile è un no-op senza evento |

## 9. TUI e GUI, lo stesso programma

GMBasic ha due mondi di finestre: quello di terminale (TUI) e quello grafico. I sample
`WindowTuiTest.bas` e `WindowTuiGraphics.bas` stanno lì a dimostrare che la stessa struttura
(`WINDOW SHOW`, pump, chiusura) vale nei due casi. Il codice si scrive una volta; cambia il
target.

## 10. Tray: la finestra che non c'è

Un'applicazione può vivere nell'area di notifica, senza finestra principale: menu, icona, stato.
La guida `Tray-Guida-Utente` e il sample `shared/samples/Tray/` coprono il caso. È la forma
giusta per un programma che deve restare attivo (watcher, server locale, promemoria) senza
occupare lo schermo.

## 11. Simboli usati

| Simbolo | Tipo | Firma / forma |
|---|---|---|
| `WINDOW DEFINE` | comando | `WINDOW DEFINE nome FROM top, left TO bottom, right [TITLE "…"] [PIXEL]` |
| `ON WINDOW` | comando | `ON WINDOW <evento> [OF <handle>] <azione>` |
| `WINDOW nome MINIMIZE` | comando | riduce a icona |
| `WINDOW nome RESTORE` | comando | ripristina da icona |
| `WINDOW nome ACTIVATE` | comando | focus + primo piano |
| `WINDOW nome CLOSE` | comando | chiude e distrugge |
| `WINDOW SELECT` | comando | `WINDOW SELECT nome` / `WINDOW SELECT 0` |
| `WINDOW PUMP` | comando | `WINDOW PUMP millisecondi` |
| `WINDOWCLOSED()` | funzione | `WINDOWCLOSED() AS INTEGER` (`-1` chiusa) |
| `WINDOWS.X()` / `WINDOWS.Y()` | funzione | posizione della finestra attiva |
| `WINDOWS.WIDTH()` / `WINDOWS.HEIGHT()` | funzione | dimensioni della finestra attiva |

## 12. Perché questi esempi non hanno un output nel libro

Aprire una finestra e attendere un click rende il programma **interattivo**: nei runner questi
sample sono marcati `' TAG: interactive` e vengono saltati, altrimenti bloccherebbero la suite
(vedi III.15). Per questo il capitolo cita il codice e il sample, non un output catturato: un
output di una finestra non è verificabile in modo deterministico, e fingerlo sarebbe peggio che
non averlo.

## 13. Errori frequenti

| Sintomo | Causa | Correzione |
|---|---|---|
| la finestra non risponde | nessun `WINDOW PUMP` nel loop | aggiungi il pump |
| il programma non termina | `DO WHILE` senza condizione di uscita | esci su `WINDOWCLOSED()` |
| l'handler "perde" le variabili | la label handler non vede le locali della procedura | usa un globale, o una SUB con stato globale |
| l'evento non scatta | registrazione su (evento, handle) sbagliata | controlla l'handle e usa `OF` o la wildcard |
| schermo nero dopo il ridimensionamento | nessun ridisegno | ridisegna su `ON WINDOW RESIZE` |
| CPU al 100% | pump senza attesa | passa un timeout al pump |
| `RESTORE` non innalza l'evento | la finestra non era minimizzata | minimizza prima di ripristinare |
| due handler sulla stessa coppia | vale solo l'ultima registrazione | usa un handler per finestra o uno senza `OF` |

## 14. Dove andare adesso

- **IV.20** — i controlli dentro la finestra.
- **IV.18** — temi e stili.
- **III.13** — il GPM che esegue gli handler.






<!-- BEGIN SURFACE (generato da tools/sh/manual_surface.sh — non modificare a mano) -->

## Riferimento completo: comandi, funzioni, parametri

> Sezione generata dalle guide del progetto per questo capitolo: contiene **tutte**
> le superfici documentate, non una selezione. La prosa dei capitoli precedenti le
> spiega; qui stanno i dettagli — parametri, varianti, esempi d'uso.

### Window-Management-Event-System

*Fonte: `Window-Management-Event-System`*


## Architettura

BASIC  ON WINDOW <ev> [OF <handle>] <azione>
           │
           ├─ IL / PCodeVM ── proxy C# (WindowManager, GfxWindowManager) ── P/Invoke ─┐
           ├─ VM/C ──────────────────────────────────────────────────────────────────┤
           └─ C99Native ─────────────────────────────────────────────────────────────┤
                                                                                      ▼
                                             gmb_window_manager_* (gmb_runtime_c)
                                                       │
                                  gmb_window_fire(handle, "EVENTO")
                                                       │
                       ┌───────────────────────────────┴───────────────────┐
                       ▼                                                   ▼
        IL / PCodeVM: EventDispatcher → task GPM          VM/C, C99Native: thunk/task sincrono

| Livello | File | Ruolo |
|---|---|---|
| stato finestre TUI | `GMBasic/native/gmb_runtime_c/src/gmb_tui_windowmanager.c` | rect, z-order, focus, stati SNAPPED/TILED, drag/resize, `gmb_window_fire` |
| statement `WINDOW` + compositor | `GMBasic/native/gmb_runtime_c/src/gmb_tui_windowstmt.c` | `DEFINE/SHOW/CLOSE/MINIMIZE/RESTORE/ACTIVATE/SELECT`, hit-test, innalzamento eventi |
| registro eventi | `GMBasic/native/gmb_runtime_c/src/gmb_console.c` | tabella per (handle, evento) → callback C |
| proxy TUI C# | `GMBasicSharp/common/ClipperLib/Controls/WindowManager.cs` | P/Invoke stateless (nessuno stato duplicato) |
| proxy PIXEL C# | `GMBasicSharp/common/CommonGraphics/Graphics/GfxWindowManager.cs` + `GfxWmNative.cs` | P/Invoke stateless |
| bridge IL | `GMBasicSharp/common/IlBackendRuntime/IlRuntime.cs` | sottoscrive gli eventi del proxy e li inoltra a `EventDispatcher` |
| bridge PCodeVM | `GMBasicSharp/common/IlBackendRuntime/PCode/PCodeProcessor.cs` | come IL |
| bridge VM/C | `GMBasic/native/gmb_vm_c/gmb_vm_tui.c` | `vm_on_window_event`, `vm_win_thunk`, spawn del task |

## Sintassi

### ON WINDOW — nove eventi, clausola `OF`

ON WINDOW OPEN GOSUB Handler        ' finestra creata (DEFINE)
ON WINDOW CLOSE GOSUB Handler       ' finestra chiusa
ON WINDOW RESIZE GOSUB Handler      ' ridimensionata
ON WINDOW MOVE GOSUB Handler        ' spostata (drag)
ON WINDOW FOCUS GOSUB Handler       ' ha ricevuto il focus
ON WINDOW MINIMIZE GOSUB Handler    ' minimizzata
ON WINDOW RESTORE GOSUB Handler     ' ripristinata (solo se minimizzata)
ON WINDOW QUIT GOSUB Handler        ' riservato: nessuna sorgente attuale
ON WINDOW PASTE GOSUB Handler       ' incolla dal front-end


ON WINDOW CLOSE OF 1 GOSUB ChiudiDoc      ' solo handle 1
ON WINDOW MINIMIZE OF h GOSUB Riduci      ' h = espressione intera, valutata ora
ON WINDOW FOCUS GOSUB Qualsiasi           ' senza OF: ogni finestra (wildcard -1)
ON WINDOW PASTE OF 0 GOSUB Incolla        ' finestra SDL (screen grafico)
ON WINDOW CLOSE OF 1 STOP                 ' deregistra la coppia (evento, handle)


ON WINDOW <evento> [OF <handle|espressione>] (GOSUB <handler> | GOTO <label> | CALL <sub>() | STOP)


### Handle
| Valore | Significato |
|---|---|
| `0` | finestra SDL dello screen grafico |
| `1 … N` | finestre TUI/PIXEL, nell'ordine di `WINDOW DEFINE` |
| `-1` | wildcard interna: è la forma senza `OF`, non si scrive nel programma |

### Tabella di registrazione
| Caso | Effetto |
|---|---|
| prima `ON WINDOW` per (evento, handle) | aggiunge la registrazione |
| seconda `ON WINDOW` per la stessa coppia | sostituisce la precedente (tutti i backend) |
| `STOP` con `OF` | rimuove la callback della coppia |
| `STOP` senza `OF` | rimuove la registrazione wildcard |

### Azioni
| Azione | Semantica |
|---|---|
| `GOSUB <handler>` | esegue il blocco come task; target: SUB, label locale o label di modulo |
| `GOTO <label>` | identico a `GOSUB` in un handler: non esiste un punto di ritorno nel codice interrotto |
| `CALL <sub>()` | invoca una procedura senza parametri come task |
| `STOP` | deregistra la coppia (evento, handle) |

#### Materializzazione degli handler con label
| Gamba | Materializzazione |
|---|---|
| C99Native | funzione `gmb_user_lfnev_<fn>_<label>` emessa in `gmb_c99_emit.c` |
| VM/C | task all'indirizzo della label, spawnato da `vm_win_thunk` |
| IL | metodo sintetico `Proc_<id>` (`IlProgramCompiler.EventLabelHandlers.cs`) |
| PCodeVM | stesso schema IL (SymbolId → task) |

## Ciclo di vita degli eventi
| Evento | Sorgente TUI | Sorgente SDL | Innalzato da |
|---|---|---|---|
| `OPEN` | `gmb_window_manager_add` (solo creazione; re-`DEFINE` = aggiornamento) | — | tutte e quattro |
| `CLOSE` | `WINDOW nome CLOSE`, pulsante | `SDL_WINDOWEVENT_CLOSE` (handle 0) | tutte; SDL solo IL/PCodeVM |
| `MINIMIZE` | `WINDOW nome MINIMIZE`, pulsante | — | tutte e quattro |
| `RESTORE` | `WINDOW nome RESTORE` se minimizzata | — | tutte e quattro |
| `FOCUS` | `ACTIVATE`, click, `Raise` | — | tutte e quattro |
| `MOVE` | drag della titlebar, `move_to`/`move_by` | — | tutte e quattro |
| `RESIZE` | drag del bordo, massimizza/ripristina | `SDL_WINDOWEVENT_RESIZED` (handle 0) | tutte; SDL solo IL/PCodeVM |
| `QUIT` | — | — | nessuna |
| `PASTE` | lettore di terminale (IL) | text-input SDL (PCodeVM, handle 0) | dipende dal front-end |

## Focus
| Operazione | Effetto sul focus | Evento |
|---|---|---|
| `WINDOW nome ACTIVATE` | dà il focus alla finestra | `FOCUS` (sempre, anche se era già attiva) |
| click sulla finestra | dà il focus alla finestra | `FOCUS` |
| `WINDOW nome MINIMIZE` | azzera il focus se era focalizzata | `MINIMIZE` |
| `WINDOW nome CLOSE` | azzera il focus se era focalizzata | `CLOSE` |
| `WINDOW SELECT 0` | riporta il buffer alla console, non cambia il focus | — |

## Dispatch: chi esegue l'handler
| Backend | Registrazione | Esecuzione |
|---|---|---|
| IL | `EventDispatcher.DispatchWindow(kind, handle)` filtra `Handle < 0 \|\| Handle == handle` | task GPM accodato, mai sul thread del dispatcher |
| PCodeVM | come IL | task GPM accodato |
| VM/C | `vm_on_window_event` → `gmb_window_register_event_of_cb` | `vm_win_thunk` legge `gmb_window_firing` e accoda un task all'indirizzo della label |
| C99Native | emitter: `gmb_window_register_event_of_cb(handle, "EVENTO", fn)` | il runtime C invoca la funzione direttamente |

## Limiti dichiarati
| Limite | Dettaglio |
|---|---|
| `QUIT` | il parser lo accetta; nessuno dei quattro backend lo innalza. Registrarlo è innocuo. |
| `PASTE` | sorgente dipendente dal front-end: IL/TUI wildcard, PCodeVM SDL handle 0, backend nativi nessuna. |
| `OF 0` in TUI | non produce eventi: la finestra SDL esiste solo in grafica (IL, PCodeVM). |
| test headless | le finestre scrivono frame ANSI: la verifica è sull'exit code e su un marker, non sul diff riga per riga. |

## Test e sample
| File | Copre |
|---|---|
| `shared/minitests/windows/window_events_matrix.bas` | `OPEN`, `CLOSE`, `MINIMIZE`, `RESTORE`, `FOCUS`, filtro `OF` |
| `shared/minitests/windows/on_window_forms.bas` | azioni, `OF` espressione, wildcard, `STOP`, sostituzione |
| `shared/minitests/controls/window_events.bas` | consegna solo all'handle indicato |
| `shared/minitests/tui/window_parity.bas` | ciclo TUI con pump e chiusura |
| `shared/samples/windows/WindowEventsComplete.bas` | sample completo, marker `events-complete-ok` |

tools/sh/parity4 shared/minitests/windows/on_window_forms.bas


## Storia
| Prima | Ora |
|---|---|
| window manager C# con stato proprio (`ClipperLib/Controls/WindowManager.cs`, `CommonGraphics/Graphics/GfxWindowManager.cs`) | classi archiviate in `GMBasicMonoRepo-Deprecated` |
| due copie dello stato (C# e C) | C unica fonte di stato, C# proxy P/Invoke stateless |
| eventi emessi dal codice C# | eventi emessi dal runtime C (`gmb_window_fire`) |

### Finestre-Eventi-Guida-Utente

*Fonte: `Finestre-Eventi-Guida-Utente`*


## 1. Comandi `WINDOW`
| Comando | Effetto |
|---|---|
| `WINDOW DEFINE nome FROM top, left TO bottom, right` | crea la finestra (coordinate a celle, o pixel con `PIXEL`) |
| `WINDOW DEFINE ... TITLE "titolo"` | crea la finestra con il titolo indicato |
| `WINDOW DEFINE ... DOUBLE`, `SINGLE` o `ROUNDED` | sceglie il bordo |
| `WINDOW DEFINE ... SHADOW` | aggiunge l'ombra |
| `WINDOW DEFINE ... PIXEL` | interpreta le coordinate come pixel logici |
| `WINDOW DEFINE ... DESKTOP n` | colloca la finestra sul desktop virtuale `n` |
| `WINDOW nome ACTIVATE` | porta la finestra davanti e le dà il focus |
| `WINDOW nome DEACTIVATE` | toglie il focus (la finestra resta visibile) |
| `WINDOW nome RELEASE` | alias di `DEACTIVATE` |
| `WINDOW nome CLOSE` | chiude e distrugge la finestra |
| `WINDOW nome MINIMIZE` | riduce a icona (ripristinabile) |
| `WINDOW nome RESTORE` | ripristina da `MINIMIZE`; no-op se non è minimizzata |
| `WINDOW SHOW` / `WINDOW HIDE` | mostra/nasconde la finestra dello **screen** (globale) |
| `WINDOW SELECT nome` / `WINDOW SELECT 0` | rende attivo il buffer della finestra (o la console) |
| `WINDOW PUMP ms` | processa gli eventi per al massimo `ms` millisecondi e ritorna |
| `WINDOW CLOSE` | richiede la chiusura della finestra SDL (screen grafico) |
| `WINDOW SET TITLE "titolo"` | cambia il titolo della finestra attiva |
| `WINDOW nome PRINT expr` | aggiunge una riga al buffer della finestra |
| `WINDOW nome TEXT expr` / `CLEAR` / `SCROLL n` | sostituisce, svuota, scorre il buffer |
| `WINDOW nome EDIT ON` / `OFF` | abilita/disabilita la modifica del buffer |
| `WINDOW THEME "nome"`, `WINDOW DESKTOP n`, `WINDOW FRAME ...`, `WINDOW SHADOW ON` / `OFF` | impostazioni globali |

WINDOW DEFINE doc FROM 0, 0 TO 10, 32 TITLE "doc"      ' handle 1
WINDOW DEFINE log FROM 12, 0 TO 22, 32 TITLE "log"     ' handle 2
WINDOW SELECT 0                                       ' i PRINT tornano sulla console


## 2. `ON WINDOW` — sintassi

ON WINDOW <evento> GOSUB <handler>            ' ogni finestra (wildcard)
ON WINDOW <evento> OF <handle> GOSUB <handler>  ' una sola finestra
ON WINDOW <evento> GOTO <label>               ' salto a una label
ON WINDOW <evento> CALL <sub>()               ' chiamata a procedura
ON WINDOW <evento> STOP                       ' deregistra l'handler


## 3. Eventi
| Evento | Quando viene innalzato | Handle |
|---|---|---|
| `OPEN` | `WINDOW DEFINE` crea la finestra; un re-`DEFINE` dello stesso nome la aggiorna e non innalza un secondo `OPEN` | finestra creata |
| `CLOSE` | `WINDOW nome CLOSE`, pulsante di chiusura, richiesta di chiusura SDL | finestra chiusa |
| `MINIMIZE` | `WINDOW nome MINIMIZE`, pulsante di riduzione | finestra minimizzata |
| `RESTORE` | `WINDOW nome RESTORE` su una finestra **minimizzata** | finestra ripristinata |
| `FOCUS` | `WINDOW nome ACTIVATE`, click sulla finestra, `Raise` del window manager | finestra attivata |
| `MOVE` | trascinamento della barra del titolo (TUI) | finestra spostata |
| `RESIZE` | ridimensionamento dal bordo o massimizzazione/ripristino (TUI) | finestra ridimensionata |
| `QUIT` | accettato dal parser, **nessuna sorgente attuale** (vedi §7) | — |
| `PASTE` | incolla di testo dal front-end (dipende dal backend, vedi §7) | 0 o wildcard |

## 4. Handler: SUB, label e visibilità
| Forma | Esempio | Cosa esegue |
|---|---|---|
| `SUB` senza parametri | `ON WINDOW CLOSE OF 1 GOSUB Chiudi` | il corpo della SUB |
| procedura con `CALL` | `ON WINDOW CLOSE OF 1 CALL Chiudi()` | la procedura chiamata |
| label locale | `ON WINDOW CLOSE OF 1 GOSUB OnClose` | il blocco della label dentro la procedura corrente |
| label di modulo | `ON WINDOW CLOSE GOTO OnClose` | il blocco della label fuori dalle procedure |

## 5. Contratto di esecuzione (GPM)

WINDOW doc MINIMIZE
SLEEP 0                      ' lascia girare l'handler di MINIMIZE
WINDOW doc RESTORE
SLEEP 0                      ' lascia girare l'handler di RESTORE


## 6. Esempio completo

IMPORT WINDOWS
SCREEN 0
DIM openCount = 0
DIM closeCount = 0
DIM doc = 1

SUB OnOpenDoc()
    openCount = openCount + 1
END SUB

SUB OnCloseDoc()
    closeCount = closeCount + 1
END SUB

FUNCTION Main() AS INTEGER
    ON WINDOW OPEN OF doc GOSUB OnOpenDoc


open= 1  close= 1


## 7. Limiti dichiarati
| Evento | Limite |
|---|---|
| `QUIT` | il parser lo accetta, ma nessuno dei quattro backend lo innalza oggi. Registrarlo è innocuo. |
| `PASTE` | dipende dal front-end: il lettore di terminale (IL, TUI) lo manda alla registrazione wildcard; PCodeVM lo manda con handle `0` dal text-input SDL; i backend nativi (VM/C, C99Native) non hanno una sorgente di incolla. |
| `CLOSE`/`RESIZE` con `OF 0` | solo in grafica SDL (IL, PCodeVM); i backend nativi eseguono il compositor ANSI e non hanno una finestra SDL. |
| output dei test | l'apertura di una finestra scrive frame ANSI: i MiniTest con finestre si verificano sull'**exit code** e su un marker finale, non sul confronto riga per riga. |

## 8. Input

## 9. ON MOUSE

## 10. MOUSE — letture di stato
| Funzione | Ritorna |
|---|---|
| `MOUSE.BUTTON()` | `-1` (vero) se un tasto (sinistro/destro/middle) è premuto, altrimenti `0` |
| `MOUSE.MIDDLE()` | `-1` se il tasto **middle** è premuto, altrimenti `0` |
| `MOUSE.DBLCLICK()` | `-1` se è avvenuto un doppio clic, altrimenti `0` |
| `MOUSE.PRESENT()` | `-1` se un mouse è presente/attivo, altrimenti `0` |
| `MOUSE.MOVE()` | `-1` se il puntatore si è mosso nell'ultimo frame, altrimenti `0` |
| `MOUSE.WHEEL()` | delta della rotellina (positivo/negativo/`0`) |
| `MOUSE.X()` / `MOUSE.Y()` | coordinate del puntatore (celle in TUI, pixel in grafica) |
| `MOUSE.LEFT()` / `MOUSE.RIGHT()` | `-1` se il tasto sinistro/destro è premuto, altrimenti `0` |

IF MOUSE.BUTTON() THEN PRINT "premuto"
PRINT MOUSE.X(); MOUSE.Y()


## 11. Controlli TUI
| Widget | Sintassi |
|--------|----------|
| CHECKBOX | `CHECKBOX row, col, "caption" INTO var` |
| LISTBOX | `LISTBOX row, col, width, height, "items" INTO var` |
| MENU | `MENU row, col, "items" INTO var` |
| TOGGLE | `TOGGLE row, col, "caption" INTO var` |

## 12. SENDKEY

SENDKEY "testo" + CHR(13)   ' invia testo + Invio


## 13. Verifica di parità
| File | Cosa copre | Esito |
|---|---|---|
| `shared/minitests/windows/window_events_matrix.bas` | `OPEN`, `CLOSE`, `MINIMIZE`, `RESTORE`, `FOCUS` con filtro `OF` | 4/4 |
| `shared/minitests/windows/on_window_forms.bas` | `CALL`, `GOSUB`, `GOTO` label, `STOP`, `OF` espressione, wildcard, sostituzione | 4/4 |
| `shared/minitests/controls/window_events.bas` | evento consegnato solo all'handle indicato | 4/4 |
| `shared/minitests/tui/window_parity.bas` | ciclo TUI con pump e chiusura | 4/4 |
| `shared/samples/windows/WindowEventsComplete.bas` | sample completo, marker `events-complete-ok` | 4/4 |

tools/sh/parity4 shared/minitests/windows/on_window_forms.bas


### Tray-Guida-Utente

*Fonte: `Tray-Guida-Utente`*


## Modello (leggere prima)

## Funzioni

### `TRAYAVAILABLE()` → LONG (-1/0)

### `TRAYBACKEND()` → STRING

### `TRAYCREATE(icon, tooltip)` → LONG (0 ok, <0 errore)

### `TRAYITEM(id, label)` → LONG (0 ok)

### `TRAYSEP()` → LONG (0 ok)

### `TRAYTOOLTIP(text)` → LONG (0 ok)

### `TRAYPOLL()` → LONG (numero di comandi/eventi processati)

### `TRAYLAST()` → LONG (id item, -1 se nessuno)

### `TRAYREMOVE()` → LONG (0)

### `TRAYSELFTEST()` → LONG (-1/0)

## Esempio minimo (loop con polling)

IMPORT TRAY
DIM r AS LONG
IF TRAYAVAILABLE() = 0 THEN
  PRINT "tray non disponibile"
  EXIT SUB
END IF
r = TRAYCREATE("", "La mia app")
r = TRAYITEM(1, "Stato")
r = TRAYITEM(99, "Esci")
DIM last AS LONG
DIM i AS LONG
FOR i = 1 TO 600
  SLEEP 1000
  r = TRAYPOLL()
  last = TRAYLAST()
  IF last = 99 THEN EXIT FOR


## Esempio completo

## Parità fra i 4 backend (IL · PCodeVM · VM/C · C99Native)

tools/sh/parity4 shared/minitests/tray/tray_parity.bas     # atteso: OK pcode/vmc/c99n == il · c99 gate PASS · == PASS


## Caveat

## Vedi anche

### WINDOW-SHOW-HIDE-Guida-Utente

*Fonte: `WINDOW-SHOW-HIDE-Guida-Utente`*


## Sintassi

IMPORT WINDOWS
WINDOW SHOW     ' mostra (o ri-mostra) la finestra dello screen
WINDOW HIDE     ' nasconde la finestra mantenendo l'app in esecuzione


### Parametri

### Valore di ritorno

### Effetti

### Differenza da `SCREEN 0`

## Esempio (app con icona nel tray)

IMPORT WINDOWS
IMPORT TRAY

SUB ApriConsole()
  SCREEN 12, 760, 440, 32
  WINDOW SHOW
  ' ... LISTBOX / BUTTON / FORM_FRAME ...
END SUB

SUB ChiudiConsole()
  WINDOW HIDE          ' l'app e l'icona nel tray restano attive
END SUB


## Relazione con le azioni per-finestra
| Comando | Effetto | Focus | Visibile | Distrugge |
|---|---|---|---|---|
| `WINDOW name ACTIVATE` | focus + porta davanti | sì | sì | no |
| `WINDOW name DEACTIVATE` | toglie il focus | no | sì | no |
| `WINDOW name RELEASE` | alias di DEACTIVATE | no | sì | no |
| `WINDOW name CLOSE` | rimuove la finestra | – | no | **sì** |
| `WINDOW name MINIMIZE` | collassa (ripristinabile) | no | no | no |
| `WINDOW name RESTORE` | da MINIMIZE a visibile | no | sì | no |
| `WINDOW SHOW` / `WINDOW HIDE` | visibilità dello **screen** (globale) | – | sì/no | no |

## Parità backend
| Backend | Implementazione |
|---|---|
| IL | `IlRuntime.WindowSetVisible(bool)` → `IGraphicsService.WindowVisible` |
| PCode / VM | `SysCall.WINDOW_ACTION` con nome `""` e azione `Show(6)`/`Hide(7)` |
| C99 | emette `gmb_winstmt_show(n)` (`#include gmb_tui_windowstmt.h`) |
| C99Native | emette `gmb_winstmt_show(n)` |

## Test di parità

<!-- END SURFACE -->







<!-- BEGIN DOMAIN TABLE -->
## Tavola sinottica del dominio

Ogni riga riporta la firma canonica (dal registry) e il primo uso reale trovato
nel corpus di test e sample `.bas`/`.inc`: dove il corpus non usa il simbolo è dichiarato.

- **JOY.AVAILABLE(x1)** — `IF JOY.AVAILABLE(0) = 0 OR JOY.AVAILABLE(0) = -1 THEN PRINT "avail-ok"` (shared/minitests/joy/basic.bas:6)
- **JOY.MASK(x1)** — `IF JOY.MASK(0) >= 0 THEN PRINT "mask-ok"` (shared/minitests/joy/basic.bas:9)
- **JOY.NAME(x1)** — `DIM n$ = JOY.NAME(0)` (shared/minitests/joy/basic.bas:7)
- **JOY.PORTS()** — `DIM p = JOY.PORTS()` (shared/minitests/joy/basic.bas:4)
- **MOUSE.BUTTON()** — `IF MOUSE.BUTTON(3) <> 0 THEN` (shared/samples/Mouse/MouseGraphicPaint.bas:63)
- **MOUSE.CLICK()** — `IF NOT MOUSE.CLICK() THEN PRINT "click0: si"` (shared/minitests/speed/gaps.bas:26)
- **MOUSE.CLICK(x1, x2, x3, x4)** — `IF NOT MOUSE.CLICK() THEN PRINT "click0: si"` (shared/minitests/speed/gaps.bas:26)
- **MOUSE.DBLCLICK()** — `PRINT "DBLCLICK:   "; MOUSE.DBLCLICK()` (shared/samples/Events/MouseGetters.bas:6)
- **MOUSE.LEFT()** — `DIM left AS DOUBLE = MOUSE.LEFT()` (shared/samples/Mouse/MouseGraphicState.bas:38)
- **MOUSE.MIDDLE()** — `DIM middle AS DOUBLE = MOUSE.MIDDLE()` (shared/samples/Mouse/MouseGraphicState.bas:40)
- **MOUSE.MOVE()** — `DIM moving AS DOUBLE = MOUSE.MOVE()` (shared/samples/Mouse/MouseGraphicState.bas:41)
- **MOUSE.PRESENT()** — `DIM present AS DOUBLE = MOUSE.PRESENT()` (shared/samples/Mouse/MouseGraphicState.bas:37)
- **MOUSE.RIGHT()** — `DIM right AS DOUBLE = MOUSE.RIGHT()` (shared/samples/Mouse/MouseGraphicState.bas:39)
- **MOUSE.WHEEL()** — `DIM wheel AS DOUBLE = MOUSE.WHEEL()` (shared/samples/Mouse/MouseGraphicState.bas:42)
- **MOUSE.X()** — `PRINT "MOUSE.X()/Y() in pixel. Premi i tasti e muovi il mouse. ESC per uscire."` (shared/samples/Mouse/MouseGraphicState.bas:30)
- **MOUSE.Y()** — `DIM py AS DOUBLE = MOUSE.Y()` (shared/samples/Mouse/MouseGraphicState.bas:36)
- **TRAY.TRAYAVAILABLE()** — `PRINT "TRAYAVAILABLE="; TRAYAVAILABLE()` (shared/samples/Tray/TrayConsoleDemo.bas:20)
- **TRAY.TRAYBACKEND()** — `PRINT "TRAYBACKEND="; TRAYBACKEND()` (shared/samples/Tray/TrayConsoleDemo.bas:19)
- **TRAY.TRAYCREATE(x1, x2)** — `r = TRAYCREATE("", "TrayConsoleDemo")          ' icona default GMBasic` (shared/samples/Tray/TrayConsoleDemo.bas:27)
- **TRAY.TRAYITEM(x1, x2)** — `r = TRAYITEM(1, "Stato")` (shared/samples/Tray/TrayConsoleDemo.bas:28)
- **TRAY.TRAYLAST()** — `last = TRAYLAST()` (shared/samples/Tray/TrayConsoleDemo.bas:45)
- **TRAY.TRAYPOLL()** — `r = TRAYPOLL()` (shared/samples/Tray/TrayConsoleDemo.bas:44)
- **TRAY.TRAYREMOVE()** — `TRAYREMOVE()` (shared/samples/Tray/TrayConsoleDemo.bas:63)
- **TRAY.TRAYSELFTEST()** — `PRINT "TRAYSELFTEST (prima)="; TRAYSELFTEST()` (shared/samples/Tray/TrayConsoleDemo.bas:21)
- **TRAY.TRAYSEP()** — `r = TRAYSEP()` (shared/samples/Tray/TrayConsoleDemo.bas:30)
- **TRAY.TRAYTOOLTIP(x1)** — `r = TRAYTOOLTIP("TrayConsoleDemo - avviato")` (shared/samples/Tray/TrayConsoleDemo.bas:32)
- **WINDOWS.DESKTOP()** — `d = WINDOWS.DESKTOP()` (shared/samples/Clipper/WindowDesktops.bas:10)
- **WINDOWS.HEIGHT()** — `DIM h = WINDOWS.HEIGHT()` (shared/samples/windows/WindowHeightWidth.bas:11)
- **WINDOWS.THEME()** — `PRINT "Applicato tema: "; WINDOWS.THEME()` (shared/samples/Clipper/WindowThemesShowcase.bas:41)
- **WINDOWS.WIDTH()** — `PRINT "=== WINDOWS.WIDTH / HEIGHT ==="` (shared/samples/windows/WindowHeightWidth.bas:9)
- **WINDOWS.WINDOWCLOSED()** — `PRINT "wclosed="; WINDOWS.WINDOWCLOSED()` (shared/minitests/windows/windows_all.bas:25)
- **WINDOWS.X()** — `wx = WINDOWS.X()` (shared/samples/Clipper/WindowPixel.bas:26)
- **WINDOWS.Y()** — `IF WINDOWS.Y() = -1 THEN PRINT "y=-1" ELSE PRINT "y=?"` (shared/minitests/windows/windows_all.bas:12)
<!-- END DOMAIN TABLE -->
