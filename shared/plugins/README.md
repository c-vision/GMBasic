# `shared/plugins/` — a cosa servono i plugin (e a chi NON servono)

Un plugin è un **descrittore di capacità per l'agente LLM**, non codice che il runtime carica.

```
plugins/<nome>/
├── PLUGIN.md   frontmatter: name, description, trigger, enabled, tools, requires, procedures
├── tool.json   schemi dei tool MCP (name, description, procedure, inputSchema)
└── <nome>.bas  RIFERIMENTO: le procedure qui dentro NON vengono caricate da nessun backend
```

## Chi li usa davvero

| consumatore | cosa fa |
|---|---|
| **agente LLM** (`AI AGENT ...`) | inietta nel system prompt descrizione + `trigger` dei plugin attivi (`AgentRecord`, `AgentServer`) e, con `AI PLUGINS ENABLE`, registra gli **schemi** di `tool.json` nel `McpToolRegistry` (`PluginRegistry.Enable`) |
| `AI PLUGINS LIST / INFO / REFRESH` | scansione della cartella (`PluginIndex.Scan`) |
| `AI.PLUGIN.CALL("nome", arg)` | legge `procedures` da `PLUGIN.md` e invoca **la prima procedura del PROGRAMMA** con quel nome: C# `CallHandler` → `_basicProcedures`; VM/C → hook in `gmb_builtin.c` |
| **compilatore** | **niente.** Nessun include nel linker/emitter: un `.bas` dentro un plugin non entra mai nella compilazione |

**Il `<nome>.bas` non viene eseguito.** È il contratto di riferimento (le firme attese): per
questo `samples/Community/Browser/BrowserPluginTest.bas` e `samples/Community/Ai/AiPlugins.bas`
**re-implementano** le procedure al loro interno (`BrowserNavigate`, `WebFetch`, …).

## Conseguenze per un programma compilato

- **Non usa l'agente** → non gli serve **nessun** plugin: zero file da distribuire.
- **Usa l'agente** → servono i **descrittori** (`PLUGIN.md` + `tool.json`) accanto all'eseguibile
  e le **procedure nel programma** (o i tool di un server MCP esterno). Si dichiarano nel suo
  `app.settings.json`:

  ```json
  { "plugins": ["web-fetch", "browser"] }
  ```

  Il compilatore copia solo i dichiarati in `<exe>/plugins/<nome>` (vedi
  [ProgramBundle](../../GMBasicSharp/common/CommonLib/ProgramBundle.cs)).
- **C99Native**: `AI.PLUGIN.CALL` **non** esegue la procedura del programma (nessuna VM BASIC nel
  binario): risponde con la stringa dichiarativa `"PLUGIN nome(arg) OK"`. Divergenza
  architetturale nota e tracciata in `samples/Community/Ai/AiPlugins.bas`; su IL/PCode/VM-C la
  procedura viene invocata davvero.

## Cosa NON mettere qui

- Implementazioni che il programma deve avere comunque (quelle vivono nel `.bas` dell'app).
- `PLUGINS_PROMPT.md`: è un **output** rigenerato da `AI PLUGINS REFRESH` in
  `<configDir>/PLUGINS_PROMPT.md` (accanto ad `ai.json`), non contenuto dell'archivio.
