# GMBasic — Copertura backend (audit automatico)

Generato da `gmb_audit --all`. Manifest in `docs/comparison/manifests/`.

**Superficie canonica**: 1523 funzioni `(namespace, nome, TokenKind)` registrate da StdLib (`BuiltinProcedures`).

| Backend | Funzioni implementate | Canonici | % |
|---|---|---|---|
| PCodeVM | 1523 | 1523 | 100.0% |
| IL | 1523 | 1523 | 100.0% |
| VMC | 1523 | 1523 | 100.0% |
| C99Native | 1523 | 1523 | 100.0% |

> Superficie unica per tutti i backend: le funzioni canoniche StdLib. Ogni backend conta quante di queste copre col suo dispatch reale. VMC via `/* NAME */` nei case; C99Native via GMB_STMT (le funzioni emettibili dal compilatore nativo).
