---
name: pdf
description: Converts a PDF to Markdown (PDFOxide, via CONVERT).
trigger: pdf, pdf document, convert pdf, extract text from pdf
version: 1.0.0
enabled: true
tools: [pdf_to_markdown]
requires: [CONVERT, AI]
procedures: [PdfToMarkdown]
---
# Plugin pdf

Operating instructions for the agent: use `AI.PLUGINCALL("pdf", src)` to convert a PDF to Markdown.
The `PdfToMarkdown` procedure uses `CONVERT <src> INTO out$` (PDFOxide backend).
