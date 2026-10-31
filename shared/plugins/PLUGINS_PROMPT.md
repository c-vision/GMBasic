# GMBASIC — Archivio Plugin Agente

> **Percorso configurato**: /Users/gaetano/Documents/Projects/GMBasicMonoRepo/shared/plugins
> **Percorso indice**: `/Users/gaetano/Documents/Projects/GMBasicMonoRepo/shared/plugins/PLUGINS_PROMPT.md`
> 7 plugin rilevati.

## Istruzioni per l'agente
Quando ricevi una richiesta che corrisponde a un plugin sotto, leggi il file
`PLUGIN.md` del plugin rilevante (percorso: `plugins/<nome>/PLUGIN.md`) e segui
le sue istruzioni. Un plugin è una **capacità operativa**: può registrare tool
invocabili e procedure BASIC. Se più plugin sono rilevanti, leggili tutti e combinali.

- **`browser`** — Automazione di un browser Chromium headless via CDP (naviga, estrai testo, esegui JS, screenshot). · stato: attivo · trigger: browser, naviga, apri pagina, screenshot, estrai testo da pagina, automazione web
- **`fs-scoped`** — Lettura/scrittura file SOLO nella cartella work/ del progetto (sandbox). · stato: attivo · trigger: leggere file, scrivere file, salvare su file, cartella di lavoro
- **`markdown`** — Converte ed elabora Markdown (via CONVERT / WEB2MD). · stato: attivo · trigger: markdown, convertire markdown, html da markdown, web to markdown
- **`mcp-tool`** — Invoca un tool di un server MCP esistente per conto dell'agente. · stato: attivo · trigger: tool mcp, invocare tool, server mcp, chiamare tool
- **`pdf`** — Converte un PDF in Markdown (PDFOxide, via CONVERT). · stato: attivo · trigger: pdf, documento pdf, convertire pdf, estrarre testo da pdf
- **`report`** — Genera un report testuale strutturato da dati (template). · stato: attivo · trigger: report, generare report, riepilogo dati, tabella
- **`web-fetch`** — Scarica una pagina web e ne produce un riassunto. · stato: attivo · trigger: url, pagina web, scaricare pagina, fetch web
