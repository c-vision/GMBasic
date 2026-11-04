# I.1 — Installazione e primo programma

## Cosa ottieni da questo capitolo

Un GMBasic che parte sulla tua macchina, un programma che stampa qualcosa e un eseguibile che
restituisce un codice di uscita alla shell. Alla fine sai scegliere fra i percorsi di esecuzione
e sai dove finiscono i file che il runtime scarica.

## 1. Cosa serve

GMBasic è un solo repository: il compilatore, i runtime, i sample e gli strumenti stanno tutti
nello stesso albero, senza sottomoduli. Per lavorare su un checkout servono due cose:

| Serve | Perché |
|---|---|
| **.NET SDK** | il compilatore C# opzionale (`gmbcs`, backend IL e PCodeVM); la CLI `gmb` e il compilatore C sono in C |
| **un compilatore C** (`cc`) | il backend C99Native traduce in C e compila con il compilatore di sistema |

Le librerie native, i feed di mercato e i cervelli non stanno in git: si scaricano dalle release
dei repository compagni, verificati con sha256, solo quando servono. Se manca una libreria, il
runtime la scarica al primo uso della funzione che la richiede.

## 2. I quattro comandi del primo avvio

Da un checkout, il front `gmb` è un binario C: se non è installato, lo si costruisce una volta.

```sh
cmake -S GMBasic/native/gmb_compiler_c -B GMBasic/native/gmb_compiler_c/build
cmake --build GMBasic/native/gmb_compiler_c/build --target gmb_cli
export PATH="$PWD/GMBasic/native/gmb_compiler_c/build:$PATH"

gmb libs ensure             # librerie native mancanti
gmb fixtures ensure         # feed di mercato per i sample
gmb brain login --github    # una volta: token del repo privato
gmb brain install brainvault
```

`ensure` scarica solo ciò che manca, quindi si può rilanciare senza conseguenze. I comandi
amministrativi (`libs`, `fixtures`, `brain`, `pkg`, `setup`, `rag`, `graph`, …) sono eseguiti dal
tool C `gmbadmin` con lo stesso argv; non esiste più un CLI C#.

Dove finisce tutto questo:

| Cosa | Comando | Destinazione |
|---|---|---|
| librerie native | `gmb libs ensure` | `~/.gmbasic/libs/<rid>/` |
| componenti esterni (torch, Chromium, OCR) | `gmb setup TORCH` e affini | `~/.gmbasic/libs/<componente>/<rid>/` |
| feed di mercato | `gmb fixtures ensure` | `shared/samples/Fixtures/<nome>/` |
| cervelli (RAG, grafi) | `gmb brain install <nome>` | `~/.gmbasic/brains/<nome>/<versione>/` |
| pacchetti con dipendenze esterne | `gmb pkg install <nome>` | cache `~/.gmbasic/packages`, oppure `vendor/` per i pin |

Un solo albero per i dati dell'utente: `~/.gmbasic/`. Lo stato locale del progetto (provider
attivi, server MCP, indici di skill e plugin) sta in `GMBasic/config/`, non entra in git perché
contiene percorsi della macchina.

## 3. Il primo programma

Un programma GMBasic è un file `.bas` in testo semplice. Il più piccolo programma utile non ha
nemmeno una `SUB`: le istruzioni a livello di modulo vengono eseguite in ordine.

```basic
' primo.bas — livello modulo: nessuna SUB, nessun Main
PRINT "Ciao da GMBasic"
PRINT "3 + 4 = "; 3 + 4
DIM s AS STRING = "una stringa tipizzata"
PRINT "str: "; s
```

Il separatore `;` di `PRINT` aggiunge uno spazio fra un valore e il successivo: per questo
`"str: "` seguito da `s` stampa `str: una stringa tipizzata`, con uno spazio solo.

Per eseguirlo:

```sh
gmb primo.bas                                    # binario installato
GMBasic/native/gmb_compiler_c/build/gmb primo.bas   # da checkout
```

## 4. Lo stesso programma sulil backend nativo

Il repository contiene il sample `shared/samples/Native/HelloNative.bas`, che è il programma qui
sotto. È il primo esempio del libro che abbiamo eseguito davvero, sul backend C99Native.

```basic
' shared/samples/Native/HelloNative.bas
PRINT "Hello from Native AOT!"
PRINT "3 + 4 = "; 3 + 4
DIM s AS STRING = "basic native string"
PRINT "str: "; s
FOR i = 1 TO 3
    PRINT "iter "; i
NEXT i
PRINT "NATIVE OK"
```

Comando (dalla radice del monorepo) e output reale:

```sh
bash tools/sh/gmb_c99native_run.sh ../shared/samples/Native/HelloNative.bas hello ""
```

```
Hello from Native AOT!
3 + 4 =  7
str: basic native string
iter  1
iter  2
iter  3
NATIVE OK
```

Il primo spazio in `3 + 4 =  7` e il doppio spazio in `iter  1` non sono refusi: uno è il
separatore di `PRINT`, l'altro è lo spazio finale dentro il letterale `"iter "` più il separatore.

## 5. Tre percorsi, quattro backend

Il linguaggio è lo stesso, cambia chi lo esegue. Dalla CLI i percorsi si scelgono con un flag:

| Percorso | Comando / flag | Cosa succede | Quando usarlo |
|---|---|---|---|
| **C99Native** (predefinito di `gmb`) | `gmb build` / `gmb run` | `.bas` → C → `cc` → eseguibile nativo in `<progetto>/bin` | distribuzione e parità (è la gamba di riferimento) |
| **p-code (VM/C)** | `gmbrepl file.bas` oppure `gmb dump chunk` + `gmb_vm_host` | bytecode A1/GMBC eseguito dal processo `gmb_vm_host` | iterazione senza compilatore C |
| **IL / PCodeVM** | `gmb --engine csharp run --engine il\|pcode` (equivale a `gmbcs run`) | DLL gestita + apphost, runtime .NET | debug del mondo gestito |
| **NativeAOT** | `gmb --engine csharp build --target native` | binario unico NativeAOT, nessun runtime .NET | distribuzione dal compilatore C# |

Il percorso p-code predefinito è eseguito da un motore p-code: la build può usare la VM/C
(processo esterno `gmb_vm_host`) oppure la PCodeVM interna. Il flag `--pcode` forza la seconda,
e la documentazione dei flag segnala quella scelta come materia ancora in assestamento: se ti
serve sapere quale motore hai davanti, guarda l'output di avvio o usa esplicitamente il flag.

I **backend** sono quattro: IL, PCodeVM, VM/C, C99Native. Percorsi e backend non sono la stessa
cosa: `-m` e `-n` sono due modi di distribuire il backend IL, il percorso p-code può finire in
VM/C o in PCodeVM, e C99Native ha il suo script. Per confrontare i quattro backend sullo stesso
programma si usa `parity4` (capitolo III.12).

## 6. Entry point e codice di uscita

Due forme di entry point, entrambe valide:

```basic
SUB Main()
    PRINT "fatto"
END SUB
```

```basic
FUNCTION Main() AS INTEGER
    PRINT "fatto"
    RETURN 2
END FUNCTION
```

Con `SUB Main()` il processo esce con codice `0`. Con `FUNCTION Main() AS INTEGER` il valore
restituito diventa il codice di uscita del processo; `END FUNCTION` senza `RETURN` vale `0`.
`Main` non riceve parametri.

```sh
gmb primo.bas
echo $?          # 2
```

Su POSIX la shell osserva solo gli ultimi 8 bit: un `RETURN 300` si vede come `44`. Il runtime
conserva il valore intero, la shell no.

## 7. Simboli di questo capitolo

| Simbolo | Forma | Note |
|---|---|---|
| `PRINT` | comando | separa i valori con `;`, che aggiunge uno spazio |
| `SUB` … `END SUB` | dichiarazione | procedura senza valore di ritorno |
| `FUNCTION` … `END FUNCTION` | dichiarazione | procedura con valore di ritorno |
| `Main` | nome riservato all'entry point | senza parametri |
| `DIM` | comando | dichiara variabili, con tipo esplicito facoltativo |

## 8. Errori frequenti

| Messaggio o sintomo | Causa | Correzione |
|---|---|---|
| il programma termina subito, senza output | file `.prg` invece di `.bas`, o percorso sbagliato | eseguire il `.bas`; `.prg` è un chunk precompilato |
| `FUNCTION Main(x AS INTEGER)` non parte | `Main` non accetta parametri | togliere il parametro |
| exit code inatteso dalla shell | valore maggiore di 255 | ricordare il modulo 256 |
| `command not found: gmb` | binario non installato, si è in un checkout | compilare `gmb` (`cmake --build GMBasic/native/gmb_compiler_c/build --target gmb_cli`) o usare il path assoluto del build |
| errore di compilazione C su un sample | manca `cc` o una libreria nativa | installare il compilatore C; `gmb libs ensure` |

## 9. Dove andare adesso

- Il capitolo **I.2** mostra la REPL, l'IDE e il Notebook: le tre superfici interattive.
- Il capitolo **I.3** entra nella sintassi: tipi, dichiarazioni, comandi contro funzioni.
- Il capitolo **III.12** spiega cosa fa ciascun backend e come si verifica la parità.




<!-- BEGIN SURFACE (generato da tools/sh/manual_surface.sh — non modificare a mano) -->

## Riferimento completo: comandi, funzioni, parametri

> Sezione generata dalle guide del progetto per questo capitolo: contiene **tutte**
> le superfici documentate, non una selezione. La prosa dei capitoli precedenti le
> spiega; qui stanno i dettagli — parametri, varianti, esempi d'uso.

### CLI-Flags

*Fonte: `CLI-Flags`*


### Distribuzione managed "slim" (default)

## Panoramica

## Flag completi

### Modalità e file
| Flag | Descrizione |
|------|-------------|
| `<file>` | Esegue file .bas — **default: P-Code (PCodeVM interna)**; .prg (P-Code) auto-detect |
| `-m` / `--managed` | Backend **managed** (esecuzione, iterazione): DLL + apphost, closure via **symlink** (~140 KB). Richiede .NET installato |
| `-m -full` | Distribuzione **managed** autonoma: closure copiata + framework .NET accanto, gira senza .NET installato (**120 MB** per un `PRINT`, prima 341 MB) |
| `-n` / `--native` | Backend **nativo**: binario singolo (NativeAOT, Mach-O/ELF), nessun runtime |
| `--pcode` | Forza il percorso P-Code (PCodeVM interna) anche per .bas |
| `--ast` | Forza AST Executor anche per .prg |
| `-noconsole` | Headless: nessuna console (server/automazione) |
| `--run <cmd>` | Esegue comando REPL all'avvio |

### Console
| Flag | Descrizione |
|------|-------------|
| `-c` / `-console` | SDL Console (finestra grafica — community) |
| `-s` | System Console (non bufferizzata) |
| `-c64` | Console Commodore 64: griglia 40x25 + **palette C64** (community — è una modalità console/palette, **non** un compilatore retro) |
| `-w <n>` | Larghezza griglia caratteri |
| `-h <n>` | Altezza griglia caratteri |

### Diagnostica
| Flag | Descrizione |
|------|-------------|
| `-l` / `-lexer` | Log lexer |
| `-p` / `-parser` | Log parser |
| `-ll` / `-linker` | Log linker |
| `-e` / `-executor` | Log executor |
| `-o` / `-orchestrator` | Log orchestrator |
| `--debug-tcp <port>` | Server debug remoto (DAP) |

### Preprocessore
| Flag | Descrizione |
|------|-------------|
| `-DSIMBOLO[=valore]` | Definisce simbolo |
| `/define:SIMBOLO[=valore]` | Formato alternativo |

### Backend C99 — percorso GPM (doppio percorso)
| Flag | Descrizione |
|------|-------------|
| `-gpm:auto` | Rilevamento automatico: GPM ⇔ il programma usa costrutti di concorrenza |
| `-gpm:always` | **GPM sempre (default)** — rete di sicurezza: mai crash da detection mancata |
| `-gpm:linear` | Preferisce il percorso lineare (veloce) **solo se sicuro**; se il programma usa concorrenza → warning e ripiego su Auto |

### Variabili d'ambiente
| Variabile | Descrizione |
|-----------|-------------|
| `GMBASIC_DEFINES` | Simboli separati da `;` |
| `GMBASIC_GPM` | Modalità GPM di default (`auto`/`always`/`linear`) — vale se `-gpm:` non è dato |
| `GMBASIC_HEADLESS=1` | Forza headless |
| `SDL_VIDEODRIVER=dummy` | Driver SDL headless |

## Formato .prg

Magic: 0x50434F44 ("PCOD")
Version: 1
Layout: Magic + Version + Name + Code + Constants + Lines


gmb test.prg


## Struttura nel repo unificato

### BRT/ — runtime services (`GMBasicSharp/common/BRT`, `GMBasicSharp/common/CommonLib`)

### PCodeVM interna (`GMBasicSharp/common/IlBackendRuntime/PCode/`)

## REPL commands (P-Code / PCodeVM interna)

COMPILE <file.bas>  → produce file.prg (chunk A1, GMBC)
LOADPRG <file.prg>  → carica per esecuzione
PRGINFO [file.prg]   → info opcodes/costanti
RUN                  → esegue .prg con la PCodeVM interna
RUN <file.bas> [-m|-n] → esegue .bas (P-Code default, managed, nativo)


## Sottocomandi CLI non-REPL (dispatch in `Program.Main` prima del REPL)
| Comando | Descrizione |
|---|---|
| `gmb rag build\|update\|status\|query <wikiDir> [opzioni]` | Costruzione RAG wiki (vedi `Guide/utente/REPL-Commands-Reference` §RAG) |
| `gmb graph build\|update\|status <wikiDir> [opzioni]` | Grafi Obsidian (vedi §Graph) |
| `gmb brain use <name>` | Collega un brain già disponibile (Second Brain, multi-use) |
| `gmb brain detach <name>` | Scollega (i file restano) |
| `gmb brain list` | Elenca i brain registrati (* = collegato) |
| `gmb brain info <name>` | Info + available + checksum + probe |
| `gmb brain install <name> <zip-or-url> [--path dir] [--activate]` | Importa uno zip `.gmbrain` (remoto URL o locale), valida manifest+checksum, installa con rollback |
| `gmb brain export <name> [--out file.zip]` | Genera lo zip di un brain esistente (spostamento fra macchine) |
| `gmb brain sign <name> <dir> [--out file.zip]` | Firma un cervello utente (probe minimi graph/vectordb) |
| `gmb libs status\|check\|ensure\|prune\|index\|publish` | **Librerie native on-demand** (le librerie NON stanno nel repo). `status` = cache `~/.gmbasic/libs/<rid>` vs indice `libs-manifest.json` per gruppo (support/engine/core/torch); `check` = verifica sha256 (exit≠0 se mancante/corrotto; `--all` include torch); `ensure` = scarica i bundle da `c-vision/GMBasicModules` (default support+core+engine, `--group`/`--file`/`--all`); `index` = rigenera `~/.gmbasic/libs-manifest.json` dalle release pubblicate; `publish <support\|engine\|core> <rid>` = pubblica un bundle (gh). `torch` è `external` (pytorch.org). Vedi `Knowledge/GMBasicModules-Repo-Fidati.md` |
| `gmb init\|pack\|bump\|pkg\|publish\|cache\|restore\|update\|rollback\|trust\|login\|logout\|keygen\|admin` | **CLI package manager** (vedi `CliPackages` + `Guide/utente/Package-Manager-Guida-Utente`): manifest v2, resolver SemVer, lock, FFI, build C, workspaces, audit/SBOM, keyring firme. `gmb register` rimosso (utenti SOLO admin) |
| `gmb restore [dir] [--locked\|--frozen] [--offline] [--production] [--verify] [--require-signed]` · `gmb update [dir] [--check] [--yes] [--production]` | Installa / aggiorna i pacchetti del progetto (`REQUIRE` + `gmb-package.json`). Download interamente in C con **progress bar** su stderr (solo console interattiva) |
| `gmb pkg <sub>` | `install\|list\|remove\|info\|search\|tree\|outdated\|update\|vendor\|why\|prune\|dedupe\|audit\|sbom\|deprecate\|undeprecate\|build\|owners\|yank\|tag\|tags\|sync` |
| **`gmb pkg install <n>[@v] [--local] [--registry u] [--cache d]`** | Senza `--local` installa nella **cache** e, se la stessa versione è in `<exe>/packages`, la **sposta**; con `--local` fa il contrario (crea `<dir dell'eseguibile>/packages` se manca). Se la versione non è in nessuna delle due, la scarica dal registry. |
| **`gmb pkg vendor <progetto> [--cache d]`** | Vendoring offline in `<progetto>/vendor/<nome>/<versione>/` (usa il lock se c'è, altrimenti le `dependencies` del manifest). |
| **`gmb publish <f> [--registry u] [--token t]`** · `gmb pack <dir> [--key s\|--key-file k]` | Pubblica (serve un **token utente**: admin → `user create` → `user status active`). `--key` = HMAC (non verificabile dal client), `--key-file` = **Ed25519** (`.gmbpkg.sig`/`.pub`). Versioni **immutabili** (409 se ripubblichi). |
| **Percorsi** | `vendor/` → `<dir dell'eseguibile>/packages` → cache (`~/.gmbasic/packages` su un client senza compilatore, `<checkout>/packages` in sviluppo). Dettaglio: `Knowledge/PackageManager-Percorsi-e-Funzionamento.md`. |
| `gmb trust add\|remove\|list\|fingerprint` · `gmb rollback [dir] [--list\|--to X]` · `gmb bump <dir> <part>` | Keyring firme · storico lock · version bump |
| `gmb add <name[@ver]> [--dev] [--dir dir]` · `gmb remove <name> [--dir dir]` | Modifica `gmb-package.json` (`dependencies`/`devDependencies`) |
| `gmb cache list\|verify\|dedup\|gc\|clean` · `gmb pack <dir> [--dry-run]` | Cache (dedup/gc **content-addressable**) · pack con dry-run. `verify [--lock f]` = tamper-detection **vs hash del lock su disco** (exit 1 se alterata) |

## Documentazione completa

### Compiler-CLI-Guide

*Fonte: `Compiler-CLI-Guide`*


## 0. Contratto unico e selezione engine

gmb check|build|run --project <dir> [--entry <file>] [--out x]   # engine C (default)
gmb --engine=csharp check|build|run --project <dir> [--entry f]  # engine C# (gmbcs)
gmb dump ast|chunk|c|disasm --project <dir> [--entry f] [--out x]
gmb --engine=csharp dump ast|pcode|il|disasm --project <dir> [--entry f]


## 1. Progetto: `check` · `build` · `run`

gmb check --project <dir> [--entry <file.bas>] [--json] [--strict] [-D NOME[=VAL]]
gmb build --project <dir> [--entry <file.bas>] [--out <exe>] [--json] \
          [--libs[=SEL]] [--plugins[=SEL]] [--skills[=SEL]] [--brains[=SEL]] [--full] \
          [--target native|c99] [-D NOME[=VAL]]
gmb run   --project <dir> [--entry <file.bas>] [--json] [--quiet] [--libs[=SEL]] …

| modo | C emesso |
|---|---|
| `auto` | lo scheduler entra solo se il programma usa concorrenza (`GO`, `SEND`/`RECEIVE`, `WAIT`, future, route HTTP, …): rilevamento automatico |
| `always` | lo scheduler è **sempre** attivo (`gmb_gpm_start` nel `main`): rete di sicurezza se il rilevamento manca un caso |
| `linear` | percorso diretto quando il programma è lineare; se c'è concorrenza ripiega su `auto` (mai crash) |

## 2. Dump per la REPL — `gmb dump` (F1, 2026-11-01)
| comando | cosa produce | note |
|---|---|---|
| `gmb dump ast <file.bas>` | AST leggibile + riepilogo linker (simboli, undefined, duplicati) | in-process, stesso front-end del compilatore |
| `gmb dump chunk <file.bas> [--out f.chunk]` | bytecode **A1/GMBC** (eseguibile con `gmb_vm_host`) | delega a `gmbchunk`; senza `--out` stampa il path temporaneo |
| `gmb dump c <file.bas> [--out f.c]` | **C emesso** (C99Native) | delega a `gmbnative --no-exe`: emette, **non** compila |
| `gmb dump disasm <file.bas>` | disassemblaggio del bytecode A1/GMBC (pc, opcode, argomento, riga) | F1b (2026-11-01): usa il disassemblatore **condiviso** `gmb_bytecode/src/gmb_disasm.c` (nessuna tabella opcode duplicata) |

$ gmb dump ast /tmp/f1.bas
; AST di /tmp/f1.bas (1 statement)
FUNCTION Main(0 params) (L1)
  PRINT (L2)
    STR "ciao"
    BINARY *
      NUM 6

$ gmb dump chunk /tmp/f1.bas --out /tmp/f1.chunk
chunk -> /tmp/f1.chunk (300 bytes)

$ gmb dump c /tmp/f1.bas --out /tmp/f1.c
emesso -> /tmp/f1.c (3077 bytes)

$ gmb dump disasm /tmp/f1.bas
; Disassembly (36 istruzioni, 3 costanti)


## 3. Valutare uno snippet — `gmb eval` (F1, 2026-11-01)

gmb eval 'PRINT "eval ok"; 6*7'
# eval ok 42


## 2bis. Comandi amministrativi in C — `gmbadmin` (F9b, 2026-11-02)
| comando | stato (C) |
|---|---|
| `pkg init/add/remove/bump/list/info/install/uninstall/restore/update/rollback/trust/cache` | **implementato** (runtime `gmb_pkg_*`) |
| `fixtures list/ensure/pull` | **implementato** |
| `setup status`, `libs status` | **implementato** |
| `brain list/info/use/detach` | **implementato** |
| `grep` | **implementato** (motore condiviso) |
| `convert` / `convfmt` / `keygen` | **implementato** (`gmb_doc`, ECDSA) |
| `pack` / `publish` / `login` / `logout` / `register` / `admin` | **implementato** (zip, **firma Ed25519 detached** `.gmbpkg.sig/.pub`, base64+POST registry, config utente chmod 600) |
| `pkg verify` | **implementato** (`firma OK (ed25519)` / `non firmato` / errore esplicito) |
| `libs status/ensure/check/publish` | **implementato** (`gmb_bootstrap`, sha256 vs indice, script di release) |
| `setup LIBS\|OCR\|TORCH\|BROWSER\|OBSCURA\|ALL` | **implementato** (manifest in `shared/settings/`, `gmbfetch` con ripresa+sha256, zip/untar) |
| `daemon` (macOS/launchd) | **implementato**: il job lancia **`gmbrepl <file>`** (non più `dotnet REPL.dll`) |
| `rag build/update/status/query` | **implementato** (`gmb_rag_index_source`/`search`, chunking 2048/256, `--prune`) |
| `graph build/update/status` | **implementato** (`gmb_graph_build`) |
| `hf` / `mlx` | **implementato** (HF API + `gmbfetch`, token da `~/.cache/huggingface/token`) |
| `graph export-obsidian`, `graph watch` (polling) | **implementato** |
| `brain` toolchain completo (`stats/verify/verify --golden/create/register/buildvault/optimize/rename/backup/export/sign/dedupe/extract/merge/convert/diff/doctor/forget`) | **implementato in C** (`gmb_brain_tool_*`); manifest `.gmbrain` con checksum identico a `System.Text.Json` |
| `pkg cache verify`, `publish --repo`, `libs index`, `pack --dry-run`, `pack --sign` | **implementato** |

cmake -S GMBasic/native/gmbadmin -B GMBasic/native/gmbadmin/build
cmake --build GMBasic/native/gmbadmin/build --target gmbadmin
gmb pkg list · gmb fixtures list · gmb brain list · gmb grep "X" dir · gmb convert f.docx -o f.md


## 2ter. Alias, download e REPL opzionale
| cosa | comando | sorgente | destinazione |
|---|---|---|---|
| librerie native | `gmb libs ensure` / `gmb install` | indice `GMBasicModules/releases/download/libs-index/index-<rid>.txt` (sha256 per file) | `~/.gmbasic/libs/<rid>/` |
| brains | `gmb brain install <nome>` | `shared/settings/brains-manifest.json` → asset del repo privato `c-vision/GMBasicBrains` | `~/.gmbasic/brains/<nome>/<ver>/` |
| token GitHub | `gmb brain login --github` | token con scope `repo` | `<configDir>/github.token` (0600) |
| fixtures | `gmb fixtures ensure` | release `fixtures-v1` | `shared/samples/Fixtures/<nome>/` |
| componenti ML/Chromium/Obscura | `gmb setup TORCH\|BROWSER\|OBSCURA` | manifest in `shared/settings/` | `~/.gmbasic/libs/<comp>/<rid>/` |
| modelli HF/MLX | `gmb hf` / `gmb mlx` | HF API | `--dest` o `samples/ai/models/` |

## Parità CLI C ↔ C# (verificata flag per flag)
| flag | `gmb` C | `gmbcs` C# | note |
|---|---|---|---|
| `--project`, `--entry`, `--out` | sì | sì | |
| `--json` | sì | sì | schema **identico** (command/project/entry/sources/modules/[exe/library]/ok/diagnostics), anche sugli errori (`ok:false`, rc 2/3). `run --json` C# **materializza** l'artefatto in `bin/` e riporta lo stesso `exe` del C
| `--strict` | sì (warning = errori) | accettato | il linker C# non produce warning |
| `--quiet`, `-q` | sì | sì | sopprime il progresso (stderr), non gli errori |
| `-D`, `-DNAME`, `--define NAME`, `--define=NAME` | sì | sì | define del preprocessore BASIC; 4 forme identiche |
| `--all` | accettato | sì | C: solo `minitests` (in build non ha effetto); C#: nessun filtro ClosureSlim |
| `--full`, `-full` | sì (= `--libs`) | sì (= `--libs` + runtime .NET) | |
| `--libs[=…]`, `-libs`, `-alllib`, `--alllibs`, `--all-libs` | sì | sì | |
| `--plugins`, `--skills`, `--brains` | sì | sì | |
| `--target native\|c99` | sì (no-op: è l'unico target) | — | `--target il\|pcode` con engine C → errore esplicito che rimanda a `--engine csharp` |
| `--target il\|pcode\|native` | — | sì | |
| `--engine c\|csharp` | sì (csharp = inoltro) | `c` → errore esplicito | |
| `-h`/`--help`, `-v`/`--version` | sì | sì | stringhe di versione distinte (`gmb`/`gmbcs`) |
| **solo C** | `--gpm`, `--root`, `--filter`, `--driver`, `--cc`, `--baseline`, `--interactive`, `--heavy`, `--csharp-cli`, `--no-exe` | rifiutati | minitests/GPM/backend C |
| **solo C#** | rifiutati | `--break`, `--step`, `--il-module` | debug IL |

### FFI (`DECLARE … LIB`) e bundle
| flag | cosa copia | destinazione |
|---|---|---|
| `--libs` (o `-libs`) | chiusura delle dipendenze del programma (C: `otool -L`/`readelf -d`; C#: set di runtime che il motore gestito carica per nome) | `<out>/libs/<rid>/` |
| `--libs=all` | tutte le librerie native del RID presenti nella cache | `<out>/libs/<rid>/` |
| `--libs=runtime\|dev\|ffi\|llmmotor\|torch` | le voci del gruppo nell'indice `~/.gmbasic/libs/<rid>/index.txt` | `<out>/libs/<rid>/` |
| `--libs=ml\|chromium\|obscura\|ocr` | la sottocartella del componente | `<out>/libs/<comp>/<rid>/` |
| `--libs=nome1,nome2` | i file richiesti (anche `SDL2`, `crypto`) | `<out>/libs/<rid>/` |
| `--plugins[=all\|nomi]` | `shared/plugins` (o `<install>/plugins`) | `<out>/plugins/<nome>/` |
| `--skills[=all\|nomi]` | `shared/skills` | `<out>/skills/<nome>/` |
| `--brains[=all\|nomi]` | `~/.gmbasic/brains` (o `<install>/brains`) | `<out>/brains/<nome>/<ver>/` |

## 3bis. Engine C# opzionale — `gmbcs` (F4, 2026-11-01)

gmbcs check <file> [--json]
gmbcs dump ast   <file>
gmbcs dump pcode <file> [--out f]      # P-Code (.pasm) dal sorgente o da un .prg
gmbcs dump il    <file> [--out f]      # IL disassemblato
gmbcs dump disasm <file[.prg]> [--out f]
gmbcs prginfo <file.prg>               # "N opcodes, M constants"
gmbcs build <file> [--target il|pcode|native] [--out dir] [--full] [--all]
gmbcs run   <file> [--engine il|pcode|native]
gmbcs eval  "<istruzioni>" [--engine il|pcode]
gmbcs debug <file> [--break N]… [--step]      # debugger P-Code, comandi su stdin


### Parità fra i due engine
| caso | ramo C | ramo C# |
|---|---|---|
| `RUN` | `gmb dump chunk` + `gmb_vm_host` | `gmbcs run --engine pcode` |
| `RUN --backend c99` | `gmb run --project` (C99Native) | `gmbcs run --engine il` |
| `EVAL` | `gmb eval` | `gmbcs eval` |

## 4. MiniTest (verifica del codegen)

gmb minitests [--root <dir>] [--filter <substr>] [--all] [--json] \
              [--driver <exe>] [--cc <cc>] [--baseline <json>]


## 5. Driver invocati dalla CLI (unica implementazione)
| tool | uso | note |
|---|---|---|
| `gmbchunk <file.bas> <out.chunk>` | `.bas` → bytecode A1/GMBC | usato da `dump chunk`, `dump disasm` (F1b) |
| `gmbnative <file.bas> <out.c> [--no-exe]` | `.bas` → C | `--no-exe` = solo emissione (usato da `dump c`) |
| `gmb_vm_host <file.chunk> [--debug-interactive]` | esecuzione/debug del bytecode | usato da `dump`+run e da `eval` |
| `gmb_bytecode/src/gmb_disasm.c` | disassemblatore **unico** (CLI `dump disasm` + REPL `DASM`) | tabella opcode in un solo posto |

## 6. Rapporto con la REPL unica (in C)

## 7. Riferimenti

### Setup-Infrastructure

*Fonte: `Setup-Infrastructure`*


## 1. Il comando `gmb setup` (CLI = REPL)

gmb setup [LIBS|TORCH|BROWSER|ALL] [--force] [--quiet] [--yes]
gmb setup status                  → RID + stato cache di TUTTI i componenti
gmb setup rm <LIBS|TORCH|BROWSER|ALL> [--force]

| Componente | Cosa scarica | Dove | Manifest |
|---|---|---|---|
| **LIBS** | Librerie native **on-demand**: gruppi `support` + `engine` + `core` (SDL2+extras, SDL3, freetype, harfbuzz, png/jpeg/tiff/webp, pdf_oxide, crypto/ssl, **tesseract**, MLX/llama/ggml, lib `gmb_*`) | `~/.gmbasic/libs/<rid>/` | `~/.gmbasic/libs-manifest.json` (indice) |
| **TORCH** | Host `libgmb_torch` + `libtorch` (CPU) — **solo macOS** | `~/.gmbasic/libs/ml/<rid>/` | `shared/settings/ml-manifest.json` |
| **BROWSER** | Chromium (Chrome for Testing) per il plugin browser | `~/.gmbasic/libs/chromium/<rid>/` | `shared/settings/chromium-manifest.json` |
| **ALL** | Tutti i componenti (chiede conferma con le dimensioni) | — | — |

## 2. Il repo remoto pubblico `c-vision/GMBasicModules`
| gruppo | cosa | release | compresso (osx-arm64) |
|---|---|---|---|
| **support** | SDL2/3+extras, freetype, harfbuzz, pdf_oxide, tesseract+leptonica+tessdata, png/jpeg/tiff/webp/avif, audio, crypto/ssl, hdf5, MoltenVK | `gmb-support-2026.10.28` | 30 MB (61 file) |
| **engine** | *plumbing* linkato da ogni dylib nostra: `libllmcodec_*`, `libllmtoken`, `libllmengine_moe_stream`, `libgmb_codec_*` | `gmb-engine-2026.10.28` | **0,5 MB** (23 file) |
| **llmmotor** | motori: `libllm_ai_mlx`+`mlx.metallib`, llama.cpp/ggml/mtmd + wrapper `libgmb_ai_*` | `gmb-llmmotor-2026.10.28` | 64 MB (22 file) |
| **core** | `libgmb_ai_llm/modelio/finetune`, `libgmb_torch(+loader)`, tray, pkg, gfx, compress/grep, `gmb_sql_*_ext` | `gmb-core-2026.10.28` | 3 MB (12 file) |
| **torch** | `libtorch_cpu`, `libc10`, `libomp`, `libtorch` | *nessuna* — **pytorch.org** | ~130 MB |

## 3. Manifest in `Resources/`

## 4. Flusso dei loader nativi

## 5. Pubblicare/aggiornare i bundle (chi li mantiene)

tools/sh/libs_publish_gh.sh <support|engine|core> <rid> [--index] [--yes]
tools/sh/libs_manifest_gh.sh build|push|show [--rid]
LLMEnv/tools/publish_libs_gmbasicmodules.sh <rid> [--push-manifest]   # engine (staging LLMEnv+FastLLM)


<!-- END SURFACE -->
