---
name: web-fetch
description: Scarica una pagina web e ne produce un riassunto.
trigger: url, pagina web, scaricare pagina, fetch web
version: 1.0.0
enabled: true
tools: [web_fetch]
requires: [HTTP, AI]
procedures: [WebFetch]
---
# Plugin web-fetch

Istruzioni operative per l'agente: usa `AI.PLUGIN.CALL("web-fetch", url)` per scaricare
e riassumere una pagina. La procedura `WebFetch` usa `HTTP.FETCH` + `AI.ASK`.
