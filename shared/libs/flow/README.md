# gmbflow — flussi di rete e matching (namespace BASIC `GRAPH`)

**Prefisso C:** `gor_` · **Dipendenze:** C99 + libm (self-contained)

Pacchetto FFI del dominio flussi/matching del namespace `GRAPH`. Motore `gmbortools`
(`gor_flow`, `gor_flow2`, `gor_graph`), copiato qui per essere autonomo. Binding BASIC:
`shared/include/flow.inc` (`IMPORT GRAPH`).

## Superficie

| Gruppo | Funzioni |
|---|---|
| Max-flow / min-cut | `MAXFLOW`, `MINCUT`, `DINIC` |
| Min-cost flow | `MINCOSTFLOW` |
| Matching | `BIPARTITEMATCH`, `GENERALMATCH`, `WEIGHTEDMATCH` |
| Circolazione / cut tree / multicommodity | `CIRCULATION`, `GOMORYHU`, `MULTICOMMODITY` |

Convenzioni:
- capacità/adiacenza: matrici dense `n×n` row-major; capacità `0` = arco assente;
- `MINCUT(..., side())`: `side(i)=0` lato sorgente, `1` lato pozzo;
- `MINCOSTFLOW(..., flowValue())`: `flowValue(0)` = valore del flusso;
- `MULTICOMMODITY`: `commodities()` = `ncomm*3` (`src, dst, demand`).

## Build

```sh
shared/libs/flow/build.sh
codesign --force --sign - shared/libs/flow/libgmbflow.dylib   # macOS
```

## Test

`shared/minitests/flowffi/`
