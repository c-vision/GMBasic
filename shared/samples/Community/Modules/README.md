# samples/modules — risoluzione dei moduli (O7)

Esempio unico per **ogni** posto in cui il compilatore cerca un modulo BASIC
(`*.bas`/`*.inc`). Vale per l'Orchestrator C# e per i driver nativi: entrambi leggono la stessa
configurazione, quindi il sample compila e gira allo stesso modo.

Entry: [`FolderResolution/AllFolders.bas`](FolderResolution/AllFolders.bas).

```
samples/modules/
  FolderResolution/                 <- radice del PROGRAMMA (dove sta la entry)
    AllFolders.bas                  IMPORT di un namespace per ogni cartella
    app.settings.json               importFolders + importFiles + compiler.searchFolders
    include/LocalInclude.bas        searchFolders "include"
    Controls/LocalControl.bas       searchFolders "Controls"
    deep/nested/Nested.bas          scansione RICORSIVA della radice
    chain/Chain.bas                 IMPORT DEMO_DEEP (grafo transitivo)
    libs/widgets/Widget.bas         searchFolders "{project}/libs/*" (glob)
    libs/helpers/Deep.bas           caricato solo perché lo importa Chain.bas
  shared/SharedModule.bas           importFolders "../shared" (FUORI dalla radice)
  pinned/PinnedOnly.bas             importFiles "../pinned/PinnedOnly.bas" (FUORI dalla radice)
```

## Regole (le stesse nei due percorsi)

1. **Radice del programma, ricorsiva**: ogni `.bas`/`.inc` sotto la cartella della entry è un
   candidato, a qualunque profondità.
2. **`importFolders`** (system + app settings): relative al progetto **e** alla root
   dell'install. Raggiungono anche cartelle fuori dall'albero del programma.
3. **`importFiles`**: file caricati sempre, relativi al progetto (poi all'install).
4. **`compiler.searchFolders`**: la lista storica `include` · `Controls` · `controls` ·
   `{install}/include` · `{install}/Controls` · `{install}/libs/*` è solo il **default**; un
   progetto la sostituisce da `app.settings.json`. Placeholder `{project}`/`{install}`, `*`
   finale = ogni sottocartella, percorsi assoluti ammessi. La scansione di ogni voce è
   ricorsiva. Una lista presente **sostituisce** quella globale; `[]` le disattiva tutte.
5. **Shadowing**: un file del progetto/degli import vince, per NOME, su quello di una cartella
   globale.
6. **`IMPORT`-driven**: essere un candidato non basta; entra nel build solo ciò che è
   raggiungibile dagli `IMPORT` della entry, seguito anche dentro i moduli caricati
   (`DEMO_CHAIN` → `DEMO_DEEP`).

## Esecuzione

```bash
# CLI di progetto (O7): discovery + check + build + run
native/gmb_compiler_c/build/gmb check --project samples/modules/FolderResolution
native/gmb_compiler_c/build/gmb run --project samples/modules/FolderResolution

# C99Native diretto (ricetta completa del runtime, anche per chart/db/AI)
tools/gmb_c99native_run.sh samples/modules/FolderResolution/AllFolders.bas o7
```

`check` stampa `7 sorgenti del progetto, 8 moduli` (le due cartelle fuori dalla radice entrano
da `importFolders`/`importFiles`). Output atteso di `run`: una riga per cartella, con la
provenienza del modulo. Gate: `native/gmb_c99native/tests/c99_module_resolve_test.sh` (risoluzione)
e `native/gmb_compiler_c/tests/gmb_cli_project_test.sh` (CLI).

## Verifica della suite

```bash
# TUTTI i MiniTests: driver → C → cc -fsyntax-only (nessun link, nessuna finestra)
native/gmb_compiler_c/build/gmb minitests            # riepilogo leggibile, exit 0/2

# GATE: confronto col baseline versionato, rosso solo sulle regressioni
tools/check_minitests.sh            # exit 0 ok · 2 regressione · 1 setup rotto
tools/check_minitests.sh --update   # rigenera il baseline (dopo un fix o un nuovo MiniTest)
```

È il punto unico: `tools/c99_compile_errors.sh` ora è solo un alias che delega qui. Gate del
comando: `native/gmb_compiler_c/tests/gmb_cli_minitests_test.sh`; baseline:
`native/gmb_compiler_c/tests/minitests_baseline.json` (35 KB, 84 FAIL noti al 2026-10-24).
Il pavimento che scende non fa fallire il gate: appare come `RISOLTO <file>`, dopodiché si
rigenera il baseline con `--update`.
