# Controls — BASIC control library

Controls written in **BASIC** as classes `CLASS <Name> INHERITS Control` (UserControl), using only
the `CONTROLS` base plus the `GraphicsContext` primitives. No new verb/TokenKind: **4-backend**
parity by construction (IL / PCode / VM-C / C99Native).

## Contents
Controls (`*.bas`), **classes only** in `NAMESPACE CustomControls` (distinct from the built-in
`CONTROLS` namespace):
- `CandleChart`, `Depth` (MarketDepth/DOM), `OrderTicket`, `Watchlist`, `Splitter`,
  `ContextMenu`, `Segmented`, `DataGrid`, `TradingMinors` (Ticker / TreeGroup / EconCalendar /
  Toolbar).

The **examples/demos** are NOT here: they live in `samples/Controls/` (they import
`CustomControls`). The **built-in** controls (AlertBox, ListBox, TBROWSE, …) are the `CONTROLS`
namespace (shared C runtime, `native/gmb_runtime_c`).

## Usage
```
IMPORT CONTROLS         ' Control base + GraphicsContext primitives
IMPORT CustomControls   ' the controls of this folder
DIM s AS Segmented = NEW Segmented()
```

## Conventions
- Every library file is **classes only** (`NAMESPACE CustomControls`, `PUBLIC CLASS`); the examples
  are in `samples/Controls/`.
- User guide: `Guide/utente/*-Controllo-Guida-Utente.md`.
- Tests: `minitests/controls/`, `minitests/tui/` (`IMPORT CustomControls`).

> See `Todo/(open) - CONTROLS-Port-Clipper-Passo-Passo.md` §12.
