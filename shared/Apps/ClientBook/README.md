# ClientBook — anagrafica clienti (BASIC, controlli Clipper + DBF)

Applicazione completa in GMBasic: **anagrafica clienti** su database **DBF**, con
interfaccia a controlli **Clipper** (`TEXTBOX`, `DATEPICKER`, `CHECKBOX`, `COMBOBOX`,
`LISTBOX`, `BUTTON`, `STATUSBAR`) e quattro modalità di presentazione scelte da riga
di comando.

## Cosa fa

- Archivio clienti con: **Cognome**, **Nome**, **Data di nascita** (DatePicker),
  **Sesso** (CheckBox `Maschio`), **Paese di nascita** (ComboBox con 20 paesi).
- Operazioni con **pulsanti**: `Aggiungi`, `Modifica`, `Elimina`, `Esci`.
- Selezionando una riga dell'elenco il cliente viene caricato nel form; `Modifica`
  salva i valori del form sul cliente selezionato, `Elimina` lo rimuove (soft-delete
  + `PACK`), `Aggiungi` accoda un nuovo cliente con i valori del form.
- Al primo avvio (archivio assente) inserisce **tre clienti di esempio**; poi è
  possibile cancellarli o modificarli.

## Modalità UI (parametro)

| parametro | modalità | selezione |
|---|---|---|
| `-c` | SystemConsole | `TEXTMODE %SYSTEM` |
| `-t` | StandardConsole (**default**) | `TEXTMODE %ANSI` |
| `-s` | SdlConsole (TUI su SDL) | `TEXTMODE %SDL` |
| `-g` | Graphic (UI pixel nativa) | `SCREEN 12, 900, 700, 32, CELLS` + tema `glass` |
| nessuno | come `-t` | |
| `--selftest` | ciclo CRUD senza UI (DBF temporaneo) | esce con `SELFTEST=PASS`/`FAIL` |
| `--demo` | render + 3 frame e uscita immediata (CI/smoke test; in `-g` salva lo screenshot) | |

Il parametro si legge con `COMMAND$`/`COMMANDCOUNT` (BASIC non ha `argv`, ma ha la
superficie `SYSTEM.COMMAND`).

**Un solo layout**: le coordinate dei controlli sono in **celle**; in grafica la
clausola `USE CELLS` di `SCREEN` le converte nel rettangolo pixel. TUI e GUI mostrano
quindi la stessa schermata con la stessa disposizione (nessun doppio layout da
mantenere).

## Compilazione ed esecuzione

Il compilatore mette l'eseguibile in **`<progetto>/bin`** e lo marca **eseguibile**
(bit 0755 messo esplicitamente dal CLI, non lasciato all'umask di `cc`). Non servono
script di lancio: si compila e si esegue il binario.

**Gli argomenti li elabora il PROGRAMMA compilato**, non il toolchain: `main(argc,
argv)` consegna la riga di comando al runtime, e l'app la legge con
`COMMAND$`/`COMMANDCOUNT`. `gmb build` si limita a produrre l'eseguibile.

```sh
# dalla ROOT del monorepo
./bin/gmb build --project shared/Apps/ClientBook --entry src/ClientBook.bas
#   → shared/Apps/ClientBook/bin/ClientBook   (eseguibile)

# modalità (default: -t) — argomenti del PROGRAMMA
./shared/Apps/ClientBook/bin/ClientBook -g
./shared/Apps/ClientBook/bin/ClientBook -c
./shared/Apps/ClientBook/bin/ClientBook          # -t

# verifica headless del ciclo CRUD
./shared/Apps/ClientBook/bin/ClientBook --selftest

# equivalente "build + run" del CLI, con inoltro degli argomenti al programma:
# dopo `--` il CLI non interpreta nulla, passa gli argv all'eseguibile
./bin/gmb run --project shared/Apps/ClientBook --entry src/ClientBook.bas -- -g
./bin/gmb run --project shared/Apps/ClientBook --entry src/ClientBook.bas -- --selftest
```

Il binario si può lanciare **da qualunque cartella**: i dati stanno accanto
all'eseguibile (`IO.BINPATH()`), non nella CWD.

## File

```
shared/Apps/ClientBook/
  README.md                     questo file
  TODO.md                       lavoro residuo
  src/ClientBook.bas            entry point (SUB Main)
  bin/                          output del compilatore — MAI versionato (gitignored)
    ClientBook                  eseguibile (compilato da `gmb build`)
    ClientBook.c / .o           C emesso e oggetto
    clienti.dbf                 archivio (creato al primo avvio)
    clientbook.png              screenshot prodotto da `-g --demo`
```

`IO.BINPATH()` è la cartella dell'eseguibile: l'archivio vive lì
(`<bin>/clienti.dbf`), così il programma compilato è autosufficiente e spostabile
insieme a `bin/`.

## Dati

`TYPE TCliente` → campi DBF `Cognome C(30)`, `Nome C(30)`, `Nascita C(10)`
(`gg/mm/aaaa`), `Sesso C(1)` (`M`/`F`), `Paese C(40)`.

La data è memorizzata come stringa `gg/mm/aaaa` per non dipendere dalle conversioni
di tipo data fra backend; le componenti si impostano/leggono dal DatePicker con le
proprietà `day`/`month`/`year`.

## Stato

- **v0.1** — CRUD completo, 4 modalità UI, layout unico a celle.
- Verificato sulla leg **C99Native** (default): `--selftest` 6/6; `-t`, `-c`, `-s`,
  `-g` e default con `--demo` tutti `rc=0`; dati e screenshot creati accanto
  all'eseguibile anche lanciandolo da `/tmp`.
