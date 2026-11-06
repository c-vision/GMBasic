# gmbconic — coni e solver conici (namespace BASIC `OPTIMIZE`)

**Prefissi C:** `expcone_`, `ipm_`, `socp_`, `sdp_`, `presolve_`, `stdform_` ·
**Dipendenze:** C99 + libm (self-contained)

Port FFI del nucleo conico di `gmbsuite/gmbortools`: barriera e scaling dei coni
esponenziale/potenza, solver interior-point per LP/QP (denso e sparso), SOCP
(denso e sparso), SDP con blocchi PSD/SOC/esponenziali, presolve LP e
trasformazione in forma standard. Binding: `shared/include/conic.inc`.

L'algebra lineare (`dmat_*`, `spchol_*`, `splu_*`) **non** è copiata: è l'unica
copia di `gmbmath-linalg` (`shared/libs/math/src/gm_sparse_linalg.c`), compilata
dentro la dylib tramite `shared-src.txt`.

## Superficie (33)

| famiglia | funzioni |
|---|---|
| coni exp/potenza | `EXPCONEBARRIER`, `EXPCONEGRAD`, `EXPCONEHESS`, `EXPCONEDUALIN`, `EXPCONEDUALPOINT`, `EXPCONENTMETRICS`, `EXPCONESCALING` |
| IPM LP/QP | `IPMLP`, `IPMLPWARM`, `IPMQP`, `IPMQPWARM`, `IPMLPCSC`, `IPMQPCSC` |
| SOCP | `SOCPSOLVE`, `SOCPSOLVESP` |
| SDP | `SDPSOLVE` |
| presolve | `PRESOLVENEW`, `PRESOLVECHANGED`, `PRESOLVEDIMS`, `PRESOLVEREDUCED`, `PRESOLVEPOSTSOLVE`, `PRESOLVEPOSTSOLVEDIR`, `PRESOLVEFREE` |
| forma standard | `STDFORMBUILD`, `STDFORMINFO`, `STDFORMGETA`, `STDFORMGETB`, `STDFORMGETC`, `STDFORMGETQ`, `STDFORMMAPX`, `STDFORMMAPDIR`, `STDFORMMAPY`, `STDFORMFREE` |

## Convenzioni

- **Niente struct C**: matrici e vettori sono array piatti con layout documentato
  (row-major per A/Q, CSC a 0-based per le varianti sparse).
- **Handle solo per lo stato**: presolve e stdform restituiscono un handle (>0) con
  tabella fissa (16 slot, uso sequenziale); tutto il resto è senza stato.
- **Ritorni**: `0` ottimo/ok, `1` max iter, `2` memoria, `3` singolare,
  `-1` argomenti/handle non validi.
- **Tolleranze**: `tol` è assoluta di Newton; il pavimento del double rende inutile
  scendere sotto `1e-8` (con `1e-10` il solver può esaurire le iterazioni pur con il
  punto corretto). Per i problemi piccoli: `tol=1e-8`, `rtol*=1e-7`,
  `nearRel=1000`, `maxIter >= 200`.
- **Lati liberi** in `STDFORMBUILD`: `-1E308` / `+1E308` (il binding li converte in
  `±INFINITY`).

## Verifica

- MiniTest: `shared/minitests/conicffi/conic_core.bas` (7 expcone),
  `conic_ipm_presolve.bas` (IPM denso/CSC/warm + presolve),
  `conic_socp_sdp.bas` (SOCP denso/sparso + SDP PSD/SOC/EXP),
  `conic_stdform.bas` (forma standard e ritorno).
- Sample: `shared/samples/include/optimize/08-Conic.bas` (expcone) e
  `16-ConicSolvers.bas` (tutte le 26 funzioni di solver/trasformazione).
- Parità: `zsh tools/sh/parity4 ../shared/samples/include/optimize/16-ConicSolvers.bas`
  → IL, PCodeVM, VM/C e C99Native danno lo stesso output.

## Note sui limiti del marshalling

Le dichiarazioni `SDPSOLVE` (33 parametri) hanno fatto emergere tre bug del path FFI
a 64 argomenti, corretti nel 2026-11-06:

- `gmb_semgraph.c` — `sig[256]` accumulava con `snprintf` oltre il buffer (stack smash);
- `PCodeProcessor.cs` — maschere `int` con `1 << pi`: il 33° argomento riavvolgeva sul bit 0;
- `IlProgramCompiler.Expressions.cs` — `scalarKindMask` limitata alle prime 16 posizioni,
  gli scalari oltre la 15ª viaggiavano come DOUBLE.
