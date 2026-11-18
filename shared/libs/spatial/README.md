# spatial — lib FFI del namespace SPATIAL

Distanze e geometria 2D (scipy.spatial): `PDIST`, `CDIST`, `DISTANCEMATRIX`,
`CONVEXHULL`, usate da IL, PCodeVM, VM/C e C99Native tramite `IMPORT SPATIAL`.

- `src/spatial.c` — implementazione C (`ofx_sp_*`), self-contained (`<math.h>`).
- `include/gmbspatial.h` — API pubblica (input flat row-major + buffer di uscita).
- `build.sh` — `gmb_lib.sh build spatial` → `libgmbspatial.<ext>`.
- export BASIC: `shared/include/spatial.inc` (spostato dalla build, namespace SPATIAL).

Il wrapper `.inc` appiattisce le matrici jagged del linguaggio e ricostruisce la
`MATRIX` per i risultati matriciali (stesso handle di prima: `MATRIX.GETVALUE`
continua a funzionare).

## Test e documentazione

- MiniTest: `shared/minitests/spatial/` (`cdist`, `convexhull`, `distancematrix`,
  `pdist`, `spatial_all`).
- Sample completo: `shared/samples/Spatial/SpatialDistances.bas` (usa tutte le funzioni).
- Guida utente: `Guide/utente/SPATIAL-Guida-Utente.md` (vault).
