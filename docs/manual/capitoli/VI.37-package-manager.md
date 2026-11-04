# VI.37 — Package manager

## Cosa ottieni da questo capitolo

Come si impacchetta codice riusabile, come lo si installa in un progetto e dove finisce sul disco.
Il modello è quello dei manifest (`gmb-package.json`), con una differenza che conta: un pacchetto
GMBasic può portare sorgenti BASIC, bytecode, assembly .NET **o** una libreria C con il suo header,
e il compilatore sa come trattare ognuno senza che tu glielo dica.

## 1. Un pacchetto è una cartella con un manifest

Non c'è un formato opaco da imparare: un pacchetto è una **cartella** con un file
`gmb-package.json` alla radice, più i file che il manifest dichiara. Il manifest sta nella stessa
posizione in tutti i casi — progetto, cache, archivio — e questo è ciò che rende un pacchetto
riconoscibile anche quando è già installato:

```
<pacchetto>/gmb-package.json        ← il manifest, sempre alla radice
```

Da lì discende tutto il resto: la ricerca, il lock, la verifica della firma e il linker dei
backend cercano quel file, non un indice esterno.

## 2. Il manifest, campo per campo

| Campo | Obbligatorio | Significato |
|---|---|---|
| `name` | sì | nome del pacchetto; è anche il nome della cartella nella cache |
| `version` | sì | versione SemVer (`1.5.6`, `0.1.1`, `2.0.0-rc1`) |
| `description` | no | una riga di descrizione, mostrata da `PACKAGE SEARCH`/`INFO` |
| `contentType` | no | `source`, `dotnet`, `vm`, `c-ffi`, `mixed` (default `source`) |
| `exports` | no | file `.inc`/`.bas` iniettati prima del sorgente quando il pacchetto è richiesto |
| `samples` | no | cartelle di esempi copiate in `<root>/samples/packages/<nome>@<versione>/` |
| `files` / `ignore` | no | allowlist e deny (glob) dei file inclusi nel `.gmbpkg` |
| `binaries` | per `dotnet`/`vm` | mappa `backend → { piattaforma → file }` |
| `ffi` | per `c-ffi` | `api`, `headers`, `libs` per piattaforma, `link`, `declareExample` |
| `build` | no | come il publisher produce gli artefatti (`system`, `target`, `cwd`, `outputs`) |
| `dependencies` | no | dipendenze runtime, `nome → vincolo` |
| `devDependencies` | no | installate solo per il pacchetto radice (escluse con `--production`) |
| `optionalDependencies` | no | se non risolvibili l'install prosegue con un avviso |
| `peerDependencies` | no | dichiarate ma **non** installate: le deve fornire il consumatore |
| `overrides` | no | forza la versione di un pacchetto, anche transitivo |
| `features` | no | dipendenze opzionali per gruppo, stile `cargo`; `default` attivo di base |
| `workspaces` | no | glob dei workspace del monorepo (`["packages/*"]`), solo nel manifest di progetto |
| `signatureAlgo` / `signaturePublicKey` / `signatureValue` | no | firma di contenuto (`ecdsa-p256-sha256`) |

Il campo che cambia davvero il comportamento è `contentType`:

| `contentType` | Cosa contiene | Cosa fa il compiler/loader |
|---|---|---|
| `source` | `.bas`/`.inc` | inietta gli `exports` prima del sorgente che lo richiede |
| `vm` | bytecode/chunk | carica il chunk già compilato, nessuna ricompilazione |
| `dotnet` | assembly .NET | carica l'assembly del backend IL per la piattaforma corrente |
| `c-ffi` | libreria C + header | aggiunge `-I`/`-L`/`-l` e rende disponibili i `DECLARE … LIB` |
| `mixed` | più forme insieme | sceglie la forma disponibile per il contesto di build |

## 3. Il ciclo di vita

| Comando | A cosa serve |
|---|---|
| `PACKAGE INIT <dir> --name X --version V [--type source\|dotnet\|vm\|c-ffi\|mixed]` | crea `gmb-package.json` e `src/` in una cartella vuota |
| `PACKAGE PACK <dir> [--out out.gmbpkg]` | impacchetta la cartella in un archivio `.gmbpkg` |
| `PACKAGE PUBLISH <file.gmbpkg> [--registry url] [--token t]` | carica l'archivio su un registry |
| `PACKAGE SEARCH <term> [--registry url]` | cerca nel registry |
| `PACKAGE INSTALL <name[@ver]> [--registry url]` | scarica e installa nella cache |
| `PACKAGE REMOVE <name>` | rimuove un pacchetto installato |
| `PACKAGE LIST` | elenca i pacchetti presenti nella cache |
| `BUMP <dir> <major\|minor\|patch\|prerelease\|X.Y.Z>` | aggiorna `version` nel manifest |

Il giro minimo, dall'inizio alla fine:

```bash
# publisher
PACKAGE INIT ./mio-pkg --name mio-pkg --version 1.0.0 --type source
PACKAGE PACK ./mio-pkg --out mio-pkg-1.0.0.gmbpkg
PACKAGE PUBLISH mio-pkg-1.0.0.gmbpkg

# consumatore
PACKAGE SEARCH mio
PACKAGE INSTALL mio-pkg@1.0.0
RESTORE
```

`PACKAGE PACK` è uno zip del contenuto: manifest, `exports`, sorgenti e gli artefatti dichiarati.
Per i backend che richiedono binari (`dotnet`, `vm`, `c-ffi`) serve il compilatore; il pack di soli
sorgenti funziona sempre. I comandi `INSTALL`/`RUN`/`RESTORE`/`UPDATE` hanno una forma REPL
dedicata; gli altri (`INFO`, `LIST`, `TREE`, `OUTDATED`, `VENDOR`, `WHY`, `PRUNE`, `DEDUPE`,
`AUDIT`, `SBOM`, `DEPRECATE`, `UNDEPRECATE`, `BUILD`, `OWNERS`, `YANK`, `TAG`, `TAGS`, `SYNC`)
sono inoltrati alla stessa implementazione che usa la CLI, quindi si comportano allo stesso modo
da REPL e da terminale.

## 4. Dove finiscono i pacchetti

La regola è una sola e vale per compiler, resolver e caricatore FFI: **vendor → locale → cache**.

| Posizione | Percorso | Quando |
|---|---|---|
| pin di progetto | `<progetto>/vendor/<nome>/<versione>/` | versioni bloccate, riproducibili e offline |
| installazione locale | `<cartella dell'eseguibile>/packages/<nome>/<versione>/` | `--local`, o install fatta accanto all'app |
| cache utente | `~/.gmbasic/packages/<nome>/<versione>/` | download dal registry, utente senza compiler |
| sviluppo | `<checkout>/packages/<nome>/<versione>/` | nel monorepo, dove le sorgenti dei pacchetti sono versionate |

`paths.packagesDir` nei settings, se impostato, **vince su tutto** (VI.38). `--cache <dir>` sposta
la cache per una singola operazione. Un `packages/` dentro un sample o sotto `samples/` non è una
posizione valida: o è vendor, o è cache.

## 5. Dipendenze, lock e rollback

Il lock è `gmb-package.lock`: elenca le versioni **esatte** risolte, con l'hash del contenuto. Non
si modifica a mano.

```basic
RESTORE              ' scansiona i REQUIRE dei sorgenti e installa ciò che manca, poi scrive il lock
RESTORE --locked     ' usa SOLO le versioni del lock: riproducibile e offline
UPDATE --check       ' mostra il piano, non tocca niente
UPDATE --yes         ' aggiorna all'ultima versione che soddisfa i vincoli
ROLLBACK --list      ' elenca gli storici del lock
ROLLBACK --to 20261008-134100-123
```

Ad ogni cambio il lock precedente viene archiviato in `<progetto>/.gmbasic/lock-history/`: è
l'unica cartella `.gmbasic` legittima **dentro** un progetto, ed è ciò che rende `ROLLBACK`
possibile senza git. I vincoli vengono da tre fonti, in quest'ordine: le `dependencies` del
manifest di progetto, i `REQUIRE` trovati nei `.bas`/`.inc`, e gli `overrides` che li forzano.

`REQUIRE` non è un `IMPORT`: prende un pacchetto per nome e versione, non un namespace. Il
compilatore risolve il pacchetto, inietta i suoi `exports` **prima** del sorgente e solo dopo
compila — per questo un `.inc` di pacchetto può dichiarare tipi usati più sotto nel tuo file.

## 6. Firma e fiducia

Le firme dei pacchetti si verificano contro un keyring locale,
`~/.gmbasic/trusted-keys.json`:

| Comando | Effetto |
|---|---|
| `TRUST ADD <name> <publicKey\|file>` | registra la chiave pubblica di un pacchetto |
| `TRUST LIST` | elenca nome e impronta delle chiavi fidate |
| `TRUST FINGERPRINT <publicKey\|file>` | calcola l'impronta di una chiave senza registrarla |
| `TRUST REMOVE <name>` | toglie la fiducia |

Il default è **TOFU** (trust on first use): la prima chiave vista per un pacchetto viene
registrata; se in seguito cambia, l'installazione lo segnala invece di accettarla in silenzio. Con
`--require-signed` la policy diventa stretta: un pacchetto non firmato viene rifiutato.

## 7. Manutenzione della cache

| Comando | Effetto |
|---|---|
| `CACHE list` | pacchetti e versioni presenti nella cache |
| `CACHE verify` | integrità sha256 rispetto al lock |
| `CACHE dedup` | internalizza i file identici come blob + hardlink (`.content`) |
| `CACHE gc` | rimuove i blob non più referenziati |
| `CACHE clean` | svuota la cache |

Su una macchina con più progetti, `dedup` è il comando che fa risparmiare spazio: due versioni dello
stesso pacchetto che condividono la maggior parte dei file smettono di occupare due volte lo stesso
contenuto. `verify` è quello da lanciare quando un build fallisce in modo strano dopo un
trasferimento.

Per capire **perché** un pacchetto è installato: `PACKAGE TREE` mostra l'albero risolto,
`PACKAGE WHY <nome>` la catena che lo ha tirato dentro, `PACKAGE OUTDATED` cosa è indietro,
`PACKAGE AUDIT` le advisory di sicurezza note, `PACKAGE SBOM` l'elenco in formato macchina.

## 8. Esempio reale: un pacchetto `c-ffi`

`packages/netlb/0.1.1/gmb-package.json` è un pacchetto completo, non un esempio didattico:

```json
{
  "name": "netlb",
  "version": "0.1.1",
  "description": "Load balancing (RR/WRR/random/LC/WLC/LRT/P2C+EWMA/consistent-hash+vnodes/HRW/Maglev + health/slow-start/retry/draining) via shim FFI gmbsuite libnetlb (self-contained).",
  "contentType": "c-ffi",
  "exports": ["netlb.inc"],
  "samples": ["samples"],
  "build": { "system": "make", "target": "packlib", "cwd": "..", "tool": "cc", "outputs": ["lib/{platform}/libnetlb.dylib", "include/netlb.h"] },
  "ffi": {
    "api": "c-abi",
    "headers": ["include/netlb.h"],
    "libs": { "osx-arm64": "lib/osx-arm64/libnetlb.dylib" },
    "link": "",
    "declareExample": "DECLARE FUNCTION POOLNEW LIB \"netlb\" ALIAS \"netlb_pool_new\" (policy AS LONG, seed AS LONG) AS LONG"
  },
  "dependencies": {}
}
```

Le quattro cose da notare:

- `exports: ["netlb.inc"]` — chi fa `REQUIRE netlb` si trova le dichiarazioni già iniettate;
- `build.outputs` con `{platform}` — il publisher compila per la piattaforma corrente e il file
  prodotto ha il nome giusto per ogni RID;
- `ffi.libs` è una mappa **per piattaforma**: un pacchetto può dichiarare solo `osx-arm64` e
  risultare non installabile altrove, il che è meglio di un binario sbagliato;
- `declareExample` è documentazione eseguibile: la riga `DECLARE` che il consumatore deve scrivere.

## 9. Cosa va in git e cosa no

| Dentro il repository | Fuori |
|---|---|
| `gmb-package.json` | `lib/` (le dylib compilate) |
| gli `exports` (`.inc`) | `.build/` (sorgenti esterni scaricati) |
| `include/`, `src/`, `build.sh` | `out/`, `samples/work/` |
| esempi e README | `*.gmbpkg` (archivi) |

Il criterio: **ciò che serve a ricostruire** sta in git; ciò che si ottiene con
`gmb pkg install`/`gmb pkg build` no. Un `.gmbpkg` in un repository è un artefatto duplicato che
invecchia.

Nel monorepo gli strumenti che preparano e versionano un pacchetto sono scritti in BASIC, non in
shell: `tools/basic/pkg-compose.bas` (compone il manifest del pacchetto da `<lib>/packaging/`,
usando `shared/include/PkgTools.inc`) e `tools/basic/pkg-emit-version.bas` (aggiorna `version` e
sincronizza il `VER :=` del Makefile).

## 10. Errori frequenti

| Sintomo | Causa | Correzione |
|---|---|---|
| `Manifest 'gmb-package.json' non trovato per il pacchetto 'X' vY` | nella cache la cartella `<nome>/<versione>/` non contiene il manifest (archivio con struttura sbagliata) | ricrea il `.gmbpkg` con `PACKAGE PACK`, che mette il manifest in radice |
| il pacchetto si installa ma i simboli non esistono | manca `exports` nel manifest | dichiara il `.inc` in `exports` |
| `REQUIRE` non trova niente | il pacchetto non è in vendor/locale/cache | `RESTORE` (o `RESTORE --locked` se il lock lo elenca) |
| build riproducibile che cambia versione | si è usato `UPDATE` invece di `RESTORE --locked` | usa `--locked` in CI |
| install rifiutata per firma | chiave cambiata o pacchetto non firmato con `--require-signed` | `TRUST FINGERPRINT` per verificare, poi `TRUST ADD`/`TRUST REMOVE` a ragion veduta |
| spazio occupato due volte | cache senza dedup | `CACHE dedup`, poi `CACHE gc` |
| link fallito su un pacchetto `c-ffi` | piattaforma non dichiarata in `ffi.libs` | il pacchetto non copre quella piattaforma: non è un problema di flag |

## 11. Dove andare adesso

- **VI.38** — dove stanno i file scaricati, chiave per chiave (`paths`).
- **II.11** — `LIBRARY` e il manifest `*.gmblib.json`: la forma «libreria» che non passa dal
  package manager.
- **III.16** — FFI: `DECLARE`, header e librerie fuori dai pacchetti.
- **A1** — la reference completa delle funzioni del namespace `PACKAGE`.



<!-- BEGIN SURFACE (generato da tools/sh/manual_surface.sh — non modificare a mano) -->

## Riferimento completo: comandi, funzioni, parametri

> Sezione generata dalle guide del progetto per questo capitolo: contiene **tutte**
> le superfici documentate, non una selezione. La prosa dei capitoli precedenti le
> spiega; qui stanno i dettagli — parametri, varianti, esempi d'uso.

### PackageManager-Manual

*Fonte: `PackageManager-Manual`*


## 0. `REQUIRE` — sintassi e versioni (canonica)

REQUIRE "nome"                 ' VERSION assente -> si risolve la versione PIU' RECENTE in cache
REQUIRE "nome" VERSION "1.0"   ' forma CANONICA del vincolo

| forma | esempi | note |
|---|---|---|
| qualunque | `*`, `latest` | accetta anche i **prerelease** |
|Compatibile | `^1.4.0` | major fissato: `^1.5` accetta `1.5.10` |
| Compatibile | `~2.3.0` | major+minor fissati: `~1.5.6` accetta `1.5.10` |
| wildcards | `1.*`, `1.2.*` | solo major, oppure major+minor |
| comparatori | `>=2.0.0` `>2.0.0` `<=2.0.0` `<2.0.0` | **uno solo** per volta |
| OR | `>=1.0.0 \|\| <0.5.0` | alternanza |
| esatta | `2.4.0` | |
| dist-tag | `next`, `beta` | risolto dal registry |

## 1. Perché GMBasic è più intelligente di pip/cargo

REQUIRE "gmb-vec" VERSION "^1.0"   (edizione=Community, backend=il, platform=osx-arm64)
   │
   ├─ cerca cache <cache pacchetti>/gmb-vec/<ver>/gmb-package.json
   ├─ se `binaries["il"]["osx-arm64"]` o `any-any` esiste → usa il binario
   ├─ else se `exports` esiste → compila i .bas col backend corrente
   ├─ else se `ffi` presente → pacchetto FFI (DECLARE / link -I -L -l)
   └─ se non in cache → download dal registry con negoziazione


## 2. Tipologie di pacchetto gestite
| Tipo | `contentType` | Artefatto | Esempio | Cosa fa il resolver |
|---|---|---|---|---|
| **A — Source-only** | `source` | `.bas` in `exports` | `gmb-collections` (27 tipi) | `ResolvedModuleSourcePaths` → compila col backend corrente |
| **B — Compilato IL** | `dotnet` | `binaries["il"][platform]/*.dll` (assembly .NET da `IlProgramCompiler`) | `gmb-chart-il` | se `backend==il` → `BinaryPath` (`.dll`); se `backend!=il` e `exports` presente → ricompila da source; **loader IL = requisito runtime** (vedi §10) |
| **B2 — Compilato VM/C** | `vm` / `vm-chunk` / `vmc` | `binaries["vm"][platform]/*.prg|.chunk` (chunk A1/GMBC) | `gmb-app` | se `backend==vm` → `ChunkPath` (eseguibile da `gmb_vm_host`); se `backend!=vm` e `exports` presente → ricompila; senza source resta il chunk |
| **C — C precompilato** | `c-ffi` (o `mixed`) | `binaries["c99"][platform]/*.so` | `gmb-matrix-c` | se `backend==c99`+platform usa il binario |
| **D — FFI C puro** (non GMBasic) | `c-ffi` | `ffi.{api,headers,libs,link,declareExample}` | `physics_engine`, `gm_go_math` | aggiunge `-I/-L/-l` al link; valido anche senza `.bas` |
| **E — Mixed** | `mixed` | `exports` + `binaries` + `ffi` | `gmb-sqlite-vec` | binario se backend match, else `exports`, else `ffi` |
| **F — Docs-only** | `docs-only` | Solo `README`/`examples` | `gmb-doc` | nessun artefatto |

## 3. Manifest `gmb-package.json`

### v1 (retrocompatibile)

{ "name":"gmb-old","version":"1.0.0","exports":["src/"],
  "binaries":{"linux-x64":"bin/libold.so"} }


### v2 (con firma di contenuto) — manifest completo con TUTTE le opzioni

{
  "name": "gmb-sqlite-vec",                    // obbligatorio: [a-z0-9][a-z0-9._-]*
  "version": "1.2.0",                          // obbligatorio: SemVer 2.0 (anche 1.2.0-rc.1+b5)
  "description": "Vettori + SQLite per GMBasic",
  "contentType": "mixed",                      // source | dotnet | vm | c-ffi | mixed | docs-only
  "backends": ["il", "c99", "vm"],             // per cosa è già precompilato; [] = source-only
  "edition": "Commercial",                     // Community | Commercial | any

  "exports": ["src/"],                         // sorgenti .bas (sempre compilabili; dir o file)

  "binaries": {                                // precompilati: backend -> platform -> path
    "il":  { "any-any": "bin/il/GmbVec.dll", "win-x64": "bin/il/GmbVec-win.dll" },
    "vm":  { "any-any": "bin/app.prg", "linux-x64": "bin/app-linux.prg" },
    "c99": { "linux-x64": "bin/c99/libvec.so", "win-x64": "bin/c99/vec.dll" }
  },



{ "latest": "1.2.0", "next": "1.3.0-rc.1", "stable": "1.2.0" }


### Lock file `gmb-package.lock`

# gmb-package.lock — generato da GMBasic PackageManager v2
# backend=il timestamp=2026-08-24T...
package@version|contentType|backend|platform|sha256(file:hash,...)
gmb-sqlite-vec@1.2.0|Mixed|c99|linux-x64|gmb-package.json:a1b2...,bin/c99/libvec.so:c3d4...,src/Main.bas:e5f6...
gmb-core@1.0.0|Source|any|osx-arm64|gmb-package.json:9a8b...


## 4. FFI — due livelli
| Livello | `ffi.api` | ABI | Per chi |
|---|---|---|---|
| **c-abi** (default, easy) | `"c-abi"` | `DECLARE ... LIB` semplice: `INTEGER/LONG/DOUBLE/STRING` (STRING copiata), `BYREF X AS DOUBLE`→`double*`, handle `LONG`. Niente struct/`PyObject`. | Librerie arbitrarie C/Go/Rust con `extern "C"` |
| **gmb-native** (performance) | `"gmb-native"` | `gmb_ffi.h` — marshalling copy-in/copy-out, liste `gmb_list_t`, callback `gmb_ffi_closure_t`, errori→`ON ERROR`. | Pacchetti GMBasic-nativi con `COLLECTIONS`/link diretto |

## 5. `gmb_ffi.h` — interfaccia moduli nativi

### Entry point

gmb_ffi_module_t* gmb_ffi_module_entry(void);   // l'host dlopen la cerca nella .so/.dylib


### Marshalling (copy-in/copy-out)

gmb_ffi_val_t gmb_ffi_make_int/double/bool/string[_len]/nil(...);
const char*   gmb_ffi_get_string(const gmb_ffi_val_t*, size_t* len);
gmb_ffi_list_t gmb_ffi_get_list(const gmb_ffi_val_t*);
gmb_ffi_closure_t gmb_ffi_get_closure(const gmb_ffi_val_t*);  // cb(ctx, ...)
void gmb_ffi_set_int_out/double_out/bool_out/string_out(...);  // BYREF multi-return
void gmb_ffi_error(const char* fmt, ...);  const char* gmb_ffi_last_error(void);


### Contratto di memoria

### Esempio modulo C (`shared/samples/FFI/NativeVec/vec_module.c`)

#include "gmb_ffi.h"
static gmb_ffi_val_t w_add(const gmb_ffi_val_t* a, size_t n) {
    if (n < 2) { gmb_ffi_error("vec_add: 2 args"); return gmb_ffi_make_nil(); }
    return gmb_ffi_make_int(gmb_ffi_get_int(&a[0],0) + gmb_ffi_get_int(&a[1],0));
}
static const gmb_ffi_fn_descr_t fns[] = { { "VecAdd", w_add, 2,2, "L,L" }, {NULL,NULL,0,0,NULL} };
static gmb_ffi_module_t mod = { "vec", GMB_FFI_VERSION, fns, NULL, NULL };
gmb_ffi_module_t* gmb_ffi_module_entry(void) { return &mod; }


## 6. Esempi pronti
| Cartella | Tipo | Contenuto |
|---|---|---|
| `shared/samples/FFI/NativeVec/` | **gmb-native** | `gmb_ffi.h` + `vec_module.c` (livello 1 + 2) + `NativeVec.bas` (DECLARE) + `Makefile` |
| `shared/samples/FFI/NativeRust/` | **c-abi Rust** | `math_rust.rs` (`#[no_mangle] extern "C"` + `CString`/`_free`) + `MathRust.bas` + `build.sh` |
| `shared/samples/FFI/NativeGo/` | **c-abi Go** | `math_go.go` (`//export` + `C.CString`/`C.free`) + `MathGo.bas` + `build.sh` |

## 6bis. Integrazione C99Native (FFI link)

## 6ter. Compilazione immediata dei pacchetti sorgente

## 7. PackageManager v2 — API C#

### Tipi pubblici

### Metodi

// Risoluzione + selezione artefatto (transitive, ciclo-safe)
Task ResolveDependenciesAsync(IEnumerable<RequireStmt> reqs, PackageBuildContext? ctx = null);
// Download con negoziazione ?backend&platform&edition + multi-registry
Task<InstalledPackage> DownloadPackageAsync(string name, string? version, string? registryUrl = null, PackageBuildContext? ctx = null);
// Lock file
void WriteLockFile(string projectDir);
// Parsing manifest v1/v2
static PackageManifest ParseManifest(JsonElement root, SourcePosition pos = default);
// Piattaforma corrente
static string GetCurrentPlatformKey();   // es. "osx-arm64"


### SemVer 2.0

### Features / dipendenze opzionali (cargo-style)

### Lock enforcement

### Update / Outdated / Tree

## 7bis. CLI pacchetti (gap pip/cargo — chiuso)

gmb init <dir> --name X --version 1.0.0 [--type source|dotnet|vm|c-ffi|mixed] [--edition E] [--api c-abi|gmb-native]
gmb pack <dir> [--out out.gmbpkg] [--backend il|c99|vm|all] [--key secret]
gmb pkg install <name[@ver]> [--registry url] [--cache dir]
gmb pkg list [--cache dir]
gmb pkg remove <name> [--cache dir]
gmb pkg info <name> [--cache dir]
gmb pkg search <term> [--registry url]
gmb pkg tree [--lock gmb-package.lock] [--cache dir]      # albero dipendenze
gmb pkg outdated [--registry url] [--cache dir]           # versioni superate
gmb pkg update <name> [--registry url] [--cache dir]      # aggiorna a latest
gmb pkg vendor <dir> [--cache dir] [--lock …]             # vendoring offline (cargo vendor)
gmb pkg tag <name> <tag> <version> [--registry url] [--token t]   # dist-tag
gmb pkg tags <name> [--registry url]
gmb login <token> [--registry url]                        # credential store ~/.gmbasic/config.json (chmod 600)
gmb logout
gmb cache list|verify|clean [--cache dir]


## 8. Configurazione

### Registry

### Cache — DOVE stanno i pacchetti (due macchine, una regola)
| macchina | cartella | chi la usa |
|---|---|---|
| **CLIENT** (app installata: **nessun compilatore**, nessun checkout) | **`~/.gmbasic/packages/<nome>/<versione>/`** | PM (`gmb pkg install`), loader FFI del runtime, app già compilata |
| **SVILUPPO** (checkout sorgente con compilatore) | **`<checkout>/packages/<nome>/<versione>/`** (nel monorepo: `<monorepo>/packages`) | compilatore (`REQUIRE`), PM, test |
| **installazione locale dell'app** (`gmb pkg install --local` · REPL `LOCAL`/`TO`) | **`<dir dell'eseguibile>/packages/<nome>/<versione>/`** (creata se manca) | l'app stessa (modello `node_modules`) |
| **pin di progetto** (facoltativo) | `<dir del programma>/vendor/<nome>/<versione>/` | compilatore: ha la **precedenza** su tutto (build offline/riproducibile) |

### Cache — note tecniche

### Server registry (`shared/samples/PackageRegistry/PackageServer.bas`)

{ "registryRoot": "/Users/gaetano/.gmbasic/packages", "serverDir": "./", "bind": "http://localhost:8087", "token": "" }


dotnet run --project executor-managed/Executor -- shared/samples/PackageRegistry/PackageServer.bas


## 8bis. Sicurezza (P7 — chiuso, firma pubblica)

## 8ter. Dist-tags (npm-style)

## 9. Test

# C# — PackageManager v2 (25 test: parsing v1/v2, SemVer 2.0 incl. prerelease/OR, selezione artefatto,
#      FFI, mixed, VM/IL, transitive, lock sha256+integrità, init+pack zip+sig, validazione)
dotnet test GMBasicSharp/tests/GMBASIC.IlBackend.Tests --filter PackageManagerV2Tests

# C — gmb_ffi.h (16 gruppi: stringhe \0/UTF-8/>64K, UAF copy-out, BYREF, errori, liste, callback, PACKED, handle)
cd GMBasic/native/gmb_runtime_c && cmake --build build -j4 && cd build && ctest -R gmb_ffi_selfcheck

# Compile-only del server
dotnet test GMBasicSharp/tests/GMBASIC.IlBackend.Tests --filter PackageServerCompileTest


## 10. Quali pacchetti possono essere scaricati e come sono identificati

## 10bis. Publish multi-utente — account, ownership, contributor (piano publish misto, Fasi A–D)
| Endpoint | Metodo | Funzione |
|---|---|---|
| `/api/admin/users/create` | POST | (admin) Crea utente autorizzato, genera **token opaco** (`{name}` → `{ok,token}`), nasce `blocked`. Il token è `PASSWORDHASH`, MAI in chiaro. |
| `/api/packages/{name}/owners` | POST | Gestisce collaboratori (`{action:add\|remove, user}`). Solo l'owner. |
| `/api/packages/{name}/yank` | POST | Yank/unyank di una versione (`{version, yank:"1"\|"0", reason}`). Download yanked → **410**. |
| `/api/packages/register` | POST | Registra un pacchetto contributor (`{repo,name,tag,verify_report}`) → `repos.json` + claim `scope`. |
| `/api/packages/{name}/materialize` | POST | Salva asset binari (base64, `{name,version,assets}`) in `registryRoot/<name>/<version>/bin/`. |
| `/api/packages/{name}/sync` | POST | Aggiorna il tag (`{tag}`), ricalcola status dal report. Solo owner/collab. |
| `/api/admin/reserve` | POST | (admin) Riserva un nome (`{name}`) → bloccato finché non assegnato. |
| `/api/admin/assign` | POST | (admin) Assegna l'owner (`{name, owner}`) a un nome riservato/libero. |
| `/api/admin/revoke` | POST | (admin) Revoca ownership (`{name, reserve:"1"|"0"}`). |
| `/api/admin/password` | POST | (admin) Cambia token admin (`{newToken}`). |
| `/api/admin/users/create` | POST | (admin) Crea utente autorizzato (`{name}`) → status `blocked`, token mostrato una volta. |
| `/api/admin/users/setkey` | POST | (admin) Registra chiave pubblica Ed25519 (`{name, publicKey}`). |
| `/api/admin/users/status` | POST | (admin) Attiva/blocca utente (`{name, status: active\|blocked}`). |
| `/api/admin/users/list` | GET | (admin) Elenco utenti (`status` + `publicKey`, senza token). |

{
  "name": "@marco/geom", "version": "0.9.1", "contentType": "mixed",
  "exports": ["src/"],
  "test": { "entry": "test/self_test.bas", "backends": ["il"] },
  "binaries": { "c99": { "osx-arm64": "https://github.com/marco/gmb-geom/releases/download/v0.9.1/libgeom.dylib" } }
}


## 11. Stato / Roadmap

### PackageManager-Publish-Contributor-Guide

*Fonte: `PackageManager-Publish-Contributor-Guide`*


## 1. Architettura in sintesi

[ Il tuo PC — repo git ]                     [ Registry (PackageServer.bas) ]
  gmb publish --repo <git-url> name@tag
    ├─ ValidateManifest ──────────────┐
    ├─ git check (manifest versionato) │  POST /api/packages/register
    ├─ compilazione exports            │   { repo, name, tag, verify_report }
    ├─ self-test (test.entry, exit 0) ─┘        │
    └─ materializzazione binari URL ──►  POST /.../materialize (base64)
                                           │
                                           ▼
                                     repos.json + claim scope
                                     (status: active | pending)

| Componente | Cosa fa |
|---|---|
| `gmb publish --repo` | verifica localmente il repo e registra il pacchetto |
| `POST /api/packages/register` | salva `repos.json` (repo, owner, tag, status, report) |
| `POST /api/packages/{name}/materialize` | materializza i binari URL in cache locale |
| `POST /api/packages/{name}/sync` | aggiorna il tag di un pacchetto già registrato |

## 2. Attività preliminari

### 2.1 Build del progetto e avvio del registry

dotnet build REPL/REPL.csproj

cd shared/samples/PackageRegistry
dotnet /path/REPL/bin/Debug/net10.0/REPL.dll --pcode PackageServer.bas


### 2.2 Account e token

# a cura dell'admin:
gmb admin user create <username> [--registry url] [--token <adminToken>]   # mostra il token (1 volta)
gmb admin user status <username> active [--registry url] [--token <adminToken>]   # attiva

# tuo:
gmb login <token> [--registry http://localhost:8087]     # salva il token in config


### 2.3 `git` installato e repo raggiungibile

## 3. Il repo è il pacchetto

### 3.1 Struttura richiesta

gmb-geom/
├── gmb-package.json      ← FONTE DI VERITÀ (root del repo, VERSIONATO, obbligatorio)
├── src/                  ← exports (file .bas)
│   └── geom.bas
├── test/
│   └── self_test.bas     ← test convenzionale (campo "test" nel manifest, raccomandato)
└── (releases/ se content type binario — asset URL nel manifest)


### 3.2 Manifest contributore (schema esteso)

{
  "name": "@marco/geom",
  "version": "0.9.1",
  "contentType": "mixed",
  "backends": ["il", "c99"],
  "edition": "Community",
  "exports": ["src/"],
  "test": { "entry": "test/self_test.bas", "backends": ["il"] },
  "binaries": {
    "c99": { "osx-arm64": "https://github.com/marco/gmb-geom/releases/download/v0.9.1/libgeom.dylib" },
    "il":  { "any-any": "https://github.com/marco/gmb-geom/releases/download/v0.9.1/Geom.dll" }
  }
}

| Campo | Significato |
|---|---|
| `name` | **`@utente/nome`** (obbligatorio — scope = account, anti-squatting) |
| `version` | SemVer 2.0 (deve corrispondere al tag `v0.9.1`) |
| `exports` | cartelle/file sorgente del pacchetto |
| `test.entry` | file eseguito dalla pipeline di verifica (self-test). **Se assente, il passo 4 usa gli exports** |
| `test.backends` | backend del self-test (default `["il"]`) |
| `binaries.<backend>.<platform>` | **URL** di release asset (nuovo: NON un path relativo) |

### 3.3 I tag SemVer

git tag v0.9.1 && git push origin v0.9.1


## 4. `gmb publish --repo` — pipeline di verifica automatica

gmb publish --repo <git-url|path> <nome@tag> [--registry url] [--token t]


gmb publish --repo file:///percorso/gmb-geom.git @marco/geom@v0.9.1 --registry http://localhost:8087


gmb publish --repo https://github.com/marco/gmb-geom @marco/geom@v0.9.1


### 4.1 I 5 passi della verifica locale
| # | Passo | Verifica | Esito se fallisce |
|---|---|---|---|
| 1 | **Struttura** | `gmb-package.json` in root, nome/versione SemVer, artefatti | errore esplicito, NON registra |
| 2 | **Repo git** | il manifest è versionato (`git ls-files`); fetch del tag | errore esplicito |
| 3 | **Compilazione** | gli `exports` (o `test.entry`) compilano col backend dichiarato | errore esplicito |
| 4 | **Self-test** | il `test.entry` esegue e termina con **exit 0** | errore esplicito |
| 5 | **Registrazione** | report di verifica JSON → `POST /api/packages/register` | — |

  [1] manifest OK: @marco/geom@0.9.1
  [2] git OK (manifest versionato, tag: v0.9.1)
  [3] compilazione OK (test/self_test.bas)
  [4] self-test OK (exit 0, 13 byte output)
Registered: @marco/geom@v0.9.1 (status OK)


### 4.2 Il report di verifica (cosa arriva al registry)

{
  "name": "@marco/geom",
  "repo": "https://github.com/marco/gmb-geom",
  "tag": "v0.9.1",
  "manifest": { "name": "@marco/geom", "version": "0.9.1" },
  "manifestJson": "{ ...manifest completo... }",
  "backend": "il",
  "testOutput": "geom test OK\n",
  "testExitCode": 0,
  "commit": "364ff1486d55dd...",
  "verifiedAt": "2026-09-01T13:07:28+00:00"
}


### 4.3 Verifica indipendente lato server (attiva)

## 5. Materializzazione dei binari (opzionale, solo se `binaries` ha URL)

# output dopo la registrazione:
  [materialize] c99/osx-arm64 <- https://github.com/.../libgeom.dylib (28 byte)
Materialize: OK (1 asset) {"ok":"1","name":"@marco/geom","version":"0.9.1","saved":"1"}


Materialize: nessun asset binario (source-only)


## 6. Aggiornare una versione — `gmb pkg sync`

git tag v0.9.2 && git push origin v0.9.2
gmb pkg sync @marco/geom --tag v0.9.2 [--registry url]
# {"ok":"1","name":"@marco/geom","tag":"v0.9.2","status":"active"}


## 7. Nomi con scope — `@utente/pacchetto`

### 7.1 Regole di validazione

### 7.2 Path di cache

@marco/geom  →  cache/@marco_geom/<versione>/


### 7.3 Anti-squatting

## 8. Installare e usare un pacchetto esterno

### 8.1 Installazione

gmb pkg install @marco/geom@0.9.1 --registry http://localhost:8087


### 8.2 Require nel codice

REQUIRE "@marco/geom" VERSION "^0.9"


### 8.3 Grafo e cache

gmb pkg tree            # mostra il grafo delle dipendenze (anche miste)
gmb pkg list            # pacchetti in cache (con nomi scoped normalizzati)
gmb cache verify        # verifica integrità sha256


## 9. Esempio end-to-end completo

# 0. Avvia il registry (terminal 1)
cd shared/samples/PackageRegistry
dotnet REPL/bin/Debug/net10.0/REPL.dll --pcode PackageServer.bas

# 1. (terminal 2) Account — lo crea l'ADMIN, poi l'utente fa login
gmb admin user create marco --token gmb_admin --registry http://localhost:8087   # mostra il token
gmb admin user status marco active --token gmb_admin --registry http://localhost:8087
gmb login <token-marco> --registry http://localhost:8087

# 2. Crea il repo
mkdir gmb-geom && cd gmb-geom
cat > gmb-package.json <<'EOF'
{ "name":"@marco/geom","version":"0.9.1","contentType":"source","edition":"Community",
  "backends":["il"],"exports":["src/"],
  "test":{ "entry":"test/self_test.bas","backends":["il"] } }
EOF


## 10. Risoluzione problemi frequenti
| Sintomo | Causa | Fix |
|---|---|---|
| `Nome pacchetto non valido` | `@scope/name` malformato o nome senza scope per contributor | rispetta `^@scope/name$` lowercase |
| `gmb-package.json mancante in root` | manifest non nella root del repo | sposta il file in root |
| `gmb-package.json non è versionato` | manifest non committato | `git add gmb-package.json && git commit` |
| `git fetch fallito / branch HEAD` | tag inesistente o repo non raggiungibile | verifica `git ls-remote <url> refs/tags/v0.9.1` |
| `Self-test fallito` | `test.entry` non esce con 0 | correggi il test; se non hai test usa `exports` |
| `entry non trovata` | `test.entry` path sbagliato | allinea il path nel manifest |
| `Connection refused` | registry non attivo | avvia `PackageServer.bas`, controlla `--registry` |
| `Pacchetto contributor non trovato` | nome/scope non registrato | `gmb publish --repo` prima |
| `403` su sync/owners | non sei owner/collaboratore | fatti aggiungere dall'owner |
| Download 404/fallisce | pacchetto non materializzato | esegui `publish --repo` con `binaries` materializzati |

## 11. Riferimenti

### PackageManager-Publish-Ufficiali-Guide

*Fonte: `PackageManager-Publish-Ufficiali-Guide`*


## 1. Architettura in sintesi

[ Il tuo PC ]                          [ Registry (PackageServer.bas) ]
  gmb pack → .gmbpkg ──POST /api/packages──►  registryRoot/<nome>/<versione>/
  gmb admin user create ──(admin)─────────►  config: users{name, tokenHash, publicKey, status}
  gmb publish  ──────(token Bearer)──────►  claim del nome + tag latest
  gmb pkg owners ──POST /.../{name}/owners►  registryRoot/claims.json
  gmb pkg yank  ────POST /.../{name}/yank─►  marker .yanked → download 410

| Componente | Cosa fa |
|---|---|
| `REPL` (CLI `gmb …`) | crea, firma, pubblica, installa, gestisce |
| `PackageServer.bas` | server HTTP del registry (porta **8087** default) |
| `~/.gmbasic/config.json` | credential store (token, registry, username, repos) — chmod 600 |
| `<cache>` | cache dei pacchetti installati: **`~/.gmbasic/packages`** su una macchina client (senza compilatore), **`<checkout>/packages`** in sviluppo (vedi `Knowledge/PackageManager-Percorsi-e-Funzionamento.md`) |
| `package-registry.json` | config unica: path, limiti, admin + utenti (chiavi, status) |
| `registryRoot/claims.json`, `repos.json` | ownership, pacchetti contributor |

## 2. Attività preliminari (prima di pubblicare)

### 2.1 Build del progetto

dotnet build REPL/REPL.csproj


### 2.2 Avvio del server registry

cd shared/samples/PackageRegistry
dotnet /path/REPL/bin/Debug/net10.0/REPL.dll --pcode PackageServer.bas


{
  "registryRoot": "/Users/gaetano/.gmbasic/packages",   // dove vivono pacchetti, claims, repos
  "serverDir": "./",
  "frontendDir": "../Web/RegistryBsp",              // pagine BSP del frontend web
  "bind": "http://localhost:8087",                  // indirizzo/porta di ascolto
  "maxPackageBytes": 104857600,                     // quota dimensione upload (100 MB default)
  "rateLimitPerMinute": 60,                         // max richieste mutate al minuto per IP
  "admin": { "tokenHash": "<PASSWORDHASH>", "enabled": "1" },
  "users": { "alice": { "name": "alice", "tokenHash": "...", "publicKey": "...", "status": "active" } }
}


curl http://localhost:8087/api/stats
# {"totalPackages":..., "totalVersions":...}


### 2.3 Puntare i comandi al registry giusto

gmb login <token> --registry http://localhost:8087   # salva RegistryUrl in ~/.gmbasic/config.json


## 3. Account utente e token

{
  "registryRoot": "...",
  "maxPackageBytes": 104857600,
  "rateLimitPerMinute": 60,
  "admin": { "tokenHash": "<PASSWORDHASH>", "enabled": "1" },
  "users": {
    "alice": { "name": "alice", "tokenHash": "<PASSWORDHASH>", "publicKey": "<base64>", "status": "active" }
  }
}


### 3.1 L'admin crea l'utente

gmb admin user create <username> [--registry url] [--token <adminToken>]
# {"ok":"1","name":"alice","status":"blocked","token":"gmb_alice_..."}


### 3.2 L'admin registra la chiave pubblica e attiva

# l'utente genera la sua coppia di chiavi Ed25519
gmb keygen <dir>              # produce <dir>/private.key e <dir>/public.key

# l'admin registra la chiave pubblica (obbligatoria per i pacchetti firmati)
gmb admin user setkey <username> "$(cat <dir>/public.key)" [--registry url] [--token t]

# l'admin attiva l'utente
gmb admin user status <username> active [--registry url] [--token t]


### 3.3 L'utente fa login con il token

gmb login <token> [--registry http://localhost:8087]


### 3.4 Elenco utenti (admin)

gmb admin user list [--registry url] [--token t]
# {"alice":{"status":"active","publicKey":"1qPLnop..."}}


### 3.4 Regole di ownership

### 3.5 L'utente admin — chi assegna i permessi

{ "admin": { "tokenHash": "<PASSWORDHASH del token>", "enabled": "1" }, "users": { ... } }


# Gestione UTENTI (chiavi pubbliche e status nella config unica)
gmb admin user create <nome> [--registry url] [--token t]     # crea, restituisce il token (1 volta)
gmb admin user setkey <nome> <chiavePubblicaB64> [--token t]  # registra la chiave Ed25519
gmb admin user status <nome> active|blocked [--token t]       # attiva/blocca
gmb admin user list [--registry url] [--token t]              # elenco (status + chiave)

# Gestione NOMI (ownership)
gmb admin reserve <nome> [--registry url] [--token t]         # riserva un nome
gmb admin assign <nome> <utente> [--registry url] [--token t] # assegna l'owner
gmb admin revoke <nome> [--keep-reserved] [--registry url] [--token t]  # revoca

# Credenziali admin
gmb admin password <nuovoToken> [--registry url] [--token t]  # cambia il token admin



Flusso tipico per proteggere i nomi ufficiali:




Endpoint HTTP corrispondenti: `POST /api/admin/reserve|assign|revoke|password`. Il token admin
si passa come `Authorization: Bearer <token>` o `?token=`.

---

## 4. Creare un pacchetto

### 4.1 `gmb init` — scaffold del progetto




| Flag | Valori | Default |
|---|---|---|
| `--name` | nome pacchetto (lowercase, `[a-z0-9._-]`) | obbligatorio |
| `--version` | SemVer 2.0 | `1.0.0` |
| `--type` | `source` / `dotnet` / `vm` / `c-ffi` / `mixed` | `source` |
| `--edition` | `Community` / `Commercial` | `Community` |
| `--api` | `c-abi` / `gmb-native` (solo per FFI) | — |

Esempio:



Crea `gmb-package.json` + cartella `src/`. Il manifest minimo:




### 4.2 Struttura consigliata




### 4.3 Manifest esteso (opzioni utili)




> I `dependencies` usano SemVer. Dal 2026-09-01 i range a **2 componenti** (`^1.0`) funzionano
> correttamente (normalizzati a `^1.0.0`).

### 4.4 Firma (obbligatoria se l'admin ha configurato la tua chiave pubblica)

Genera una coppia di chiavi Ed25519 in una cartella:




**Registra la chiave pubblica** presso l'admin (così il server può verificare le tue firme):




Firma il pacchetto in fase di `pack`:




Il client verifica la firma all'installazione (firma non valida → installazione fallita, exit 1).
In alternativa `--key <secret>` firma con HMAC condiviso.

> **Verifica firma lato server**: se la tua `publicKey` è configurata nel registry, ogni pacchetto
> che pubblichi DEVE essere firmato con la chiave corrispondente. Il server rifiuta (403):
> - pacchetti **non firmati** (manca `.gmbpkg.sig`/`.gmbpkg.pub`);
> - pacchetti firmati con una **chiave diversa** da quella in config;
> - firme **non valide** sul manifest.
> Se non hai una chiave configurata, i pacchetti non firmati sono ancora accettati (modo permissivo).

---

## 5. Pubblicare

### 5.1 `gmb pack` — crea il `.gmbpkg`



| Flag | Effetto |
|---|---|
| `--out` | nome del file `.gmbpkg` (default `nome-version.gmbpkg` nella cartella) |
| `--backend` | backend inclusi (default: tutti quelli dichiarati; `all` = tutti) |
| `--key` / `--key-file` | firma HMAC / Ed25519 del contenuto |




### 5.2 `gmb publish` — carica sul registry







Cosa succede sul server (in ordine):
1. verifica del **token** (header `Authorization: Bearer <token>` o `?token=` o body);
2. **firma** (se la tua `publicKey` è configurata): il `.gmbpkg` deve essere firmato Ed25519 con
   quella chiave (non firmato / chiave diversa / firma invalida → **403**);
3. se il nome non ha claim → il pacchetto diventa **tuo** (owner) e viene aggiunto a `ownedPackages`;
4. se il claim esiste → controllo che tu sia **owner o collaboratore** (altrimenti 403);
5. **immutabilità**: se `registryRoot/<nome>/<versione>` esiste già → **409**;
6. **rate-limit** per IP (oltre `rateLimitPerMinute` → **429**) e **quota** `maxPackageBytes` (→ **413**);
7. scompatta, copia in cache, aggiorna `tags.json` con `latest = <versione>`.

> **Dimensione pacchetti**: il publish supporta pacchetti fino a **decine di MB** (verificato 15MB
> in 1.2s). Il presunto "limite 3MB" era un bug del parser HTTP (body multi-chunk con header inclusi),
> corretto il 2026-09-01. Il limite effettivo è `maxPackageBytes` nella config.

### 5.3 Pubblicare una nuova versione



> La versione 1.0.0 resta **immutabile** (409 se provi a riscriverla). SemVer è la tua API:
> incrementa major/minor/patch con coerenza.

### 5.4 Verifica post-publish




---

## 6. Gestire il pacchetto dopo la pubblicazione

### 6.1 Collaboratori — `gmb pkg owners`

Solo l'**owner** può gestire i collaboratori. Un collaboratore può ripubblicare nuove versioni
ma non gestire l'ownership.







L'owner **non può** essere rimosso come collaboratore; i collaboratori devono essere account
registrati (404 altrimenti).

### 6.2 Yank — ritirare una versione rotta







Effetti:
- il server scrive il marker `registryRoot/<nome>/<versione>/.yanked` (con `yankedBy`, `reason`, `when`);
- **download** di quella versione → **HTTP 410 Gone**;
- le altre versioni restano attive;
- `--un` rimuove il marker (il download torna 200).

### 6.3 Dist-tags

I tag (es. `latest`) vivono in `registryRoot/<nome>/tags.json`:




`latest` punta sempre all'ultima versione pubblicata; crea tag personalizzati (es. `stable`,
`next`) per puntare a versioni specifiche.

---

## 7. Installare e usare un pacchetto

### 7.1 Installazione via CLI







Se il pacchetto è firmato, installare una firma non valida fa fallire l'operazione con exit 1.

### 7.2 Installazione via REPL (comando `PACKAGE`)

Dentro il REPL:




- `PACKAGE INSTALL <name> [@ <version>] [FROM <url>]` — installa nella cache;
- `PACKAGE RUN <name>` — esegue un pacchetto vm-chunk come programma (il suo `SUB Main`).

### 7.3 Usare il pacchetto nel codice BASIC




Il resolver (transitivo, con features opzionali) carica il pacchetto e le sue dipendenze nel
backend corrente (IL/VM/C99). `gmb pkg tree` mostra il grafo.

### 7.4 Gestione cache e aggiornamenti




---

## 8. Esempio end-to-end completo




---

## 9. Risoluzione problemi frequenti

| Sintomo | Causa | Fix |
|---|---|---|
| `Unauthorized: token utente mancante o non valido` | token errato/assente o utente `blocked` | `gmb login <token>`; se bloccato fatti attivare dall'admin |
| `409` su publish | versione già esistente (immutabile) | incrementa la versione |
| `403` su publish | non sei owner/collaboratore del nome, o pacchetto non firmato | fatti aggiungere dall'owner / firma con la tua chiave |
| `403` "nome riservato" | il nome è riservato dall'admin senza owner | l'admin fa `gmb admin assign` |
| `413` "pacchetto troppo grande" | supera `maxPackageBytes` | aumenta la quota in config |
| `429` "rate limit" | troppe richieste in un minuto dallo stesso IP | attendi o aumenta `rateLimitPerMinute` |
| `Connection refused` | server registry non attivo o porta sbagliata | avvia `PackageServer.bas`, controlla `--registry` |
| Download `410` | versione yanked | usa un'altra versione o `--un` |
| `Pacchetto 'x' non trovato nella cache` | non installato | `gmb pkg install` |

### PackageManager-User-Guide

*Fonte: `PackageManager-User-Guide`*


## 1. Cos'è un pacchetto GMBasic

## 2. Comandi principali

# inizializza un nuovo pacchetto (progetto "source")
gmb init ./mio-pkg --name mio-pkg --version 1.0.0 --type source

# compila e impacchetta in mio-pkg-1.0.0.gmbpkg
gmb pack ./mio-pkg

# installa un pacchetto dal registry nella cache
gmb pkg install gmb-vec@^1.0

# installazione LOCALE: accanto all'eseguibile (modello node_modules), spostando dalla cache
gmb pkg install gmb-vec@^1.0 --local

# elenca i pacchetti installati
gmb pkg list

# mostra info su un pacchetto installato


### Dove stanno i pacchetti e gestione della cache

gmb cache list     # elenca i pacchetti in cache
gmb cache verify   # verifica l'integrità (sha256 per file)
gmb cache clean    # svuota la cache


### Credenziali e firma

gmb login <token>            # salva il token del registry (~/.gmbasic/config.json, chmod 600)
gmb logout                   # rimuove il token
gmb keygen ./chiavi          # genera una coppia Ed25519 (private.key / public.key)
gmb pack ./mio-pkg --key-file ./chiavi/private.key   # firma il pacchetto


## 3. Usare un pacchetto in un programma BASIC

REQUIRE "gmb-vec" VERSION "^1.0"

SUB Main()
  PRINT gmb-vec.Magnitude(3, 4)   ' 5
END SUB


### Eseguire un pacchetto vm-chunk come programma

' nel REPL/CLI:
PACKAGE RUN nome-pacchetto


## 4. Sviluppare un pacchetto

### 4.1 Inizializza

gmb init ./mia-libreria --name mia-libreria --version 1.0.0 --type source


### 4.2 Struttura minima (pachetto source)

mia-libreria/
├── gmb-package.json
└── src/
    └── lib.bas        # procedure/funzioni esportate


### 4.3 Compila e impacchetta

gmb pack ./mia-libreria
# → mia-libreria-1.0.0.gmbpkg (zip con manifest + exports + binari + firma)


gmb pack ./mia-libreria --backend il        # solo backend IL
gmb pack ./mia-libreria --backend all       # tutti i backend
gmb pack ./mia-libreria --key segreto       # firma HMAC-SHA256
gmb pack ./mia-libreria --key-file ./chiavi/private.key   # firma pubblica Ed25519


### 4.4 Manifest d'esempio

{
  "name": "mia-libreria",
  "version": "1.0.0",
  "contentType": "source",
  "edition": "Community",
  "backends": ["il", "c99", "vm"],
  "exports": ["src/"],
  "dependencies": { "gmb-collections": "^1.0" }
}


### 4.5 Pubblica

# l'account lo crea l'ADMIN del registry (niente self-register):
gmb admin user create <username> [--registry url] [--token <adminToken>]   # mostra il token (1 volta)
gmb admin user status <username> active [--registry url] [--token <adminToken>]   # attiva

# poi salvi il token nella config:
gmb login <token> [--registry http://registry:8087]

# pubblica: il primo publish "reclama" il nome (diventi owner)
gmb publish ./mia-libreria-1.0.0.gmbpkg [--registry http://registry:8087]


### 4.6 Pubblica da un repo git (contributor, modello GO)

gmb publish --repo <git-url|path> <name@tag> [--registry url]


## 5. Il registry web (frontend BSP)

# dal sample (configura registryRoot/frontendDir/bind in package-registry.json)
dotnet run --project REPL/REPL.csproj -- shared/samples/PackageRegistry/PackageServer.bas


## 6. Buone pratiche

## 7. Risoluzione dei problemi
| Problema | Cosa fare |
|---|---|
| `pkg install` non trova il pacchetto | Controlla `--registry` e che il registry sia attivo (porta 8087). |
| "FIRMA NON VALIDA!" | Il pacchetto è firmato ma la chiave non torna; rifiuta l'installazione. |
| Il pacchetto non funziona sul backend | Verifica `backends` nel manifest; il PackageManager sceglie per il backend attivo. |
| Serve una versione prerelease | Usa il vincolo esplicito (`@ "1.0.0-rc.1"`). |
| Build non riproducibile | Usa `gmb-package.lock` e `--locked`/`--lock`. |
| Registry vuoto nel frontend | Controlla `registryRoot` in `package-registry.json` e che la cache abbia pacchetti. |

## 8. Riferimenti

### Package-Manager-Guida-Utente

*Fonte: `Package-Manager-Guida-Utente`*


## 1. Dove archivia (cache, vendor, config, lock)
| Cosa | Posizione | Note |
|---|---|---|
| **Cache pacchetti** | SVILUPPO: `<checkout>/packages/<nome>/<versione>/` · CLIENT (senza compilatore): **`~/.gmbasic/packages/<nome>/<versione>/`** | `PackageManager.CacheRoot` = `GmbiPaths.PackagesDir`; override con `--cache` o `paths.packagesDir`. |
| **Installazione locale** | `<dir dell'eseguibile>/packages/<nome>/<versione>/` | `gmb pkg install --local` · REPL `PACKAGE INSTALL … LOCAL`: l'app si porta i pacchetti accanto a sé (creata se manca; sposta dalla cache la stessa versione). |
| **Config utente** | `~/.gmbasic/config.json` (chmod 600) | Registry, token, repos. |
| **Registry remoto** | `GMB_REGISTRY` o `~/.gmbasic/config.json` | Default `http://localhost:8087`. |
| **Vendor (offline)** | `<progetto>/vendor/<nome>/<versione>/` | Pin di progetto (`gmb pkg vendor <progetto>`); il compilatore lo preferisce a tutto. |
| **Lockfile** | `<progetto>/gmb-package.lock` | Versioni pinnate + sha256 per-file; generato dal resolve/restore. |

gmb-package.json          # manifest
duckdb.inc                # exports: dichiarazioni (include, .inc per l'FFI)
include/gmbduckdb.h       # ffi.headers (→ -I)
lib/osx-arm64/libgmbduckdb.dylib   # ffi.libs (→ -L -l)
lib/osx-arm64/libduckdb.dylib      # binaries (bundlata)


## 2. Come recupera i pacchetti (ordine di risoluzione)

## 3. Comandi CLI (`gmb …`)

## 4. Comandi REPL

## 5. `REQUIRE` nel codice BASIC

REQUIRE "duckdb"                          ' ultima versione (massimo SemVer)
REQUIRE "duckdb" VERSION "^1.5.0"         ' vincolo — forma CANONICA
REQUIRE "gmb-sqlite3" VERSION "1.0.0"     ' versione esatta


## 6. Manifest `gmb-package.json`

{
  "name": "duckdb",
  "version": "1.5.6",
  "description": "…",
  "contentType": "source|dotnet|vm|c-ffi|c-ffi-source|mixed",
  "backends": ["il", "c99", "vm"],
  "edition": "any|Community|Commercial",
  "exports": ["duckdb.inc"],                       // sorgenti/moduli (.bas/.inc)
  "samples": ["samples"],                          // esempi inclusi nel pacchetto — vedi §6c/§6d
  "binaries": { "any": { "osx-arm64": "lib/…/libx.dylib" } },  // per backend/piattaforma
  "ffi": {
    "api": "c-abi|gmb-native",
    "headers": ["include/x.h"],                    // → -I
    "libs": { "osx-arm64": "lib/…/libx.dylib" },   // → -L -l<basename>
    "link": "-lfoo",                               // flag extra
    "declareExample": "DECLARE FUNCTION X LIB \"x\" (a AS LONG) AS LONG"


## 6b. Sorgente esterna `source` (Fase 1: GitHub release)
| Campo | Obbl. | Significato |
|---|---|---|
| `type` | ✅ | `"github-release"` |
| `repo` | ✅ | `"owner/repo"` (es. `duckdb/duckdb`) |
| `tag` | — | Template del tag della release. Default `"v{version}"`; `"{version}"` = versione del pacchetto. Ammette un tag **letterale** (es. `"gmb-libs-2026.08.28"`). |
| `assets` | ✅ | `platformKey → nome asset`. Chiave `"any"` = fallback. platformKey ∈ {`osx-arm64`,`osx-x64`,`linux-x64`,`linux-arm64`,`win-x64`,…}. |
| `dest` | — | Cartella (relativa all'install) dove estrarre. Default `"lib/{platform}"`. |
| `headers` | — | Nomi di file presenti nell'asset da copiare anche in `include/` (per `-I`). |
| `sha256` | — | `platformKey → sha256` atteso. Se assente usa il `digest` della API GitHub. |

"source": {
  "type": "github-release", "repo": "duckdb/duckdb", "tag": "v{version}",
  "assets": {
    "osx-arm64": "libduckdb-osx-universal.zip", "osx-x64": "libduckdb-osx-universal.zip",
    "linux-x64": "libduckdb-linux-amd64.zip", "linux-arm64": "libduckdb-linux-arm64.zip",
    "win-x64": "libduckdb-windows-amd64.zip", "win-arm64": "libduckdb-windows-arm64.zip"
  },
  "headers": ["duckdb.h"]
}


## 6c. Compilazione C del pacchetto (`build`)
| Campo | Obbl. | Significato |
|---|---|---|
| `system` | — | `"make"` \| `"cmake"` \| `"command"`. `make` → `make <target> PLAT=<platformKey>`; `cmake` → `cmake -S . -B build-gmb -DPLAT=<platformKey> && cmake --build build-gmb`; `command` → comando shell. |
| `target` | — | Target di make/cmake. |
| `command` | — | Comando shell alternativo (`{platform}` = platformKey; es. `"make packlib PLAT={platform}"`). |
| `cwd` | — | Cartella dove eseguire (relativa alla dir del manifest; es. `".."`). |
| `tool` | — | Compilatore richiesto (`cc`/`gcc`/`clang`/`cmake`) — usato per il **rilevamento** client. |
| `outputs` | ✅ | File che il build deve produrre (relativi alla dir del manifest); `{platform}` sostituito. Se mancano → errore. |

### 6c.1 Publisher-side (materialize) — `gmb pkg build`

gmb pkg build <dir-del-pacchetto>     # esegue il build per la piattaforma corrente, verifica gli outputs
gmb pack <dir> [--out pkg.gmbpkg]     # include ffi.libs/headers (+ samples) — la lib compilata viaggia nel .gmbpkg
gmb publish pkg.gmbpkg --registry …   # pubblica


### 6c.2 Client-side (opt-in) — `contentType: "c-ffi-source"`

## 6d. Samples del pacchetto (`samples`)

## 6e. Tipi di dipendenza e `overrides`

### `files` / `ignore` (cosa entra nel `.gmbpkg`)

{
  "dependencies":         { "duckdb": "^1.5.0" },
  "devDependencies":      { "gmb-testkit": "*" },
  "optionalDependencies": { "gpu-accel": "^2.0.0" },
  "peerDependencies":     { "runtime-x": "^1.0.0" },
  "overrides":            { "libz": "1.2.13" }
}


## 6f. Workspaces (monorepo)

{ "name": "my-monorepo", "version": "0.1.0", "workspaces": ["packages/*"] }


## 7. FFI: pacchetti C-FFI e include `.inc`

## 8. Registry (server + porte + avvio)

dotnet run --project REPL/REPL.csproj -- shared/samples/PackageRegistry/PackageServer.bas


{
  "registryRoot": "/Users/…/.gmbasic/packages",
  "serverDir": "./",
  "frontendDir": "../Web/RegistryBsp",
  "bind": "http://localhost:8087",
  "maxPackageBytes": 104857600,
  "rateLimitPerMinute": 60,
  "admin": { "tokenHash": "…", "enabled": "1" },
  "users": { "alice": { "name": "alice", "tokenHash": "…", "publicKey": "…", "status": "active" } }
}


## 9. Lockfile `gmb-package.lock`

# gmb-package.lock — generato da GMBasic PackageManager v2
# backend=il timestamp=2026-10-05T…Z
# package@version|contentType|backend|platform|sha256(file:hash,...)
duckdb@1.5.6|Mixed|il|osx-arm64|duckdb.inc:…,gmb-package.json:…,include/gmbduckdb.h:…,lib/…:…


## 10. Esempio end-to-end (pacchetto `duckdb`)

# 1) (autore) crea e impacchetta
gmb init packages/duckdb --name duckdb --version 1.5.6 --type c-ffi --api c-abi
# ... popola duckdb.inc, include/, lib/ e gmb-package.json ...
gmb pack packages/duckdb --out duckdb-1.5.6.gmbpkg

# 2) (registry) avvia il server
dotnet run --project REPL/REPL.csproj -- shared/samples/PackageRegistry/PackageServer.bas

# 3) (utente) installa e usa
gmb pkg install duckdb --registry http://localhost:8087
# nel progetto:
#   main.bas:  REQUIRE "duckdb"
#   duckdb.inc fornisce: DECLARE FUNCTION DUCKDB_OPEN LIB "gmbduckdb" (path AS STRING) AS LONG
gmb restore            # scopre REQUIRE e installa
gmb restore --locked   # reinstall deterministico dal lock


## 10b. REQUIRE su tutti i backend + esempio verificato

tools/sh/parity4 shared/samples/includes/duckdb_demo.bas     # PASS su il/pcode/vmc/c99n/c99


## 11. Sicurezza / firme / integrità

### 11.1 Chiavi e firma

### 11.2 Trust anchor (keyring) — la firma da sola NON basta

### 11.3 Integrità della cache (lock con hash per-file)

### 11.4 Gate di compatibilità

## 12. Note operative

## 13. Pubblicare un pacchetto (passo-passo)

### 13.1 Scaffold

gmb init packages/miopkg --name miopkg --version 1.0.0 --type c-ffi --api c-abi


### 13.2 Anatomia di un pacchetto `c-ffi`

### 13.3 Dove mettere i binari (4 strategie)
| Caso | Dove | Come |
|---|---|---|
| Piccoli / sempre richiesti | dentro il pacchetto | `lib/<platform>/…`, pubblicati nel registry |
| Grandi, **già precompilati a monte** | release GitHub di terzi | blocco **`source`** (§6b), es. `duckdb/duckdb` |
| Grandi, **compilati da te** | release GitHub tua (es. **`c-vision/GMBasicModules`**) | carica `mioPkg-<platform>.zip` e puntaci il `source` |
| **Compilati all'install dal client** | nel pacchetto (sorgente C) | `contentType:"c-ffi-source"` + blocco **`build`** (§6c): il client compila se manca il precompilato |

### 13.4 Impacchetta (e firma)

gmb pack packages/miopkg --out miopkg-1.0.0.gmbpkg         # .gmbpkg (zip del pacchetto)
gmb pack packages/miopkg --key private.key                 # firma Ed25519 (opzionale, §11)


### 13.5 Pubblica sul registry

# registry locale di riferimento (sample BASIC):
dotnet run --project REPL/REPL.csproj -- shared/samples/PackageRegistry/PackageServer.bas &
gmb publish miopkg-1.0.0.gmbpkg --registry http://localhost:8087 --token <TOKEN>


### 13.6 Consuma

REQUIRE "miopkg"           ' risolve, scarica (inclusa l'eventuale source), include l'.inc
IMPORT MIOPKG WITH ALIAS   ' → MIOPKG.<Membro>(...)


### 13.7 Checklist autore

## 14. Architettura — il PM di riferimento è in **C**
| Modulo C | Compito |
|---|---|
| `gmb_semver` | SemVer 2.0 + range (`parse`/`satisfies`/`find_best`) |
| `gmb_pkg_manifest` | parse di `gmb-package.json` (ABI a path) |
| `gmb_pkg_lock` | lockfile + `sha256` per-file + integrità |
| `gmb_pkg_trust` | keyring locale + HMAC-SHA256 |
| `gmb_pkg_glob` | match glob (`files`/`ignore`) |
| `gmb_pkg_contentaddr` | cache content-addressable (blob + hardlink/symlink) + GC |
| `gmb_pkg_registry` | client registry HTTP (`gmb_net_http_get`): dettaglio, dist-tag, download |
| `gmb_pkg_resolver` | risoluzione completa: BFS/transitivi, vincoli, **overrides**, **locked**, **locali**, features, **dist-tag**, **download+unzip**, gate |
| `gmb_pkg_artifact` | `SelectArtifact` (contentType/binario/chunk/ffi) + flag C99 `-I/-L/-l` |
| `gmb_pkg_install` | `source` (github-release) + copia samples + client-build |
| `gmb_pkg_pack` | creazione `.gmbpkg` (zip standard + firma HMAC) |
| `gmb_pkg_advisories` | `audit` (advisory) + SBOM CycloneDX |
| `gmb_pkg_ed25519` / `gmb_pkg_ecdsa` | firme (OpenSSL; Ed25519 RFC 8032) |
| `gmb_pkg_util` | gate di compatibilità, `safe_dir`, `platform_key` |
| `gmb_pkg_graph` | albero dipendenze (`why`) + peer mancanti |

<!-- END SURFACE -->
