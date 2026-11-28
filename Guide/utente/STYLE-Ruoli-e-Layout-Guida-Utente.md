# STYLE — ruoli semantici del tema e limiti del layout

> **Stato**: nessuna clausola nuova, nessun comando nuovo: sono **chiavi dello
> `STYLE`** già esistente sui controlli (`BUTTON`, `LABEL`, `PANEL`, `LISTBOX`, …) più le
> clausole di layout `EXPAND` / `ANCHOR` / `ALIGN`. Valgono **sulle quattro gambe**
> (IL, PCodeVM, VM/C, C99Native) e su **entrambi i percorsi** (GRAPHICS/PIXEL e TEXT/TUI).
> Implementazione condivisa: `GMBasic/native/gmb_runtime_c/src/gmb_theme_roles.c` e
> `src/gmb_layout.c`. MiniTest: `shared/minitests/controls/layout_clamp.bas`,
> `shared/minitests/controls/layout_manager.bas`, `shared/minitests/controls/a11y_options.bas`,
> `shared/minitests/ai/ai_list_options.bas`. Esempio completo:
> `shared/samples/graphics/ThemeRoles.bas`. I flag globali di accessibilità
> (`ui.high_contrast`, `ui.text_scale`, `ui.reduce_motion`,
> `ui.focus_always_visible`) sono nella **§7**.

## 1. Sintassi in una schermata

```basic
BUTTON 40, 40, 160, 40, "Salva" NAME "bSave" EXPAND H STYLE "role=primary grow=50"
BUTTON 40, 100, 160, 40, "Info" NAME "bInfo" EXPAND H STYLE "role=info maxw=300"
BUTTON 40, 160, 100, 40, "Tenue" NAME "bMuted" EXPAND H STYLE "role=muted grow=0 minw=240"
BUTTON 40, 220, 120, 40, "Fisso" NAME "bNone"
```

Le chiavi dello `STYLE` sono separate da **spazio**, nella forma `chiave=valore`
(il valore può essere fra virgolette). Le chiavi di stile e quelle di layout
convivono nella **stessa** stringa: ognuna viene letta dal parser che la riguarda e
le altre sono ignorate.

| Chiave | Valore | Significato |
|---|---|---|
| `role=` / `tone=` | nome del ruolo (tabella §2) | il controllo dichiara **cosa è**; i colori li decide il tema |
| `contrast=` | numero ≥ 1, oppure `off`/`none`/`0` | contrasto minimo richiesto per l'inchiostro del ruolo; default **4.5** (WCAG AA) |
| `minw=` `minh=` | unità del controllo, `0` = nessun limite | dimensione minima |
| `maxw=` `maxh=` | unità del controllo, `0` = nessun limite | dimensione massima |
| `grow=` | percento `0…200`, default `100` | quota del delta della finestra che il controllo assorbe |
| `flow=` | `wrap`, `grid`, `none` (contenitore) | disposizione automatica dei figli (§1.1) |
| `cols=` `gap=` `pad=` | numero ≥ 0 (contenitore) | colonne della griglia, spazio fra celle, margine interno |
| `group=` | nome di un contenitore (figlio) | entra nel gruppo disposto da quel contenitore |

Le clausole di layout (fuori dallo `STYLE`, come `NAME`/`TAG`):

```basic
BUTTON x, y, w, h, "testo" [NAME "n"] [STYLE "…"]
       [EXPAND H|V|BOTH|NONE]
       [ANCHOR LEFT|RIGHT|TOP|BOTTOM|ALL|NONE]     ' combinabili con '+': anchor=left+top
       [ALIGN LEFT|CENTER|RIGHT|TOP|MIDDLE|BOTTOM] ' combinabili con '+': align=center+middle
```

`EXPAND`/`ANCHOR`/`ALIGN`/`MIN*`/`MAX*`/`GROW` rispondono al **resize della pagina**
(`WINDOW RESIZE w, h`); il rect dichiarato e la dimensione della finestra al momento
della creazione sono la **base** del calcolo.

### 1.1 Contenitori: WrapBox e Grid

Due chiavi dello `STYLE` dispongono **automaticamente i figli** di un contenitore.
Non sono comandi nuovi: sono dati letti dallo stesso runtime su tutte e 4 le gambe.

| Chiave | Dove | Valore | Significato |
|---|---|---|---|
| `flow=` | **contenitore** | `wrap` / `grid` / `none` | modalità di disposizione dei figli |
| `cols=` | contenitore | numero ≥ 1 | griglia: numero di colonne |
| `gap=` | contenitore | numero ≥ 0 | spazio fra celle/righe |
| `pad=` | contenitore | numero ≥ 0 | margine interno del contenitore |
| `group=` | **figlio** | nome del contenitore | il figlio entra nel gruppo di quel contenitore |

Alias comodi: `wrap=1` equivale a `flow=wrap`; `grid=3` equivale a
`flow=grid cols=3`.

```basic
' contenitore: LISTBOX/PANEL/… con un rect REALE (un BUTTON in TUI è alto 2 righe)
LISTBOX 4, 2, 56, 16, " " INTO dummy NAME "cards" STYLE "flow=grid cols=3 gap=1 pad=1"
BUTTON 0, 0, 8, 2, "A" NAME "a" STYLE "group=cards"
BUTTON 0, 0, 8, 2, "B" NAME "b" STYLE "group=cards"
BUTTON 0, 0, 8, 2, "C" NAME "c" STYLE "group=cards"
BUTTON 0, 0, 8, 2, "D" NAME "d" STYLE "group=cards"

' wrap: i figli vanno a capo quando la riga è piena
LISTBOX 4, 24, 56, 10, " " INTO dummy NAME "tags" STYLE "flow=wrap gap=1"
BUTTON 0, 0, 20, 2, "uno" NAME "t1" STYLE "group=tags"
BUTTON 0, 0, 20, 2, "due" NAME "t2" STYLE "group=tags"
```

- **GRID**: `cols` colonne di celle uguali; righe = `ceil(figli/cols)`; ogni figlio
  riempie la sua cella. Con `pad=1`, `gap=1`, area `56×16`, 3 colonne e 4 figli:
  celle `17×6` in `(5,3) (23,3) (41,3) (5,10)` (celle).
- **WRAP**: ogni figlio conserva la sua larghezza; quando non entra più nella riga
  va a capo. L'altezza della riga è quella del figlio più alto.
- I limiti `minw/maxw/minh/maxh` del figlio **continuano a valere** dentro la cella;
  un figlio più largo del contenitore viene ridotto alla riga.
- Il contenitore deve avere un `NAME`: è la chiave con cui i figli lo trovano.
- La disposizione avviene **subito** quando si dichiara `flow=`/`group=` e a ogni
  `WINDOW RESIZE`: non serve aspettare il primo resize.
- Il contenitore può essere qualsiasi controllo con un rect reale (`LISTBOX`, `PANEL`,
  `TREE`, `GRID`…). Un `BUTTON` in TUI è alto sempre 2 righe, quindi non è adatto a
  fare da contenitore alto.

MiniTest di riferimento: `shared/minitests/controls/layout_flow.bas`.

## 2. Ruoli: cosa dichiara il programma

| Ruolo | Alias accettati | Uso previsto |
|---|---|---|
| `default` | `base`, `neutral` | superficie neutra (dialog/finestra) |
| `primary` | `main` | azione principale: accento del tema |
| `secondary` | `sub` | azione secondaria: colore di selezione del tema |
| `accent` | — | evidenza/titolo |
| `success` | `ok`, `positive` | esito positivo |
| `warning` | `warn`, `caution` | attenzione |
| `danger` | `error`, `destructive` | errore / azione distruttiva |
| `info` | `note` | informazione |
| `muted` | `disabled`, `dim` | disattivato / secondario tenue |

Il nome è **case-insensitive**; un nome sconosciuto viene ignorato (il controllo resta
senza ruolo, cioè si comporta come prima dell'introduzione della chiave).

## 3. Semantica

**Cascata** — dal più forte al più debole:

1. **override espliciti** del controllo (`fg=`, `bg=`, `focuscolor=`, `errorcolor=`,
   `selcolor=`, `color=`, …): vincono sempre, anche su un ruolo;
2. **ruolo + stato** (normale, hover, premuto, disabilitato, selezionato, con focus);
3. **tema** attivo, compreso un `WINDOW THEME` eseguito **dopo** la creazione.

Il punto 3 è la ragione per cui i ruoli non sono colori: nel percorso TEXT il colore
del ruolo è **risolto a ogni disegno** (`gmb_control_style_effective`), quindi un
controllo `role=primary` segue il cambio tema a runtime senza essere ricreato. Nel
percorso PIXEL il ruolo è applicato allo stile del widget alla creazione
(`style_apply`): i form si costruiscono dopo la scelta del tema, quindi non ci sono
colori stantii.

**Stato di errore = `danger`.** Un controllo **con ruolo** che entra in stato di errore
mostra i colori del ruolo `danger`, qualunque fosse il suo ruolo: un campo `primary` in
errore deve diventare rosso, non restare accento. Un controllo **senza ruolo** mantiene
il comportamento storico (colori di errore del tema).

**Focus e selezione.** Per un controllo con ruolo il focus usa la tinta del ruolo e la
selezione la superficie selezionata del ruolo; i colori di testo sono scelti per
restare leggibili (bianco o nero, quello che contrasta di più).

**`contrast=`** è il rapporto minimo di contrasto richiesto per l'inchiostro:

- default **4.5** (WCAG AA, testo normale) quando si imposta un ruolo;
- `contrast=7` chiede AAA; `contrast=3` basta per testo grande e bordi;
- `contrast=off` (o `0`) disattiva la garanzia.

Il rapporto di un polo (bianco o nero) su qualunque sfondo è sempre almeno ~4.58,
quindi **AA è sempre raggiungibile**; una soglia AAA su uno sfondo di luminanza media
non è fisicamente raggiungibile e viene restituito il polo migliore.

**Limiti di dimensione** (`minw/minh/maxw/maxh`):

- valgono **già alla creazione** (un minimo dichiarato non viene ignorato fino al
  primo resize) e a ogni ricalcolo;
- un limite `0` significa "nessun limite"; un valore negativo viene letto come `0`;
- se minimo e massimo si contraddicono (`minw` > `maxw`) **vince il massimo**, che è il
  vincolo visivo;
- il valore è nell'**unità del controllo**: pixel nel percorso GRAPHICS, celle nel
  percorso TEXT.

**Quota del delta** (`grow=`):

- `grow=100` (default) = il controllo assorbe tutto il delta: con `EXPAND H` la
  larghezza diventa `base + delta`;
- `grow=50` = assorbe metà del delta: due pannelli affiancati che crescono entrambi
  "tutto" si sovrapporrebbero, con `grow=50` ciascuno prende la sua parte;
- `grow=0` = non cresce (ma i limiti continuano a valere);
- il residuo resta al contenitore: **la somma delle quote non deve fare 100**;
- valori fuori scala vengono riportati in `0…200`; il calcolo è intero
  (`dx * grow / 100`), quindi deterministico e identico su tutte le gambe.

`grow` vale su entrambi gli assi quando il controllo è agganciato su entrambi i bordi
(`EXPAND BOTH`, `ANCHOR ALL`).

## 4. Esempio completo

`shared/samples/graphics/ThemeRoles.bas` — base 640×320 → 960×480 (delta +320 in x,
+160 in y), eseguito headless: stampa la tabella dei rect prima, dopo il resize, dopo
un cambio tema e dopo il ritorno alla base. Valori attesi:

| Controllo | Proprietà | Prima | Dopo |
|---|---|---|---|
| `bSave` | `role=primary` `EXPAND H` `grow=50` | w=160 | w=320 (160 + 320/2) |
| `bDel` | `role=danger` `ANCHOR RIGHT` | x=400 | x=720 |
| `bInfo` | `role=info` `EXPAND H` `maxw=300` | w=160 | w=300 (tetto) |
| `bMuted` | `role=muted` `EXPAND H` `grow=0` `minw=240` | w=240 | w=240 |
| `bNone` | nessuna proprietà | x=40 w=120 | invariato |

## 5. Disponibilità per gamba e percorso

| Gamba | Percorso | Come arriva lo `STYLE` |
|---|---|---|
| C99Native | TEXT e PIXEL | diretta: il C generato chiama il runtime |
| VM/C | TEXT e PIXEL | diretta: `gmb_control_apply_style_handle` |
| IL / PCodeVM | TEXT e PIXEL | P/Invoke a `gmb_control_apply_style_handle` (stessa funzione C) |

Non esiste una variante `C…` separata e non c'è alcun comportamento polimorfico da
forzare: la chiave è letta dallo stesso codice C su qualunque bersaglio (console,
finestra, grafica). Su un programma headless i colori non hanno effetto visibile, ma
i **limiti e la quota del layout** sì: i rect si possono leggere con
`CONTROL GET nome, "width"` / `"height"`, come fanno MiniTest ed esempio.

## 6. Riferimenti

- Implementazione: `GMBasic/native/gmb_runtime_c/src/gmb_theme_roles.c`,
  `include/gmb_theme_roles.h`, `src/gmb_layout.c`, `include/gmb_layout.h`,
  `src/gmb_tui_style.c` (cascata TEXT), `src/gmb_form.c` (percorso PIXEL),
  `src/gmb_tui_app.c` (parsing delle chiavi nello `STYLE`).
- Test dell'unità: `GMBasic/native/gmb_runtime_c/tests/gmb_theme_roles_selfcheck.c`
  (contrasto WCAG di riferimento, AA su ogni ruolo × 15 preset, cascata TUI, override,
  cambio tema a runtime, `grow`/`minw`/`maxw`, idempotenza del reflow).
- MiniTest: `shared/minitests/controls/layout_clamp.bas` (limiti e quota),
  `shared/minitests/controls/layout_manager.bas` (EXPAND/ANCHOR/ALIGN).
- Esempio: `shared/samples/graphics/ThemeRoles.bas`,
  `shared/samples/graphics/LayoutManager.bas`.

## 7. Accessibilità runtime — flag globali dell'interfaccia

Quattro flag **globali** (non per controllo) che si attivano **senza toccare il
programma**: valgono su tutta l'interfaccia, sia sul percorso TEXT/TUI sia su
GRAPHICS/PIXEL, e anche a runtime (il runtime li rilegge a ogni disegno).

| Voce del registro | Valore | Effetto |
|---|---|---|
| `ui.high_contrast` | `1` / `0` (alias `on/off/true/false/si/no`) | testo **AAA (7:1)** e bordi **4.5:1** su ruoli e tema; anche il percorso PIXEL garantisce testo, testo disabilitato, bordo e focus leggibili |
| `ui.text_scale` | numero `0.75…3`, default `1` | scala il testo dell'interfaccia (font della console, big text, font grafico configurato); `0`/negativo/non numerico = `1` |
| `ui.reduce_motion` | `1` / `0` | niente blink del cursore (resta acceso) e niente ridisegno periodico ogni 200 ms: si ridisegna solo su danno/forzatura |
| `ui.focus_always_visible` | `1` / `0` | il controllo a fuoco ha sempre un fondo distinguibile: se il tema non distingue il focus dal normale, se ne deriva uno (superficie normale mescolata con l'accento, o inversione) |

Si impostano dal registro opzioni, quindi **non sono variabili d'ambiente**:

```basic
AI SET OPTION "ui.high_contrast", "1"
AI SET OPTION "ui.text_scale", "1.5"
AI SET OPTION "ui.reduce_motion", "1"
AI SET OPTION "ui.focus_always_visible", "1"
```

oppure, per tutta l'applicazione, dalla sezione `engine` dei settings:

```json
{ "engine": { "ui.high_contrast": "1", "ui.text_scale": "1.5" } }
```

Lettura: `v = AI.OPTION("ui.high_contrast")`; elenco completo con
`AI LIST OPTIONS`. Valore vuoto = default (contrasto normale, scala 1.0, movimento
e focus del tema), quindi un programma che non imposta nulla non cambia
comportamento.

**Note di semantica.**

- `ui.high_contrast` alza le soglie in **un solo punto** (`gmb_a11y_min_text_ratio` /
  `gmb_a11y_min_border_ratio`) usato dai ruoli e dalla cascata TUI; sul percorso
  PIXEL lo stile del widget viene corretto alla creazione. È una scelta
  dell'**utente**, quindi prevale anche su un `fg=` esplicito del programma.
  AAA non è sempre fisicamente raggiungibile (sfondi di luminanza media): in quel
  caso si usa il polo più leggibile (bianco o nero), che è il massimo possibile.
- `ui.text_scale` non tocca le dimensioni **automatiche** (`0` = eredita): scala solo
  una dimensione già decisa.
- `ui.focus_always_visible` non sovrascrive un `focuscolor=` esplicito del programma
  né un tema che già distingue il focus.
- Su IL/PCodeVM il valore dei flag viene **inoltrato al registro C** (`gmb_opt_set`)
  perché è il codice C a disegnare: senza inoltro il set riuscirebbe ma il colore o
  la scala non cambierebbero.

Test: `shared/minitests/controls/a11y_options.bas` (round-trip del registro),
`shared/minitests/ai/ai_list_options.bas` (le quattro voci nell'elenco) e
`tests/gmb_theme_roles_selfcheck.c` (`test_a11y`: parse, registro → snapshot, AAA su
ogni preset, anello di focus, scala, movimento).

## 8. Filtro daltonia — `ui.color_filter`

Quinta voce globale, stessa natura dei flag di accessibilità: si attiva **senza
toccare il programma** e vale su tutta l'interfaccia. Trasforma i colori con una
matrice 3×3 (Machado, Oliveira & Fernandes 2009, grado 1.0) applicata all'RGB.

| Valore | Effetto |
|---|---|
| `""` / `none` | default: nessuna trasformazione |
| `deuteranopia` | deficit del verde (la forma più diffusa) |
| `protanopia` | deficit del rosso |
| `tritanopia` | deficit del blu |
| `achromatopsia` | nessun colore: solo luminanza (grigio) |

Il confronto è **case-insensitive** e accetta abbreviazioni e alias: i prefissi
`deut`, `prot`, `trit`, `achrom`, e `gray` / `grey` / `mono` per l'acromatopsia.
Si accettano anche i nomi italiani `rosso`, `verde`, `blu`. **Un valore sconosciuto
spegne il filtro** (non è un errore: la voce resta tollerante come gli altri flag).

```basic
AI SET OPTION "ui.color_filter", "deuteranopia"
v$ = AI.OPTION("ui.color_filter")      ' -> "deuteranopia"
AI SET OPTION "ui.color_filter", ""    ' spento
```

oppure per tutta l'applicazione, dalla sezione `engine` dei settings:

```json
{ "engine": { "ui.color_filter": "deuteranopia" } }
```

**Dove si applica (e dove no).** Il filtro agisce sui **punti di uscita finali**,
non sulle sorgenti: una sola trasformazione, quindi nessun colore viene filtrato
due volte.

| Coperto dal filtro | Non filtrato (di proposito) |
|---|---|
| present SDL/Vulkan (i pixel che finiscono a schermo) | il colore `default` del terminale (`is_default`): è la scelta del terminale, non un RGB noto |
| `SAVE SCREEN` (sia la cattura GPU sia il fallback headless) | la **pagina** come dato: `GRAPHICS.POINT` / `IMAGEGET` leggono i colori originali |
| codifica ANSI della TUI (`gmb_render_screen` / `_diff`) | |
| rendering a celle di SDLConsole | |

Conseguenza pratica: `SAVE SCREEN` mostra **ciò che si vede** (filtrato), mentre
il programma che rilegge un pixel dalla pagina ottiene il colore **originale**.
È la scelta che rende il filtro una lente di presentazione e non una modifica
dei dati.

Il byte alto dei colori `0xAARRGGBB` (alpha/flag delle superfici traslucide dei
temi "glass") è **conservato**: si trasformano solo i 24 bit RGB. Il cambio di
filtro invalida il frame da solo (hook `on_change`), quindi non serve ridisegnare
a mano né ricreare i controlli.

Test: `shared/minitests/controls/ui_color_filter.bas` (round-trip del registro,
prefissi e alias, valore sconosciuto = spento) e
`tests/gmb_color_filter_selfcheck.c` (matrici, coerenza cella/RGBA, alpha
conservato).

## 9. Golden image della UI — `tools/sh/ui_golden_check.sh`

Il filtro e i ruoli del tema sono **visivi**: un selfcheck prova la matematica, ma
non che i pixel arrivino a schermo. Il gate lo verifica con un'immagine di
riferimento (golden) per RID.

```bash
tools/sh/ui_golden_check.sh              # gate: esce 1 se è rosso
tools/sh/ui_golden_check.sh --quiet      # per la CI
tools/sh/ui_golden_check.sh --tol 0.5    # tolleranza gmbimgdiff in percentuale
tools/sh/ui_golden_check.sh --bless      # RISCRIVE i golden del RID
```

Cosa fa, eseguendo `shared/minitests/controls/ui_golden.bas` in un SDL headless
(driver `dummy`):

1. **Coppia normale/filtrata.** Per la UI completa e per una palette a colori noti
   confronta la schermata normale con quella filtrata: devono **differire**. Se
   sono identiche il filtro non sta arrivando ai pixel e il test non prova nulla.
2. **Golden per RID.** Confronta le 4 schermate con
   `shared/minitests/controls/golden/<nome>.<rid>.png` usando `gmbimgdiff`
   (default `tol=0.0`: la resa è deterministica).

| Golden | Contenuto |
|---|---|
| `ui_golden.<rid>.png` | UI completa (griglia `flow=grid`, ruoli del tema, controlli) |
| `ui_golden_deut.<rid>.png` | la stessa con `ui.color_filter = deuteranopia` |
| `ui_palette.<rid>.png` | dipinto a colori noti (palette VGA16) |
| `ui_palette_deut.<rid>.png` | lo stesso con il filtro |

**I golden sono per RID** (`osx-arm64`, `osx-x64`, `linux-x64`, `linux-arm64`):
la resa di testo e superfici può cambiare tra piattaforme, quindi un golden non si
copia da un RID all'altro.

**Regola del bless.** `--bless` riscrive i golden del RID corrente. Si usa **solo
dopo aver guardato le immagini** prodotte: un bless fatto "per far tornare verde"
congela un difetto. Il flusso corretto è: eseguire, aprire le 4 PNG, verificare a
occhio che la UI sia integra (chrome, bordi, testo) e che la versione filtrata sia
plausibile, poi benedire.

Build, esecuzione e screenshot avvengono in `${TMPDIR}/gmb-ui-golden-<rid>`:
**fuori dal repo**, così il working tree condiviso non si sporca.

**Note pratiche del campione** (valgono per chi scrive un golden nuovo):

- i dipinti puri (`LINE … "BF"`, palette) vanno fatti **prima** di creare i
  controlli: il form possiede la pagina e li ridisegnerebbe sopra;
- la UI si crea **una sola volta**; il cambio di filtro non richiede ricreazione;
- il titolo di un `PANEL` traslucido resta sotto i figli: per un contenitore di
  griglia si usa un titolo vuoto con un `pad` adeguato;
- nei campioni grafici serve `shot_completo = 1` prima di `SCREEN`;
- `WINDOW THEME` ridispone i figli del flow: nel golden non si cambia tema.

Test: `shared/minitests/controls/ui_golden.bas` (campione) e
`tools/sh/ui_golden_check.sh` (gate).
