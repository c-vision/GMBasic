---
name: report
description: Genera un report testuale strutturato da dati (template).
trigger: report, generare report, riepilogo dati, tabella
version: 1.0.0
enabled: true
tools: [report_generate]
requires: [STRING, AI]
procedures: [ReportGenerate]
---
# Plugin report

Istruzioni operative per l'agente: usa `AI.PLUGIN.CALL("report", args)` per generare un
report testuale. La procedura `ReportGenerate` compone titolo, sezioni e righe in un
documento ordinato.
