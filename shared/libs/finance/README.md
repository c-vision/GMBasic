# finance — lib FFI del namespace FINANCE

Formule finanziarie pure (rendite, ammortamenti, flussi di cassa, ratei, date
finanziarie) usate da IL, PCodeVM, VM/C e C99Native tramite `IMPORT FINANCE`.

- `src/finance.c` — implementazione C (`ofx_fin_*`), self-contained (`<math.h>`).
- `include/gmbfinance.h` — API pubblica.
- `build.sh` — `gmb_lib.sh build finance` → `libgmbfinance.<ext>`.
- export BASIC: `shared/include/finance.inc` (spostato dalla build, namespace FINANCE).

Le date sono **OADate** (giorni dal 1899-12-30): il wrapper `.inc` converte i
`DATETIME` del linguaggio.

## Test e documentazione

- MiniTest: `shared/minitests/finance/` (un file per funzione + `finance_all.bas`).
- Sample completo: `shared/samples/Finance/FinanceGeneralTests.bas` (usa tutte le 23 funzioni).
- Guida utente: `Guide/utente/FINANCE-Guida-Utente.md` (vault).
- Implementazione C# storica archiviata: `gmbasicmonorepo-deprecated/finance/`.
