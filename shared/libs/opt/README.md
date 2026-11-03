# gmbopt — ottimizzazione numerica (34 funzioni `OPTIMIZE`)

**Part of:** [gmbsuite](../README.md) · **Prefix:** `gor_optb_` · **Dependency:** C99 + libm
(self-contained: nessun link a gmbmath/gmbcompute).

Pacchetto FFI della superficie BASIC `OPTIMIZE.*` (34 funzioni). Il motore numerico è lo
stesso di `gmbsuite/gmbortools` (`gmi_opt`, `gmi_optfit`, `simplex`, `gor_opt_api`), copiato
qui per essere autonomo. Il binding BASIC è `shared/include/opt.inc` (`IMPORT OPT`).

## Perché una superficie diversa da `OPTIMIZE.*`

L'ABI FFI non restituisce array composti: i risultati array si scrivono in un buffer `out()`
fornito dal chiamante, e le matrici sono **piatte row-major** con dimensioni esplicite
(`A(), m, n`). I callback sono **lambda** (`FN ...`): è la forma che funziona su tutti e
quattro i backend (IL, PCodeVM, VM/C, C99Native).

## API (`shared/include/opt.inc`, namespace `OPT`)

| Gruppo | Funzioni |
|---|---|
| **Radici/minimo 1D** | `NEWTON`, `BISECTION`, `BRENT`, `SECANT`, `GOLDENSECTION`, `BRENTQ`, `BRENTH`, `RIDDER`, `TOMS748`, `BRENTMIN`, `MINIMIZESCALAR` (`MINIMIZESCALARM` con metodo) |
| **Minimizzatori** | `NELDERMEAD`, `BFGS`, `LBFGS`, `CONJUGATEGRADIENT`, `POWELL`, `TNC`/`TNCB`, `COBYLA`/`COBYLAB`, `SLSQP`/`SLSQPB`, `MINIMIZE`/`MINIMIZEM`/`MINIMIZEB` |
| **Globali** | `BRUTE`/`BRUTEN`, `BASINHOPPING`/`BASINHOPPINGN`, `DUALANNEALING`/`DUALANNEALINGN`, `DIFFERENTIALEVOLUTION` |
| **Sistemi** | `FSOLVE`, `BROYDEN`, `LEASTSQUARESFIT`/`LEASTSQUARESFITB` |
| **Curve fitting** | `LEVENBERGMARQUARDT`, `LEVENBERGMARQUARDTVEC`, `CURVEFIT` |
| **Algebra / LP** | `LEASTSQUARES`, `CHOLESOLVESOLVE`, `SIMPLEX`, `LINPROG`/`LINPROGB`/`LINPROGEQ`/`LINPROGFULL` |

Guida utente completa (34 funzioni, parametri, esempi, esito §4.1):
`AiWiki/01-Projects/GMBasic/Guide/utente/Optimize-FFI-Guida-Utente.md`.

Convenzioni:
- `MINIMIZESCALAR(f,a,b)` è una dichiarazione diretta alla variante `_default` (brent);
  `MINIMIZESCALARM(f,a,b,method)` passa il metodo.
- `out` dei minimizzatori/sistemi/LP: `UBOUND(x0)+1` (o `n` esplicito);
  `SIMPLEX` → `n+1` (soluzione + obiettivo); curve fitting → `np`.
- `bounds`/`ranges` appiattiti: `2*n` valori `(lo0, hi0, lo1, hi1, ...)`.
- `LEASTSQUARESFIT`: `m = UBOUND(out)+1` (numero di residui, `>= n`).
- I SUB si invocano con `CALL OPT.NAME(...)`.

## Build

```sh
tools/sh/gmb_lib.sh build opt     # -> shared/libs/opt/libgmbopt.<ext>
tools/sh/gmb_lib.sh check opt     # install_name @rpath + file attesi
```

## Test

- Motore C: `gmbsuite/gmbortools` → `make test-optb` (38 check: tutte le 34 funzioni).
- BASIC 4 backend: `tools/sh/test_minitests_parity.sh opt/opt_` (4 file).
- Esempi: `shared/samples/include/opt/01-Scalar.bas` … `06-Linear.bas`.

## Note

- Callback **nominati** (`FUNCTION f(x() AS DOUBLE) ...`) e **lambda** sono entrambi
  supportati su **tutti e 4 i backend** (verificato per le 5 forme vettoriali: 128/129/132/144/145).
- La lunghezza dell'array di ritorno di un callback vettoriale è implicitamente quella
  dell'ultimo array in ingresso (per `LEASTSQUARESFIT` il binding ripad a `m`).
