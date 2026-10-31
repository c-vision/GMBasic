# spec — contratto canonico condiviso

Contenuto **versionato** e consumato da entrambi i progetti:

- `registry/canonical.registry.json` — superficie canonica `(namespace, nome, TokenKind, arità)`;
  id stabili append-only (procedure 1.., comandi 1000000+, costanti 2000000+).
- `registry/generated/CanonicalLookup.g.cs` — lookup C# generata dal registry (fonte Sharp).
- `registry/stdlib_tokens.csv` — coppie `(ns, token)` per audit/coverage.
- `registry/architectures/*.json` — descrittori architetture LLM.

Regole:
- Il **TokenKind** è la chiave canonica; il lexeme non è mai chiave di dispatch.
- Il registry si modifica **append-only**: mai riusare un id.
- Il lato C consuma `registry/generated/gmb_registry.gen.h` (generato) e valida con i propri test;
  il lato Sharp è la fonte di `StdLib`/TokenKind.
- Ogni modifica al contratto va riflessa in `conformance/` e, per l'ABI, in `runtime-abi/`.
