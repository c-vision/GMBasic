# TODO — TokenKind L_*/P_* taxonomy (lexer vs parser/linker)

> **Status 2026-09-09**: SERIOUS bug (vault) — dedicated C99Native MEMBER-TABLE DONE
> (`gmb_member_tok`, 71 members, commit `e99b1c744`/`93e262614`). Next: function qualifiers and class-field types.

> **STATUS 2026-09-08: NO RENAME REFACTOR.** User decision: do not rename any
> token (`L_*`/`P_*` do not apply as a rename of the enum members: it would be a huge refactoring
> and the picture is not clear). L_*/P_* remains a **conceptual lens** to understand the origin of a
> token (Lexer vs Parser/Linker), not a naming convention to impose. The real priority is the
> SERIOUS operational bug: `gmb_token_names[]` misaligned with the enum + downstream dispatch from
> strings (`from_name` as oracle) — see below and the bug in the vault.

## 1. Taxonomic definition

| Family | Who generates it | What it represents | Current examples |
|---|---|---|---|
| **L_*** | **Lexer** (from the source spelling) | structure/statement keywords, operators/delimiters, primitive types, words of the space-separated command, literal constants | `If`, `For`, `Print`, `Dbf`, `Use`, `Add`(generic), `StringType`, `As`, `True`, `PolarX`→but PolarX is P if it is resolution |
| **P_*** | **Parser/Linker** (synthesized by resolution) | unique qualified name `(ns, function)`; statement **sub-command** (multi-word compound); **compound command**; **object member**; **disambiguation token** for collisions (same lexeme in different ns, or word that is statement AND function); **canonical aliases/legacy** (Sqr vs Sqrt); tokens not present in the source as a single word | `TorchAdd`, `MatrixAddColVector`, `McpServerAdd`, `AiSubPlugins`, `AiPermission`, `Meta` |

Assignment criterion (checklist for each token):

1. Does the lexeme of this token appear in the source **as a single word with a unique and
   unambiguous spelling** and is it dispatched **immediately** by the lexer? → **L_***.
2. Is the token the **result of the resolution** of a name (qualified `NS.NOME`, or bare ambiguous),
   of a **compound sub-command** (multiple words: `AI MCP SERVER ADD`), of a **member** (`.Campo`),
   or of a **collision** (same lexeme with different meanings)? → **P_***.

### Canonical "confused" cases today (to be reclassified as P_*)

1. **Already existing qualified P_***: `TorchAdd`, `MatrixAddColVector`, `OptimizePowell`, `PolarX` —
   the lexeme does not match the spelling (`"ADD"`, `"POLAR_X"`). Already correct by construction.
2. **P_* statement sub-command**: today the parser recognizes them per token but converts them to a string
   (`mutex_action`/`win_name`/`verb`/`go_args[0].var`) and the emitter compares them again by name.
   C99Native residuals 2026-09-08: `AI MCP SERVER ADD|REMOVE|ENABLE|DISABLE`,
   `REGISTER TOOL|AGENT`, `UNREGISTER TOOL` (`st_mcp`), verbs `AI PERMISSION`/`AI PLUGINS`
   (`emit_ai_domain`), `SPRITE GETPIXELS`/`SPRITE LOAD`. MCP/`AiSub*` tokens already exist
   in the enum (`McpServerAdd`, `McpRegisterTool`, `AiSubPlugins`, `AiPermission`, `AiSubDiscover`, …)
   → the parser must **save the token**, not the string.
3. **P_* namespace collision**: same function name in several ns (e.g. `SHARPERATIO` in METRICS and
   PORTFOLIO; `CVAR`; …). Already solved in C99Native with key `(ns, token)`; in the L/P taxonomy the
   resolved and unique token is P_*.
4. **P_* word that is statement AND function** (`RUN`, `SELECT`, `GET`, `SET`, `USE`…): the L_* lexeme is
   ambiguous; the function meaning must be resolved by the parser to a disambiguation P_* (do not force
   it into `_builtInFunctionMap` — documented deliberate exclusion).
5. **P_* object members**: `.Count/.Peek/.Body/.Description` — members on an instance, synthesized
   by the parser after the dot; some already have tokens in C99Native ("member" tokens added 2026-09-08).
6. **P_* aliases/legacy**: `SQRT` alias of `Sqr` resolved to `Sqrt`; `IMPORT ... AS` aliases (virtual
   namespace) resolved to the physical ns in the parser.

> **Principle rule (user, 2026-09-08):** in the code there are NO comparison strings
> as dispatch keys: every compared word is a member of the TokenKind enum. The tokens that
> **appear in BASIC** (keywords/source spellings) remain "normal" tokens (lexer/parsing).
> The tokens that **do not appear in BASIC** (fabricated by the parser/lowering/disambiguation) have
> the **`P_*` prefix**.

## 2. Current state (baseline 2026-09-08)

- Single flat enum `GMB_TOK_*` (~1712 tokens, of which the audit classifies ~1130 dispatch / ~582 parsing).
- C99Native: **0** per-name comparisons with existing TokenKind/keywords in the emitter/preprocessor files;
  16-18 per-name residuals = words WITHOUT a token (declared per-name) → they become the new P_*
  (`PERMISSION`/`PLUGINS` verbs, MCP sub-commands, `FLOAT`, `Regression`, prefixes/suffixes).
- C# backends: same enum `common/Ast/TokenKind.cs`; PCodeVM/IL dispatch on `BuiltInFunction`
  (enum value) and on `StatementKind`; the C# emitter still has ~386 per-string dispatch.
- C# does **NOT** share the L/P definition with C (`gmb_tokenkind.h` is generated/different): an
  alignment plan is needed where the two worlds do NOT talk via lexeme (separate C/C# rule).

## 3. Phases

### Phase 0 — Audit and automatic classification (0.5-1 session)
- Script `tools/classify_tokens_lp.py`: starting from `tools/stdlib_tokens.csv` + `gmb_tokenkind.h` +
  lexer keyword table, it produces for each token the L/P class according to the §1 checklist and a manifest
  `docs/comparison/tokenkind-LP-classification.csv` (commit `-f`, never by hand).
- Output: how many L / how many P; list of the "already wrong" P (statement tokens that today the parser
  degrades to a string) and of the missing P candidates (sub-commands without an enum).
- Verification criterion: idempotent report + empty diff on a second run.

### Phase 1 — (NOW) Tokens for the C99Native residual sub-commands and per-token dispatch
- Goal: bring C99Native to **0 per-name comparisons** also on statement sub-commands.
- Actions:
  1. Add the **missing P tokens** to the enum where they do not exist (check first: many `Mcp*`/
     `AiSub*` already present). Name them according to the sub-command pattern (`McpServerAdd`…).
  2. Introduce in the AST an **`act_tk`** field (next to `mutex_action`) or replace the string fields
     with the token for domain statements; the `gmb_parser_stmt_mcp.c`/AI/SPRITE domain parsers
     save the token instead of (or in addition to) the string.
  3. Emitter (`st_mcp`, `emit_ai_domain`, `emit_sprite_domain`): switch/table per `act_tk`, never per
     lexeme. Removal of the residual `strcasecmp`.
  4. Verification: automatic scan 0 residuals (§tools), `gmb_driver` on the AI MCP/SERVER/AGENT samples,
     C99 suite regression.
- Note: it does not touch the C# backends.

### Phase 2 — Physical L_/P_ split (2-3 sessions, C99Native + generators)
- Mechanical rename driven by the Phase 0 manifest: `GMB_TOK_X` → `GMB_LTOK_X` (or `GMB_TOK_L_X`) /
  `GMB_PTOK_X` in `gmb_tokenkind.h`, `gmb_tokenkind.c` (names), `gmb_ns_lookup.h`/`gmb_ns_types.h`
  (regenerated with `--all`/`--c99native-types`), `tools/stdlib_tokens.csv`.
- Split into separate headers: `gmb_tokenkind_l.h` + `gmb_tokenkind_p.h` included by a common one.
- Audit `--tokenkind-class` updated: per-name violations = P that are dispatched per lexeme
  (never per L; L parsing tokens are not "dispatch").
- Verification: 21 `gmb_driver` goldens + audit `--all` unchanged in count.

### Phase 3 — C# backend alignment (2-3 sessions)
- Same L/P classification on `common/Ast/TokenKind.cs` (rename map generated by Phase 0).
- `Lexer.Keywords.cs` (L), `_builtInFunctionMap`/`CommandNameToTokenKindMap` (P) documented with the
  family in the token comment.
- No runtime bridge between C and C#: the two taxonomies evolve in parallel with the SAME spec
  (the `.csv` is the shared source), NEVER in-process reuse.

### Phase 4 — C# emitter migration (~386 per-string dispatch → P token)
- The bulk of the per-name residual; apply the golden rule with keys = resolved P tokens
  (`NativeProcedureDefinition.Kind`), not lexeme nor `ns` guard. Separate and large phase (§20 PROPOSAL).

## 4. Verification tools (gate)

- `python3 tools/audit/no_stub.py --quiet` (exit 1).
- Per-name residual scan: ad-hoc script (0 residuals with token/keyword; explicit whitelist of the token-less ones).
- `dotnet run --project tools/audit --no-build -- --tokenkind-class` (0 violations).
- `dotnet run --project tools/audit --no-build -- --all` (manifest 982; C99Native coverage unchanged).
- Driver: `bash /tmp/run_cn_an.sh <sample>`; `gmb_driver` goldens.

## 5. Operating constraints (AGENTS.md)
- Generators ONLY `--all`/Phase 0 scripts, never manual editing of generated manifests/headers.
- Small commits; before every commit `git diff --cached --stat`; only your own files.
- Backup + validation on a copy before scripts that rewrite shared files (multi-AI).

## 6. First executable item (Phase 1)
1. `gmb_ast.h`: add `int act_tk;` to the `gmb_stmt` struct (init 0 in `gmb_stmt_new`).
2. `gmb_parser_stmt_mcp.c`: `s->act_tk = GMB_TOK_McpServerAdd;` (etc.) next to `mutex_action`.
3. `gmb_c99_emit.c` `st_mcp`: switch per `s->act_tk`.
4. Same pattern for the `PERMISSION`/`PLUGINS` verbs (`emit_ai_domain`) and `SPRITE GETPIXELS/LOAD`.
5. Residual scan + regression.

## 7. Status 2026-09-08 (applied progress)
- **Phase 1 completed for the C99Native statement sub-commands**:
  - `gmb_ast.h`: `act_tk` field; `gmb_parser_stmt_mcp.c` and `gmb_parser_stmt_domain.c` save the
    P_* TokenKind of the sub-command (domain keyword or CommandName map for the Identifier verbs).
  - Emitters `st_mcp`/`emit_ai_domain` dispatch per `act_tk` (never lexeme).
- **P_* section started in tokenkind** (`gmb_tokenkind.h`, commented block `P_*: token
  fabbricati dal PARSER/LINKER`): first members `GMB_TOK_P_Float` (legacy `AS FLOAT` type synonym,
  lookup from the spelling) and `GMB_TOK_P_Regression` (base of the `RegressionResult` result-type).
- **C99Native result**: 0 per-name comparisons with existing TokenKind/keywords in `src/*.c`
  (`--tokenkind-class` OK). Per-name residuals = ONLY NON-tokenizable cases (structural/synthetic):
  type prefix `STRING(n)`, object prefixes `ERR.`/`ARRAYS.`, suffix `DATE$`, synthetic internal
  lowering names `_assign_`/`_true_`. Do NOT force tokens for these (they are not dispatch).
- **Lesson**: `gmb_token_names[GMB_TOK_COUNT]` is an explicit init with NULL holes at the tail; to
  add a token with spelling lookup you must append both the enum and the `names` entry at the tail
  (positional check with mini-C: `tokenkind_from_name` == enum value).
- **2026-09-08 discovery (residual Phase 1 blocker)**: `gmb_token_names[]` is **misaligned**
  with the enum: `GMB_TOK_COUNT=2183` but effective enum members ~2144; NULL holes and wrong legacy
  spellings (e.g. `from_name("DATE$")=1924` which is the `DATEMONEY` token). So `name()`/`from_name()`
  are not reliable keys for per-token dispatch beyond the misalignment point.
  **Correction required before converting the residuals**:
  1. Realign `gmb_token_names[]` = one entry per enum member (generated from the enum name).
  2. Move the alternative spellings (`"DATE$"`, `"FLOAT"`, `"_assign_"`, `"_true_"`, keywords,
     sub-commands) into **dedicated lookup maps** (lexer keyword / parser CommandName map /
     special spellings) and NOT into the positional `names`.
  3. With the names aligned, the residuals become per-token comparisons: `_assign_`/`_true_`
     (they do not appear in BASIC) → **`P_*`** (e.g. `P_SynthAssign`/`P_SynthTrue`); `STRING(n)` → base
     `StringType` (keyword); `DATE$` → dedicated token via spelling map; `ARRAYS.`/`ERR.` → token
     of the prefix resolved by the parser.
