# III.16 — FFI e librerie native

## Cosa ottieni da questo capitolo

Chiamare codice C da BASIC: una `DECLARE`, poi la funzione si usa come tutte le altre. Il
capitolo dice cosa si può passare (e cosa no), perché la firma deve essere omogenea e come si
risolve il nome della libreria — i tre punti su cui si sbaglia.

## 1. Dichiarare

```basic
DECLARE FUNCTION Nome LIB "libreria" [ALIAS "simbolo_nativo"] (p1 AS T1, p2 AS T2) AS Ritorno
DECLARE SUB      Nome LIB "libreria" [ALIAS "simbolo_nativo"] (p1 AS T1)
```

| Clausola | Significato |
|---|---|
| `LIB` | nome o percorso della libreria (`.dylib`, `.so`, `.dll`) |
| `ALIAS` | nome del simbolo esportato, se diverso dal nome BASIC |
| `DECLARE FUNCTION` | si usa in un'espressione, ritorna un valore |
| `DECLARE SUB` | si usa come istruzione, non ritorna nulla |

Senza `ALIAS` il simbolo nativo ha lo stesso nome della funzione BASIC.

## 2. Tipi al confine

| Tipo BASIC | Forma nativa |
|---|---|
| `DOUBLE` | `double` |
| `LONG`, `INTEGER`, `QUAD`, `BYTE` | `int64_t` (tutti a 8 byte) |
| `STRING` | puntatore; ammesso come parametro e come ritorno |
| array (`a() AS ...`) | puntatore al buffer, solo `DOUBLE` o `LONG`/`QUAD` |
| `BYREF` scalare | puntatore a un valore: solo `DOUBLE`/`LONG`/`QUAD`/`ULONG`/`UQUAD` |
| struct/UDT | per valore, 1-2 campi |

Il formato delle chiamate non usa maschere a bit: `argc` è a 16 bit e ogni argomento ha il
suo descrittore. Il compilatore accetta oggi fino a **64 argomenti** per chiamata (la VM/C ha
buffer dinamici, quindi margine ulteriore); un array, una stringa o un `BYREF` contano un
argomento, una struct conta un campo per argomento (limite 8 per UDT/callback nella prima
iterazione).

## 3. La regola della firma omogenea

Se la firma contiene un **array**, una **stringa**, un **`BYREF`**, una **callback** o una
**struct**, allora **tutti** i parametri e il ritorno devono essere interi (`AS LONG`): il
puntatore viaggia come intero.

Fra soli scalari semplici la firma può essere **mista** (`DOUBLE` e interi insieme), ma resta
una regola: il tipo dichiarato deve combaciare con quello reale del C. Una firma sbagliata non
dà un errore: dà un numero senza senso — è il modo più comune di perdere un pomeriggio.

## 4. Un esempio eseguito

`shared/samples/FFI/FfiDropinLibs.bas`, backend C99Native, con la libreria trovata per
auto-scoperta in `libs/`:

```
║ FFI drop-in: include/ + libs/ (auto-discovery) ║

demo_scale(3.0, 4.0) =  12
demo_add(21, 21)     =  42
demo_prime(17)       =  1

Test FfiDropinLibs completato.
```

Le tre righe coprono i tre casi: `DOUBLE` puro (`demo_scale`), interi (`demo_add`) e un
predicato che ritorna un intero usato come verità (`demo_prime(17)` → `1`, cioè primo).

## 5. Il drop-in senza pacchetto

Due file, nessun manifest:

```
progetto/
  FfiDropin.inc        ' PUBLIC DECLARE FUNCTION ... LIB "demo"
  libs/libdemo.dylib   ' la libreria compilata
  Main.bas             ' IMPORT FfiDropin
```

Il `.inc` accanto al programma vince su quello della `include/` globale; la libreria si cerca
nella `libs/` risalendo dal programma. È il modo più rapido per provare una libreria prima di
farne un pacchetto.

## 6. Come si risolve il nome della libreria

| Ordine | Candidato |
|---|---|
| 1 | il nome così com'è: percorso relativo o nome nudo |
| 2 | nome + estensione di piattaforma (`libdemo` → `libdemo.dylib`) |
| 3 | `libs/` risalendo dal programma (il più vicino vince) |
| 4 | librerie precompilate di un pacchetto risolto da `REQUIRE` |

L'ordine è identico sui quattro backend: una `LIB` che funziona su uno funziona su tutti.

## 7. Errori frequenti

Tutti questi sono **errori di compilazione**, con messaggio identico sui backend:

| Messaggio | Causa | Correzione |
|---|---|---|
| `massimo 64 argomenti supportati` | troppi argomenti | raggruppa in una struct |
| `firma mista LONG/DOUBLE non supportata` | array/stringa/BYREF in una firma mista | dichiara tutto `AS LONG` |
| `un parametro array, STRING o BYREF richiede firma INT64` | stesso caso, visto dall'altro lato | idem |
| `tipo elemento non supportato per il parametro array` | `INTEGER`/`BYTE`/`SINGLE` in un array | usa `DOUBLE` o `LONG` |
| `BYREF di STRING non supportato` | stringa per riferimento | passa `BYVAL` |
| `il parametro BYREF richiede una variabile semplice` | hai passato un'espressione | assegna prima a una variabile |

## 8. Cosa non è coperto

- array come **tipo di ritorno**: no, solo come parametro;
- array di stringhe (`char**`): no, nessuna rappresentazione nativa;
- strutture grandi: 1-2 campi per valore;
- callback: ammesse come parametro, con la firma omogenea.

Sono limiti dichiarati del marshalling, non sorprese: se ti servono, la strada è passare un
puntatore a un buffer e gestire la memoria dal lato C.

## 9. Dove andare adesso

- **II.11** — `LIBRARY`: il percorso inverso, esportare BASIC verso C.
- **III.15** — i MiniTest FFI (`shared/minitests/ffi/`) come modello di prova.
- **III.12** — cosa succede quando la libreria carica dentro il processo.

<!-- BEGIN SURFACE (generato da tools/sh/manual_surface.sh — non modificare a mano) -->

## Riferimento completo: comandi, funzioni, parametri

> Sezione generata dalle guide del progetto per questo capitolo: contiene **tutte**
> le superfici documentate, non una selezione. La prosa dei capitoli precedenti le
> spiega; qui stanno i dettagli — parametri, varianti, esempi d'uso.

### FFI-Guida-Utente

*Fonte: `FFI-Guida-Utente`*


## 1. Dichiarazione

### `DECLARE FUNCTION`

DECLARE FUNCTION <NomeBASIC> LIB "<libreria>" [ALIAS "<nome_nativo>"] ([param]) AS <TipoRitorno>


### `DECLARE SUB`

DECLARE SUB <NomeBASIC> LIB "<libreria>" [ALIAS "<nome_nativo>"] ([param])


### Parametri

DECLARE FUNCTION <Nome> LIB "<lib>" [ALIAS "<simbolo>"] (<p1> AS <T1>, <p2> AS <T2>, ...) AS <TRitorno>


### Chiamata

## 1bis. BYREF scalare (2026-09-28)

DECLARE FUNCTION VecDivMod LIB "libvec_module" ALIAS "vec_divmod" (a AS LONG, b AS LONG, BYREF r AS LONG) AS LONG
DIM remainder AS LONG
DIM q AS LONG
q = VecDivMod(17, 5, remainder)       ' vec_divmod (native C): long vec_divmod(long a, long b, long* rem)
' q = 3, remainder = 2


## 2. NAMESPACE e IMPORT

### Includerlo in un file `.inc`

' FfiMathLib.inc
NAMESPACE FfiMathLib

PUBLIC DECLARE FUNCTION vec_scale LIB "libvec" (a AS DOUBLE, b AS DOUBLE) AS DOUBLE
PUBLIC DECLARE FUNCTION ns_add LIB "libvec" ALIAS "vec_add" (a AS LONG, b AS LONG) AS LONG


### Import completo — simboli non qualificati

IMPORT FfiMathLib
PRINT vec_scale(3.0, 4.0)   ' ok


### Import con alias — simboli qualificati

IMPORT FfiMathLib AS FM
PRINT FM.vec_scale(3.0, 4.0)  ' ok


## 3. ALIAS

DECLARE FUNCTION ns_add LIB "libvec" ALIAS "vec_add" (a AS LONG, b AS LONG) AS LONG


## 4. Tipi supportati
| Tipo BASIC | Tipo C | Supporto | Note |
|---|---|---|---|
| `DOUBLE` | `double` | ✅ | Contratto all-double |
| `LONG` | `int64_t` / `long` | ✅ | Contratto all-long (OP_CALL_FFI_T) |
| `INTEGER` | `int` | ✅ | Trattato come INT64 |
| `QUAD` | `int64_t` | ✅ | Trattato come INT64 |
| `BYTE` | `int8_t` | ✅ | Trattato come INT64 |
| **`STRING`** (param/ritorno) | `const char*` | ✅ | 2026-09-28, vedi §4ter |
| **`BYREF` scalare** (`AS DOUBLE`/LONG/QUAD/ULONG/UQUAD) | puntatore | ✅ | 2026-09-28, vedi §1bis |
| `BYREF` di STRING | puntatore-a-puntatore | ❌ | Errore compile-time su tutte le backend (char**, fuori scope) |
| **array** (`a() AS DOUBLE`/interi) | puntatore | ✅ | 2026-09-28, vedi §4bis |
| misto LONG+DOUBLE | — | ❌ | Errore compile-time su tutte le backend |

## 4bis. Array BYREF (2026-09-28)

DECLARE SUB VecFill LIB "libvec_module" ALIAS "vec_fill" (buf() AS DOUBLE, n AS LONG)
DIM v(5) AS DOUBLE
VecFill(v, 5)          ' vec_fill (native C): void vec_fill(double* buf, long n)


## 4ter. STRING — parametro e ritorno (2026-09-28)

DECLARE FUNCTION VecLabel LIB "libvec_module" ALIAS "vec_label" (id AS LONG, name AS STRING) AS STRING
DIM s$ = VecLabel(7, "alpha")   ' vec_label (native C): const char* vec_label(long id, const char* name)


### SUB void

## 4quater. Struct/UDT per valore (2026-09-29)

TYPE Point
    X AS DOUBLE
    Y AS DOUBLE
END TYPE

DECLARE FUNCTION VecMakePoint LIB "libvec_module" ALIAS "vec_make_point" (x AS DOUBLE, y AS DOUBLE) AS Point
DECLARE FUNCTION VecPointLen LIB "libvec_module" ALIAS "vec_point_len" (p AS Point) AS DOUBLE
' vec_make_point (native C): vec_point_t vec_make_point(double x, double y)  -- struct { double x, y; }
' vec_point_len  (native C): double vec_point_len(vec_point_t p)

DIM p AS Point
p = VecMakePoint(3, 4)
PRINT p.X; p.Y             ' 3  4
PRINT VecPointLen(p)       ' 5


TYPE Labeled
    V AS DOUBLE
    N AS LONG
END TYPE
DECLARE FUNCTION VecMakeLabeled LIB "libvec_module" ALIAS "vec_make_labeled_dn" (v AS DOUBLE, n AS DOUBLE) AS Labeled
DIM lab AS Labeled
lab = VecMakeLabeled(3.5, 7)
PRINT lab.V; lab.N          ' 3.5  7  -- kind misto sul RITORNO, ammesso


### Struct annidate (2026-09-29)

TYPE Inner
    X AS DOUBLE
END TYPE
TYPE Outer          ' 2 campi FOGLIA totali (Inner.X + Y) → ammissibile
    I AS Inner
    Y AS LONG
END TYPE

DECLARE FUNCTION VecNestedRoundtrip LIB "libvec_module" ALIAS "vec_nested_roundtrip" (o AS Outer) AS Outer

DIM o AS Outer
o.I.X = 3.5 : o.Y = 7
DIM r AS Outer
r = VecNestedRoundtrip(o)
PRINT r.I.X; r.Y            ' 4.5  8


## 4quinquies. Callback come parametro (2026-09-29)

DECLARE FUNCTION VecApply LIB "libvec_module" ALIAS "vec_cb_apply" (cb AS FUNCTION(x AS LONG) AS LONG, x AS LONG) AS LONG
' native C: long vec_cb_apply(long (*cb)(long), long x) { return cb(x); }

DIM r AS LONG
r = VecApply(FN(x AS LONG) AS LONG => x * 2, 21)
PRINT r                    ' 42 -- la lambda e' stata invocata DAL nativo, non da BASIC


## 4sexies. Firma mista LONG/DOUBLE fra scalari semplici (2026-10)

DECLARE FUNCTION VecMixed LIB "libvec_module" ALIAS "vec_mixed" (id AS LONG, price AS DOUBLE, qty AS LONG) AS DOUBLE
' native C: double vec_mixed(long id, double price, long qty) { return price * qty; }
PRINT VecMixed(7, 2.5, 7)   ' 17.5


## 5. Limiti

## 6. Messaggi errore esatti (identici su 4 backend)

FFI '<sym>': massimo 64 argomenti supportati dal marshalling FFI.
FFI '<sym>': firma mista LONG/DOUBLE non supportata dal marshalling FFI (tutti i parametri e il ritorno AS LONG oppure AS DOUBLE).
FFI '<sym>': un parametro array, STRING o BYREF richiede firma INT64 (dichiarare gli altri parametri/il ritorno AS LONG — il puntatore viaggia come intero).
FFI '<sym>': tipo elemento non supportato per il parametro array '<nome>' (solo AS DOUBLE o LONG/QUAD, non INTEGER/BYTE/SINGLE — larghezza nativa a 8 byte).
FFI '<sym>': BYREF di STRING non supportato dal marshalling FFI (passare BYVAL).
FFI '<sym>': tipo non supportato per il parametro BYREF '<nome>' (solo AS DOUBLE o LONG/QUAD, non INTEGER/BYTE/SINGLE — larghezza nativa a 8 byte).
FFI '<sym>': il parametro BYREF '<nome>' richiede una variabile semplice come argomento (non un'espressione).


## 7. Esempio completo

### `libvec_module.c`

long vec_add(long a, long b) { return a + b; }
double vec_scale(double a, double b) { return a * b; }
long vec_isprime(long n) { ... }


### `FfiMathLib.inc`

NAMESPACE FfiMathLib
PUBLIC DECLARE FUNCTION vec_scale LIB "libvec_module" (a AS DOUBLE, b AS DOUBLE) AS DOUBLE
PUBLIC DECLARE FUNCTION ns_add  LIB "libvec_module" ALIAS "vec_add" (a AS LONG, b AS LONG) AS LONG
PUBLIC DECLARE FUNCTION ns_prime LIB "libvec_module" ALIAS "vec_isprime" (n AS LONG) AS LONG
PUBLIC DECLARE SUB vec_bump LIB "libvec_module" (v AS DOUBLE)
PUBLIC DECLARE FUNCTION vec_tick LIB "libvec_module" () AS LONG


### `Main.bas`

IMPORT FfiMathLib
IMPORT FfiMathLib AS FM

SUB Main()
    PRINT vec_scale(3.0, 4.0)          ' 12
    PRINT FM.ns_add(21, 21)            ' 42
    FOR i = 2 TO 20
        IF FM.ns_prime(i) THEN PRINT i ' primi
    NEXT i
    vec_bump(1)
    vec_bump(2)
    PRINT vec_tick()                   ' 2
END SUB


## 8. Backend coverage
| Funzionalità | IL | PCodeVM | VM/C | C99Native |
|---|---|---|---|---|
| DOUBLE | ✅ | ✅ | ✅ | ✅ |
| LONG (int64) | ✅ | ✅ | ✅ | ✅ |
| SUB void | ✅ | ✅ | ✅ | ✅ |
| ALIAS | ✅ | ✅ | ✅ | ✅ |
| NAMESPACE PUBLIC | ✅ | ✅ | ✅ | ✅ |
| IMPORT / IMPORT AS | ✅ | ✅ | ✅ | ✅ |
| **STRING** param+ritorno (2026-09-28) | ✅ | ✅ | ✅ | ✅ |
| **BYREF scalare** (2026-09-28, §1bis) | ✅ | ✅ | ✅ | ✅ |
| BYREF di STRING | ❌ (errore) | ❌ | ❌ | ❌ |
| **array BYREF** (2026-09-28) | ✅ | ✅ | ✅ | ✅ |
| **Struct/UDT per valore, kind uniforme** (2026-09-29, §4quater) | ✅ | ✅ | ✅ | ✅ |
| **Struct/UDT per valore, kind misto sul RITORNO** (2026-09-29, §4quater) | ✅ | ✅ | ✅ | ✅ |
| **Struct/UDT per valore, ANNIDATO** (2026-09-29, §4quater) | ✅ | ✅ | ✅ | ✅ |
| Struct/UDT a kind misto come PARAMETRO | ❌ (errore) | ❌ | ❌ | ❌ |
| Struct/UDT >2 campi | ❌ (errore) | ❌ | ❌ | ❌ |
| **Callback come parametro** (2026-09-29, §4quinquies) | ✅ | ✅ | ✅ | ✅ |
| Callback con firma interna mista LONG/DOUBLE | ❌ (errore) | ❌ | ❌ | ❌ |
| Callback con arità >4 | ❌ (errore) | ❌ | ❌ | ❌ |
| Firme miste | ❌ (errore) | ❌ | ❌ | ❌ |

### Fix parità C99Native DOUBLE (commit `15e050e9d`)

### MiniTests di riferimento (`shared/minitests/ffi/`)
| File | Asse | Oracolo |
|---|---|---|
| `ffi_ns_noalias.bas` | IMPORT completo (chiamata non qualificata) | `a= 6|b= 12` |
| `ffi_ns_alias.bas` | IMPORT ... AS (chiamata qualificata) | `a= 6|b= 12` |
| `ffi_ns_int.bas` | LONG omogeneo (OP_CALL_FFI_T) | `add= 42|prime= 1` |
| `ffi_double.bas` | DOUBLE all-double + nidificazione | `mean= 5.25|sum3= 9.8` |
| `ffi_alias_top.bas` | ALIAS top-level | `scaled= 20` |
| `ffi_sub_void.bas` | SUB void + FUNCTION long | `tick= 3` |
| `ffi_libs_folder.bas` | **drop-in: dylib in `libs/` (auto-discovery)** | `scaled= 20\|add= 42\|prime= 1` |
| `ffi_types_int.bas` | tipi interi INTEGER/QUAD/BYTE (→ int64) | `int= 1\|quad= 1\|byte= 1` |
| `ffi_alias_sub.bas` | ALIAS su DECLARE SUB + zero-arg | `tick= 2` |
| `ffi_multiarg.bas` | arità 4 DOUBLE e 8 LONG | `avg= 5\|sum8= 36` |
| `ffi_array_byref.bas` | **array BYREF** (`buf() AS DOUBLE, n AS LONG`), §4bis | `v0= 0\|v1= 10\|v4= 40` |
| `ffi_array_byref_int.bas` | **array BYREF elemento LONG** in input a FUNCTION (§4bis) | `sum= 15\|sum0= 0` |
| `ffi_array_byref_multi.bas` | **3 array** nella stessa chiamata (`out(i)=a(i)+b(i)`) | `c0= 11\|c1= 22\|c2= 33` |
| `ffi_string.bas` | **STRING** param+ritorno, LONG misto, verifica copy-out (§4ter) | `a=7:alpha\|b=9:beta\|a_after=7:alpha` |
| `ffi_array_byref_quad.bas` | **array BYREF elemento QUAD** (non solo LONG, stessa larghezza) | `sum= 15` |
| `ffi_array_and_string.bas` | **array E STRING nella stessa firma** (entrambi puntatori) | `total= 20` |
| `ffi_byref_scalar_long.bas` | **BYREF scalare elemento LONG** (§1bis), div/mod | `q= 3\|rem= 2` |
| `ffi_byref_scalar_double.bas` | **BYREF scalare elemento DOUBLE** (§1bis), IN/OUT | `pv= 10` |
| `ffi_udt_struct.bas` | **Struct/UDT per valore**, kind uniforme (2 UDT distinti, DOUBLE e LONG), §4quater | `p1.X= 3\|p1.Y= 4\|len= 5\|p3.X= 4\|p3.Y= 5\|i3.A= 11\|i3.B= 22` |
| `ffi_udt_mixed_kind_return.bas` | **Struct/UDT per valore**, kind MISTO sul ritorno (entrambi gli ordini di campo), §4quater | `a.V= 3.5\|a.N= 7\|b.N= 9\|b.V= 2.25` |
| `ffi_struct_nested.bas` | **Struct/UDT per valore, ANNIDATO** (campo il cui tipo è un altro TYPE), parametro E ritorno, §4quater | `o.I.X= 3.5\|o.Y= 7\|r.I.X= 4.5\|r.Y= 8` |
| `ffi_callback.bas` | **Callback come parametro**: arità 1, arità 2, combinato con un array, kind interno diverso dal kind esterno forzato, §4quinquies | `apply= 42\|combine2= 43\|sum_transformed= 30\|apply_i2d= 7` |

## 9. Ordine di ricerca delle librerie e drop-in (`include/` + `libs/`)
| # | Candidato | Note |
|---|---|---|
| 1 | **nome as-is** (path relativo alla CWD, o nome nudo) | `LIB "./NativeVec/libvec_module"` / `LIB "libvec_module"` |
| 2 | **nome + estensione** (`.dylib`/`.so`/`.dll`) | `libvec_module` → `libvec_module.dylib` |
| 3 | **`libs/` risalendo dal CWD** | `<dir>/libs/<nome><ext>` o `<dir>/libs/lib<base><ext>`; nearest-first; max 5 livelli su C# (IL/PCode), 16 sul runtime C (VM/C, C99Native) |
| 4 | **preload pacchetti REQUIRE** (match per basename) | librerie dei pacchetti risolti (`REQUIRE`) |

### Drop-in senza pacchetto

### Risoluzione `include/` (sorgenti del modulo)

### Resources-Native-Libs-SDL

*Fonte: `Resources-Native-Libs-SDL`*


## 1. Architettura: come i binding SDL2-CS caricano le librerie native
| Binding | Nome caricato | File in `~/.gmbasic/libs/<rid>/` richiesto |
|---|---|---|
| SDL2 core | `SDL2.dll` **e** `libSDL2.dll` | `SDL2.dll` + `libSDL2.dll` |
| SDL2_ttf | `libSDL2_ttf.dll` | `libSDL2_ttf.dll` |
| SDL2_image | `libSDL2_image.dll` | `libSDL2_image.dll` |
| SDL2_mixer | `libSDL2_mixer.dll` | `libSDL2_mixer.dll` |
| SDL3 (sdl2-compat/extensioni) | `libSDL3.dylib` | `libSDL3.dylib` |

## 2. Il meccanismo csproj (`NativeLibsPath`)

<NativeLibsPath Condition="$([MSBuild]::IsOSPlatform('OSX')) AND ... == 'Arm64'">..\Resources\libs\osx-arm64</NativeLibsPath>
<ItemGroup Condition="'$(NativeLibsPath)' != ''">
    <None Include="$(NativeLibsPath)/*.dylib" CopyToOutputDirectory="PreserveNewest" Link="%(Filename)%(Extension)" />
    <None Include="$(NativeLibsPath)/*.dll"  CopyToOutputDirectory="PreserveNewest" Link="%(Filename)%(Extension)" />
    <None Include="$(NativeLibsPath)/libSDL3.dylib" CopyToOutputDirectory="PreserveNewest" Link="libSDL3.dylib" />
</ItemGroup>


## 3. Storia dei problemi (cosa è andato storto)

## 4. Soluzione adottata (2026-08-27)

## 5. Come ripristinare le librerie SDL (guida rapida)

D=~/.gmbasic/libs/osx-arm64
cp /opt/homebrew/lib/libSDL2-2.0.0.dylib        "$D/libSDL2.dylib"
cp /opt/homebrew/lib/libSDL2-2.0.0.dylib        "$D/libSDL2.dll"   # copia .dll!
cp /opt/homebrew/lib/libSDL2-2.0.0.dylib        "$D/SDL2.dll"      # copia .dll!
cp /opt/homebrew/lib/libSDL2_ttf-2.0.0.dylib    "$D/libSDL2_ttf.dylib"
cp /opt/homebrew/lib/libSDL2_ttf-2.0.0.dylib    "$D/libSDL2_ttf.dll"
cp /opt/homebrew/lib/libSDL2_image-2.0.0.dylib  "$D/libSDL2_image.dylib"
cp /opt/homebrew/lib/libSDL2_image-2.0.0.dylib  "$D/libSDL2_image.dll"
cp /opt/homebrew/lib/libSDL2_mixer-2.0.0.dylib  "$D/libSDL2_mixer.dylib"
cp /opt/homebrew/lib/libSDL2_mixer-2.0.0.dylib  "$D/libSDL2_mixer.dll"
cp /opt/homebrew/lib/libSDL3.dylib              "$D/libSDL3.dylib"
cp /opt/homebrew/lib/libfreetype.6.dylib        "$D/libfreetype.6.dylib"
cp /opt/homebrew/lib/libharfbuzz.0.dylib        "$D/libharfbuzz.0.dylib"


## 6. Troubleshooting (errori tipici)
| Errore | Causa | Fix |
|---|---|---|
| `dlopen(libSDL2.dll)` / `libSDL2_ttf.dll` non trovato | manca la copia `.dll` in `~/.gmbasic/libs/<rid>/` (non basta il `.dylib`) | §5 |
| `Failed loading SDL3 library` | manca `libSDL3.dylib` | `cp /opt/homebrew/lib/libSDL3.dylib ~/.gmbasic/libs/osx-arm64/` |
| `MSB3030 libSDL3.dylib non trovato` (build csproj) | `~/.gmbasic/libs/<rid>` vuota/manca il file | ripristina `libSDL3.dylib` (il csproj ora è robusto: `Exists(...)`) |
| `~/.gmbasic/libs` sparita / librerie perse | era gitignored | ora versionata: `git checkout -- ~/.gmbasic/libs` |

## 7. Regole d'oro

## Vedi anche

### FFI-Guida-Utente

*Fonte: `FFI-Guida-Utente`*


## 1. Dichiarazione

### `DECLARE FUNCTION`

DECLARE FUNCTION <NomeBASIC> LIB "<libreria>" [ALIAS "<nome_nativo>"] ([param]) AS <TipoRitorno>


### `DECLARE SUB`

DECLARE SUB <NomeBASIC> LIB "<libreria>" [ALIAS "<nome_nativo>"] ([param])


### Parametri

DECLARE FUNCTION <Nome> LIB "<lib>" [ALIAS "<simbolo>"] (<p1> AS <T1>, <p2> AS <T2>, ...) AS <TRitorno>


### Chiamata

## 1bis. BYREF scalare (2026-09-28)

DECLARE FUNCTION VecDivMod LIB "libvec_module" ALIAS "vec_divmod" (a AS LONG, b AS LONG, BYREF r AS LONG) AS LONG
DIM remainder AS LONG
DIM q AS LONG
q = VecDivMod(17, 5, remainder)       ' vec_divmod (native C): long vec_divmod(long a, long b, long* rem)
' q = 3, remainder = 2


## 2. NAMESPACE e IMPORT

### Includerlo in un file `.inc`

' FfiMathLib.inc
NAMESPACE FfiMathLib

PUBLIC DECLARE FUNCTION vec_scale LIB "libvec" (a AS DOUBLE, b AS DOUBLE) AS DOUBLE
PUBLIC DECLARE FUNCTION ns_add LIB "libvec" ALIAS "vec_add" (a AS LONG, b AS LONG) AS LONG


### Import completo — simboli non qualificati

IMPORT FfiMathLib
PRINT vec_scale(3.0, 4.0)   ' ok


### Import con alias — simboli qualificati

IMPORT FfiMathLib AS FM
PRINT FM.vec_scale(3.0, 4.0)  ' ok


## 3. ALIAS

DECLARE FUNCTION ns_add LIB "libvec" ALIAS "vec_add" (a AS LONG, b AS LONG) AS LONG


## 4. Tipi supportati
| Tipo BASIC | Tipo C | Supporto | Note |
|---|---|---|---|
| `DOUBLE` | `double` | ✅ | Contratto all-double |
| `LONG` | `int64_t` / `long` | ✅ | Contratto all-long (OP_CALL_FFI_T) |
| `INTEGER` | `int` | ✅ | Trattato come INT64 |
| `QUAD` | `int64_t` | ✅ | Trattato come INT64 |
| `BYTE` | `int8_t` | ✅ | Trattato come INT64 |
| **`STRING`** (param/ritorno) | `const char*` | ✅ | 2026-09-28, vedi §4ter |
| **`BYREF` scalare** (`AS DOUBLE`/LONG/QUAD/ULONG/UQUAD) | puntatore | ✅ | 2026-09-28, vedi §1bis |
| `BYREF` di STRING | puntatore-a-puntatore | ❌ | Errore compile-time su tutte le backend (char**, fuori scope) |
| **array** (`a() AS DOUBLE`/interi) | puntatore | ✅ | 2026-09-28, vedi §4bis |
| misto LONG+DOUBLE | — | ❌ | Errore compile-time su tutte le backend |

## 4bis. Array BYREF (2026-09-28)

DECLARE SUB VecFill LIB "libvec_module" ALIAS "vec_fill" (buf() AS DOUBLE, n AS LONG)
DIM v(5) AS DOUBLE
VecFill(v, 5)          ' vec_fill (native C): void vec_fill(double* buf, long n)


## 4ter. STRING — parametro e ritorno (2026-09-28)

DECLARE FUNCTION VecLabel LIB "libvec_module" ALIAS "vec_label" (id AS LONG, name AS STRING) AS STRING
DIM s$ = VecLabel(7, "alpha")   ' vec_label (native C): const char* vec_label(long id, const char* name)


### SUB void

## 4quater. Struct/UDT per valore (2026-09-29)

TYPE Point
    X AS DOUBLE
    Y AS DOUBLE
END TYPE

DECLARE FUNCTION VecMakePoint LIB "libvec_module" ALIAS "vec_make_point" (x AS DOUBLE, y AS DOUBLE) AS Point
DECLARE FUNCTION VecPointLen LIB "libvec_module" ALIAS "vec_point_len" (p AS Point) AS DOUBLE
' vec_make_point (native C): vec_point_t vec_make_point(double x, double y)  -- struct { double x, y; }
' vec_point_len  (native C): double vec_point_len(vec_point_t p)

DIM p AS Point
p = VecMakePoint(3, 4)
PRINT p.X; p.Y             ' 3  4
PRINT VecPointLen(p)       ' 5


TYPE Labeled
    V AS DOUBLE
    N AS LONG
END TYPE
DECLARE FUNCTION VecMakeLabeled LIB "libvec_module" ALIAS "vec_make_labeled_dn" (v AS DOUBLE, n AS DOUBLE) AS Labeled
DIM lab AS Labeled
lab = VecMakeLabeled(3.5, 7)
PRINT lab.V; lab.N          ' 3.5  7  -- kind misto sul RITORNO, ammesso


### Struct annidate (2026-09-29)

TYPE Inner
    X AS DOUBLE
END TYPE
TYPE Outer          ' 2 campi FOGLIA totali (Inner.X + Y) → ammissibile
    I AS Inner
    Y AS LONG
END TYPE

DECLARE FUNCTION VecNestedRoundtrip LIB "libvec_module" ALIAS "vec_nested_roundtrip" (o AS Outer) AS Outer

DIM o AS Outer
o.I.X = 3.5 : o.Y = 7
DIM r AS Outer
r = VecNestedRoundtrip(o)
PRINT r.I.X; r.Y            ' 4.5  8


## 4quinquies. Callback come parametro (2026-09-29)

DECLARE FUNCTION VecApply LIB "libvec_module" ALIAS "vec_cb_apply" (cb AS FUNCTION(x AS LONG) AS LONG, x AS LONG) AS LONG
' native C: long vec_cb_apply(long (*cb)(long), long x) { return cb(x); }

DIM r AS LONG
r = VecApply(FN(x AS LONG) AS LONG => x * 2, 21)
PRINT r                    ' 42 -- la lambda e' stata invocata DAL nativo, non da BASIC


## 4sexies. Firma mista LONG/DOUBLE fra scalari semplici (2026-10)

DECLARE FUNCTION VecMixed LIB "libvec_module" ALIAS "vec_mixed" (id AS LONG, price AS DOUBLE, qty AS LONG) AS DOUBLE
' native C: double vec_mixed(long id, double price, long qty) { return price * qty; }
PRINT VecMixed(7, 2.5, 7)   ' 17.5


## 5. Limiti

## 6. Messaggi errore esatti (identici su 4 backend)

FFI '<sym>': massimo 64 argomenti supportati dal marshalling FFI.
FFI '<sym>': firma mista LONG/DOUBLE non supportata dal marshalling FFI (tutti i parametri e il ritorno AS LONG oppure AS DOUBLE).
FFI '<sym>': un parametro array, STRING o BYREF richiede firma INT64 (dichiarare gli altri parametri/il ritorno AS LONG — il puntatore viaggia come intero).
FFI '<sym>': tipo elemento non supportato per il parametro array '<nome>' (solo AS DOUBLE o LONG/QUAD, non INTEGER/BYTE/SINGLE — larghezza nativa a 8 byte).
FFI '<sym>': BYREF di STRING non supportato dal marshalling FFI (passare BYVAL).
FFI '<sym>': tipo non supportato per il parametro BYREF '<nome>' (solo AS DOUBLE o LONG/QUAD, non INTEGER/BYTE/SINGLE — larghezza nativa a 8 byte).
FFI '<sym>': il parametro BYREF '<nome>' richiede una variabile semplice come argomento (non un'espressione).


## 7. Esempio completo

### `libvec_module.c`

long vec_add(long a, long b) { return a + b; }
double vec_scale(double a, double b) { return a * b; }
long vec_isprime(long n) { ... }


### `FfiMathLib.inc`

NAMESPACE FfiMathLib
PUBLIC DECLARE FUNCTION vec_scale LIB "libvec_module" (a AS DOUBLE, b AS DOUBLE) AS DOUBLE
PUBLIC DECLARE FUNCTION ns_add  LIB "libvec_module" ALIAS "vec_add" (a AS LONG, b AS LONG) AS LONG
PUBLIC DECLARE FUNCTION ns_prime LIB "libvec_module" ALIAS "vec_isprime" (n AS LONG) AS LONG
PUBLIC DECLARE SUB vec_bump LIB "libvec_module" (v AS DOUBLE)
PUBLIC DECLARE FUNCTION vec_tick LIB "libvec_module" () AS LONG


### `Main.bas`

IMPORT FfiMathLib
IMPORT FfiMathLib AS FM

SUB Main()
    PRINT vec_scale(3.0, 4.0)          ' 12
    PRINT FM.ns_add(21, 21)            ' 42
    FOR i = 2 TO 20
        IF FM.ns_prime(i) THEN PRINT i ' primi
    NEXT i
    vec_bump(1)
    vec_bump(2)
    PRINT vec_tick()                   ' 2
END SUB


## 8. Backend coverage
| Funzionalità | IL | PCodeVM | VM/C | C99Native |
|---|---|---|---|---|
| DOUBLE | ✅ | ✅ | ✅ | ✅ |
| LONG (int64) | ✅ | ✅ | ✅ | ✅ |
| SUB void | ✅ | ✅ | ✅ | ✅ |
| ALIAS | ✅ | ✅ | ✅ | ✅ |
| NAMESPACE PUBLIC | ✅ | ✅ | ✅ | ✅ |
| IMPORT / IMPORT AS | ✅ | ✅ | ✅ | ✅ |
| **STRING** param+ritorno (2026-09-28) | ✅ | ✅ | ✅ | ✅ |
| **BYREF scalare** (2026-09-28, §1bis) | ✅ | ✅ | ✅ | ✅ |
| BYREF di STRING | ❌ (errore) | ❌ | ❌ | ❌ |
| **array BYREF** (2026-09-28) | ✅ | ✅ | ✅ | ✅ |
| **Struct/UDT per valore, kind uniforme** (2026-09-29, §4quater) | ✅ | ✅ | ✅ | ✅ |
| **Struct/UDT per valore, kind misto sul RITORNO** (2026-09-29, §4quater) | ✅ | ✅ | ✅ | ✅ |
| **Struct/UDT per valore, ANNIDATO** (2026-09-29, §4quater) | ✅ | ✅ | ✅ | ✅ |
| Struct/UDT a kind misto come PARAMETRO | ❌ (errore) | ❌ | ❌ | ❌ |
| Struct/UDT >2 campi | ❌ (errore) | ❌ | ❌ | ❌ |
| **Callback come parametro** (2026-09-29, §4quinquies) | ✅ | ✅ | ✅ | ✅ |
| Callback con firma interna mista LONG/DOUBLE | ❌ (errore) | ❌ | ❌ | ❌ |
| Callback con arità >4 | ❌ (errore) | ❌ | ❌ | ❌ |
| Firme miste | ❌ (errore) | ❌ | ❌ | ❌ |

### Fix parità C99Native DOUBLE (commit `15e050e9d`)

### MiniTests di riferimento (`shared/minitests/ffi/`)
| File | Asse | Oracolo |
|---|---|---|
| `ffi_ns_noalias.bas` | IMPORT completo (chiamata non qualificata) | `a= 6|b= 12` |
| `ffi_ns_alias.bas` | IMPORT ... AS (chiamata qualificata) | `a= 6|b= 12` |
| `ffi_ns_int.bas` | LONG omogeneo (OP_CALL_FFI_T) | `add= 42|prime= 1` |
| `ffi_double.bas` | DOUBLE all-double + nidificazione | `mean= 5.25|sum3= 9.8` |
| `ffi_alias_top.bas` | ALIAS top-level | `scaled= 20` |
| `ffi_sub_void.bas` | SUB void + FUNCTION long | `tick= 3` |
| `ffi_libs_folder.bas` | **drop-in: dylib in `libs/` (auto-discovery)** | `scaled= 20\|add= 42\|prime= 1` |
| `ffi_types_int.bas` | tipi interi INTEGER/QUAD/BYTE (→ int64) | `int= 1\|quad= 1\|byte= 1` |
| `ffi_alias_sub.bas` | ALIAS su DECLARE SUB + zero-arg | `tick= 2` |
| `ffi_multiarg.bas` | arità 4 DOUBLE e 8 LONG | `avg= 5\|sum8= 36` |
| `ffi_array_byref.bas` | **array BYREF** (`buf() AS DOUBLE, n AS LONG`), §4bis | `v0= 0\|v1= 10\|v4= 40` |
| `ffi_array_byref_int.bas` | **array BYREF elemento LONG** in input a FUNCTION (§4bis) | `sum= 15\|sum0= 0` |
| `ffi_array_byref_multi.bas` | **3 array** nella stessa chiamata (`out(i)=a(i)+b(i)`) | `c0= 11\|c1= 22\|c2= 33` |
| `ffi_string.bas` | **STRING** param+ritorno, LONG misto, verifica copy-out (§4ter) | `a=7:alpha\|b=9:beta\|a_after=7:alpha` |
| `ffi_array_byref_quad.bas` | **array BYREF elemento QUAD** (non solo LONG, stessa larghezza) | `sum= 15` |
| `ffi_array_and_string.bas` | **array E STRING nella stessa firma** (entrambi puntatori) | `total= 20` |
| `ffi_byref_scalar_long.bas` | **BYREF scalare elemento LONG** (§1bis), div/mod | `q= 3\|rem= 2` |
| `ffi_byref_scalar_double.bas` | **BYREF scalare elemento DOUBLE** (§1bis), IN/OUT | `pv= 10` |
| `ffi_udt_struct.bas` | **Struct/UDT per valore**, kind uniforme (2 UDT distinti, DOUBLE e LONG), §4quater | `p1.X= 3\|p1.Y= 4\|len= 5\|p3.X= 4\|p3.Y= 5\|i3.A= 11\|i3.B= 22` |
| `ffi_udt_mixed_kind_return.bas` | **Struct/UDT per valore**, kind MISTO sul ritorno (entrambi gli ordini di campo), §4quater | `a.V= 3.5\|a.N= 7\|b.N= 9\|b.V= 2.25` |
| `ffi_struct_nested.bas` | **Struct/UDT per valore, ANNIDATO** (campo il cui tipo è un altro TYPE), parametro E ritorno, §4quater | `o.I.X= 3.5\|o.Y= 7\|r.I.X= 4.5\|r.Y= 8` |
| `ffi_callback.bas` | **Callback come parametro**: arità 1, arità 2, combinato con un array, kind interno diverso dal kind esterno forzato, §4quinquies | `apply= 42\|combine2= 43\|sum_transformed= 30\|apply_i2d= 7` |

## 9. Ordine di ricerca delle librerie e drop-in (`include/` + `libs/`)
| # | Candidato | Note |
|---|---|---|
| 1 | **nome as-is** (path relativo alla CWD, o nome nudo) | `LIB "./NativeVec/libvec_module"` / `LIB "libvec_module"` |
| 2 | **nome + estensione** (`.dylib`/`.so`/`.dll`) | `libvec_module` → `libvec_module.dylib` |
| 3 | **`libs/` risalendo dal CWD** | `<dir>/libs/<nome><ext>` o `<dir>/libs/lib<base><ext>`; nearest-first; max 5 livelli su C# (IL/PCode), 16 sul runtime C (VM/C, C99Native) |
| 4 | **preload pacchetti REQUIRE** (match per basename) | librerie dei pacchetti risolti (`REQUIRE`) |

### Drop-in senza pacchetto

### Risoluzione `include/` (sorgenti del modulo)

<!-- END SURFACE -->
