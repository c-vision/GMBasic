---
name: web-fetch
description: Downloads a web page and produces a summary of it.
trigger: url, web page, download page, web fetch
version: 1.0.0
enabled: true
tools: [web_fetch]
requires: [HTTP, AI]
procedures: [WebFetch]
---
# Plugin web-fetch

Operating instructions for the agent: use `AI.PLUGIN.CALL("web-fetch", url)` to download and
summarize a page. The `WebFetch` procedure uses `HTTP.FETCH` + `AI.ASK`.
