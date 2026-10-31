# `GMBasic/config/` — cosa è versionato e cosa è stato locale

Questa cartella è il `<configDir>` del compilatore: `ai.json`, parametri, registrazioni fatte
a runtime. **Regola (2026-10-31): in git solo ciò che è indipendente dalla macchina e serve
come *seed* deterministico. Tutto il resto è stato locale e va in `.gitignore`.**

## Versionato (seed deterministico, uguale su ogni macchina)

| file | perché sta in git |
|---|---|
| `connectors.json` | **fixture**: 53 connettori configurati. Il MiniTest `shared/minitests/connector/list.bas` ha oracolo `list= 13527` (lunghezza della lista resa come stringa), quindi il contenuto è un contratto. Nessun percorso assoluto, nessun segreto (`secrets: null` su tutti). |
| `providers.json` | **fixture**: provider + `roleOrder` + `activeIndex` + `schemes`. `AI.PROVIDERS()` legge **questo file** quando c'è (C: `gmb_ai_providers_json` → contenuto del file; se manca → `gmb_provider_v2_info`, forma diversa ad array) e il MiniTest `shared/minitests/ai/gen_AI_Providers.bas` ha come oracolo esattamente il suo contenuto. Nessun percorso assoluto, nessun segreto: i tre provider sono i default del codice (`ProviderServer.EnsureDefaults`: fixture-embedding/fixture-completion/keyword-reranker) e gli schemi sono `ProviderConfig.DefaultSchemes` (hf/huggingface/ollama/openai/anthropic). |

Il catalogo dei 52 connettori disponibili e i 6 server MCP noti vivono **nel codice**
(`ConnectorMarketplace.cs`, `McpMarketplace.cs`).

> [!IMPORTANT] `providers.json` è **anche** stato (l'utente può cambiare provider attivo,
> crediti, stato). Se un'operazione lo riscrive, il diff è atteso: è il prezzo di avere un
> oracolo deterministico per `gen_AI_Providers.bas`. Non cancellarlo per "pulire": il test
> leggerebbe la forma ad array e non combacerebbe più.

## Locale (NON in git — `.gitignore`)

| file | default quando manca |
|---|---|
| `ai.json` | `AiConfig.Load()` → vuoto; `modelPaths`/`skillPaths` sono i percorsi di modelli e skill dell'utente, `skillsIndexPath` è l'override dell'indice |
| `brains.json` | `BrainRegistry` → registro vuoto (i cervelli si installano con `AI BRAIN INSTALL` / `gmb brain install`) |
| `skills-index.json` | `skill_index_load_persisted()` → nessun override (indice in `<configDir>/SKILLS_PROMPT.md`) |
| `mcp.json` | `McpConfig.Load()` → nessun server; i 6 noti sono in `McpMarketplace.cs` |
| `ide.settings.json` | `IdeSettings.Load()` → `new IdeSettings()` (default del codice) |
| `support.agent.json` | artefatto del sample `shared/samples/Community/Ai/AgentBasic.bas` (`AI AGENT SAVE`) |
| `recursivemas/` | squadre RecursiveMAS: artefatti di sample/test (`TeamServer.Save`) |

## Perché

`mcp.json` conteneva `python3 /var/folders/.../gmb_mcp_pool_test.py` e server finti (`fake`,
`fakehttp`, `files`, `demo_full_surface`) rimasti da un test; `ai.json` conteneva
`/Users/<nome>/...`; `support.agent.json` e `recursivemas/hfteam/` erano residui di
sample/test. Con quei file in git il comportamento del programma dipendeva da **quale
macchina** aveva fatto il checkout — e il MiniTest `ai_envreads.bas` era già escluso dalla
parità cross-leg proprio perché «il contenuto varia per macchina».

Un checkout pulito parte dai default del codice su tutti e due i mondi (C# e C), che
coincidono: per i file **locali** nessun file è necessario e non si introduce alcuna
divergenza; per i due **fixture** versionati il contenuto è deterministico e identico su ogni
macchina (verificato: nessun percorso assoluto, nessun segreto).

**Non rimettere in git** uno di questi file: se serve un nuovo default, si mette **nel codice**
(o in `shared/settings/`, dove vivono le configurazioni condivise del compilatore), non nello
stato di una macchina.
