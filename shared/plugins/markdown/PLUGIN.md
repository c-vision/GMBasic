---
name: markdown
description: Converts and processes Markdown (via CONVERT / WEB2MD).
trigger: markdown, convert markdown, html from markdown, web to markdown
version: 1.0.0
enabled: true
tools: [markdown_convert]
requires: [CONVERT, AI]
procedures: [MarkdownConvert]
---
# Plugin markdown

Operating instructions for the agent: use `AI.PLUGIN.CALL("markdown", src)` to convert Markdown
(or a URL into Markdown) to HTML/text. The `MarkdownConvert` procedure uses `CONVERT`/`WEB2MD`.
