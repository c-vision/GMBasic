# III.14 — Debug e diagnostica

## Cosa ottieni da questo capitolo

Leggere un errore e sapere dove guardare. `TRY`/`CATCH` per gestirlo, `ON ERROR` per il codice
vecchio, e i pochi strumenti che ti dicono qualcosa quando il messaggio non basta.

## 1. Catturare: `TRY`/`CATCH`/`FINALLY`

```basic
TRY
    SELECT CASE N
        CASE 1
            ERROR 11, "Errore A (Divisione per zero)"
        CASE ELSE
            PRINT "Nessun errore generato nel TRY."
    END SELECT
CATCH ex AS ERR WHEN ex.Number = 11
    PRINT "Catturato errore 11. Desc: "; ex.Description
CATCH ex AS ERR
    PRINT "Catturato un errore non specifico. Codice: "; ex.Number
FINALLY
    PRINT "Blocco FINALLY eseguito. Pulizia in corso..."
END TRY
```

| Clausola | Comportamento |
|---|---|
| `CATCH ex AS ERR` | cattura qualunque errore; `ex` è locale al blocco |
| `CATCH ex AS ERR WHEN cond` | cattura solo se la condizione è vera |
| più `CATCH` | valutati **dall'alto verso il basso**, vince il primo compatibile |
| `FINALLY` | eseguito **sempre**: uscita normale, errore o `GOTO` fuori dal `TRY` |
| `ERROR n, "testo"` | lancia un errore con codice e descrizione |
| `ex.Number`, `ex.Description` | codice e testo dell'errore catturato |

Eseguito (`shared/samples/Exceptions/ExceptionsTryIfSelect.bas`, backend C99Native):

```
Dentro il blocco TRY...
Nessun errore generato nel TRY.
IF a riga singola eseguito.
Blocco FINALLY eseguito. Pulizia in corso...
Programma terminato.
```

Il `FINALLY` stampa anche se nessuno ha lanciato niente: è la garanzia che la pulizia avvenga.

## 2. Il codice vecchio: `ON ERROR`

```basic
ON ERROR GOTO handler
LET x = 5 / 0
PRINT "OK (dopo RESUME NEXT)"
ON ERROR GOTO 0
END SUB

handler:
    PRINT "  Catturato errore:"; ERR.Description
    RESUME NEXT
```

`RESUME NEXT` riprende dall'istruzione successiva a quella che ha fallito, `ON ERROR GOTO 0`
disattiva l'handler. È il pattern Clipper, e resta utile quando devi adattare codice esistente.
Per il codice nuovo usa `TRY`: l'handler non è una label condivisa con il flusso normale.

## 3. Anatomia di un messaggio

Un errore non gestito esce con codice, testo e riga:

```
[GMBASIC] ERROR 9999: d.VALUE dovrebbe essere 3.14 (line 73)
```

- il **codice** (`9999`) viene dal programma o dal runtime;
- il **testo** è quello che hai scritto in `ERROR`;
- la **riga** è quella del sorgente, non del C generato.

Se il programma esce con un codice diverso da zero senza stampare nulla, l'errore è nella fase
precedente (compilazione) o nel processo esterno: guarda l'output di compilazione, non il
programma.

## 4. Il debug interattivo: cosa c'è davvero

L'IDE espone breakpoint, step-in/over/out, stack frame, variabili e valutazione di espressioni
tramite un'unica interfaccia, e non sa quale backend stia eseguendo. Oggi il driver
**line-aware** è quello della PCodeVM interna: gli altri due rispondono con un errore esplicito.

| Driver | Stato |
|---|---|
| PCodeVM | operativo (breakpoint, step, variabili, stack) |
| VM/C | stub: errore chiaro, nessun debug a riga |
| C99Native | stub: errore chiaro, nessun debug a riga |

Sul backend nativo il debug si fa per **stampa**: è il motivo per cui il transpiler tiene il C
generato (`tools/out/c99native/<nome>.c`) accanto all'eseguibile — se un valore è sbagliato, lo
si legge là.

## 5. Cosa non c'è più

Le direttive `LEXER ON`, `PARSER ON`, `EXECUTOR ON` del REPL legacy non fanno più parte del
prodotto: non aspettarti un log del parser. Se ti serve sapere cosa ha capito il compilatore,
usa `COMPILE <file> -dump` per vedere il disassemblaggio e, sul nativo, il file C generato.

## 6. Errori frequenti

| Sintomo | Causa | Correzione |
|---|---|---|
| `FINALLY` non eseguito | il processo è terminato prima (`EXIT` di sistema, crash) | non usare `END` dentro il `TRY` |
| `CATCH` non cattura | un `CATCH` precedente è già compatibile | ordina dal più specifico al generico |
| handler `ON ERROR` che riparte da capo | `RESUME` al posto di `RESUME NEXT` | usa `RESUME NEXT` |
| dopo l'errore il programma continua senza messaggi | handler che inghiotte e non registra | stampa almeno `ex.Number` e `ex.Description` |
| `CATCH` di una classe di errore utente che non cattura | istanza di un'altra classe, o `AS ERR` al posto di `AS <Classe>` | la clausola deve dichiarare la classe giusta (o una sua base) |

Una classe di errore utente funziona su **tutte e quattro le gambe** (2026-11-02):
`shared/samples/Exceptions/ExceptionsCustom.bas` compila ed esegue su IL, PCodeVM, VM/C e
C99Native con lo stesso output. L'emitter nativo classifica correttamente l'istanza
(`THROW NEW Classe(...)` è un valore boxato, non un intero) e il dispatch per tipo — comprese
le sottoclassi e il filtro `WHEN` — è identico al backend IL. Guida completa:
`Guide/utente/Exceptions-Guida-Utente.md`.

## 7. La disciplina che rende utile il resto

Un errore va riprodotto **su un file, sul backend che lo mostra**. Un messaggio senza il file e
senza il comando che lo ha prodotto non è un bug: è un aneddoto. Quando scrivi un caso, includi
il percorso del sample, la riga, il backend e l'output esatto — è ciò che rende un difetto
risolvibile da qualcun altro.

## 8. Dove andare adesso

- **II.7** — `GOTO`, `GOSUB` e `ON ERROR` nel flusso.
- **III.15** — verificare che il fix funzioni davvero.
- **III.12** — cosa fa il runtime quando un errore attraversa un task.



<!-- BEGIN SURFACE (generato da tools/sh/manual_surface.sh — non modificare a mano) -->

## Riferimento completo: comandi, funzioni, parametri

> Sezione generata dalle guide del progetto per questo capitolo: contiene **tutte**
> le superfici documentate, non una selezione. La prosa dei capitoli precedenti le
> spiega; qui stanno i dettagli — parametri, varianti, esempi d'uso.

### Debug-Error-Workflow

*Fonte: `Debug-Error-Workflow`*


## Panoramica Strumenti (legacy — ritirati)
| Strumento | Scopo | Stato |
|-----------|-------|-------|
| `test_debug/` | Debug interattivo con tracing completo | ❌ Legacy/ritirato |
| `test_run/` | Compile gate (solo compilazione) | ❌ Legacy/ritirato |
| `test_dasm/` | Compile + disassemble + unit test interni | ❌ Legacy/ritirato |
| `test_exec_all/` | Run headless di TUTTI i sample | ❌ Legacy/ritirato |
| `test_exec_dbf/` | Gate runtime DBF (5 sample critici) | ❌ Legacy/ritirato |
| `test_exec_sql/` | Gate runtime SQL (4 sample) | ❌ Legacy/ritirato |
| REPL | Esecuzione interattiva (`Interpreter/Interpreter.csproj`) | Legacy (in riscrittura su VM/C) |
| Strumento | Scopo | Comando |
|-----------|-------|---------|
| Conformance | Compile + codegen P-Code di tutti gli esempi community | `dotnet run --project tools/runners/GMBASIC.Conformance` |
| vmtest | Esegue un file via VM/C (chunk A1) e mostra output/exit | `dotnet run --project tools/runners/GMBASIC.Conformance -- vmtest shared/samples/community/<categoria>/<file>.bas` |
| Build VM/C | Compila `gmb_vm_host` | `cd GMBasic/native/gmb_vm_c && cmake --build build -j4` |
| Debug C (VM/C) | Backtrace crash della VM/C | `lldb -b -o run -o "bt 15" -o quit GMBasic/native/gmb_vm_c/build/gmb_vm_host` |

## Flusso di Debug per Bug in Sample BASIC

### 1. Conformance Gate (sempre prima)

dotnet run --project tools/runners/GMBASIC.Conformance


### 2. Runtime su Singolo Sample (VM/C)

dotnet run --project tools/runners/GMBASIC.Conformance -- vmtest shared/samples/community/<categoria>/<file>.bas


### 3. Pattern di Bug Comuni (lesioni di riferimento, legacy GPM)

#### Loop Infinito (FOR/DO/WHILE)

#### SUB/FUNCTION non parte

#### END SUB riparte da capo

#### Symbol non trovato dopo EXIT

#### Runner Stuck

### 4. Logging Diagnostico

LEXER ON    # logging lexer
PARSER ON   # logging parser
EXECUTOR ON # logging executor (verbose trace)


Executor.VerboseExecutor = true;  // tracing istruzioni
executor.TraceEnabled = true;     // tracing scheduler


### 5. Queue Errori (legacy C#, riferimento)

### 6. Verifica Output con EXPECT

REM EXPECT: Test completato
REM EXPECT: 3 OK


## Note per Debug Loop (legacy GPM — vedi VMC-Loops per l'equivalente VM/C)

## Riferimenti

### Debug-Unificato-E-Backend

*Fonte: `Debug-Unificato-E-Backend`*


## 1. Eliminazione del bridge C `libgmb_vm_bridge`
| Punto di uso | Prima (bridge C) | Ora |
|---|---|---|
| Default P-Code | `VmCBridge.Run(chunk)` | `PCodeVmRunner.Run(chunk)` |
| REPL | `VmCBridge.Run` | `PCodeVmRunner.Run` |
| IDE (IMMEDIATE) | `VmCBridge.Run` | `PCodeVmRunner.Run` |
| `.prg` (LOADPRG) | `VmCBridge.Run` | `PCodeVmRunner.Run` |

## 2. Debug unificato — 3 backend dietro `IDebugDriver`

### Contratto (`GMBasicSharp/common/BRT/IDebugDriver.cs`)

Start(chunk, breakpoints) / Continue / StepIn / StepOver / StepOut
SetBreakpoint(line) / ClearBreakpoints
IsPaused / IsRunning / CurrentLine
StackFrames / GetVariables(frameId) / Evaluate(expr)
TakeOutput / Stop


### Driver
| Driver | File | Stato |
|---|---|---|
| `PCodeDebugDriver` | `GMBasicSharp/common/IlBackendRuntime/PCode/PCodeDebugDriver.cs` | **operativo** (line-aware) |
| `VmCDebugDriver` | `GMBasicSharp/REPL/VmCDebugDriver.cs` | stub (errore chiaro) |
| `C99DebugDriver` | `GMBasic/native/gmb_c99native/src (C99Native ritirato)` | stub (errore chiaro) |

### Wrapper IDE

### Test

dotnet test GMBasicSharp/tests/GMBASIC.IlBackend.Tests/GMBASIC.IlBackend.Tests.csproj -p:GmbEdition=Commercial --filter "FullyQualifiedName~PCodeDebugDriverTests"


## 3. Flag backend — `-m`/`--managed` e `-n`/`--native`
| Flag | Significato | Richiede |
|---|---|---|
| *(nessun flag)* | **P-Code con PCodeVM interna** (default) | niente |
| `-m` / `--managed` | DLL managed + apphost stub | .NET installato |
| `-n` / `--native` | Singolo binario nativo (NativeAOT) | niente (libs di sistema) |

# default P-Code
dotnet run --project GMBasicSharp/Executor/Executor.csproj -p:GmbEdition=Commercial -- shared/samples/Native/HelloNative.bas
# managed (DLL + apphost)
... -- -m shared/samples/Native/HelloNative.bas
# native (NativeAOT)
... -- -n shared/samples/Native/HelloNative.bas


## File toccati (commit `8fd24a7c`, 23 file)

<!-- END SURFACE -->
