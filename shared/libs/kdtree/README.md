# kdtree — lib FFI del namespace KDTREE

`scipy.spatial.KDTree`: build su punti flat (n × m), query k-NN (indici o
distanze) e query_ball_point. Usata da IL, PCodeVM, VM/C e C99Native tramite
`IMPORT KDTREE`.

- `src/kdtree.c` — implementazione C (`ofx_kd_*`), self-contained, tabella
  statica di handle (`int`).
- `include/gmbkdtree.h` — API pubblica (query su buffer del chiamante).
- `build.sh` — `gmb_lib.sh build kdtree` → `libgmbkdtree.<ext>`.
- export BASIC: `shared/include/kdtree.inc` (spostato dalla build, namespace KDTREE).

## Test e documentazione

- MiniTest: `shared/minitests/kdtree/` (`create`, `query`, `querydist`,
  `queryball`, `kdtree_all`).
- Sample: `shared/samples/Math/KdTreeSciPyParity.bas`, `KdTreeAdvanced.bas`.
- Guida utente: `Guide/utente/KdTree-Manual.md` (vault).
- Implementazione C# storica archiviata: `gmbasicmonorepo-deprecated/kdtree/`.
