# gmbor — ricerca operativa combinatoria (namespace BASIC `OPTIMIZE`)

**Prefisso C:** `gor_` · **Dipendenze:** C99 + libm (self-contained)

Pacchetto FFI della prima slice di OR combinatoria: **PACK (9) + ASSIGN (9) + COVER (14) +
CP (3) = 35 funzioni**. Motore `gmbortools` (`gor_pack`, `gor_assign`, `gor_cover`, `gor_cp`),
copiato qui. Binding BASIC: `shared/include/ortools.inc` (`IMPORT OPTIMIZE`).

## Convenzioni
- matrici dense row-major;
- output scalari singoli come array di 1 (`nChosen()`, `uncovered()`, ...);
- dove la firma C supera 8 argomenti, i parametri di servizio sono compattati o fissati:
  - `SET_COVER_SA`: `t0=1.0`, `alpha=0.995`;
  - `SET_COVER_GRASP`/`SET_COVER_RGRASP`/`KMEANS`: seed fisso;
  - `SET_COVER_RGRASP`: `alphas()` = `[nalpha, a0, a1, ...]`.

## Build
```sh
shared/libs/or/build.sh
codesign --force --sign - shared/libs/or/libgmbor.dylib   # macOS
```

## Test
`shared/minitests/orffi/`
