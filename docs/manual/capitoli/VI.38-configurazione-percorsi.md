# VI.38 — Configurazione e percorsi

## Cosa ottieni da questo capitolo

Quale file di impostazioni viene letto, in quale ordine, e dove finisce ogni cosa che il programma
scarica o produce. È il capitolo che si consulta quando un percorso «non torna»: quasi sempre la
risposta è che un file in una posizione più vicina al programma ha vinto su quello condiviso.

## 1. Due file, due ruoli

| File | Ruolo | Dove sta |
|---|---|---|
| `system.settings.json` | default **globali**, uguali per tutti i programmi della macchina/checkout | `shared/settings/system.settings.json` |
| `app.settings.json` | sovrascritture **per applicazione** | accanto al `.bas` principale (o nella cartella di lavoro) |

La separazione non è cosmetica: `system.settings.json` è il file che si aggiorna con
l'installazione, `app.settings.json` è quello che un progetto porta con sé. Se modifichi il primo
per le esigenze di un singolo programma, la modifica vale per tutti e la prossima installazione la
sovrascrive.

## 2. Ordine di caricamento

Per `system.settings.json` il loader cerca, nell'ordine:

1. `<root>/config/system.settings.json` — la copia locale editabile (ignorata da git);
2. `<root>/Resources/system.settings.json` — il template distribuito;
3. la cartella dell'eseguibile.

Per `app.settings.json`:

1. la cartella del `.bas` principale (o la cartella di lavoro del processo);
2. accanto all'eseguibile, quando il programma è compilato e distribuito.

Perché questa scala: un programma compilato che porta con sé le proprie impostazioni deve funzionare
anche lanciato da un'altra directory. È la ragione per cui l'eseguibile confezionato contiene un
`app.settings.json` proprio (vedi §6).

## 3. Come i due file si fondono

Ci sono due semantiche, e sapere quale vale per quale chiave evita quasi tutti gli errori:

| Chiave | Semantica | Cosa succede |
|---|---|---|
| `defaultNamespaces`, `importFolders` | **unione** | le voci dell'app si aggiungono a quelle globali |
| `compiler.*` | **sostituzione per chiave** | una lista presente nell'app rimpiazza quella globale; una lista assente la lascia intatta; una lista vuota `[]` la disattiva |

`compiler` non si somma perché i flag del compilatore non sono cumulativi: accodare `-O2` a `-O3`
produrrebbe una riga di comando ambigua. Le liste vuote servono proprio a togliere qualcosa — per
esempio `"cflags": []` per far emergere i warning del C generato.

```json
// app.settings.json di un progetto che vuole i warning
{
  "compiler": {
    "cflags": ["-std=c99", "-O2", "-Wall"]
  }
}
```

`-ffp-contract=off` e gli altri flag **critici di parità** non sono configurabili: sono costanti nel
codice che li richiede, perché un bit di differenza fra backend non deve poter dipendere da una
preferenza utente.

## 4. Le sezioni di `system.settings.json`

| Sezione | Contenuto |
|---|---|
| `defaultNamespaces` | namespace importati implicitamente (`SYSTEM`, `CONSOLE`, `GRAPHICS`, `MATH`, `STRINGS`, `DATETIME`, `CONVERSIONS`, `IO`, `CONCURRENCY`, `ARRAYS`) |
| `importFolders` | cartelle in cui cercare i moduli `.bas` degli `IMPORT`; risoluzione `<progetto>/<voce>` poi `<install>/<voce>` |
| `consoleConfiguration` | una voce per console (0-3): `charGridWidth`, `charGridHeight`, `fontPath`, `fontSize`, `fontHinting`, `palette`, `useNativeResolutionRendering`, `useLogicalSizeScaling` |
| `libraryFolders` | cartelle delle librerie dinamiche (`bin/`, `lib/`) |
| `defaultProjectsFolder` | cartella dei nuovi progetti (`PROJECTS`) |
| `paths` | posizione delle cartelle dati (§5) |
| `compiler` | toolchain C e cartelle di ricerca dei moduli (§6) |

La palette si può indicare per nome (`"palette": "c64Classic"`): è la stessa famiglia di nomi del
capitolo IV.18, non un colore singolo.

## 5. La sezione `paths`

Ogni chiave accetta un percorso **assoluto** o **relativo alla root del progetto** (la cartella che
contiene `GMBASIC.sln`); `~` e le variabili d'ambiente sono espanse dal loader. Il valore `null`
significa «usa il default», che è quello documentato qui sotto.

| Chiave | Default | Contenuto |
|---|---|---|
| `configDir` | `config/` | `ai.json`, `providers.json`, `mcp.json`, `vaults.db`, `brain.db`, `brains.json`, `ide.settings.json` |
| `skillsDir` | `<monorepo>/shared/skills` (fallback `<root>/skills`) | archivio delle skill (`SKILL.md`) |
| `pluginsDir` | `<monorepo>/shared/plugins` (fallback `<root>/plugins`) | plugin operativi |
| `logsDir` | `ai-logs/` | log degli agenti |
| `ragDir` | `rag/` | database vettoriali costruiti da `RAG BUILD` |
| `graphsDir` | `graphs/` | export di `GRAPH BUILD` |
| `brainsDir` | `brains/` | cervelli installati da `BRAIN INSTALL` |
| `brainsExportsDir` | `brains-exports/` | export `.gmbrain` |
| `libsDir` | `libs-runtime/` | componenti runtime scaricati on-demand (`torch/ml`, Chromium, OCR), in `<libsDir>/<componente>/<rid>/` |
| `packagesDir` | `packages/` | pacchetti installati, `<packagesDir>/<nome>/<versione>/` |
| `libsCacheDir` | `~/.gmbasic/libs` | cache delle librerie native, `<libsCacheDir>/<rid>/` |
| `abiDir` | — | descrittori `.gmbarch` di LLMEnv (risolto dai dylib C al primo uso) |
| `tempDir` | `null` → cartella temporanea del sistema | cache temporanee (kv-swap, clone dei marketplace, staging download) |

Due distinzioni che generano confusione:

- **`libsDir` ≠ `libsCacheDir`**: il primo è dove vanno i *componenti* scaricati (TORCH, browser,
  OCR), il secondo è la cache delle *librerie native di supporto*, indicizzata per RID. In un
  checkout di sviluppo la build appena fatta in `GMBasic/build/libs/<rid>/` ha la precedenza sulla
  cache.
- **`packagesDir` configurato vince sempre**: se lo imposti, il resolver non cerca né in `vendor/`
  né nella cache utente. È la chiave da usare per un build completamente offline in una cartella
  nota.

## 6. La sezione `compiler`

| Chiave | Significato |
|---|---|
| `cc` | compilatore C (`cc` = quello di sistema). **Non** scrivere `null`: la riga di comando partirebbe vuota |
| `cflags` | flag di compilazione (default: `-std=c99 -O2 -ffunction-sections -fdata-sections -w`) |
| `ldflags` | flag di link (default: `-Wl,-dead_strip`; su Linux `-Wl,--gc-sections`) |
| `defines` | macro `-D` |
| `searchFolders` | cartelle in cui si cercano i moduli `.bas`/`.inc` |

`searchFolders` ha due placeholder e una regola:

- `{project}` = cartella della entry, `{install}` = root di GMBasic;
- una voce **relativa senza placeholder** si cerca nel progetto, poi risalendo (`include/`,
  `Controls/`), poi nella root dell'install;
- `*` finale = tutte le sottocartelle di un livello; la scansione di ogni voce è ricorsiva.

Nel monorepo gli asset condivisi stanno in `<monorepo>/shared` mentre `{install}` è
`<monorepo>/GMBasic`: per questo i default contengono `{install}/../shared/include` e
`{install}/../shared/libs/*`. Se un `IMPORT` non risolve, la prima cosa da guardare è questa lista.

## 7. Override da riga di comando

Per una singola compilazione i valori si sovrascrivono senza toccare i file:

```bash
gmb build --cc=/opt/llvm/bin/clang --cflags=-O3 --ldflags=-flto --defines=NDEBUG
```

Hanno la stessa semantica per chiave di `app.settings.json` e stanno **sopra** i settings: servono
per provare un profilo diverso (un compilatore specifico, un'ottimizzazione) senza modificare un
file condiviso con altre build.

## 8. Un `app.settings.json` che fa più di un lavoro

Oltre a `compiler` e `paths`, l'`app.settings.json` di un programma dichiara **cosa gli serve**:
contenuto (`skills`, `plugins`), dati (`brains`, `fixtures`), e le sezioni di runtime.

```json
{
  "compiler": { "cflags": ["-std=c99", "-O2"] },
  "skills": ["board"],
  "plugins": ["web-fetch"],
  "brains": ["manual@1.2.0"],
  "fixtures": ["markets"]
}
```

Il senso delle dichiarazioni è la distribuzione: quando il programma viene confezionato, ciò che è
dichiarato qui viene portato **accanto all'eseguibile** — le skill nell'archivio `<exe>/skills`, i
plugin in `<exe>/plugins`, i cervelli in `<exe>/brains`, le fixture in
`shared/samples/Fixtures/<nome>/` per il compilatore. Un eseguibile spedito senza queste
dichiarazioni si avvia con l'agente privo di capacità: `AI.SKILLSLIST()` torna vuota e non c'è
nessun tool.

Per questo il confezionamento produce un `app.settings.json` **fuso**: parte da
`shared/settings/system.settings.json`, applica l'`app.settings.json` del programma (che vince
chiave per chiave) e tiene solo le sezioni che il runtime legge davvero. Anche i manifest dei
componenti (`settings/libs-manifest.json`, `settings/brains-manifest.json`) vengono portati
accanto all'eseguibile, così il programma distribuito sa cosa può scaricare e da dove.

## 9. Niente variabili d'ambiente

GMBasic non si configura con variabili d'ambiente. I parametri passano dal **registro delle
opzioni** (`AI SET OPTION`, sezione `engine` dei settings) e dai file descritti qui sopra. Se
trovi una variabile d'ambiente citata in un vecchio appunto, considerala storia: il
comportamento non si governa da lì, e una configurazione «che funziona solo nel tuo terminale»
non è una configurazione.

## 10. Errori frequenti

| Sintomo | Causa | Correzione |
|---|---|---|
| una modifica ai settings non ha effetto | esiste un file più vicino che vince (app sopra system, `config/` sopra `Resources/`) | cerca il file effettivo nell'ordine di §2 |
| `IMPORT` non trova un modulo che esiste | manca la cartella in `searchFolders` o in `importFolders` | aggiungi la voce (o usa il path relativo corretto) |
| i flag di compilazione «spariscono» | una lista presente in `app.settings.json` rimpiazza quella globale | ometti la chiave per ereditare, `[]` per disattivare |
| `cc` non trovato (`/bin/sh: -c: command not found`) | `"cc": null` scritto nel file | ometti la chiave o indica un percorso reale |
| un pacchetto è installato ma non viene trovato | `paths.packagesDir` punta altrove | allinea la chiave o usa `--cache` consapevolmente |
| due programmi si pestano i piedi sui database RAG | `ragDir` condiviso | dai a ciascun progetto il suo `paths.ragDir` |
| l'eseguibile distribuito non trova le skill | l'`app.settings.json` del programma non le dichiara | aggiungi `"skills": [...]` e ri-confeziona |

## 11. Dove andare adesso

- **VI.37** — package manager: cosa finisce in `packages/`, `vendor/` e cache utente.
- **I.1** — installazione: dove si trovano `system.settings.json` e gli asset condivisi.
- **IV.18** — i nomi delle palette usati in `consoleConfiguration`.
- **III.16** — le cartelle delle librerie native e come si dichiarano a `DECLARE`.



<!-- BEGIN SURFACE (generato da tools/sh/manual_surface.sh — non modificare a mano) -->

## Riferimento completo: comandi, funzioni, parametri

> Sezione generata dalle guide del progetto per questo capitolo: contiene **tutte**
> le superfici documentate, non una selezione. La prosa dei capitoli precedenti le
> spiega; qui stanno i dettagli — parametri, varianti, esempi d'uso.

### Options-Guida-Utente

*Fonte: `Options-Guida-Utente`*


## 1. Come si configura

### 1.1 Da BASIC (comando + funzione)

IMPORT AI WITH ALIAS

' Scrittura (comando: nessuna parentesi). Persiste in app.settings.json.
AI SET OPTION "context", "8192"
AI SET OPTION "moe_swap_dir", "work/experts"

' Lettura (funzione: parentesi obbligatorie).
DIM ctx AS STRING
ctx = AI.Option("context")
PRINT "context = "; ctx


### 1.2 Nei file di settings (JSON, sezione `engine`)

{
  "engine": {
    "context": "8192",
    "thinking": "off",
    "moe_swap_dir": "work/experts"
  }
}


### 1.3 Da riga di comando (FastLLM / LLMEnv)

fastllm options                      # elenca tutte le opzioni con descrizione
fastllm chat ds4 --opt context=8192 --opt thinking=off


## 2. Parametri del motore (condivisi GMBasic ↔ LLMEnv)
| Nome | Tipo | Default | Spiegazione |
|---|---|---|---|
| `context` | int | `0` | Capienza contesto/KV in token (`0` = default del modello). |
| `maxtokens` | int | `262144` | Tetto sui token generati. |
| `thinking` | bool | `off` | Reasoning on/off (blocchi di ragionamento). |
| `offload` | bool | `off` | Streaming/cache degli esperti MoE da disco. |
| `offload_cache_mb` | int | `14336` | RAM per la cache esperti (MB). |
| `dtype` | string | `auto` | Tipo pesi: `auto` \| `f16` \| `bf16`. |
| `kvquant` | string | `off` | Quantizzazione KV: `off` \| `4` \| `8[:soglia]`. |
| `prefill` | int | `0` | Chunk di prefill (`0` = default). |
| `temp` | float | `1.0` | Temperatura di sampling. |
| `topp` | float | `1.0` | Top-p. |
| `minp` | float | `0.0` | Min-p. |
| `seed` | int | `-1` | Seed (`-1` = casuale). |
| `tqmemory` | bool | `on` | TurboQuant esperti: `on`=packed (poca RAM) / `off`=affine-2bit (max velocità). |

## 3. Parametri specifici GMBasic

AI SET OPTION "moe_swap_dir", "/Users/me/models/qwen-moe"


## 4. Switch interni (`GMB_*`)

AI SET OPTION "GMB_MOE_NOCONTIG", "1"
AI SET OPTION "GMB_KVQUANT_BITS", "8"
PRINT AI.Option("GMB_MOE_NOCONTIG")


GMB_AGENT_MODE GMB_AI_LIB_DIR GMB_ARCH_DIR GMB_ARGMAX_CAST GMB_ASYNC_DECODE GMB_ASYNC_LAG
GMB_AUTO_FLOOR GMB_AUTO_NOFLOOR GMB_AUTO_NOSAMP GMB_B4_F32 GMB_B4_GS GMB_B4_KEEP GMB_BATCH_SPEC
GMB_BATCH_VARLEN GMB_BENCH_FINANCE_DIR GMB_BENCH_TOP_K GMB_BENCH_TOP_P GMB_BF16_DENSE
GMB_BF16_SCALES GMB_BONSAI_EMBED_INV GMB_BONSAI_FWD_INV GMB_BONSAI_HAD_ORDER GMB_BONSAI_ONCE
GMB_BONSAI_SIGNS GMB_BRAIN_MODEL GMB_BRAIN_URL GMB_BSP_BACKEND GMB_BW_CEILING_GBPS GMB_CB_KVBITS
GMB_CLEAR_CACHE_EVERY GMB_COMPILE GMB_COMPILE_ASYNC GMB_COMPILE_DENSE GMB_COMPILE_FUSE
GMB_COMPILE_QWEN35 GMB_DB_LIB GMB_DF2_ASTYLE_VERIFY GMB_DF2_CAP_DIFF GMB_DF2_DRAFT GMB_DF2_FASTRMS
GMB_DF2_NO_GPU_W GMB_DF2_SEQ_REAPPEND GMB_DF2_SEQ_VERIFY GMB_DF2_SWIGLU_METAL GMB_DF2_TARGET
GMB_DISPATCH GMB_DS_SORTED GMB_DSV4_SWAP GMB_DSV41_METAL GMB_DSV41_MTP GMB_DSV41_WIRELOG
GMB_EXECUTOR_NATIVE GMB_F16_SCALES GMB_F32_SCALES GMB_FFI_NO_LIBS_SCAN GMB_FFN_WCACHE GMB_FIXKV
GMB_FIXTURE_EMBED GMB_FORCE_PLAIN_APPEND GMB_FORCE_REPREFILL GMB_FUSE_GATEUP GMB_FUSE_GEGLU
GMB_FUSE_GEGLU_METAL GMB_FUSE_MOE_DECODE GMB_FUSE_MOE_DECODE_VERIFY GMB_FUSE_MOE_MXFP4
GMB_FUSE_MOE_MXFP4_VERIFY GMB_FUSE_MOE_SGY GMB_FUSE_ROPE GMB_FUSE_ROPE_VERIFY
GMB_FUSE_SWIGLU_COMPILE GMB_FUSE_SWIGLU_METAL GMB_FUSE_SWIGLU_PREFILL GMB_GDN_EVAL
GMB_GDN_GATE_METAL GMB_GDN_GATE_METAL_VERIFY GMB_GEAR GMB_GEMMA_FLASH_DECODE
GMB_GEMMA_FLASH_DECODE_VERIFY GMB_GEMMA_FLASH_PREFILL GMB_GEMVQ4 GMB_GEMVQ4_VERIFY GMB_GFX_NULL


## 5. Diagnostica (restano variabili d'ambiente)

## 6. Esempi completi

IMPORT AI WITH ALIAS
SUB Main()
  AI SET OPTION "context", "8192"
  AI SET OPTION "thinking", "off"
  PRINT "context="; AI.Option("context")
  PRINT "thinking="; AI.Option("thinking")
END SUB


{ "engine": { "context": "8192", "moe_swap_dir": "work/experts" } }


fastllm chat ds4 --opt context=8192 --opt temp=0.7


## Riferimenti

### Paths-Config-User-Guide

*Fonte: `Paths-Config-User-Guide`*


## 0. Reference Card — in una riga
| Cosa vuoi | Come si fa |
|---|---|
| Sapere dove sta il file `system.settings.json` **effettivo** | `p$ = SYSTEM.SYS("SETTINGSPATH")` |
| Leggere una chiave della sezione `paths` | `v$ = JSON.READVALUE(p$, "paths", "ragDir")` |
| Elencare le chiavi di una sezione | `keys = JSON.READSECTION(p$, "paths")` |
| Sapere la root del progetto | `SYSTEM.SYS("CWD")` (se lanciato dalla root) |
| Cambiare una cartella dati | editare `<root>/config/system.settings.json` → sezione `paths` |
| Tornare alle vecchie cartelle (`~/.gmbasic`) | mettere i percorsi assoluti nelle chiavi `paths` |
| Dove stanno i modelli AI | in `modelPaths` di `config/ai.json` (vedi AI-Providers-Guide) |

## 1. Il concetto: la root del progetto come casa dei dati
| Cartella | Default sezione `paths` | Contiene |
|---|---|---|
| `config/` | `configDir` | i file di **stato/config** del layer AI e dell'IDE (vedi §2) |
| `shared/skills/` | `skillsDir` | l'archivio **skills** (ogni skill = sottocartella con `SKILL.md`) + `SKILLS_PROMPT.md` (indice) |
| `shared/plugins/` | `pluginsDir` | i **plugin operativi** (browser, pdf, markdown, mcp-tool, fs-scoped...) |
| `ai-logs/` | `logsDir` | i **log degli agenti AI** (`AI LOG WRITE/WRITELINE`) — git-ignorata |
| `~/.gmbasic/rag/` | `ragDir` | i **database RAG** costruiti da `RAG BUILD` (`<nome>.db`) — **cache utente**: sono dati PRODOTTI dal compilatore, non stanno nel checkout |
| `~/.gmbasic/graphs/` | `graphsDir` | gli **export dei knowledge graph** di `GRAPH BUILD` (`.graphml`, `.mermaid`, `.json`, `.stats.json`) — cache utente |
| `brains/` | `brainsDir` | i **cervelli installati** (`BRAIN INSTALL`): `<name>/rag/...`, `<name>/graphs/...` |
| `brains-exports/` | `brainsExportsDir` | gli **export** di `BRAIN EXPORT` (zip `.gmbrain`) |
| `~/.gmbasic/libs/` | `libsDir` | i **componenti runtime scaricati on-demand**: `ml/<rid>/` (libtorch), `chromium/<rid>/` (browser), `obscura/<rid>/`, `ocr/<rid>/` — fuori dal repo |
| `~/.gmbasic/libs/<rid>/` | `libsCacheDir` | le **librerie native** (nostre pubblicate + esterne: SDL, OpenSSL, codec, torch host…) — cache, scaricata on-demand |
| `GMBasic/build/libs/<rid>/` | — (build) | gli **artefatti di build** delle nostre librerie (sviluppo; git-ignorata) |

## 2. `config/` — i file di stato e configurazione
| File | Contenuto |
|---|---|
| `system.settings.json` | il **settings globale di GMBasic** (console, namespace di default, e la sezione `paths` con le cartelle dati). È la **copia locale editabile**: il sistema la cerca PRIMA di quella in `Resources/` |
| `ai.json` | la **config AI unificata**: `modelPaths` (dove cercare i modelli GGUF/MLX), `skillPaths`, `skillsIndexPath`, `pluginsDefaultPath`, `logPath`, `hfToken`/`hfRepo` (campo legacy, letto per compatibilità ma **mai più scritto** — vedi §2bis), chunk RAG, parametri PageRank |
| `providers.json` | i **provider AI configurati** (version, providers, roleOrder, activeIndex, schemes) — quelli che vedi con `AI PROVIDERS`; le chiavi API nel `config` di ogni provider sono sempre un riferimento `$env:NOME` (mai in chiaro — vedi §2bis) |
| `mcp.json` | la **lista dei server MCP** consumabili come client (`servers`) |
| `brains.json` | il **registry dei cervelli** (quali brain esistono, `attached`/`brains`) |
| `ide.settings.json` | le **impostazioni dell'IDE/REPL** (tab, tema, font, pannelli) |
| `vaults.db` | il **database dei vault** (SQLite: tabelle `vaults`, `vault_folders`, `note_links`, `brain_attach`, `vault_policy`) — **NON versionato** (dato locale generato a runtime) |
| `recursivemas/` | i **team locali del marketplace RecursiveMAS** |
| `brain.db` | la **memoria persistente** del Second Brain (se creata) — **NON versionato** |

## 2bis. Segreti — `.env` (root del progetto, MAI versionato)

# <root>/.env — esempio
HF_TOKEN=hf_xxxxxxxxxxxxxxxxxxxx
DEEPSEEK_APIKEY=sk-xxxxxxxxxxxxxxxxxxxx
GROQ_APIKEY=gsk_xxxxxxxxxxxxxxxxxxxx
OPENAI_APIKEY=
ANTHROPIC_APIKEY=


## 3. La sezione `paths` di `system.settings.json`

// <root>/config/system.settings.json  (sezione "paths") — i valori sotto sono i DEFAULT;
// `null` = cache utente ~/.gmbasic/<nome> per rag/graphs e libs.
"paths": {
  "configDir":    "config",             // file di stato/config AI
  "skillsDir":    "skills",             // archivio skills
  "pluginsDir":   "plugins",            // plugin operativi
  "logsDir":      "ai-logs",            // log agenti AI
  "ragDir":       null,                 // null → ~/.gmbasic/rag (database RAG; i builder scrivono qui)
  "graphsDir":    null,                 // null → ~/.gmbasic/graphs (export GRAPH BUILD)
  "brainsDir":    "brains",             // cervelli installati
  "brainsExportsDir": "brains-exports", // export BRAIN EXPORT
  "libsDir":      null,                 // null → ~/.gmbasic/libs (componenti torch/chromium/ocr)
  "tempDir":      null                  // null → /tmp del SO (cache temporanee)
}


### Esempio: tornare alle vecchie cartelle `~/.gmbasic` senza ricompilare

"paths": {
  "configDir": "~/.gmbasic",             // tutti i file di config tornano lì
  "ragDir":    "~/.gmbasic/rag",
  "graphsDir": "~/.gmbasic/graphs",
  "libsDir":   "~/.gmbasic",             // .../ml/<rid>, .../chromium/<rid>
  ...
}


## 4. Dove il sistema cerca `system.settings.json`
| # | Percorso | Perché |
|---|---|---|
| 1 | `<root>/config/system.settings.json` | la **copia locale editabile** (git-ignorata) — è quella che modifichi tu |
| 2 | `<root>/Resources/system.settings.json` | il **template versionato** (default di distribuzione) |
| 3 | cartella dell'eseguibile (`system.settings.json` o `Assets/Config/...`) | per installazioni distribuite |

## 5. Comandi BASIC: conoscere e leggere i percorsi

### 5.1 `SYSTEM.SYS("SETTINGSPATH")` — il percorso effettivo

p$ = SYSTEM.SYS("SETTINGSPATH")
PRINT "settings: "; p$


### 5.2 `JSON.READVALUE` / `JSON.READSECTION` — leggere il settings da BASIC

IMPORT JSON
IMPORT SYSTEM
IMPORT IO

SUB Main()
    DIM p$ = SYSTEM.SYS("SETTINGSPATH")
    IF NOT IO.FileExists(p$) THEN PRINT "? nessun file settings (default in codice)" : END

    ' legge una chiave della sezione "paths" (JSON annidato)
    PRINT "ragDir    = "; JSON.ReadValue(p$, "paths", "ragDir")
    PRINT "graphsDir = "; JSON.ReadValue(p$, "paths", "graphsDir")

    ' chiave assente/null → ritorna il default (4° argomento)
    PRINT "tempDir   = ["; JSON.ReadValue(p$, "paths", "tempDir", "(default: tmp del SO)"); "]"

    ' elenca le chiavi di primo livello della sezione

| Chiamata | Significato |
|---|---|
| `JSON.ReadValue(key$)` | legge `key` da `app.settings.json` (progetto, INI) |
| `JSON.ReadValue(key$, section$)` | forma INI su `app.settings.json` (progetto) |
| `JSON.ReadValue(file$, section$, key$)` | legge `key` nella sezione `section` del **file dato** |
| `JSON.ReadValue(file$, section$, key$, default$)` | come sopra, con default se assente/null |
| `JSON.ReadSection(file$, section$)` | elenca le chiavi della sezione (JSON) o le righe (INI) |

### 5.3 Esempio completo verificato

' output reale (su questa macchina):
' settings path = /Users/gaetano/Documents/Projects/GMBasicMonoRepo/GMBasic/config/system.settings.json
' ragDir    = rag
' graphsDir = graphs
' tempDir   = [(default: tmp del SO)]
' chiavi paths: 10


## 6. Come funziona sotto il cofano (C# e C, due mondi che convergono)

## 7. I comandi di setup dei componenti (`SETUP TORCH/BROWSER/OCR/ALL`)

SETUP ALL                 ' scarica libs + torch + chromium (chiede conferma)
SETUP TORCH               ' solo libtorch  → ~/.gmbasic/libs/ml/<rid>/
SETUP BROWSER             ' solo Chromium  → ~/.gmbasic/libs/chromium/<rid>/
SETUP OCR                 ' (incluso nel bundle LIBS → ~/.gmbasic/libs/<rid>/)
SETUP status              ' mostra cosa è già scaricato e dove


## 8. Domande frequenti

## 9. Riferimenti

<!-- END SURFACE -->
