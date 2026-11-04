# gmbgametheory — teoria dei giochi (namespace BASIC `OPTIMIZE`)

**Prefisso C:** `gor_` · **Dipendenze:** C99 + libm (self-contained)

Giochi a somma zero e cooperativi. Motore `gmbortools` (`gor_game`, `gor_game_minimax`),
con `gm_linalg_solve` minimale locale per non trascinare gmbmath. Binding:
`shared/include/gametheory.inc`.

## Superficie (5)
| Funzione | Contenuto |
|---|---|
| `GAMESADDLE` | saddle point in strategie pure |
| `GAMEVALUE` | valore misto via LP + strategie |
| `GAMEMINIMAX` | Brown-Robinson (`out` = valore, p, q) |
| `NASH2P` | Nash bimatrice (`out` = p, q) |
| `SHAPLEY` | valore di Shapley con callback `v(mask AS DOUBLE)` (il trampolino FFI non ammette firme miste LONG/DOUBLE) |

## Build
```sh
shared/libs/gametheory/build.sh
codesign --force --sign - shared/libs/gametheory/libgmbgametheory.dylib
```
