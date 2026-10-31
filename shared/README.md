# shared

Asset comuni **versionati** tra `GMBasic` (C) e `GMBasicSharp` (C#):

- `samples/`, `APPS/`, `minitests/` — programmi BASIC e corpus con oracolo `' EXPECT:`
- `include/`, `Controls/` — `.inc` e esempi controlli
- `settings/` — settings condivisi (niente stato: `brain.db`/`rag`/config runtime esclusi)
- `samples/Fixtures/` — fixture piccole; i feed grandi (`markets`, `option_chains`, `rates`,
  `darkpool`) sono symlink all'SSD esterno
- `docs/`, `skills/`, `plugins/`, `webapp/` — materiale condiviso

Il contratto (registry/TokenKind) è in `../spec/`; i runner in `../conformance/` e
`../runtime-abi/`.

## Contratto (in questo repo)
- `spec/` — registry canonico/TokenKind/arità.
- `conformance/` — runner `.bas` + `EXPECT`.
- `runtime-abi/` — symbols e `check_all.sh` (ABI 1.0.0).
