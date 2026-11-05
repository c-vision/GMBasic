# gmbscheduling — cammini con finestre temporali (namespace BASIC `OPTIMIZE`)

**Prefisso C:** `gsc_` · **Dipendenze:** C99 + libm (self-contained)

Scheduling su grafi temporali: dato un grafo orientato con una finestra `[start, end]` per
nodo, elenca i cammini `s -> t` in cui ogni arco rispetta il tempo minimo di connessione
(`end[u] + min_connect <= start[v]`). Arriva da `gmbortools/gor_graph2.c` (era
`paths_time_window`), spostato qui perché è **scheduling**, non teoria dei grafi.
Binding: `shared/include/scheduling.inc` → `OPTIMIZE.SCHEDPATHSTIMEWINDOWS`.

## Superficie (1)
| Funzione | Contenuto |
|---|---|
| `SCHEDPATHSTIMEWINDOWS` | cammini `s->t` ammissibili; `paths()` = maxPaths*n, `lens()` = maxPaths |

## Parametri
- `adj()`: `n*n` row-major, `1` = arco `u->v`;
- `windows()`: `2*n+1` valori = `start(0..n-1) | end(0..n-1) | minConnect`;
- `s`, `t`: nodo iniziale/finale;
- `maxPaths`: quanti cammini restituire (i primi trovati in DFS).
