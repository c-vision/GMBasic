# APPS — complete applications in GMBasic

This folder hosts **complete applications written in GMBasic**, obtained by porting existing
applications written in other languages (Go, Python, C#, Rust, ...).

Each application is **self-contained**: its folder holds every file needed to compile and run
the BASIC program (`*.bas` entry point, app-local `*.inc` modules, data/config files, docs).

## Layout

```
APPS/
  README.md                  <- this file (conventions, app index)
  <AppName>/                 <- one dedicated folder per application
    README.md                <- REQUIRED: what it is, upstream links, license/attribution,
                               scope, how to build/run, status
    TODO.md                  <- REQUIRED: complete port todo (upstream feature-by-feature)
    src/                     <- BASIC sources
      <AppName>.bas          <- entry point (SUB Main)
      <AppName>.*.inc        <- optional app-local modules (NAMESPACE ... + IMPORT <Namespace>)
    fixtures/                <- OPTIONAL: versioned input data (never written at runtime)
    bin/                     <- runtime output + artefatti compilati, generated — NEVER versioned
```

> **Perché `bin/` e non `work/`**: `work/` è **riservata ai sample** (`shared/samples/**/work/`)
> e contiene **solo artefatti-dati** prodotti da un programma BASIC per verifica (AGENTS §11.1
> cat.4). Le app complete non sono sample: i loro output e i binari vanno in **`bin/`**, la
> cartella di output di default del compilatore (AGENTS §9.6), che è già gitignorata e va svuotata
> dopo ogni esecuzione.

Shared, reusable code does **not** live here — it lives in the two dedicated root folders:

| What | Where | Notes |
|---|---|---|
| **Reusable includes** (module source for other GMBasic programs: `.inc` files with `NAMESPACE` + `PUBLIC`) | **`<monorepo>/shared/include/`** | Reached through `compiler.searchFolders` in `shared/settings/system.settings.json`: `include`, `Controls`, `controls`, `{install}/../shared/include`, `{install}/../shared/Controls`, `{install}/../shared/libs/*`, `{install}/libs/*` (`{install}` = `GMBASIC_ROOT`, cioè `GMBasic/` nel monorepo). Un file dentro l'app **shadowa** per nome quello globale. Usato con `IMPORT <Namespace>` — stesso meccanismo di `shared/include/Rbac.inc` / `Validation.inc`. |
| **Reusable libraries** (bigger, standalone library code: BASIC modules + header/esportazioni) | **`<monorepo>/shared/libs/<nome>/`** | Le librerie sono **progetti** versionati (`src/`, `include/`, `build.sh`, `samples/`, `<nome>.inc`). **Non** sono un cache di dipendenze: i **binari** sono ignorati (`shared/.gitignore`: `libs/*/lib*.{dylib,a,so,dll}`, `libs/*.{dylib,a,so,dll}`) e si producono/pubblicano con `tools/sh/gmb_lib.sh build|check|publish`. La vecchia cache `/libs/` (~130 MB) **non esiste più**. Per una libreria nuova: `shared/libs/<nome>/` + export in `shared/include/<nome>.inc` (regola: `Knowledge/` del vault, `Librerie-e-Pacchetti`). |

## How the compiler finds an app's files

The compiler treats **the directory of the entry `.bas` file** as the *project root* and
discovers, recursively, every `*.bas` and `*.inc` below it (`GMBasicSharp/Executor/Orchestrator.cs`,
`DiscoverSourceFiles`; lato C: `GMBasic/native/gmb_compiler_c/frontend/src/gmb_inc_resolve.c`,
stessa semantica). Poi risolve gli `IMPORT` a partire dall'entry file.

**Dove cerca i file** (in ordine, i file dell'app **shadowano** per nome quelli globali):

1. scansione **ricorsiva** di `*.bas`/`*.inc` nella cartella dell'app;
2. `importFolders` + `importFiles` dei settings (`shared/settings/system.settings.json` +
   `app.settings.json`), risolti sia rispetto al progetto sia rispetto all'install;
3. `compiler.searchFolders` (stessi settings): default `include`, `Controls`, `controls`,
   `{install}/../shared/include`, `{install}/../shared/Controls`, `{install}/../shared/libs/*`,
   `{install}/libs/*` — dove `{install}` è la radice dell'install (`GMBASIC_ROOT`, nel monorepo
   `GMBasic/`). Non c'è alcun file-marker: la lista è **configurazione**, non una risalita dalla
   cartella dell'app.

There is **no textual `INCLUDE` directive** in the language: modularity is `IMPORT` + `.inc`
modules (optionally packages: `gmb-package.json` / `REQUIRE`, see `GMBasicSharp/REPL/CliPackages.cs`).

Consequence: keep one app per folder and keep the entry file at the app level you want scanned —
a stray `SUB Main` in a sibling file of the same project root is a link-time risk.

## Output conventions

* Generated data (databases, exports, logs, dumps) goes to **`bin/` inside the app folder** —
  e.g. `APPS/Masume/bin/masume.sqlite`. `**/bin/` is gitignored, so runtime artifacts never
  pollute the repository or the app's tracked contents.
* **`work/` non si usa nelle app**: esiste solo sotto `shared/samples/**/work/` (artefatti-dati
  dei sample). Vedi la nota nel §Layout.
* Never write generated files into the repo root (see `AGENTS.md` §9) — the root is not a
  scratch space.
* `AGENTS.md` §11 (4 categories of files) applies to apps too: fixtures are *versioned inputs*,
  `bin/` is *generated output*.

### Data-file extensions and git

`.gitignore` ignores `*.db`, `*.dbf`, `*.dat`, `*.isdb`, `*.ntx`, `*.fpt` repo-wide, and
`**/bin/` hides anything generated. Consequence: a **fixture** database cannot be committed
with those extensions — use `.sqlite` / `.sqlite3` (not ignored), or generate the fixture at
runtime with a seed script (`git add -f` is a last resort, and never for generated output).

## How to run an app

**Tutti i comandi si lanciano dalla ROOT del monorepo** (`GMBasicMonoRepo/`); i percorsi delle
app sono quindi `shared/APPS/<AppName>/src/<AppName>.bas`.

```sh
# Dev loop — un file, una gamba
tools/sh/harness.sh il    shared/APPS/<AppName>/src/<AppName>.bas
tools/sh/harness.sh pcode shared/APPS/<AppName>/src/<AppName>.bas
tools/sh/harness.sh vmc   shared/APPS/<AppName>/src/<AppName>.bas

# Release: eseguibile NativeAOT (REPL è l'unico progetto eseguibile)
dotnet run --project GMBasicSharp/REPL -- -n shared/APPS/<AppName>/src/<AppName>.bas

# C99Native (frontend C + emitter → `cc` → esecuzione)
bash tools/sh/gmb_c99native_run.sh ../shared/APPS/<AppName>/src/<AppName>.bas   # path relativo a GMBasic/
```

> [!warning] **`GMBasicSharp/Executor` NON è eseguibile** (`OutputType=Library`): `dotnet run
> --project GMBasicSharp/Executor` fallisce. I runner di `tools/runners/` (`test_ilrun`,
> `test_pcode`, `test_vmrun`) sono l'unico percorso di esecuzione di un `.bas`.
>
> **Simboli compile-time (`-D`)**: **nessun runner li passa** — `test_ilrun`/`test_pcode`/`test_vmrun`
> non hanno l'opzione e `REPL -D` vale per le direttive della REPL, non per l'esecuzione di un file.
> Per provare un modo diverso si aggiunge in testa al file la riga `#DEF <SIMBOLO>` (equivalente
> in-sorgente di `-D`, verificato) — il supporto CLI è tracciato nel gap
> `Todo/(open) - CLI-Args-e-Env-per-Applicazioni-BASIC` (vault del monorepo).

Gli script per-app vivono nella cartella dell'app (`ServiceConsole/console_tui.sh`,
`console_gui.sh`) e impostano da soli la working directory (CWD = `shared/`, così l'app trova il
proprio file di config); risolvono gli strumenti dalla root del monorepo.

Note: BASIC currently has **no runtime argv / environment access** (no `ARGV`, no `ENV$()`
builtin — verificato su `shared/spec/registry/surface.csv`; `REPL/Arguments.cs` interpreta solo
la CLI del tool). Apps therefore select their behaviour through `#IFDEF` compile-time symbols
(`-D<APP>_MODE_<MODE>`), project/config files, and interactive UI. Il gap è tracciato nel vault:
`01-Projects/GMBasicMonoRepo/Todo/(open) - CLI-Args-e-Env-per-Applicazioni-BASIC.md`.

## Rules for a port (non-negotiable)

1. **No stubs.** Every ported feature either works or is explicitly documented as not
   implemented (with a clear runtime error / TODO entry). Silent no-ops are forbidden
   (`Startup-Protocol.md` — "DIVIETO ASSOLUTO DI CREARE STUB").
2. **Canonical syntax.** Commands never take parentheses; functions always do; the namespace is
   the domain name (`AGENTS.md` §1–§2). Same-word read/write pairs must be split into a command
   (write) and a function (read).
3. **Docs stay in the vault.** User manuals, guides, analyses, plans → Obsidian
   `01-Projects/GMBasicMonoRepo/` (`Guide/`, `Knowledge/`, `Piani/`, ...). The repo `docs/` folder is
   gitignored and off-limits. The app's own `README.md` / `TODO.md` are part of the app.
4. **Licence compliance.** Every port keeps the upstream licence and attribution in its
   `README.md` (upstream repository URL, licence name, what was reused and what was rewritten).
5. **Verified, not assumed.** Say how the app was executed and what output was observed. Run the
   narrowest command that exercises the change (no massive test sweeps without permission,
   `AGENTS.md` §0).
6. **LLM: il modello si configura nei settings dell'app** (regola, 2026-11-01). Ogni app che
   **esiste o esisterà** e usa un modello LLM **deve** permettere di specificarlo nei **propri**
   settings (project file/`.cfg`/`app.settings.json` della app), **mai hardcoded** nel `.bas`.
   Convenzione: sezione `[llm]` con `model = "<alias/provider>"` (+ `capability` opzionale);
   `model` vuoto = provider già attivo (default del linguaggio); alias non registrato = **errore
   esplicito** (nessun fallback silenzioso). Riferimento vivo: `Masume` (`.masume.toml` §`[llm]`,
   `LlmActivate`). Motivo: cambiare modello (locale/remoto, quantizzato, altro) senza ricompilare
   e senza toccare il codice dell'app.

## App index

| App | Upstream | Language | Licence | Status |
|---|---|---|---|---|
| [`Masume`](Masume/README.md) | [turanmahmudov/masume](https://github.com/turanmahmudov/masume) — terminal database client with AI chat and an MCP server | Go | Apache-2.0 | M1 headless core (status / seed / objects / run / check) — in progress; **PCode verificato**; C99Native non compila (bug aperto nel vault: `Bugs/(open) - Masume-C99Native-Emissione-DoubleToString`) |
| [`ServiceConsole`](ServiceConsole/README.md) | — (originale) | BASIC | GMBasic | banco di prova **daemon + GUI polimorfica** (HTTP in-language, tray, console TUI; script `console_tui.sh`/`console_gui.sh`) |

> **Non è più qui**: l'IDE TUI (`GMBasicTuiIde`, prima `APPS/GMBasicIde`) è stato promosso a
> **progetto autonomo del monorepo** il 2026-10-31 → `GMBasicTuiIde/` alla root (accanto al
> submodule grafico `GMBasicIDE/`). Motivo: serve da banco di misura del footprint di un programma
> BASIC complesso e compilato.

A port starts only when its folder has `README.md` + `TODO.md` and an entry `.bas` that compiles
and runs.

