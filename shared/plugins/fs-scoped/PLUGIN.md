---
name: fs-scoped
description: Lettura/scrittura file SOLO nella cartella work/ del progetto (sandbox).
trigger: leggere file, scrivere file, salvare su file, cartella di lavoro
version: 1.0.0
enabled: true
tools: [fs_read, fs_write, fs_list]
requires: [STRING]
procedures: [FsRead, FsWrite, FsList]
---
# Plugin fs-scoped

Istruzioni operative per l'agente: operazioni file SICURE limitate alla cartella `work/`
del progetto (mai fuori). Usa `AI.PLUGIN.CALL("fs-scoped", ...)`.
