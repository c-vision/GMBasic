# gmbpgm — indice PGM (namespace BASIC `PGM`)

**Prefisso C:** `gor_pgm` · **Dipendenze:** C99 + libm (self-contained)

Indice **Piecewise Geometric Model**: approssima `rank(x)` di un insieme ordinato con
segmenti lineari a errore massimo `eps` (Swing Filter di Ferragina-Vinciguerra).
Motore `gmbortools` (`gor_pgm`, derivato da AppendPGM, MIT), copiato qui per essere
autonomo. Binding BASIC: `shared/include/pgm.inc` (`IMPORT PGM`).

## Superficie (4)

| BASIC | C | note |
|---|---|---|
| `PGM.BUILD(x(), n, eps)` | `gfx_pgm_build` | handle > 0, -1 su errore |
| `PGM.RANK(h, key)` | `gfx_pgm_rank` | posizione stimata |
| `PGM.RANGE(h, key, out())` | `gfx_pgm_range` | `out(0)=lo`, `out(1)=hi` garantiti |
| `PGM.FREE(h)` | `gfx_pgm_free` | libera l'handle |

Handle: tabella statica da **16** indici vivi per processo, uso sequenziale (non
thread-safe: documentato nello shim).

## Build

```sh
shared/libs/pgm/build.sh
```

## Test

`shared/minitests/pgm/`
