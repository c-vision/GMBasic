# II.11 — Moduli, preprocessore e librerie

## Cosa ottieni da questo capitolo

Come si spezza un programma in file, come si importano solo i simboli che servono, e come si
produce una libreria condivisa da riusare — da BASIC o da C.

## 1. `NAMESPACE`

Un namespace raggruppa procedure, tipi e costanti in un dominio:

```basic
NAMESPACE Matematica

PUBLIC FUNCTION Add(a AS DOUBLE, b AS DOUBLE) AS DOUBLE
    RETURN a + b
END FUNCTION

PRIVATE FUNCTION Interno() AS DOUBLE
    RETURN 1
END FUNCTION

END NAMESPACE
```

`PUBLIC` rende il simbolo visibile dall'esterno, `PRIVATE` (default) lo tiene interno. Un
namespace può essere esteso su più file.

## 2. `IMPORT`: sei forme

| Forma | Significato |
|---|---|
| `IMPORT Matematica` | importa tutti i simboli `PUBLIC` |
| `IMPORT Matematica AS M` | tutto sotto l'alias `M` |
| `IMPORT Matematica (Add, Mul)` | solo i simboli elencati |
| `IMPORT Matematica (Add AS Somma)` | selettivo, con rinomina |
| `IMPORT Matematica (Add, Mul) AS M` | selettivo sotto alias |
| `IMPORT Matematica.*` | equivalente alla forma completa |

`IMPORT` è una **direttiva di modulo**, non un comando: le parentesi qui servono a selezionare,
non a eseguire.

L'alias crea un **namespace virtuale**: `M.Add(...)` è lo stesso codice di
`Matematica.Add(...)`, con un nome più corto. Non nasce un dominio nuovo.

## 2bis. Struttura delle librerie importate

In GMBasic **non esiste una direttiva `INCLUDE`**. Il ruolo di `#include` è coperto da tre
canali distinti, che rispondono a tre domande diverse:

| Canale | Sintassi | Risolve | File |
|---|---|---|---|
| **Modulo** | `IMPORT <NAMESPACE>` | un file `.inc` (o `.bas`) che dichiara `NAMESPACE` | `shared/include/`, cartelle di progetto, `compiler.searchFolders` |
| **Pacchetto** | `REQUIRE "nome" VERSION "^1.0"` | un pacchetto installato dal package manager; i suoi export sono preposti al programma | `vendor/`, `<exe>/packages/`, `~/.gmbasic/packages/` |
| **FFI** | `DECLARE FUNCTION … LIB "gmbnome"` | una libreria dinamica (`libgmbnome.dylib`/`.so`/`.dll`) | `build/libs/<rid>/`, `shared/libs/<nome>/`, `~/.gmbasic/libs/<rid>/` |

Il file `.inc` è il **testo del modulo**; `IMPORT` è il canale che lo porta dentro il
programma. `REQUIRE` non importa un file: dichiara una dipendenza che il package manager
installa e i cui export diventano disponibili. `DECLARE` non importa codice BASIC: collega un
simbolo a una funzione C.

### Dove stanno i file

| Percorso | Contenuto |
|---|---|
| `shared/include/<nome>.inc` | export pubblico del modulo (`NAMESPACE`, `DECLARE`) |
| `shared/libs/<nome>/include/gmb<nome>.h` | header C della stessa libreria |
| `shared/libs/<nome>/` | progetto di una libreria **nostra** (`build.sh`, `src/`, `include/`, `samples/`) |
| `packages/<nome>/<versione>/` | sorgenti di un **pacchetto** con dipendenze esterne |
| `GMBasic/build/libs/<rid>/` | artefatti di build (gitignored) |
| `~/.gmbasic/libs/<rid>/` | cache delle librerie pubblicate + librerie esterne |
| `~/.gmbasic/packages/<nome>/<ver>/` | pacchetti installati su un client |
| `<progetto>/vendor/<nome>/<ver>/` | pin offline del progetto |

Una libreria **nostra e autocontenuta** sta in `shared/libs/` con l'export in
`shared/include/`; un **pacchetto** sta in `packages/`; una libreria **esterna** sta nella
cache o dentro un pacchetto.

### Risoluzione di `IMPORT`

Il resolver è unico per C99Native e bytecode
(`GMBasic/native/gmb_compiler_c/frontend/src/gmb_inc_resolve.c`, parità con
`Executor/Orchestrator.cs`):

1. scansione ricorsiva della cartella del programma (`*.bas`, `*.inc`);
2. `importFolders` / `importFiles` dei settings (`system.settings.json`, `app.settings.json`);
3. `compiler.searchFolders` (default `include`, `Controls`, `controls`, `{install}/include`,
   `{install}/Controls`, `{install}/libs/<nome>`; ammessi `{project}`, `{install}` e il `*`
   finale per espandere le sottocartelle);
4. un file di progetto/import **shadowa per nome** quello delle cartelle globali;
5. gli `IMPORT` di **ogni** file caricato sono seguiti in profondità (dipendenze prima del
   dipendente), non solo quelli della entry.

Un `.inc` raggiunto da due path diversi viene canonicalizzato e incluso una volta sola: senza
questo, le `SUB` del namespace risulterebbero duplicate.

### Risoluzione di `REQUIRE`

```basic
REQUIRE "duckdb" VERSION "^1.5.0"
IMPORT DUCKDB

SUB Main()
    PRINT DUCKDB.AVAILABLE()
END SUB
```

Precedenza d'uso: **vendor** del progetto → **`<exe>/packages`** → **cache**
(`<checkout>/packages` in sviluppo, `~/.gmbasic/packages` su client). Il resolver consulta
anche **lockfile** (`gmb restore --locked`) e **registry** per scegliere la versione; il
download avviene in `restore`/`install`, mentre il solo resolve fallisce con `FileNotFound`.
Il lock `gmb-package.lock` fissa versioni e hash; `gmb restore --frozen` fallisce se il lock
divergerebbe dai `REQUIRE`.

Un pacchetto C-FFI porta header e libreria:

```
packages/duckdb/1.5.6/
  gmb-package.json                 # manifest v2
  duckdb.inc                       # exports: dichiarazioni BASIC
  include/gmbduckdb.h              # ffi.headers
  lib/osx-arm64/libgmbduckdb.dylib # ffi.libs
```

### Risoluzione FFI

Il loader cerca per **basename** (`.dylib`/`.so`/`.dll`), poi per path esatto, nell'ordine:
risalendo dal CWD (`libs` → `shared/libs` → `build/libs/<rid>`) → root del monorepo (stessa
sequenza) → `lib/<piattaforma>` del pacchetto → `~/.gmbasic/libs/<rid>`. La convenzione è
`LIB "gmb<nome>"` → `libgmb<nome>.dylib`.

### Esempi reali

| File | Cosa mostra |
|---|---|
| `shared/samples/Modules/MathUtils.inc` + `ImportSelective.bas` | modulo `.inc`, `IMPORT` selettivo |
| `shared/samples/Modules/ImportAlias.bas` | `IMPORT … AS` |
| `shared/samples/include/duckdb_demo.bas` | `REQUIRE` + `IMPORT` di un pacchetto |
| `shared/samples/FFI/FfiNamespace.bas` | `DECLARE … LIB` in un namespace |
| `shared/include/media.inc` | export FFI completo di una libreria nostra |

### Perché `gmbsuite-implemented` non è una fonte

Le librerie già migrate sono uscite da `gmbsuite` con il commit `6791d98a` (2026-10-29),
perché il monorepo è la fonte di verità. `gmbsuite-implemented` è una copia di consultazione
senza git e senza manifest, e può essere più vecchia: per il codice attivo si guarda sempre
`shared/libs/` o `packages/`. Dettaglio: `Knowledge/Infrastruttura-Includes.md`.

## 3. Preprocessore su più file

Le direttive di II.4 valgono anche attraverso i file inclusi: un simbolo definito in un file
è visibile dove quel file è incluso, e le direttive dentro stringhe o commenti restano testo.
Per i progetti multi-target la stessa base di codice si seleziona con `#IFDEF`, senza duplicare
i file.

## 4. Produrre una libreria: `LIBRARY`

Una riga in testa al file cambia il **prodotto**: non un eseguibile, ma una libreria condivisa.

```basic
LIBRARY "mymath"

PUBLIC FUNCTION Add(a AS INTEGER, b AS INTEGER) AS INTEGER
    Add = a + b
END FUNCTION

PUBLIC FUNCTION Mul(a AS DOUBLE, b AS DOUBLE) AS DOUBLE
    Mul = a * b
END FUNCTION
```

Cosa cambia:

- **niente entry point**: un file `LIBRARY` non ha `SUB Main` e il corpo top-level non parte;
- si esportano **solo** le `SUB`/`FUNCTION` marcate `PUBLIC`; il resto resta interno;
- i simboli esportati hanno nome `<libreria>_<Proc>`: `mymath_Add`, `mymath_Mul`.

Il prodotto sono due file: la libreria (`mymath.dylib` su macOS, `mymath.so` su Linux) e il
**manifest** `mymath.gmblib.json`, che dichiara i simboli e le firme. Un consumatore sa cosa può
chiamare senza leggere il sorgente.

### Il confine ABI

| Tipo BASIC | Tipo C al confine |
|---|---|
| `INTEGER`, `LONG`, `BYTE`, `BIT` | `gmb_int_t` |
| `DOUBLE`, `SINGLE` | `gmb_double_t` |
| `STRING` (ingresso) | `const char*` + `gmb_int_t <p>_len` |
| `STRING` (uscita) | `const char*` terminata da NUL, di proprietà della libreria |
| `SUB` | `void` |

Nella versione attuale l'ABI copre **scalari e stringhe**: array, `BYREF`, UDT e closure restano
interni alla libreria. È un limite dichiarato, non un difetto nascosto.

L'accesso avviene con `dlopen`/`dlsym`, e si usa `RTLD_LAZY`: la libreria porta dentro il runtime
C, che referenzia anche i simboli grafici, e con il binding immediato servirebbe l'intero stack
grafico nel processo chiamante.

## 5. Usare una libreria da BASIC

```basic
SUB Main()
    GMB MODULE REGISTER "math.slot", "work/mymath.dylib"
    GMB MODULE PRIORITY "math.slot", 10
    GMB MODULE SYM "math.slot", "mymath_Add"
    GMB MODULE CALL "math.slot", "mymath_Add|iii|2,3"     ' stampa 5
END SUB
```

La specifica di `CALL` è `simbolo|firma|argomenti`: nella firma la prima lettera è il tipo di
ritorno, le altre i parametri (`i` intero, `d` double, `s` stringa). Lo slot con priorità più
alta vince: è il meccanismo con cui si sostituisce un'implementazione senza ricompilare il
programma.

## 6. Esempio eseguito

`shared/samples/Modules/ImportSelective.bas`, backend C99Native:

```
Chiamata a Add(7.0, 3.0) senza prefisso (importato selettivamente)...
Risultato:     10

Chiamata a Multiply(7.0, 3.0) senza prefisso (importato selettivamente)...
Risultato:     21

Chiamata a Subtract(7, 3) con prefisso (non importato selettivamente)...
Risultato:     4
```

Le tre chiamate sono la prova del selettivo: `Add` e `Multiply` sono state importate per nome e si
usano **senza prefisso**; `Subtract` non è nell'elenco, quindi si chiama solo con il nome del
namespace davanti. È la differenza fra "importo tutto" e "importo ciò che dichiaro".

## 7. Errori frequenti

| Sintomo | Causa | Correzione |
|---|---|---|
| nome non trovato dopo `IMPORT (…)` | il simbolo non è nell'elenco selettivo | aggiungilo, o usa la forma completa |
| `PUBLIC` ignorato | il simbolo è dichiarato dentro un blocco non pubblico | dichiara a livello di namespace |
| la libreria "non parte" | un file `LIBRARY` non ha entry point | è corretto: si carica, non si esegue |
| `IMPORT` non trova il modulo | `.inc` fuori dalle cartelle di ricerca | mettilo nel progetto o aggiungi il path a `importFolders` |
| `REQUIRE` fallisce con `FileNotFound` | pacchetto non in cache/vendor | `gmb restore`, oppure `gmb pkg install <nome>` |
| `DECLARE … LIB` non carica | basename diverso da `libgmb<nome>` o file fuori dai path | controlla il nome e l'ordine di ricerca FFI |
| simbolo non trovato in `GMB MODULE CALL` | firma o nome diversi dal manifest | controlla il manifest `*.gmblib.json` |
| errore di caricamento con `RTLD_NOW` da C | il runtime referenzia simboli grafici | usa `RTLD_LAZY`, come fa il caricatore di GMBasic |

## 8. Dove andare adesso

- **II.4** — direttive del preprocessore in dettaglio.
- **III.16** — FFI: chiamare librerie esterne dal BASIC.
- **`Guide/utente/Librerie-Importate-Guida-Utente.md`** — guida pratica a `IMPORT`, `REQUIRE`, `DECLARE`.
- **VI.37** — package manager: distribuire ciò che hai costruito.
- **VI.38** — percorsi e configurazione del progetto.







<!-- BEGIN SURFACE (generato da tools/sh/manual_surface.sh — non modificare a mano) -->

## Riferimento completo: comandi, funzioni, parametri

> Sezione generata dalle guide del progetto per questo capitolo: contiene **tutte**
> le superfici documentate, non una selezione. La prosa dei capitoli precedenti le
> spiega; qui stanno i dettagli — parametri, varianti, esempi d'uso.

### LIBRARY-gmblib-Guide

*Fonte: `LIBRARY-gmblib-Guide`*


## 1. Il file sorgente

LIBRARY "mymath"

PUBLIC FUNCTION Add(a AS INTEGER, b AS INTEGER) AS INTEGER
  Add = a + b
END FUNCTION

PUBLIC FUNCTION Mul(a AS DOUBLE, b AS DOUBLE) AS DOUBLE
  Mul = a * b
END FUNCTION


## 2. Il contratto ABI (che tipi passano al confine)
| Tipo BASIC                         | Tipo C nell'ABI (ingresso → uscita) |
|------------------------------------|-------------------------------------|
| `INTEGER`, `LONG`, `BYTE`, `BIT`…  | `gmb_int_t`                         |
| `DOUBLE`, `SINGLE` (default)       | `gmb_double_t`                       |
| `STRING`                           | `const char*` + `gmb_int_t <p>_len` in ingresso · `const char*` (lib-owned) in uscita |
| `SUB` (nessun ritorno)             | `void`                              |

gmb_int_t mymath_Add(gmb_int_t a, gmb_int_t b);


const char* mymath_Greet(const char* name, gmb_int_t name_len);


## 3. Il prodotto: `.dylib` + manifest

{
  "name": "mymath",
  "abi_version": 1,
  "artifact": "work/mymath.dylib",
  "exports": [
    { "symbol": "mymath_Add", "params": { "a": "gmb_int_t", "b": "gmb_int_t" } },
    { "symbol": "mymath_Mul", "params": { "a": "gmb_double_t", "b": "gmb_double_t" } },
    { "symbol": "mymath_Greet",
      "params": { "name": "const char*", "name_len": "gmb_int_t" } }
  ],
  "depends": ["gmb_runtime_c"]
}


## 4. Come si compila

tools/sh/parity4 shared/samples/C99/GmbLibMath.bas


mkdir -p /tmp/mymath && cp mymath.bas /tmp/mymath/
GMBasic/native/gmb_compiler_c/build/gmb build --project /tmp/mymath
# → /tmp/mymath/bin/mymath.dylib     (--json aggiunge "library": true)
GMBasic/native/gmb_compiler_c/build/gmb run --project /tmp/mymath   # costruisce, non esegue


## 5. Usare la libreria da un altro programma BASIC (`GMB MODULE`)

SUB Main()
  GMB MODULE REGISTER "math.slot", "work/mymath.dylib"
  GMB MODULE PRIORITY "math.slot", 10
  GMB MODULE LIST
  ' SYM: il simbolo è PRESENTE nella dylib vincente dello slot (errore se non risolto).
  GMB MODULE SYM "math.slot", "mymath_Add"
  ' CALL: INVOCA l'export. Spec "simbolo|sig|argomenti"; sig: prima lettera = ritorno,
  ' poi i parametri (i=int64, d=double, s=stringa); argomenti separati da virgola.
  GMB MODULE CALL "math.slot", "mymath_Add|iii|2,3"     ' stampa 5
  GMB MODULE CALL "math.slot", "mymath_Mul|ddd|2.5,4"   ' stampa 10
END SUB


## 6. Da fuori (C e altri linguaggi)

#include <dlfcn.h>
#include "gmb_runtime.h"
typedef gmb_int_t (*add_fn)(gmb_int_t, gmb_int_t);
typedef const char* (*greet_fn)(const char*, gmb_int_t);
void* h = dlopen("work/mymath.dylib", RTLD_LAZY);
add_fn add = (add_fn)dlsym(h, "mymath_Add");
gmb_int_t r = add(2, 3);                                   /* 5 */
greet_fn greet = (greet_fn)dlsym(h, "mymath_Greet");
const char* s = greet("Mondo", -1);                        /* "Ciao, Mondo" */


## 7. Limiti attuali (v1)

## 8. Riferimenti

### Librerie-Importate-Guida-Utente

*Fonte: `Librerie-Importate-Guida-Utente`*


## 1. I tre canali
| Canale | Sintassi | A cosa serve | Da dove arriva |
|---|---|---|---|
| **Modulo** | `IMPORT <NAMESPACE>` | usare un `.inc` che dichiara `NAMESPACE` | `shared/include/`, cartella del progetto, `compiler.searchFolders` |
| **Pacchetto** | `REQUIRE "nome" VERSION "^1.0"` | installare un pacchetto e usarne gli export | `vendor/`, cache `~/.gmbasic/packages/`, registry |
| **FFI** | `DECLARE FUNCTION … LIB "gmbnome"` | chiamare una funzione C in una libreria dinamica | `shared/libs/`, `build/libs/<rid>/`, `~/.gmbasic/libs/<rid>/` |

## 2. Dove stanno i file
| Percorso | Cosa contiene |
|---|---|
| `shared/include/<nome>.inc` | export pubblico del modulo (`NAMESPACE`, `DECLARE`) |
| `shared/libs/<nome>/include/gmb<nome>.h` | header C della stessa libreria |
| `shared/libs/<nome>/` | sorgenti + build di una libreria **nostra** (`build.sh`, `src/`, `samples/`) |
| `packages/<nome>/<versione>/` | sorgenti di un **pacchetto** con dipendenze esterne |
| `GMBasic/build/libs/<rid>/` | artefatti di build (gitignored) |
| `~/.gmbasic/libs/<rid>/` | cache delle librerie pubblicate + librerie esterne |
| `~/.gmbasic/packages/<nome>/<ver>/` | pacchetti installati su un client |
| `<progetto>/vendor/<nome>/<ver>/` | pin offline del progetto |

## 3. Importare un modulo `.inc` (`IMPORT`)

' shared/samples/Modules/MathUtils.inc
NAMESPACE MATHUTILS

PUBLIC FUNCTION Add(a AS DOUBLE, b AS DOUBLE) AS DOUBLE
    RETURN a + b
END FUNCTION

PUBLIC FUNCTION Multiply(a AS DOUBLE, b AS DOUBLE) AS DOUBLE
    RETURN a * b
END FUNCTION
END NAMESPACE


IMPORT MATHUTILS (Add, Multiply)    ' solo i due simboli, senza prefisso

SUB Main()
    PRINT Add(7, 3)                 ' 10
    PRINT Multiply(7, 3)            ' 21
END SUB


## 4. Installare e usare un pacchetto (`REQUIRE`)

' shared/samples/include/duckdb_demo.bas
REQUIRE "duckdb" VERSION "^1.5.0"
IMPORT DUCKDB

SUB Main()
    PRINT "duckdb available="; DUCKDB.AVAILABLE()
END SUB


gmb restore                 # installa le dipendenze del progetto e riscrive il lock
gmb restore --locked        # installa esattamente le versioni del lock
gmb restore --frozen        # CI: fallisce se il lock divergerebbe
gmb pkg vendor              # copia i pacchetti in vendor/ (offline)
gmb pkg install duckdb      # installa un singolo pacchetto dal registry


packages/duckdb/1.5.6/
  gmb-package.json                 # manifest v2
  duckdb.inc                       # exports: dichiarazioni BASIC
  include/gmbduckdb.h              # header C
  lib/osx-arm64/libgmbduckdb.dylib # libreria FFI
  lib/osx-arm64/libduckdb.dylib    # libreria esterna bundlata


## 5. Chiamare una libreria C (`DECLARE … LIB`)

NAMESPACE MEDIA

PUBLIC DECLARE FUNCTION AVAILABLE LIB "gmbmedia" ALIAS "gmbmedia_available" () AS LONG
PUBLIC DECLARE FUNCTION IMGREAD LIB "gmbmedia" ALIAS "gmbmedia_img_read" (path AS STRING) AS LONG
PUBLIC DECLARE FUNCTION IMGW LIB "gmbmedia" ALIAS "gmbmedia_img_w" (h AS LONG) AS LONG

END NAMESPACE


IMPORT MEDIA

SUB Main()
    DIM h AS LONG
    h = MEDIA.IMGREAD("foto.png")
    IF h < 0 THEN
        PRINT "errore"
        EXIT SUB
    END IF
    PRINT MEDIA.IMGW(h)
    MEDIA.IMGFREE(h)
END SUB


## 6. Errori frequenti
| Sintomo | Causa | Correzione |
|---|---|---|
| `IMPORT X` non trova il modulo | manca il `.inc` o non è in una cartella di ricerca | mettilo nel progetto o aggiungi il path a `importFolders` |
| simbolo duplicato / `SUB` definita due volte | lo stesso `.inc` raggiunto da due path | usa un solo path canonico; il resolver deduplica i file, non i namespace duplicati |
| `REQUIRE` fallisce con `FileNotFound` | pacchetto non in cache/vendor e nessun download | `gmb restore`, oppure `gmb pkg install <nome>` |
| il pacchetto è installato ma il simbolo manca | export non elencato nel manifest / `.inc` non importato | controlla `exports` e aggiungi `IMPORT` |
| `DECLARE … LIB` non carica | nome lib diverso da `libgmb<nome>` o file fuori dai path | controlla il basename e l'ordine di ricerca FFI |
| funzione trovata ma risultato errato | firma FFI diversa dall'ABI C | confronta tipi e `ALIAS` con l'header |

## 7. Verifica
| File | Cosa copre |
|---|---|
| `shared/samples/Modules/ImportSelective.bas` | `IMPORT` selettivo e qualificato |
| `shared/samples/Modules/ImportAlias.bas` | `IMPORT … AS` |
| `shared/samples/include/duckdb_demo.bas` | `REQUIRE` + `IMPORT` di un pacchetto |
| `shared/samples/FFI/FfiNamespace.bas` | `DECLARE … LIB` in un namespace |
| `shared/minitests/ffi/` | firme, BYREF, array, stringhe |
| `tools/sh/parity4 <file.bas>` | parità sui quattro backend |

## 8. `gmbsuite` e `gmbsuite-implemented`
| Prima | Ora |
|---|---|
| le lib già migrate stavano anche in `gmbsuite` | sono uscite con il commit `6791d98a` (2026-10-29) |
| `gmbsuite-deprecated` era la copia di lavoro | rinominata `gmbsuite-implemented` (2026-11-02), senza git |
| due possibili sorgenti | una sola fonte: `shared/libs/` o `packages/` del monorepo |

## 9. Vedi anche
| Documento | Contenuto |
|---|---|
| `Guide/utente/Package-Manager-Guida-Utente.md` | comandi, manifest, registry, lock |
| `Guide/utente/FFI-Guida-Utente.md` | `DECLARE`, tipi, callback, errori |
| `Guide/utente/LIBRARY-gmblib-Guide.md` | produrre una libreria da BASIC |
| `Knowledge/Infrastruttura-Includes.md` | risoluzione `IMPORT`/`REQUIRE`/FFI, storia gmbsuite |
| `Knowledge/Librerie-e-Pacchetti.md` | layout canonico delle tre aree |
| `Knowledge/PackageManager-Percorsi-e-Funzionamento.md` | cache, vendor, lock, registry |

### PrecompilerMultiTarget

*Fonte: `PrecompilerMultiTarget`*


## Descrizione

## Costanti predefinite
| Costante | Attivazione | Esempio |
|----------|-------------|---------|
| `WINDOWS` | Su Windows | `#IFDEF WINDOWS` |
| `LINUX` | Su Linux | `#IFDEF LINUX` |
| `MACOS` | Su macOS | `#IFDEF MACOS` |
| `X64`, `ARM64`, `X86`, `ARM` | Architecture-dependent | `#IFDEF ARM64` |
| `DEBUG` | Build Debug | `#IFDEF DEBUG` |
| `RELEASE` | Build Release | `#IFDEF RELEASE` |

## Sintassi supportata

#IFDEF LINUX
  PRINT "Linux"
#ENDIF

#IF LINUX AND ARM64 THEN
  PRINT "Linux su ARM64"
#ENDIF

#IFDEF WINDOWS
  PRINT "Windows"
#ELSEIF MACOS
  PRINT "macOS"
#ELSE
  PRINT "Altro"
#ENDIF


## Implementazione

## Compatibilità

## Sample

## Relazioni

<!-- END SURFACE -->
