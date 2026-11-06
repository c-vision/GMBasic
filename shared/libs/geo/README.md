# gmbgeo — geometria piana / GIS (namespace BASIC `GEO`)

**Prefisso C:** `geo_` · **Dipendenze:** C99 + libm (self-contained)

Port FFI di **gmbgeo** (ispirato a shapely/geopandas): misure, predicati, distanze,
forme (hull/simplify/trasformazioni), ritaglio, overlay booleano, geodesia,
I/O GeoJSON/WKT e indice spaziale a griglia.

## Rappresentazione

Ogni geometria è un **array del TYPE `GeoPoint`** (`X`, `Y` AS DOUBLE) + il numero di
vertici `n`. Il layout del TYPE è identico a quello della struct C `geo_pt`, quindi il
buffer viaggia contiguo (copy-in/copy-out dell FFI) e i punti singoli si passano e
tornano **per valore**. Il bounding box è il TYPE `GeoBox` (`MinX`, `MinY`, `MaxX`,
`MaxY`). Poligoni: vertici in ordine, senza ripetere il primo; area/perimetro
assumono il verso (antiorario = area positiva).

## Superficie (`shared/include/geo.inc`, `IMPORT GEO`)

| famiglia | funzioni |
|---|---|
| misure | `POLYAREA`, `PERIMETER`, `CENTROID` (ritorna `GeoPoint`), `DIST` |
| forma (ritorni) | `CENTROID` → `GeoPoint`, `BBOX` → `GeoBox` |
| predicati / distanze | `POINTINPOLY`, `ONSEGMENT`, `DISTPOINTSEG`, `DISTSEGSEG` |
| intersezione | `SEGINTERSECT`, `SEGINTERSECTION` (punto in `out(0)`, array di 1 `GeoPoint`) |
| forma | `BBOX` (`out(0..3)=minx,miny,maxx,maxy`), `CONVEXHULL`, `SIMPLIFY` (Douglas-Peucker), `TRANSLATE`, `SCALE`, `ROTATE`, `CLIP` (Sutherland-Hodgman, clip convesso antiorario) |
| geodesia | `PROJECT`/`UNPROJECT` (ritornano `GeoPoint`: est/nord in metri, lon/lat in gradi), `HAVERSINE` (metri, sfera R=6378137) |
| overlay | `OVERLAY` (0=intersezione, 1=unione, 2=differenza A−B; Greiner-Hormann) |
| spatial join | `SPATIALJOIN` (offset `aoff`/`boff` sull array di `GeoPoint`, coppie in `pairs()`) |
| I/O | `GEOJSONPARSE`, `GEOJSONWRITE`, `WKTPARSE`, `WKTPOINT`, `WKTWRITE` |
| indice | `GRIDNEW`, `GRIDQUERY` (finestra `GeoBox`), `GRIDFREE` (handle 7301+) |

## Cosa NON è qui (già coperto dalla superficie BASIC)

`GEO.CONVEXHULL` restituisce il **poligono** (xs/ys) e `SPATIAL.CONVEXHULL` gli
**indici** dei vertici su una matrice X/Y: sono due forme diverse, entrambe disponibili.
`GEO.WKTWRITE` è un'aggiunta (gmbgeo ha solo il parser WKT): serve al round-trip dei test.

| gmbgeo | dove |
|---|---|
| distanze batch fra insiemi di punti | `SPATIAL.PDIST` / `CDIST` / `DISTANCEMATRIX` — `GEO.DIST` resta la primitiva **scalare** usata anche dalle altre funzioni |
| trigonometria (sin/cos per rotazioni custom) | `MATH` |
| operazioni su matrici | `MATRIX` |
| esperimenti/varianti non geometriche | — |

## Sorgenti

| file | origine |
|---|---|
| `src/gmbgeo.c`, `src/gmbgeo_ops.c`, `include/gmbgeo.h` | `gmbsuite/gmbgeo` (copia identica) |
| `src/geo_ffi.c` | shim ABI di questo pacchetto (array paralleli, handle griglia) |

## Guida, sample e prove

- Guida utente (vault): `Guide/utente/GEO-Guida-Utente.md` (riferimento completo).
- Sample: `shared/samples/Geo/GeoWorkbench.bas` (pratica edilizia che usa tutte le 30 funzioni).
- Manuale: `docs/manual/capitoli/V.31c-geo.md`.
- Parit00e0: `zsh tools/sh/parity4 ../shared/samples/Geo/GeoWorkbench.bas`.

## Build e test

```sh
shared/libs/geo/build.sh
GMB_MINI_ONLY="geo/geo_planar.bas" tools/sh/test_minitests_c99native
zsh tools/sh/test_minitests_parity.sh "geo/geo_io.bas"
```

MiniTest: `geo/geo_planar.bas` (misure, predicati, forma, clip, overlay) e
`geo/geo_io.bas` (proiezioni, I/O GeoJSON/WKT, spatial join, griglia).
