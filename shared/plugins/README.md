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
- **`AI.PLUGIN.CALL` su tutte le gambe** (2026-10-31): la procedura del programma viene
  invocata su IL, PCodeVM, VM/C **e C99Native**. Su C99Native non c'è una VM BASIC nel
  binario, quindi il transpiler emette un **dispatcher generato** (`__gmb_plugin_call`) verso
  le `gmb_user_*` del programma; il nome della procedura arriva da `PLUGIN.md`
  (`procedures[0]`, runtime C `gmb_plugin_procedure`). Prima rispondeva la stringa
  dichiarativa `"PLUGIN nome(arg) OK"` senza eseguire nulla.
- **Argomenti**: il primo argomento va nel primo parametro con la stessa regola di
  `mcp_push_args` — testo numerico → numero, altrimenti stringa (la VM converte
  numero→stringa se il parametro è `STRING`); i parametri restanti restano al default (`""`,
  che un parametro numerico converte in `0`). Su VM/C l'**arità** viene dal chunk
  (`arity_addrs`/`arity_vals`), senza la quale un secondo parametro scalzava il primo.
  Verificato 4/4 da `shared/minitests/ai/gen_AI_PluginCallExec.bas` (caso stringa, caso
  numerico e plugin senza procedura).

## Cosa NON mettere qui

- Implementazioni che il programma deve avere comunque (quelle vivono nel `.bas` dell'app).
- `PLUGINS_PROMPT.md`: è un **output** rigenerato da `AI PLUGINS REFRESH` in
  `<configDir>/PLUGINS_PROMPT.md` (accanto ad `ai.json`), non contenuto dell'archivio.
