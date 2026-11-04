# gmbconic — coni esponenziali/potenza (namespace BASIC `OPTIMIZE`)

**Prefisso C:** `expcone_` · **Dipendenze:** C99 + libm (self-contained)

Barriera, gradiente, Hessiana, duale e scaling per i coni esponenziale/potenza
(`gor_conic`/`expcone`). Include una copia locale di `dmat_eig_jacobi` (3x3) per non
trascinare gmbmath. Binding: `shared/include/conic.inc`.

## Superficie (7)
`EXPCONEBARRIER`, `EXPCONEGRAD`, `EXPCONEHESS`, `EXPCONEDUALIN`, `EXPCONEDUALPOINT`,
`EXPCONENTMETRICS`, `EXPCONESCALING` (`out` = Theta|Thin|rt|scale).

Non esposti: `socp_solve`/`sdp_ipm` completi (struct `SocpCone`, decine di parametri:
fuori dal marshalling FFI a 8 argomenti).
