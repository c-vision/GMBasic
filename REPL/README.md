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
- [ ] **F5** — prova di eliminabilità completa (oggi: `otool -L` = nessuna dipendenza
      .NET; `--csharp-cli /nonexistent` → comandi C tutti verdi, `IL`/`PCODE` dichiarati
      non disponibili).
- [ ] Sorgenti: saranno spostati da `GMBasic/native/repl` quando la REPL sarà completa
      e testata (fase F2/F3 del TODO §6). Fino ad allora restano lì e vengono compilati
      dal loro CMake.
- [ ] `CMakeLists.txt` — nascerà con lo spostamento dei sorgenti.

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
