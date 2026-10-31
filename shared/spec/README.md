# spec — shared canonical contract

**Versioned** content, consumed by both projects:

- `registry/canonical.registry.json` — canonical surface `(namespace, name, TokenKind, arity)`;
  stable append-only ids (procedures 1.., commands 1000000+, constants 2000000+).
- `registry/generated/CanonicalLookup.g.cs` — C# lookup generated from the registry (Sharp side is
  the source).
- `registry/stdlib_tokens.csv` — `(ns, token)` pairs for audit/coverage.
- `registry/architectures/*.json` — LLM architecture descriptors.

Rules:
- The **TokenKind** is the canonical key; the lexeme is never a dispatch key.
- The registry is modified **append-only**: never reuse an id.
- The C side consumes `registry/generated/gmb_registry.gen.h` (generated) and validates it with its
  own tests; the Sharp side is the source of `StdLib`/TokenKind.
- Every contract change must be reflected in `conformance/` and, for the ABI, in `runtime-abi/`.
