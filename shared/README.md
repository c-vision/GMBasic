# shared

Materiale **comune ai due mondi** (`GMBasic/` in C, `GMBasicSharp/` in C#), versionato.

| cartella | contenuto |
|---|---|
| `samples/` | programmi BASIC di esempio (`Community/`, `Commercial/`, `Native/`) + `Fixtures/` |
| `samples/Fixtures/` | **fixture**: i 4 feed di mercato (`markets`, `option_chains`, `rates`, `darkpool`) + le fixture piccole. I feed si scaricano con `gmb fixtures ensure` **sempre qui** — niente symlink, nessun disco esterno (decisione 2026-10-31) |
| `minitests/` | corpus con oracolo `' EXPECT:` (la gamba nativa è il riferimento) |
| `APPS/` | applicazioni complete che usano il compilatore |
| `include/`, `Controls/` | export `.inc` delle librerie e esempi di controlli |
| `libs/` | sorgenti delle **nostre** librerie FFI (`src/`, `include/`, `build.sh`) |
| **`settings/`** | **seed condivisi del compilatore**: `system.settings.json` e i manifest (fixtures, brains, libs, ml, chromium, obscura) + `providers.json`. Vedi `settings/README.md` |
| `skills/` | archivio skill (markdown) — vedi `skills/README.md` |
| `plugins/` | descrittori dei plugin per l'agente (`PLUGIN.md` + `tool.json`) — vedi `plugins/README.md` |
| `spec/`, `conformance/`, `runtime-abi/` | contratto: registry canonico/TokenKind, runner `.bas` con `EXPECT`, elenco dei simboli ABI C→C# (`check_all.sh`, ABI 1.0.0) |
| `webapp/` | frontend condiviso delle app |

## Regola: seed vs stato

- **Seed** (versionati, identici su ogni macchina) → `settings/`. Es. `providers.json`, che il
  runtime legge in fallback quando manca il file locale.
- **Stato** dell'installazione (provider attivi, server MCP, cervelli, indici) → `<configDir>`
  (`GMBasic/config`, o `<exe>/config` in un programma distribuito). **Non va in git**: contiene
  percorsi della macchina.
- La documentazione del monorepo sta in **`docs/`** alla radice (non c'è più `shared/docs`).
