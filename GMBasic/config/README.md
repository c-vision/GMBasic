# `GMBasic/config/` — NIENTE codice o dati condivisi: solo stato locale

Questa cartella è il `<configDir>` del compilatore **e** del programma compilato (per un
eseguibile distribuito è `<exe>/config/`). Contiene **solo stato di questa installazione**:
nessun file qui è versionato (unica eccezione: questo README).

**Regola (2026-10-31):** un default non va mai nello stato di una macchina. Se serve un
default, va **nel codice** (catalogo/`EnsureDefaults`) o in **`shared/settings/`** (config
condivise del compilatore, versionate e indentiche su ogni macchina), con *fallback* a
runtime.

## Cosa può esserci qui (tutto locale, `.gitignore`)

| file | chi lo scrive | default quando manca |
|---|---|---|
| `ai.json` | `AI PATH/SKILLS`, `AiConfig` | `AiConfig.Load()` → vuoto |
| `brains.json` | `AI BRAIN INSTALL`, registry cervelli | registro vuoto |
| `skills-index.json` | `AI SKILLS SET INDEX` (gamba C) | nessun override |
| `SKILLS_PROMPT.md`, `PLUGINS_PROMPT.md` | `AI SKILLS/PLUGINS REFRESH` | rigenerati |
| `mcp.json` | `AI MCP SERVER ADD/REMOVE` | nessun server (i 6 noti stanno in `McpMarketplace.cs`) |
| `providers.json` | `AI PROVIDER SAVE` | **seed** `shared/settings/providers.json` (fallback in entrambe le gambe) |
| `connectors.json` | `ConnectorConfig.Save` (solo C#) | rigenerato da `ConnectorMarketplace.Catalog` (53 voci) |
| `ide.settings.json` | IDE | `new IdeSettings()` |
| `support.agent.json` | sample `AgentBasic.bas` (`AI AGENT SAVE`) | nessun agente |
| `recursivemas/` | `TeamServer.Save` | nessuna squadra |
| `*.db` (`brain.db`, `vaults.db`, `orchestration.db`) | memoria/vault/registry | creati al primo uso |

## Perché era sbagliato tenerli in git

`mcp.json` conteneva `python3 /var/folders/.../gmb_mcp_pool_test.py` e server finti (`fake`,
`fakehttp`, `files`, `demo_full_surface`); `ai.json` conteneva `/Users/<nome>/...`;
`support.agent.json` e `recursivemas/hfteam/` erano residui di sample/test. Con quei file in
git il comportamento dipendeva da **quale macchina** aveva fatto il checkout — e il MiniTest
`ai_envreads.bas` era già escluso dalla parità cross-leg proprio perché «il contenuto varia
per macchina».

## Le due eccezioni storiche, risolte

- **`connectors.json`** sembrava una fixture (un MiniTest con oracolo `list= 13527`); in realtà
  la gamba C **non legge il file**: serializza il proprio catalogo in codice, e il risultato è
  identico con e senza file (verificato). Lato C# `ConnectorConfig.Load()` rigenera dal
  catalogo quando il file manca. Quindi: nessun seed necessario.
- **`providers.json`** è l'unico caso in cui il file *era* l'oracolo: `AI.PROVIDERS()` legge il
  file e `shared/minitests/ai/gen_AI_Providers.bas` ha il suo contenuto come EXPECT. Il seed
  canonico è ora **`shared/settings/providers.json`** (versionato, nessun percorso assoluto,
  nessun segreto); entrambe le gambe lo leggono in fallback e il bundle lo copia in
  `<exe>/config/providers.json` per i programmi distribuiti.

Vedi anche [shared/settings/README.md](../../shared/settings/README.md).
