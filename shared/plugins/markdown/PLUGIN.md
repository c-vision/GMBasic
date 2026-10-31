---
name: markdown
description: Converte ed elabora Markdown (via CONVERT / WEB2MD).
trigger: markdown, convertire markdown, html da markdown, web to markdown
version: 1.0.0
enabled: true
tools: [markdown_convert]
requires: [CONVERT, AI]
procedures: [MarkdownConvert]
---
# Plugin markdown

Istruzioni operative per l'agente: usa `AI.PLUGIN.CALL("markdown", src)` per convertire
Markdown (o un URL in Markdown) in HTML/testo. La procedura `MarkdownConvert` usa
`CONVERT`/`WEB2MD`.
