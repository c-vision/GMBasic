# STYLE — ruoli semantici del tema e limiti del layout

> **Stato**: nessuna clausola nuova, nessun comando nuovo: sono **chiavi dello
> `STYLE`** già esistente sui controlli (`BUTTON`, `LABEL`, `PANEL`, `LISTBOX`, …) più le
> clausole di layout `EXPAND` / `ANCHOR` / `ALIGN`. Valgono **sulle quattro gambe**
> (IL, PCodeVM, VM/C, C99Native) e su **entrambi i percorsi** (GRAPHICS/PIXEL e TEXT/TUI).
> Implementazione condivisa: `GMBasic/native/gmb_runtime_c/src/gmb_theme_roles.c` e
> `src/gmb_layout.c`. MiniTest: `shared/minitests/controls/layout_clamp.bas`,
> `shared/minitests/controls/layout_manager.bas`. Esempio completo:
> `shared/samples/graphics/ThemeRoles.bas`.

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
