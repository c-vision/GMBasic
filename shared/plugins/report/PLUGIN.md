---
name: report
description: Generates a structured text report from data (template).
trigger: report, generate report, data summary, table
version: 1.0.0
enabled: true
tools: [report_generate]
requires: [STRING, AI]
procedures: [ReportGenerate]
---
# Plugin report

Operating instructions for the agent: use `AI.PLUGINCALL("report", args)` to generate a text report.
The `ReportGenerate` procedure composes title, sections and rows into a single document.
