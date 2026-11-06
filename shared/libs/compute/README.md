# compute — sorgenti condivisi di `gmbcompute`

**Non produce una dylib.** Questo contenitore esiste per tenere **una sola copia** dei
sorgenti del progetto `gmbsuite/gmbcompute` (layer `gc_*`: array, BLAS 1/2/3, GEMM,
sparse, pool, SIMD, checksum, compress, graphblas, double-double).

## Chi lo usa

Viene compilato **dentro** le dylib che ne hanno bisogno, tramite `shared-src.txt`:

| libreria | riga in `shared-src.txt` | cosa prende |
|---|---|---|
| `shared/libs/math` | `compute` | tutti i `gc_*.c` |
| `shared/libs/stats` | `compute` + `math` | `gc_*` + `gm_*`/`gmi_*` |
| `shared/libs/graphics` | `compute/src/gc_core.c` + 4 file di `math` | solo il nucleo |

Il meccanismo è in `tools/sh/gmb_lib.sh` (`build_one`): una riga senza `.c` indica un intero
contenitore (`shared/libs/<nome>/src`, ricorsivo), una riga con `.c` indica un singolo file
relativo a `shared/libs/`. Le dylib risultanti restano **self-contained** (nessuna dipendenza
runtime aggiuntiva) e gli export sono identici a prima della deduplica — verificato con
`nm` (1141 math, 1842 stats, 492 graphics, insiemi identici).

## Regola

Aggiungere un file `gc_*` **solo qui**. Non ricopiarlo nelle librerie: basta che la libreria
consumatrice lo elenchi (o elenchi `compute`) in `shared-src.txt`.
