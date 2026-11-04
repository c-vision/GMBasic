# gmbmathprog — programmazione matematica (namespace BASIC `OPTIMIZE`)

**Prefisso C:** `gor_` · **Dipendenze:** C99 + libm (self-contained)

LP/MILP + model builder. Motore `gmbortools` (`gor_lp`, `gor_model`, `simplex`), copiato
qui. Binding BASIC: `shared/include/mathprog.inc` (`IMPORT OPTIMIZE`).

## Superficie (19 funzioni)
| Gruppo | Funzioni |
|---|---|
| LP | `LPSOLVE`, `LPREVISED`, `LPBOUNDS`, `LPDUAL`, `LPREDUCEDCOSTS`, `LPALTERNATE` |
| MILP | `MILPBB`, `MATHHARDFIXING`, `MATHLOCALBRANCHING`, `GOMORYCUT` |
| Model builder | `MODELNEW`, `MODELFREE`, `MODELADDVAR`, `MODELADDCONSTR`, `MODELSETOBJ`, `MODELSOLVE`, `MODELVARVALUE`, `MODELNUMVARS`, `MODELTOJSON` |

Convenzioni: forma standard `min c'x s.t. A x = b, x >= 0`; `intIdx()` = `nint | indici`;
`info()` impacchetta obj/fixFrac/rounds/kStart secondo la funzione.

Fuori per ora: `gor_lagr_subgradiente` (callback a 4 argomenti con out pointer, non
esprimibile col trampolino FFI attuale).

## Build
```sh
shared/libs/mathprog/build.sh
codesign --force --sign - shared/libs/mathprog/libgmbmathprog.dylib
```

## Test
`shared/minitests/mathprogffi/`
