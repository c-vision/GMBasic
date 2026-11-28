# Vista testuale virtuale — guida utente

Una **LISTBOX** può mostrare un **testo** invece di voci fisse: un documento
Markdown reso in forma leggibile, un diff con le righe marcate, oppure una lista
molto grande (log, risultati di ricerca) a scorrimento virtuale.

Non è un controllo nuovo e non aggiunge comandi: la vista è una coppia di
**chiavi `STYLE`** (`mode=`, `virtual=`), il testo si assegna con
`CONTROL SET … "text"` e si legge riga per riga con `listcount` / `index`.
`STYLE` e `CONTROL SET` esistono già, quindi funziona identica sulle 4 gambe
(IL, PCodeVM, VM/C, C99Native) e sul percorso a celle **e** su quello pixel.

## 1. Sintassi

```basic
LISTBOX x, y, w, h, "" NAME "doc" STYLE "mode=markdown"
CONTROL SET "doc", "text", testo$

' oppure, in un colpo:
LISTBOX x, y, w, h, "" NAME "diff" STYLE "mode=diff virtual=1"
```

| Chiave `STYLE` | Valori | Significato |
|---|---|---|
| `mode=` | `plain` (default), `markdown`, `diff` | Come il testo viene impaginato |
| `virtual=1` | `1`/`0` | Lista grandi: le righe si dispongono a runtime (la vista nasce `plain`) |

Le due chiavi valgono **solo** per una LISTBOX: sugli altri controlli sono ignorate.
La larghezza di impaginazione è quella del controllo (in celle TUI, in celle
calcolate dal rect nel percorso pixel), quindi il testo si rifà capo da solo quando
la finestra si allarga o si restringe.

## 2. I tre modi

### `mode=plain`
Una riga di vista per ogni riga del testo (separatore `CHR$(13)`/`CHR$(10)`),
troncate alla larghezza del controllo. È il comportamento storico delle voci.

### `mode=markdown`
Il testo viene interpretato come Markdown e **impaginato**, non mostrato grezzo:

| Blocco Markdown | Resa |
|---|---|
| `# Titolo`, `## …`, `### …` | riga di titolo, con stile `HEAD1`/`HEAD2`/`HEAD3` del tema (il `#` non compare) |
| paragrafo | righe rifatte capo (`gmb_str_wrap`, conta i caratteri UTF-8) |
| `> citazione` | rientro `│ ` su ogni riga, stile `QUOTE` |
| `- voce` / `1. voce` | `• ` / `n. ` con continuazione allineata |
| ```` ``` ```` blocco codice ```` ``` ```` | righe verbatim, stile `CODE` |
| tabella `a|b` + riga `---` | colonne allineate, larghezza adattata al controllo |

I colori di riga arrivano dal **tema** (ruoli semantici); la riga selezionata
mantiene i colori di selezione normali. Lo stile di una riga si legge con
`CONTROL GET n, "rowstyle"`.

### `mode=diff`
Il testo è un diff unificato (`---`, `+++`, `@@`, `+riga`, `-riga`, ` riga`).
Ogni riga conserva il suo marcatore e riceve lo stile di ruolo del caso
(`DIFF_ADD`, `DIFF_DEL`, `DIFF_HUNK`), così aggiunte e rimozioni si distinguono
anche in scala di grigi.

## 3. Assegnare e leggere il testo

```basic
' assegnazione (il testo è la SORGENTE: la vista si ricostruisce)
CONTROL SET "doc", "text", testo$

' quante righe ha la vista
CONTROL GET "doc", "listcount" INTO n

' riga corrente: dopo SET "index" (o dopo una freccia) "text" è la riga DELLA VISTA
CONTROL SET "doc", "index", 3
CONTROL GETS "doc", "text" INTO riga$
```

- `CONTROL SET n, "text", s$` — quando il terzo argomento è una **stringa** il
  runtime la instrada sul canale testuale (in precedenza andava perduta: veniva
  letta come numero 0).
- `CONTROL SET n, "markdown", s$` / `"diff", s$` — assegnano il testo **e** scelgono
  il modo in un solo passo (equivalente a `mode=` nello `STYLE`).
- `CONTROL SET n, "items", s$` — comportamento storico: voci separate da `|`.
- `CONTROL GET n, "listcount"` — numero di righe della vista.
- `CONTROL GETS n, "text"` — riga corrente già **impaginata** (titolo senza `#`,
  diff con il marcatore): è ciò che l'utente vede, non la sorgente.
- `CONTROL GET n, "rowstyle"` — stile della riga corrente (`0` normale, `1/2/3`
  titolo di livello 1/2/3, `4` codice/tabella, `5` citazione, `6` regola,
  `7` diff aggiunta, `8` diff rimozione, `9` hunk `@@`, `10` intestazione diff).

La **sorgente** è sempre posseduta dal controllo: cambiare `mode=` via `STYLE`
(o `CONTROL SET … "markdown"` / `"diff"`) **reimpagina** il testo già assegnato,
non lo perde. Le righe mostrate sono sempre copie, quindi una lettura non
invalida la vista.

## 4. Lista grande (`virtual=1`)

Con grandi volumi (log di migliaia di righe) la LISTBOX resta una lista normale —
selezione, `index`, `toprow` — ma le righe vengono prodotte dalla vista solo per
l'altezza visibile più un margine: non si materializza l'intero contenuto a
colonne. Il numero totale è in `listcount`, così un contatore tipo
«riga 120 di 5000» si calcola senza scorrere il testo.

```basic
LISTBOX 1, 2, 80, 20, "" NAME "log" STYLE "virtual=1"
CONTROL SET "log", "text", log$
CONTROL GET "log", "listcount" INTO tot
CONTROL SET "log", "index", tot - 1     ' vai all'ultima riga (tail)
```

## 5. Disponibilità e verifiche

| Gamba | `mode=` / `virtual=` |
|---|---|
| IL | sì (parità per costruzione: chiavi `STYLE`, dati) |
| PCodeVM | sì |
| VM/C | sì |
| C99Native | sì |

- Motore: `GMBasic/native/gmb_runtime_c/include/gmb_textview.h`,
  `src/gmb_textview.c` (impaginazione) e `src/gmb_md.c` (parser Markdown).
- Selfcheck deterministico: target CMake `gmb_textview_selfcheck`.
- MiniTest: `shared/minitests/controls/ui_textview.bas` (campo `EXPECT`).
- Sample completo: `shared/samples/Controls/TextViewer.bas`.
- Golden image del pannello Markdown/diff: `tools/sh/ui_golden_check.sh`.

## 6. Riferimenti

- Ruoli e layout: `Guide/utente/STYLE-Ruoli-e-Layout-Guida-Utente.md`.
- Animazione dei controlli: `Guide/utente/Animazione-Guida-Utente.md`.