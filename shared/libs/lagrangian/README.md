# gmblagrangian — rilassamento lagrangiano con subgradiente (namespace BASIC `OPTIMIZE`)

**Prefisso C:** `gor_lagr` · **Dipendenze:** C99 + libm

Decomposizione lagrangiana per `min f(x) s.t. A x >= b, x in X`: i vincoli sono
rilassati in funzione obiettivo con moltiplicatori non negativi, aggiornati lungo il
subgradiente con passo di Polyak e decadimento. Binding: `shared/include/lagrangian.inc`.

## Superficie (1)
| Funzione | Contenuto |
|---|---|
| `LAGRANGIANSUBGRADIENT` | `out` = [lower bound, lambda(m), best x(nx)], ritorna il lower bound |

La callback del subproblema usa la forma **ritorno ARRAY** (descriptor 144) perché il
motore ha bisogno di *due* risultati (valore + vettore): `r(0)` = valore, `r(1..nx)` = x.
Le forme con array di output scritto dalla callback non sono portabili (IL copia gli
array in ingresso), quindi il vettore viaggia nel valore di ritorno.
