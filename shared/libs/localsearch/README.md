# gmblocalsearch — motore di ricerca locale (namespace BASIC `OPTIMIZE`)

**Prefisso C:** `gor_` · **Dipendenze:** C99 + libm

Motore local search stateful di `gmbortools` (`gor_ls`), con API compatta per BASIC:
stato = vettore LONG, callback di valutazione (soft), acceptors selezionabili.
Binding: `shared/include/localsearch.inc`.

## Superficie (5)
| Funzione | Contenuto |
|---|---|
| `LSSOLVE` | solve con hill climbing, 1 partenza (`initial()` **IN/OUT** = stato migliore; `out` = hard, soft, steps) |
| `LSSOLVECFG` | solve con `starts` e `acceptor` (0..6); `initial()` **IN/OUT** |
| `LSNEAREST` | nearest neighbour da matrice distanza |
| `LSCHEAPEST` | cheapest insertion (ciclo) |
| `LSNOWMS` | ms di CPU |

Nota: RNG, termination e acceptor sono usati **internamente** dal motore; la superficie
BASIC espone i knob utili (steps, starts, acceptor) invece dei singoli struct.

**Il motore MASSIMIZZA** il punteggio `soft` (un problema di minimo si scrive negando il
costo) e lo stato migliore torna al chiamante dentro `initial()`.
