---
name: pdf
description: Converte un PDF in Markdown (PDFOxide, via CONVERT).
trigger: pdf, documento pdf, convertire pdf, estrarre testo da pdf
version: 1.0.0
enabled: true
tools: [pdf_to_markdown]
requires: [CONVERT, AI]
procedures: [PdfToMarkdown]
---
# Plugin pdf

Istruzioni operative per l'agente: usa `AI.PLUGIN.CALL("pdf", src)` per convertire un PDF
in Markdown. La procedura `PdfToMarkdown` usa `CONVERT <src> INTO out$` (backend PDFOxide).
