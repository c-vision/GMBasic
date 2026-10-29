# TODO — Tassonomia TokenKind L_*/P_* (lexer vs parser/linker)

> **Stato 2026-09-09**: bug GRAVE (vault) — MEMBER-TABLE dedicata del C99Native FATTA
> (`gmb_member_tok`, 71 membri, commit `e99b1c744`/`93e262614`). Prossimo: qualificati funzione e tipi-campo-classe.

> **STATO 2026-09-08: NESSUN REFACTOR DI RINOMINA.** Decisione utente: non rinominare alcun
> token (`L_*`/`P_*` non si applicano come rinomina dei membri enum: sarebbe un refactoring enorme
> e il quadro non è chiaro). L_*/P_* resta una **lente concettuale** per capire l'origine di un
> token (Lexer vs Parser/Linker), non una convenzione di naming da imporre. La priorità reale è il
> bug GRAVE operativo: `gmb_token_names[]` disallineata all'enum + dispatch a valle da stringhe
> (`from_name` come oracolo) — vedi sotto e il bug nel vault.

## 1. Definizione tassonomica

| Famiglia | Chi lo genera | Cosa rappresenta | Esempi attuali |
|---|---|---|---|
| **L_*** | **Lexer** (dalla grafia sorgente) | keyword di struttura/statement, operatori/delimitatori, tipi primitivi, parole del comando a spazi, costanti letterali | `If`, `For`, `Print`, `Dbf`, `Use`, `Add`(generico), `StringType`, `As`, `True`, `PolarX`→ma PolarX è P se è risoluzione |
| **P_*** | **Parser/Linker** (sintetizzati dalla risoluzione) | nome qualificato `(ns, funzione)` univoco; **sub-comando** di statement (composto multi-parola); **comando composto**; **membro di oggetto**; **token di disambiguazione** per collisioni (stesso lexeme in ns diversi, o parola statement E funzione); **alias/legacy canonici** (Sqr vs Sqrt); token non presenti nel sorgente come singola parola | `TorchAdd`, `MatrixAddColVector`, `McpServerAdd`, `AiSubPlugins`, `AiPermission`, `Meta` |

Criterio d'assegnazione (checklist per ogni token):

1. Il lexeme di questo token appare nel sorgente **come una sola parola con grafia univoca e non
   ambigua** ed è dispatchato **subito** dal lexer? → **L_***.
2. Il token è il **risultato della risoluzione** di un nome (qualificato `NS.NOME`, o nudo ambiguo),
   di un **sottocomando composto** (più parole: `AI MCP SERVER ADD`), di un **membro** (`.Campo`),
   o di una **collisione** (stesso lexeme con significati diversi)? → **P_***.

### Casi canonici "confusi" oggi (da riclassificare in P_*)

1. **P_* qualificate già esistenti**: `TorchAdd`, `MatrixAddColVector`, `OptimizePowell`, `PolarX` —
   il lexeme non coincide con la grafia (`"ADD"`, `"POLAR_X"`). Già corrette per costruzione.
2. **P_* sub-comando di statement**: oggi il parser le riconosce per-token ma le converte in stringa
   (`mutex_action`/`win_name`/`verb`/`go_args[0].var`) e l'emitter le ricompare per-nome.
   Esempi C99Native residui 2026-09-08: `AI MCP SERVER ADD|REMOVE|ENABLE|DISABLE`,
   `REGISTER TOOL|AGENT`, `UNREGISTER TOOL` (`st_mcp`), verbi `AI PERMISSION`/`AI PLUGINS`
   (`emit_ai_domain`), `SPRITE GETPIXELS`/`SPRITE LOAD`. Tokens MCP/`AiSub*` esistono già
   nell'enum (`McpServerAdd`, `McpRegisterTool`, `AiSubPlugins`, `AiPermission`, `AiSubDiscover`, …)
   → il parser deve **salvare il token**, non la stringa.
3. **P_* collisione tra namespace**: stesso nome funzione in più ns (es. `SHARPERATIO` in METRICS e
   PORTFOLIO; `CVAR`; …). Già risolto nel C99Native con chiave `(ns, token)`; in tassonomia L/P il
   token risolto e univoco è P_*.
4. **P_* parola statement E funzione** (`RUN`, `SELECT`, `GET`, `SET`, `USE`…): il lexeme L_* è
   ambiguo; il significato funzione va risolto dal parser a un P_* di disambiguazione (non forzare
   nel `_builtInFunctionMap` — esclusione deliberata documentata).
5. **P_* membri di oggetto**: `.Count/.Peek/.Body/.Description` — membri su istanza, sintetizzati
   dal parser dopo il punto; alcuni hanno già token nel C99Native (token "member" aggiunti 2026-09-08).
6. **P_* alias/legacy**: `SQRT` alias di `Sqr` risolto a `Sqrt`; alias `IMPORT ... AS` (namespace
   virtuale) risolti al ns fisico nel parser.

> **Regola di principio (utente, 2026-09-08):** nel codice NON esistono stringhe di confronto
> come chiavi di dispatch: ogni parola confrontata è un membro dell'enum TokenKind. I token che
> **compaiono nel BASIC** (keyword/grafie sorgente) restano token "normali" (lexer/parsing).
> I token che **non compaiono nel BASIC** (fabbricati dal parser/lowering/disambiguazione) hanno
> il **prefisso `P_*`**.

## 2. Stato attuale (baseline 2026-09-08)

- Enum unico piatto `GMB_TOK_*` (~1712 token, di cui audit classifica ~1130 dispatch / ~582 parsing).
- C99Native: **0** confronti per-nome con TokenKind/keyword esistenti nei file emitter/preprocessore;
  residui 16-18 per-nome = parole SENZA token (per-nome dichiarato) → diventano i nuovi P_*
  (`PERMISSION`/`PLUGINS` verbi, sub-comandi MCP, `FLOAT`, `Regression`, prefissi/suffissi).
- Backend C#: stesso enum `common/Ast/TokenKind.cs`; PCodeVM/IL dispatchano su `BuiltInFunction`
  (valore enum) e su `StatementKind`; emitter C# ha ancora ~386 dispatch per stringa.
- C# **NON** condivide la definizione L/P con il C (`gmb_tokenkind.h` è generato/diverso): serve un
  piano di allineamento che NON si parli via lexeme tra i due mondi (regola C/C# separati).

## 3. Fasi

### Fase 0 — Audit e classificazione automatica (0,5-1 sessione)
- Script `tools/classify_tokens_lp.py`: partendo da `tools/stdlib_tokens.csv` + `gmb_tokenkind.h` +
  keyword table lexer, produce per ogni token la classe L/P secondo la checklist §1 e un manifest
  `docs/comparison/tokenkind-LP-classification.csv` (commit `-f`, mai a mano).
- Output: quanti L / quanti P; lista dei P "già sbagliati" (token di statement che oggi il parser
  degrada a stringa) e dei candidati P mancanti (sub-comandi senza enum).
- Criterio di verifica: report idempotente + diff vuoto su seconda esecuzione.

### Fase 1 — (SUBITO) Token per i sub-comandi residui del C99Native e dispatch per-token
- Obiettivo: chiudere il C99Native a **0 confronti per-nome** anche sui sub-comandi di statement.
- Azioni:
  1. Aggiungere i **P token mancanti** all'enum dove non esistono (verifica prima: molti `Mcp*`/
     `AiSub*` già presenti). Nominare secondo il pattern del sottocomando (`McpServerAdd`…).
  2. Introdurre nell'AST un campo **`act_tk`** (accanto a `mutex_action`) oppure rimpiazzare i campi
     stringa con il token per gli statement domain; parser `gmb_parser_stmt_mcp.c`/domain AI/SPRITE
     salvano il token invece di (o in aggiunta a) la stringa.
  3. Emitter (`st_mcp`, `emit_ai_domain`, `emit_sprite_domain`): switch/tabella per-`act_tk`, mai per
     lexeme. Rimozione degli `strcasecmp` residui.
  4. Verifica: scan automatico 0 residui (§strumenti), `gmb_driver` sui sample AI MCP/SERVER/AGENT,
     suite C99 regress.
- Nota: non tocca i backend C#.

### Fase 2 — Split fisico L_/P_ (2-3 sessioni, C99Native + generatori)
- Rinomina meccanica guidata dal manifest Fase 0: `GMB_TOK_X` → `GMB_LTOK_X` (o `GMB_TOK_L_X`) /
  `GMB_PTOK_X` in `gmb_tokenkind.h`, `gmb_tokenkind.c` (nomi), `gmb_ns_lookup.h`/`gmb_ns_types.h`
  (rigenerati con `--all`/`--c99native-types`), `tools/stdlib_tokens.csv`.
- Split in header separati: `gmb_tokenkind_l.h` + `gmb_tokenkind_p.h` inclusi da un comune.
- Audit `--tokenkind-class` aggiornato: violazioni per-nome = P che vengono dispatchati per lexeme
  (mai per L; i token di parsing L non sono "dispatch").
- Verifica: 21 golden `gmb_driver` + audit `--all` invariati nel conteggio.

### Fase 3 — Allineamento backend C# (2-3 sessioni)
- Stessa classificazione L/P su `common/Ast/TokenKind.cs` (mappa di rinomina generata dalla Fase 0).
- `Lexer.Keywords.cs` (L), `_builtInFunctionMap`/`CommandNameToTokenKindMap` (P) documentati con la
  famiglia nel commento del token.
- Nessun ponte di runtime tra C e C#: le due tassonomie evolvono in parallelo con la STESSA spec
  (il `.csv` è la fonte condivisa), MAI riuso in-process.

### Fase 4 — Migrazione emitter C# (~386 dispatch per stringa → P token)
- Il grosso residuo per-nome; applicare la regola d'oro con chiavi = P token risolti
  (`NativeProcedureDefinition.Kind`), non lexeme né `ns` guard. Fase separata e ampia (§20 PROPOSAL).

## 4. Strumenti di verifica (gate)

- `python3 tools/audit/no_stub.py --quiet` (exit 1).
- Scan residui per-nome: script ad-hoc (0 residui con token/keyword; whitelist esplicita dei senza-token).
- `dotnet run --project tools/audit --no-build -- --tokenkind-class` (0 violazioni).
- `dotnet run --project tools/audit --no-build -- --all` (manifest 982; C99Native copertura invariata).
- Driver: `bash /tmp/run_cn_an.sh <sample>`; golden `gmb_driver`.

## 5. Vincoli operativi (AGENTS.md)
- Generatori SOLO `--all`/script di Fase 0, mai editing manuale di manifest/header generati.
- Commit piccoli; prima di ogni commit `git diff --cached --stat`; solo file propri.
- Backup + validazione su copia prima di script che riscrivono file condivisi (multi-AI).

## 6. Primo item eseguibile (Fase 1)
1. `gmb_ast.h`: aggiungere `int act_tk;` alla struct `gmb_stmt` (init 0 in `gmb_stmt_new`).
2. `gmb_parser_stmt_mcp.c`: `s->act_tk = GMB_TOK_McpServerAdd;` (ecc.) accanto a `mutex_action`.
3. `gmb_c99_emit.c` `st_mcp`: switch per-`s->act_tk`.
4. Stesso pattern per verbi `PERMISSION`/`PLUGINS` (`emit_ai_domain`) e `SPRITE GETPIXELS/LOAD`.
5. Scan residui + regressione.

## 7. Stato 2026-09-08 (progressi applicati)
- **Fase 1 completata per i sub-comandi di statement del C99Native**:
  - `gmb_ast.h`: campo `act_tk`; `gmb_parser_stmt_mcp.c` e `gmb_parser_stmt_domain.c` salvano il
    TokenKind P_* del sottocomando (keyword di dominio o CommandName map per i verbi Identifier).
  - Emitter `st_mcp`/`emit_ai_domain` dispatchano per `act_tk` (mai lexeme).
- **Sezione P_* avviata nel tokenkind** (`gmb_tokenkind.h`, blocco commentato `P_*: token
  fabbricati dal PARSER/LINKER`): primi membri `GMB_TOK_P_Float` (sinonimo tipo `AS FLOAT` legacy,
  lookup dalla grafia) e `GMB_TOK_P_Regression` (base del result-type `RegressionResult`).
- **Risultato C99Native**: 0 confronti per-nome con TokenKind/keyword esistenti in `src/*.c`
  (`--tokenkind-class` OK). Residui per-nome = SOLO casi NON-tokenizzabili (strutturali/sintetici):
  prefisso tipo `STRING(n)`, prefissi oggetto `ERR.`/`ARRAYS.`, suffisso `DATE$`, nomi sintetici
  interni del lowering `_assign_`/`_true_`. NON forzare token per questi (non sono dispatch).
- **Lezione**: `gmb_token_names[GMB_TOK_COUNT]` è init esplicito con buchi NULL in coda; per
  aggiungere un token con lookup dalla grafia bisogna appendere in coda sia l'enum sia la voce
  `names` (verifica posizionale con mini-C: `tokenkind_from_name` == valore enum).
- **Scoperta 2026-09-08 (blocker Fase 1 residua)**: `gmb_token_names[]` è **disallineata**
  all'enum: `GMB_TOK_COUNT=2183` ma membri enum effettivi ~2144; buchi NULL e grafie legacy
  sbagliate (es. `from_name("DATE$")=1924` che è il token `DATEMONEY`). `name()`/`from_name()`
  non sono quindi chiavi affidabili per il dispatch per-token oltre il punto di disallineamento.
  **Correzione richiesta prima di convertire i residui**:
  1. Riallineare `gmb_token_names[]` = una voce per membro enum (generata dal nome enum).
  2. Spostare le grafie alternative (`"DATE$"`, `"FLOAT"`, `"_assign_"`, `"_true_"`, keyword,
     sotto-comandi) in **mappe di lookup dedicate** (keyword lexer / CommandName map parser /
     grafie speciali) e NON nella `names` posizionale.
  3. Con la names allineata, i residui diventano confronti per-token: `_assign_`/`_true_`
     (non compaiono nel BASIC) → **`P_*`** (es. `P_SynthAssign`/`P_SynthTrue`); `STRING(n)` → base
     `StringType` (keyword); `DATE$` → token dedicato via map grafie; `ARRAYS.`/`ERR.` → token
     del prefisso risolto dal parser.
