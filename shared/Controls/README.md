# Controls — libreria di controlli BASIC

Controlli scritti in **BASIC** come classi `CLASS <Nome> INHERITS Control` (UserControl),
usando solo la base `CONTROLS` + le primitive del `GraphicsContext`. Nessun verbo/TokenKind
nuovo: parità **4 backend** per costruzione (IL / PCode / VM-C / C99Native).

## Contenuto
Controlli (`*.bas`), **solo classi** `NAMESPACE CustomControls` (distinto dal namespace
built-in `CONTROLS`):
- `CandleChart`, `Depth` (MarketDepth/DOM), `OrderTicket`, `Watchlist`, `Splitter`,
  `ContextMenu`, `Segmented`, `DataGrid`, `TradingMinors` (Ticker / TreeGroup /
  EconCalendar / Toolbar).

Gli **esempi/demo** NON stanno qui: sono in `samples/Community/Controls/` (importano
`CustomControls`). I controlli **built-in** (AlertBox, ListBox, TBROWSE, …) sono il namespace
`CONTROLS` (runtime C condiviso, `native/gmb_runtime_c`).

## Uso
```
IMPORT CONTROLS         ' base Control + primitive GraphicsContext
IMPORT CustomControls   ' i controlli di questa cartella
DIM s AS Segmented = NEW Segmented()
```

## Convenzioni
- Ogni file-libreria è **solo classi** (`NAMESPACE CustomControls`, `PUBLIC CLASS`); gli esempi sono in `samples/Community/Controls/`.
- Guida utente: `Guide/utente/*-Controllo-Guida-Utente.md`.
- Test: `minitests/controls/`, `minitests/tui/` (`IMPORT CustomControls`).

> Vedi `Todo/(open) - CONTROLS-Port-Clipper-Passo-Passo.md` §12.
