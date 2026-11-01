# REPL — la REPL unica del monorepo (C)

Questa cartella ospita la **REPL canonica** del monorepo: un binario **C** che:
1. gestisce input, prompt, comandi, console (System/Standard/SDL/C64), IDE, help,
   debugger, package e fixtures;
2. **pilota** il compilatore C tramite le sue **opzioni** (`gmb dump ast|chunk|c`,
   `gmb eval`, `gmb check|build|run`, `gmb_vm_host`);
3. **non include** lexer/parser/linker/compiler: nessun `#include "gmb_parser.h"` qui.

Il compilatore C# è un **engine opzionale** (`gmbcs`, processo esterno): finché esiste,
la parità con il ramo C è **totale** (nessun degrado). Quando verrà eliminato, la REPL
continua a funzionare identica per il mondo C.

## Stato

- [x] `helps/` — i JSON dell'help (54 file) vivono **qui** (non più in `shared/helps`).
- [x] **Motore C completo** (F0–F3): AST/DASM/PRGINFO/COMPILE/EVAL/RUN via `gmb`,
      `FIXTURES`/`SETUP`/`LLM`, e i comandi di pacchetto (`ADD`/`REMOVE`/`BUMP`/`RESTORE`/
      `UPDATE`/`ROLLBACK`/`TRUST`/`CACHE`) sul runtime C.
- [x] **Engine C# opzionale** (F4a): `--engine c|csharp` + `gmbcs` (`GMBasicSharp/CompilerCli`).
      Con `csharp`: `IL`/`PCODE` reali, `EVAL`, dump, `COMPILE`/`RUN` mappati su P-Code e IL.
      `--csharp-cli <path>` rende l'assenza del mondo C# simulabile (F5).
      Parità: `tools/sh/repl_engine_parity.sh <file.bas>`.
- [x] **F4b** — debug con engine C#: `gmbcs debug` (P-Code: `c/s/o/u/b/bt/p/q`), usato da
      `BREAK`/`STEP` + `RUN`; verificato interattivamente (stop, `bt`, `c`).
- [x] **F5** — prova di eliminabilità: worktree con `GMBasicSharp/` **rimosso** → la REPL,
      `gmb` e `gmb_vm_host` si costruiscono e i comandi C sono verdi; `IL`/`PCODE` dichiarano
      l'assenza. Riproducibile: `tools/sh/repl_no_csharp_proof.sh`.
- [x] **F6** — la REPL **C#** è stata eliminata (`GMBasicSharp/REPL`); il **compilatore C# resta**
      con due CLI: `gmbcs` (engine: run/dump/build/debug) e `gmb` (`GMBasicSharp/Cli`:
      pacchetti/brain/libs/setup/daemon/rag/graph/grep). Questa cartella è l'unica REPL.
- [x] **Sorgenti qui** (F6, 2026-11-01): tutti i `.c/.h` e il `CMakeLists.txt` vivono in
      `REPL/`; build `cmake -S REPL -B REPL/build && cmake --build REPL/build --target gmb_repl`.
      `GMBasic/native/repl/` non esiste più.

## Contratto con i compilatori

| operazione | comando |
|---|---|
| AST | `gmb dump ast <file.bas>` |
| bytecode A1/GMBC | `gmb dump chunk <file.bas> --out f.chunk` |
| C emesso (C99Native) | `gmb dump c <file.bas> --out f.c` |
| disasm | `gmb dump disasm <file.bas>` *(F1b: disassemblatore condiviso)* |
| snippet | `gmb eval "<istruzioni>"` |
| progetto | `gmb check\|build\|run --project <dir>` |
| run bytecode | `gmb_vm_host <file.chunk>` (+ `--debug-interactive`) |
| engine C# (opzionale) | `gmbcs check\|build\|run\|dump …` |

Piano completo e inventario comandi: `TODO.md` §6.
