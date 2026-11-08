# shared/libs/quant — `libgmbquant` (FFI)

Copia compilata della libreria di sviluppo `GMBSuite/gmbquant`: espone in BASIC, via
FFI, le funzioni della superficie `TRADING.*` della StdLib.

## Layout

| percorso | contenuto |
|---|---|
| `include/gm_quant*.h` | API C pubblica (copiata da `GMBSuite/gmbquant/include`) |
| `src/gq_*.c` | implementazione (copiata da `GMBSuite/gmbquant/src`) |
| `src/quant_ffi.c` | shim ABI FFI (`ofx_q_*`), unico file non in gmbsuite |
| `shared/include/{trading,metrics,volatility,fundamentals,bonds,derivatives}.inc` | export BASIC (il build li sposta dal lib dir a `shared/include/`) |

## Export e namespace (ondata 3, 89 funzioni)

| `.inc` | namespace | funzioni |
|---|---|---|
| `trading.inc` | `TRADING` | 12 (valutazione azionaria, CAPM, rendimenti) |
| `metrics.inc` | `METRICS` | 18 (performance e rischio) |
| `volatility.inc` | `VOLATILITY` | 3 (volatilità, downside) |
| `fundamentals.inc` | `FUNDAMENTALS` | 11 (EPS/DPS/NAV, P/E, P/B, P/S, payout, yield) |
| `bonds.inc` | `BONDS` | 9 (prezzo/rendimento, duration, BEY, current yield, YTM, ZCB) |
| `derivatives.inc` | `DERIVATIVES` | 16 (Black-Scholes/Merton, binomiale, 11 greche) |
| `portfolio.inc` | `PORTFOLIO` | 19 (metriche multi-asset, min-var/max-Sharpe/risk-parity, Kelly, BL, CVaR/CDaR) |
| `backtest.inc` | `BACKTEST` | 1 (`SIMULATE` su matrice 2-D + `RUNFLAT`, pesi uguali) |

`TRADING` è il nome di transizione: quando i builtin `TRADING.*` saranno rimossi
il namespace prenderà il nome `TRADING` (vedi `Guide/utente/Quant-Guida-Utente.md`).

`PORTFOLIO` e `BACKTEST` restituiscono **ARRAY** (non più il `PortfolioResult`/`BacktestResult`):
`[rendimento, volatilità, Sharpe, pesi...]` e `[metriche, curva...]`. L'ABI è documentata
nell'intestazione dei due `.inc`; è la stessa scelta di `optimize.inc` (le strutture
composte non attraversano l'FFI). Anche le matrici sono passate appiattite dai wrapper.

## Build e verifica

```bash
bash tools/sh/gmb_lib.sh build quant    # ricompila shared/libs/quant/libgmbquant.*
bash tools/sh/gmb_lib.sh check quant    # install_name + presenza degli export .inc
GMB_MINI_ONLY="quant/" tools/sh/test_minitests_c99native
zsh tools/sh/test_minitests_parity.sh "quant/"
zsh tools/sh/parity4 ../shared/samples/Quant/QuantLab.bas
```

La dylib va copiata in `GMBasic/build/libs/<rid>/` e `~/.gmbasic/libs/<rid>/` perché le
gambe IL/PCode/VM-C la caricano da lì (`gmb_lib.sh build` non lo fa).

## Note

- La semantica (rendimenti in frazione, `DividendYield` in percentuale, input non
  valido → 0, date `BONDS` come OADate) è quella dei builtin; il riferimento di parità è
  `CommonLib.Trading`.
- **C# usa il P/Invoke**: le vecchie formule C# (`General.cs`, `Metrics.cs`, `Bonds.cs`,
  `Options.cs`) sono archiviate in `gmbasicmonorepo-deprecated/`; al loro posto ci sono
  facade che chiamano `gq_*` di questa lib. IL/PCodeVM passano da qui, VM/C e C99Native
  usano la stessa lib direttamente.
- Nessuna dipendenza esterna oltre libm; monodirezionale `gmbquant -> gmbmaths`.
- Il codice C è duplicato qui e in `GMBSuite/gmbquant` per scelta: la copia di
  sviluppo ha il `Makefile` + self-test, questa è quella compilata dall'FFI.
- Questa è una **lib** (`shared/libs/quant`), non un package: nessuna copia in
  `packages/` o `shared/packages/`.
