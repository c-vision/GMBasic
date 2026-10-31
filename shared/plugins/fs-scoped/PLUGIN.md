---
name: fs-scoped
description: File read/write restricted to the project `work/` folder (sandbox).
trigger: read file, write file, save to file, working folder
version: 1.0.0
enabled: true
tools: [fs_read, fs_write, fs_list]
requires: [STRING]
procedures: [FsRead, FsWrite, FsList]
---
# Plugin fs-scoped

Operating instructions for the agent: SAFE file operations limited to the project `work/` folder
(never outside). Use `AI.PLUGIN.CALL("fs-scoped", ...)`.
