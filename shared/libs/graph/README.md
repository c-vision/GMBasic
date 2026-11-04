# gmbgraph — teoria dei grafi (namespace BASIC `GRAPH`)

**Part of:** gmbsuite · **Prefisso C:** `gor_` · **Dipendenza:** C99 + libm (self-contained)

Pacchetto FFI del namespace BASIC **`GRAPH`**: algoritmi di teoria dei grafi su matrici
dense, motore `gmbortools` (`gor_graph`, `gor_graph2`, `gor_graph3`) copiato qui per essere
autonomo. Binding BASIC: `shared/include/graph.inc` (`IMPORT GRAPH`).

> I grafi dei **brain** (conoscenza/wiki) NON sono qui: sono nel namespace builtin `BRAINS`
> (`BRAINS.BFS`, …), con le funzioni wiki-only in `BRAINS.BYTAG` / `BRAINS.BYFOLDER` /
> `BRAINS.QUERYMULTI`. La separazione è del 2026-11-04: il vecchio builtin `GRAPH` è stato
> rinominato `BRAINS` per liberare `GRAPH` alla teoria dei grafi.

## Rappresentazione

- `adj()` — LONG, 0/1, matrice `n × n` row-major (`adj(i*n+j)` = arco i→j).
- `w()` — DOUBLE, peso; **arco assente = `GRAPH.ABSENT()`** (1e30). Lo shim normalizza
  ogni peso ≥ 1e29 a `INFINITY` (sentinella di gmbortools): BASIC non ha `INFINITY`.

Gli array `out()` sono dimensionati dal chiamante (`n` per order/comp/dist/color/comm,
`n*n` per clousure/subgraph, `n-1` per MST).

## Superficie (`shared/include/graph.inc`)

| Gruppo | Funzioni |
|---|---|
| Struttura | `DEGREE`, `HASCYCLE`, `ISBIPARTITE` |
| Traversamento | `BFS`, `DFS`, `TOPOSORT`, `COMPONENTS`, `SCC` |
| Percorsi | `DIJKSTRA`, `BELLMANFORD`, `FLOYDWARSHALL` |
| Centralità/community | `PAGERANK`, `BETWEENNESS`, `CLOSENESS`, `LOUVAIN` |
| Colorazione/clique/chiusura | `COLORING`, `MAXCLIQUE`, `TRANSITIVECLOSURE` |
| Proprietà | `DIAMETER`, `ECCENTRICITY`, `CLUSTERING`, `SUBGRAPH` |
| MST | `MSTKRUSKAL` |
| Sentinella | `ABSENT()` |

## Sorgenti (copia autonoma)

| file | origine |
|---|---|
| `src/gor_core.c`, `src/gor_graph.c`, `src/gor_graph2.c` | `gmbsuite/gmbortools` |
| `src/gor_graph3.c` | `gmbsuite/gmbortools` (nuovo: porting da `gmb_graph*` nativo) |
| `src/graph_ffi.c` | shim ABI di questo pacchetto (int64 ↔ int, costruzione `gor_graph`) |

Unica modifica rispetto all'originale: nessuna (i file restano identici); l'adattamento
dei tipi FFI vive in `graph_ffi.c`.

## Build

```sh
shared/libs/graph/build.sh            # -> shared/libs/graph/libgmbgraph.<ext>
tools/sh/gmb_lib.sh check graph       # install_name @rpath + file attesi
# macOS: dopo ogni rebuild la firma va rifatta
codesign --force --sign - shared/libs/graph/libgmbgraph.dylib
```

## Test

- Motore C: `gmbsuite/gmbortools` → `make test` (include `test_graph3`).
- BASIC 4 backend: `tools/sh/test_minitests_parity.sh graphffi/`
  (`graph_core.bas`, `graph_weighted.bas`).
