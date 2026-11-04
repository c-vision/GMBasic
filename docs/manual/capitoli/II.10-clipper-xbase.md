# II.10 — Compatibilità Clipper/xBase

## Cosa ottieni da questo capitolo

Le forme che rendono eseguibile il codice scritto per Clipper 5.3: l'operatore macro `&`, la
sostituzione di testo nelle stringhe, `EVAL`, i code block e lo scope dinamico. Il criterio è
dichiarato: **più flessibile, mai meno**.

## 1. L'operatore macro `&`

Compila ed esegue a runtime il testo contenuto in una variabile carattere.

| Forma | Parametro | Significato |
|---|---|---|
| `&cVar` | variabile carattere | compila ed esegue il **contenuto** |
| `&cVar.` | con punto finale | il punto separa la macro dal testo adiacente (`&cVar.suffix`) |
| `&(cExpr)` | espressione carattere | valuta l'espressione, poi compila ed esegue il risultato |
| `&("2+3")` | costante | compilata **una volta** a tempo di compilazione |

```basic
DIM cVar AS STRING = "X"
DIM cExpr AS STRING = "X * 2 + 1"
PRINT &cVar          ' il valore di X
PRINT &(cExpr)       ' X*2+1 compilato ed eseguito
```

Il testo compilato può essere un'**espressione** o un **comando**: `&("PRINT 42")`,
`&("X = X + 5")`.

## 2. Sostituzione di testo nelle stringhe

```basic
cName = "world"          ' variabile di modulo (globale)
PRINT "Hello &cName!"    ' Hello world!
```

La regola Clipper è precisa e va rispettata: si espandono **solo** le variabili globali di modulo
(in terminologia Clipper, `PRIVATE`/`PUBLIC`). Le locali, le `STATIC`, i campi e gli elementi di
array restano **testo letterale**. Il punto termina la sostituzione: `"&cName.suffix"`.

## 3. Macro annidate

Il contenuto di una macro può contenere altre macro: l'espansione è ricorsiva.

```basic
DIM cThree AS STRING = "hello"
DIM cTwo AS STRING = "cThree"
DIM cOne AS STRING = "&cTwo"
PRINT &cOne           ' hello
```

## 4. `EVAL` decide dal tipo del primo argomento

| Forma | Comportamento |
|---|---|
| `EVAL(cStringa)` | macro-evaluator: compila ed esegue il testo, ritorna il risultato |
| `EVAL(bBlocco)` | invoca il code block senza argomenti |
| `EVAL(bBlocco, a1, a2, …)` | invoca il blocco con gli argomenti |

```basic
PRINT EVAL("2+3")                  ' 5
DIM b = { |x| RETURN x + 1 }
PRINT EVAL(b, 5)                   ' 6
```

`EVAL(<stringa>)` **non** accetta argomenti aggiuntivi: gli argomenti appartengono
all'invocazione di un blocco.

## 5. Code block

```basic
DIM area = { |w, h| RETURN w * h }   ' corpo a statement
DIM inc  = { |x| x + 1 }             ' corpo a espressione: vale l'ultima espressione
DIM k    = { || 3.14 }               ' zero parametri

PRINT area(6, 7)    ' 42
PRINT inc(5)        ' 6
```

- I parametri si dichiarano fra barre verticali; il tipo è opzionale (`|s AS STRING|`) e, se
  assente, è inferito dall'uso nel corpo.
- Un corpo a espressione ritorna il valore dell'ultima espressione, come in Clipper.
- Un blocco può catturare variabili del chiamante.
- `AEVAL array, { |v, i| … }` passa **elemento e indice 0-based**.

## 6. Scope dinamico

`DYNAMIC` è la terzo pilastro della compatibilità: la visibilità segue la catena di chiamate, non il
blocco. Cascata, shadowing e ripristino al ritorno sono descritti in II.5 §6.

## 7. Esempio eseguito

`shared/samples/Clipper/ClipperMacroEvalBlocksExtreme.bas`, backend C99Native:

```
net= 122
tax= 0.22
gross(100)= 122
gross(200)= 244
vargross(150)= 183
area= 42
bang=X!
raw=x!
  gross[ 0 ]= 122
  gross[ 1 ]= 244
  gross[ 2 ]= 366
nested=deep!
tax2= 0.1
subst=Hello &cName!
inline=X!
fromfn= 6
infvar= 6
stmt=y
capt= 6
param= 42
```

Le righe che spiegano le forme del capitolo:

- `net= 122` è `&(cNet)` con `cNet = "VAL(cPrice) * (1 + TaxRate)"`: la macro legge una globale
  (`TaxRate = 0.22`) e una locale via `VAL`.
- `tax= 0.22` è `&which`, dove `which` contiene il **nome** della variabile: il nome stava nei
  dati, non nel codice.
- `gross(100)= 122` e `gross(200)= 244` sono `EVAL` su un blocco prodotto da una macro costante:
  compilato una volta, invocato con argomenti diversi.
- `vargross(150)= 183` è un blocco il cui **testo** veniva da una variabile: l'espansione a tempo
  di compilazione funziona quando il contenuto è noto.
- `gross[ 0 ]= 122` … `gross[ 2 ]= 366` sono `AEVAL` con `(elemento, indice)`.
- `nested=deep!` è la macro annidata `&cOne` → `cTwo` → `cThree`.
- `subst=Hello &cName!` mostra la **sostituzione mancata**: `cName` è dichiarata `GLOBAL` ma la
  stringa è costruita in un contesto in cui la sostituzione non si applica, e il testo resta
  letterale. È il comportamento ortodosso, non un errore: la regola di §2 è restrittiva apposta.

## 8. Limiti dichiarati

- Le stringhe costruite **dinamicamente** a runtime (concatenate, lette da file) non sono
  espandibili dai transpiler C: restano il residuo documentato della compatibilità.
- Un errore dentro il testo compilato non abortisce il programma: viene reso come stringa
  `MACRO ERROR: <messaggio>`.

## 9. Errori frequenti

| Sintomo | Causa | Correzione |
|---|---|---|
| `&` usato per concatenare | `&` è la macro, non un operatore binario | usa `+` |
| la sostituzione non avviene | la variabile è locale, `STATIC` o campo | porta il valore in una globale, o costruisci con `+` |
| `EVAL("x", 1)` errore | la forma stringa non prende argomenti | invoca un blocco, non una stringa |
| `MACRO ERROR: …` stampato | il testo compilato non è valido | correggi la stringa; l'errore è nel dato, non nel programma |
| un blocco ritorna `NIL` | il corpo non ha un'ultima espressione | aggiungi `RETURN <valore>` |

## 10. Dove andare adesso

- **II.5** — `DYNAMIC` e lo scope dinamico.
- **II.9** — `AEVAL`, `AMAP`, `AFILTER` sui code block.
- **IV.23** — comandi DBF in sintassi legacy (`DBF USE`, campi, aree di lavoro).
- **II.11** — come si organizza il codice Clipper in moduli.



<!-- BEGIN SURFACE (generato da tools/sh/manual_surface.sh — non modificare a mano) -->

## Riferimento completo: comandi, funzioni, parametri

> Sezione generata dalle guide del progetto per questo capitolo: contiene **tutte**
> le superfici documentate, non una selezione. La prosa dei capitoli precedenti le
> spiega; qui stanno i dettagli — parametri, varianti, esempi d'uso.

### Clipper-Macro-Eval-Blocks-Compat

*Fonte: `Clipper-Macro-Eval-Blocks-Compat`*


## 1. Semantica Clipper 5.3 (requisiti)

### Macro operator `&` (compila ESPRESSIONI)

### Code block `{ |parametri| corpo }`

## 1bis. Parametri e risultati (GMBasic)

### Macro `&` — parametri
| Forma | Parametro | Semantica |
|---|---|---|
| `&<cVar>` | variabile carattere | compila+esegue il contenuto; `.` termina la macro |
| `&(<cExpr>)` | espressione che produce stringa | valutata prima, poi compilata+eseguita |
| `"…&cVar…"` | text substitution | solo PRIVATE/PUBLIC (globale/modulo); local = testo letterale |
| `&cOne` con `cOne="&cTwo"` | annidamento | catena risolta fino al valore finale |

### Code block `{ |p1[, p2, …]| corpo }` — parametri e risultato
| Aspetto | Regola |
|---|---|
| Parametri | tra `\|…\|`; 0..N; tipo inferito dall'uso (call, concat con stringa) o `AS TIPO` |
| Corpo | statement; **valore di ritorno = ultima espressione** oppure `RETURN` esplicito |
| Risultato | numerico o **stringa** (concat `+` con operando stringa ⇒ risultato stringa) |
| Invocazione | `EVAL(b, args...)` oppure diretta `b(args...)` |
| Ritorno implicito | `{ \|\| expr }` = 0 parametri, ritorna `expr` |

### `EVAL` — parametri
| Forma | Parametri | Risultato |
|---|---|---|
| `EVAL(bBlock [, args...])` | block + 0..N argomenti, legati **posizionalmente** ai parametri | valore del block (numero/stringa) |
| `EVAL({ \|p\| ... } [, args...])` | block **inline** (anche 0-arg, con catture); i tipi dei parametri sono inferiti anche dagli **argomenti del call-site** | valore del block; usabile in espressione, assegnazione e statement |
| `EVAL(<fn>, args...)` / `EVAL(Mk(), args...)` | block **restituito da funzione** (`FUNCTION Mk() AS FUNCTION(x) AS T`): firma dal FunctionPointerType di ritorno; anche `DIM b = Mk()` (firma inferita) | valore del block |
| `EVAL("espressione")` | stringa macro (GMBasic) | valore dell'espressione; se è un comando (`"X = 42"`) lo esegue e ritorna il valore assegnato |

### `AEVAL` — parametri
| Forma | Parametri del block | Note |
|---|---|---|
| `AEVAL array, block` | **arity 1**: `{ \|elem\| … }` — solo elemento | comando, senza parentesi |
| | **arity 2**: `{ \|elem, idx\| … }` — elemento + **indice 0-based** | `nStart`/`nCount` Clipper NON supportati in GMBasic |

DIM v = {10, 20, 30}
DIM W = 0
AEVAL v, { |n, i| W := W + n * (i + 1) }   ' 10*1+20*2+30*3 = 140
DIM s$ = {"a", "b", "c"}
DIM cat AS STRING = ""
AEVAL s$, { |e| cat := cat + e }            ' abc


## 2. Stato GMBasic (implementazione)
| # | Feature Clipper | Implementazione |
|---|---|---|
| D1 | `EVAL(bBlock, args...)` | Nodo `EvalExpr` + dispatch per tipo nel Linker (stringa→`MacroExpr`, function pointer→`FunctionExpr` indiretto, Any/Inferred→runtime); parser a N argomenti; param/ritorno **stringa** inferiti anche da concat `+` con operando stringa (fix 2026-10-02) |
| D2 | Text substitution `"…&cVar…"` | C# `RewriteTextSubstitution` → `InterpolatedStringExpr`; C99Native concat in `emit_expr` (solo globali/modulo: PRIVATE/PUBLIC Clipper) |
| D3 | Macro annidate `&cOne` | Catena provider eval annidata: prima i locali del chiamante, poi i globali (IL/PCode); C99* via `gmb_expr` `eval_nested` |
| D4 | Code block a corpo-espressione `{ \|\| expr }` | Parser statement-first + fallback espressione = RETURN implicito (un `=` a livello statement resta assegnazione), C# e C99Native |
| D5 | `&("{ \|x\| … }")` macro→block | Espansione a compile-time: forma costante (parser) e variabile con contenuto costante (Linker `RewriteMacroExpr` + parser nativo) → lambda tipizzata 5/5; `EVAL(b, args)` indiretto |
| D6 | `AEVAL` 2-param (elemento, indice) | Arità del block come 3° argomento di AEVAL; PCode/VM-C indice solo ai block 2+; C99Native firma 2-arg; C99Native loop con arità; **indice 0-based**, niente `nStart`/`nCount` in GMBasic |
| D7 | eval-block (scope locali) | Snapshot per-sito macro + inferenza parametri lambda dall'uso nel corpo |
| D8 | `&cVar` (macro variabile) | ✅ (regressione) |
| D9 | `&(expr)` | ✅ (regressione) |

### Verifica

## 3. Batteria e test

### Clipper-Macro-Eval-Blocks-Guida-Utente

*Fonte: `Clipper-Macro-Eval-Blocks-Guida-Utente`*


## 1. Operatore macro `&`
| forma | parametro | significato |
|---|---|---|
| `&cVar` | `cVar` = variabile **carattere** | compila+esegue il **contenuto** di `cVar` |
| `&cVar.` | `.` = terminatore di macro | separa la macro dal testo adiacente (`&cVar.suffix`) |
| `&(cExpr)` | `cExpr` = espressione **carattere** | valuta `cExpr`, poi compila+esegue la stringa risultante (permette campi/elementi d'array) |

DIM cVar AS STRING = "X"
DIM cExpr AS STRING = "X * 2 + 1"
PRINT &cVar              ' valore di X
PRINT &(cExpr)           ' X*2+1 compilato ed eseguito
PRINT &("2+3")           ' 5  (macro costante: compilata una sola volta)


## 2. Sostituzione di testo nelle stringhe

cName = "world"          ' variabile GLOBALE/modulo
PRINT "Hello &cName!"    ' Hello world!


## 3. Macro annidate

DIM cThree AS STRING = "hello"
DIM cTwo AS STRING = "cThree"
DIM cOne AS STRING = "&cTwo"
PRINT &cOne              ' hello (cOne → "&cTwo" → cThree → "hello")


## 4. `EVAL` — macro-evaluator e invocazione di code block
| forma | primo argomento | comportamento |
|---|---|---|
| `EVAL(cString)` | **stringa** | macro-evaluator: compila+esegue il testo e ritorna il risultato (come `&(cString)`) |
| `EVAL(bBlock)` | **code block** | invoca il block senza argomenti e ritorna il suo valore |
| `EVAL(bBlock, arg1, arg2, …)` | **code block** | invoca il block con gli argomenti (Clipper: `EVAL(bBlock [, args...])`) |

DIM b = { |x| RETURN x + 1 }
PRINT EVAL(b, 5)         ' 6
PRINT EVAL("2+3")        ' 5
DIM b0 = { || 3.14 }
PRINT EVAL(b0)           ' 3.14


## 5. Code block `{ |parametri| corpo }`

DIM area = { |w, h| RETURN w * h }   ' corpo a statement
DIM inc  = { |x| x + 1 }             ' corpo a espressione: valore = ultima espressione
DIM k    = { || 3.14 }               ' zero parametri
PRINT area(6, 7)                     ' 42
PRINT inc(5)                         ' 6
PRINT k()                            ' 3.14

| elemento | sintassi | note |
|---|---|---|
| parametri | `\|a, b\|` | zero o più, separati da virgola; `AS <tipo>` opzionale (`\|s AS STRING\|`) |
| corpo | statement (`RETURN`/`IF`/assegnazioni/…) | `RETURN <expr>` esplicito |
| corpo a espressione | `{ \|x\| x + 1 }` | il valore di ritorno è **l'ultima espressione** (parità Clipper) |
| tipo parametri | inferito dall'uso nel corpo | es. `UPPER(s)` → `s` è STRINGA |
| assegnazione a variabile | `DIM b = { … }` | il tipo è un puntatore a funzione |
| ritorno da funzione / annidamento | sì | i block possono catturare variabili del chiamante |

## 6. `AEVAL` con code block

DIM nums = {10, 20, 30}
AEVAL nums, { |v, i| PRINT "indice="; i; " valore="; v }   ' i = 0,1,2 (0-based)


## 7. Errori e diagnostica

## 8. Verifica (4 backend)
| file | contenuto | esito |
|---|---|---|
| `shared/minitests/clipper/macro_core.bas` | `&`, macro annidate, text substitution | PASS 5/5 |
| `shared/minitests/clipper/eval_block.bas` | `EVAL(block, args)`, macro→block | PASS 5/5 |
| `shared/minitests/clipper/eval_macro_string.bas` | `EVAL("expr")`, assegnazione | PASS 5/5 |
| `shared/minitests/clipper/macro_var_block.bas` | macro su variabile → block | PASS 5/5 |
| `shared/samples/Clipper/ClipperMacroEvalBlocksExtreme.bas` | sample estremo (tutte le forme) | PASS 5/5 |
| `shared/samples/Clipper/ClipperCodeBlocks.bas` | AEVAL/AMAP/… + block | PASS 5/5 |

## Vedi anche

### ClipperLib-Clipper-5.3-Reference

*Fonte: `ClipperLib-Clipper-5.3-Reference`*


EventLoop (UiKey / UiMouse / UiResize)
   ├─ IInputSource (console RawInputReader | telnet TelnetInputSource | test KeySource)
   ├─ BlinkClock (BLINK_MS = 400) → ClipperState.BlinkOn + Control.BlinkOn
   ├─ legacy loops: ClipperRead.Read(), ClipperInput.Accept(), MEMOEDIT
   ├─ Controls EventHandler / FocusManager
   └─ IApp sessions (ModalHost nesting: IApp ⇄ ClipperForm)


## Namespaces
| Namespace | Content |
|---|---|
| `ClipperLib.Clipper` | Clipper 5.3 functions (`ClipperOutput`, `ClipperInput`, `ClipperRead`, `ClipperSet`, `ClipperDates`, `ClipperStrings`, `ClipperNumbers`, `ClipperScrn`, `ClipperPrinter`, `ClipperMouse`, `ClipperGlobal`, `FormatPicture`, `Get`, `GetBuilder`, `KeyConstants`) |
| `ClipperLib.App` | `EventLoop`, `ModalHost`, `ConsoleRenderer`, `IInputSource`, `UiEvent`, `MouseAction`, `AppRunner` |

## `ClipperSet` — SET functions
| Function | Clipper | Description |
|---|---|---|
| `bool SetConfirm(bool? on)` | SET CONFIRM | Enter on the last GET field must be confirmed (beep) instead of exiting |
| `bool SetWrap(bool? on)` | SET WRAP | Cursor wraps around at field start/end |
| `string SetDelimiters(string?)` | SET DELIMITERS | Two delimiters drawn around the active GET; a single char is doubled, longer strings truncate to 2 |
| `bool SetEscape(bool? on)` | SET ESCAPE | ESC terminates READ (default true) |
| `bool SetBell(bool? on)` | SET BELL | Invalid key / rejected input beeps |
| `int SetTypeahead(int?)` | SET TYPEAHEAD | Typeahead buffer size (0 disables; negative clamps to 0) |
| `bool SetIntensity(bool? on)` | SET INTENSITY | Bright colors |
| `string SetDateFormat(string?)` | SET DATE FORMAT | Date format used by DTOC/CTOD (`dd/mm/yyyy`, `mm-dd-yy`, …) |
| `bool SetCentury(bool? on)` | SET CENTURY | 2- or 4-digit year output |
| `int SetEpoch(int?)` | SET EPOCH | 100-year window for 2-digit year parsing |
| `int SetDecimals(int?)` | SET DECIMALS | Default decimal places for STR/transform |
| `bool SetFixed(bool? on)` | SET FIXED | Fixed vs scientific numeric display |
| `bool SetPrinter(bool? on)` | SET PRINTER | Echo output to the printer buffer |
| `string SetDevice(string?)` | SET DEVICE | `"SCREEN"` or `"PRINTER"` |
| `int? SetMessage(int?)` | SET MESSAGE | Message row (0 disables); `Message()` reads the last `PROMPT` text |

## `ClipperOutput` — SAY/BOX/output
| Function | Description |
|---|---|
| `SetColor(spec)` / `SetColorTo(spec)` | Set fg/bg from a spec like `"W+/B"`, `"R+/W"` (see `ColorParser`) |
| `Say(row, col, expr)` | Write text at fixed position, updates `Row()`/`Col()` |
| `SayPic(row, col, expr, picture)` | `Say` with a PICTURE applied |
| `SayColor(row, col, expr, color)` | `Say` with an ad-hoc color |
| `DevOut(s)` / `DispOut(s)` / `QQOut(s)` | Write at current `Row()`/`Col()`; also goes to the printer when `SET PRINTER ON` |
| `DevOutPic(s, pic)` | `DevOut` with a PICTURE |
| `QOut(s)` | Write at current position and advance the row |
| `TextBlock(s)` | Split on `\n` and `QOut` each line |
| `Box(top, left, bottom, right, style)` / `DispBox(...)` | Draw a box; `style` is 9 border chars or a keyword (`SINGLE`/`DOUBLE`) |
| `ClearScreen()` / `ClearRegion(top, left, bottom, right)` | Blank the buffer (or a region) |
| `Beep()` | Console beep (silent where unsupported); only when `SET BELL ON` |
| `Render()` | Push the `ScreenBuffer` through the current `RenderHook` |
| `SetCursor(bool)` / `GetCursor()` | Show/hide the blinking cell cursor |

## `ClipperInput` — INKEY/NEXTKEY/ACCEPT
| Function | Description |
|---|---|
| `int InKey()` | Next key (blocks); returns `KeyConstants` values or `0` on timeout |
| `int InKeyTimeout(int ms)` | Like `InKey` but returns after `ms` (0 = poll) |
| `(int key, KeyModifiers) InKeyTimeoutEx(int ms)` | Also returns modifiers |
| `int NextKey()` | Peek at the next key **without consuming** it |
| `int LastKey()` / `int ReadKey()` | Last key read / last key that ended a READ |
| `void Keyboard(string s)` | Inject keystrokes into the typeahead buffer |
| `void SetKey(int n, Action proc)` | SET KEY — run `proc` when key `n` is pressed |
| `bool Accept(ref string buf)` | INKEY-line: append printable chars, Backspace deletes, Enter/ESC return |
| `SetKeySource(Func<int>)` / `ClearKeySource()` | Inject a synthetic key source (used by the headless test harness) |
| `SetRenderHook(Action<ScreenBuffer>)` | Swap the transport renderer |
| `EnableMouseMotion()` | Enter SGR mouse tracking **+ motion** (`?1003h`); demos that only need clicks use button tracking, enabled automatically on `InKey` |
| `DisableMouseCapture()` | Leave SGR mouse tracking mode |

## `ClipperRead` + `Get` — GET/READ system

### Setup

ClipperRead.AddGet(new Get { Row = 5, Col = 10, Value = nome, Picture = "@! XXXXXXXXXX",
    Color = "W/B", FocusColor = "W+/B", ErrorColor = "R+/W",
    Valid = s => s.Length >= 3, Apply = s => nome = s });
ClipperRead.AddGet(GetBuilder.ForValue(5, 30, ref qty).Range(1, 999).Width(5).Build());
ClipperRead.DrawGets();   // draw the fields and their delimiters
var exit = ClipperRead.Read();  // block until ESC (6) or final Enter (0)

| Function | Description |
|---|---|
| `AddGet(Get)` | Register a field |
| `Read()` | Edit loop; returns `0` (Enter) or `6` (ESC) |
| `ResetGets()` | Clear the field list and READ state |
| `DrawGets()` | Draw all fields (delimiters, colors, blink cursor) |
| `GetActive()` / `GetAt(col, row)` | Current / hit-test field index |
| `Updated()` / `ReadExit()` | `UPDATED()` / `READEXIT()` after the READ |
| `ApplyAll()` | Apply every field (used when READ ends) |
| `IsValid(Get)` | Combined `Valid` + `Range` predicate (used for live coloring) |

### `Get` properties
| Property | Description |
|---|---|
| `Row`, `Col`, `Value` | Position and raw value (for `@R` pictures the literals are not stored) |
| `Picture` | PICTURE template (see grammar below) |
| `Width`, `Fill` | Display width and blank fill char |
| `Valid` | `Func<string, bool>` — validation checked on every redraw and on Enter |
| `Range` | `(double lo, double hi)` — numeric min/max (empty = `0`, so a positive min flags empty fields) |
| `AcceptKey` | `Func<char, bool>` — **blocks the key in the keypress**; if false the char is never inserted |
| `Rejected` | `Action<char>` — fired on every keypress rejection (PICTURE template, `@A`, `AcceptKey`) |
| `Color`, `FocusColor`, `ErrorColor` | Normal / focused / invalid colors (error color is the "email" red state) |
| `When` | `Func<bool>` — skip the field while the predicate is false |
| `Apply` | `Action<string>` — writes the final value back (called on Enter and ApplyAll) |
| `Cur`, `Sel`, `Changed`, `Updated` | Cursor position, selection, and change flags |
| `ScrollPos` | Scroll window position for `@S` fields |

### Editing keys
| Key | Action |
|---|---|
| printable | Insert/overtype at the cursor; **the PICTURE template and `AcceptKey` are enforced at keypress** |
| `Enter` | Next field; on the last field, exits (or beeps if `SET CONFIRM ON`). A field failing `Valid`/`Range` refuses to advance (beep) |
| `ESC` | Terminate READ (`SET ESCAPE ON`; `Apply` is **not** called, but the typed text stays in `Get.Value`) |
| `Backspace` / `Delete` | Delete left / under cursor; a selection is deleted as a block |
| `←` / `→` | Move cursor (wraps with `SET WRAP ON`) |
| `Home` / `End` | Jump to start / end |
| `Tab` / `↓` · `Shift+Tab` / `↑` | Next / previous field |
| mouse click | Focus the clicked field (`GetAt` hit-testing) |

### Validation model

## PICTURE — `FormatPicture`

### `@`-functions
| Function | Meaning |
|---|---|
| `@A` | alpha only (letters); enforced at keypress in GET |
| `@B` | left-justify numbers |
| `@C` | `CR` after positive numbers |
| `@D` | date in `SET DATE FORMAT` |
| `@E` | British date `dd/mm/yy` |
| `@F` | European numbers (`,` decimal, `.` thousands) |
| `@I` | insert mode in GET |
| `@J` / `@N` | right-justify |
| `@K` | smart cursor |
| `@L<c>` | numeric fill character |
| `@R` | literal-insert: template literals are auto-inserted and **not** part of the stored value |
| `@S<n>` | scroll field with a window of `n` cells |
| `@T` | trim leading/trailing spaces |
| `@X` | `DB` after negative numbers |
| `@Z` | blank when zero |
| `@(` / `@)` | negatives in parentheses (with/without leading space) |
| `@$` | floating `$` |
| `@!` | uppercase |

### Template symbols
| Symbol | Matches |
|---|---|
| `A` | letter only |
| `N` | letter or digit |
| `X` | any character |
| `9` | digit |
| `#` | digit or sign (`-`/`+`) |
| `L` | `T`/`F` (logic) |
| `Y` | `Y`/`N` |
| `!` | any character, uppercased |
| `0` | digit (leading zeros preserved) |
| `$`, `*`, `.`, `,`, `(` `)`, and any other char | literal |

### Input-time enforcement

### Examples
| Value | Picture | Output |
|---|---|---|
| `1234567.89` | `999,999.99` | ` 1,234,567.89` |
| `123456` | `@R (999) 999` | `(123) 456` |
| `-1234.5` | `@( 999,999.99` | `( 1,234.50 )` |
| `1200` | `@$ 999,999.99` | `$  1,200.00` |
| `0` | `@Z 9999` | `    ` |
| `hello world` | `@!` | `HELLO WORLD` |
| `hello` | `@! @J XXXXXX` | ` HELLO` |
| `ABC` | `@A XXXXXX` | `ABC   ` |
| `abc123` | `@N XXXXXX` | `abc123` |
| today | `99/99/9999` | `31/07/2026` |
| today | `@E` | `31/07/26` |

## `ClipperDates` — date functions

## `ClipperStrings` — string functions

## `ClipperNumbers` — numeric functions

## `ClipperScrn` — screen functions

## `ClipperPrinter` — printer functions

## `ClipperMouse` — mouse state
| Function | Description |
|---|---|
| `IsMouse()` | Mouse present / last event was a mouse event |
| `MRow()` / `MCol()` | Last mouse row / column |
| `MLeft()` / `MRight()` / `MMiddle()` | Button down |
| `MLeftPress()` / `MRightPress()` / `MMiddlePress()` | Button pressed this poll |
| `MLeftLess()` / `MRightLess()` / `MMiddleLess()` | Button released |
| `MLeftShift()` / `MRightShift()` / `MMiddleShift()` | Down + Shift |
| `MLeftControl()` / `MRightControl()` / `MMiddleControl()` | Down + Ctrl |
| `MLeftM()` / `MRightM()` / `MMiddleM()` | Down + dragged |
| `MouStat(button, out row, out col, out mods)` | 1=left, 2=right, 3=middle; returns false if the button is up |
| `MPosition(row?, col?)` | Set/get the mouse position |
| `MouseCursor(row?, col?, type?)` | Position + cursor shape (`"arrow"`, `"text"`, …) |
| `MRelease()` | `"L"`/`"R"`/`"M"` last release, `""` if none |
| `UpdateFromEvent(MouseAction)` | Feed a synthetic event (used by tests) |

## Key constants

## Headless test harness

ClipperGlobal.SetScreenSize(24, 80);
ClipperOutput.ClearScreen();
ClipperInput.SetKeySource(() => keys.Dequeue());   // throws when exhausted
ClipperInput.SetRenderHook(_ => { });
using (Headless.Keys('1', '2', 'a', '3', KeyConstants.K_ENTER))
{
    Assert.Equal(0, ClipperRead.Read());
}
Assert.Equal("123", ClipperGlobal.State.Gets[0].Value);  // 'a' rejected by the template


CLIPPERLIB_RUN_HEADLESS=1 dotnet test


### DYNAMIC-Guida-Utente

*Fonte: `DYNAMIC-Guida-Utente`*


## Sintassi

DYNAMIC area AS STRING = "clienti"
DYNAMIC record AS INTEGER = 0

| parte | significato |
|---|---|
| `nome` | identificatore; vive nel frame dinamico della procedura che lo dichiara |
| `AS TIPO` | `STRING` → valore stringa; `INTEGER`/`DOUBLE` → numerico (default: dedotto dal valore) |
| `= valore` | inizializzatore, valutato al momento della dichiarazione |

## Semantica (§2.5)

## Esempio

SUB ApriArea(nome AS STRING)
    DYNAMIC area AS STRING = nome
    CALL Leggi()
    PRINT "ApriArea:"; area
END SUB

SUB Leggi()
    PRINT "vede:"; area       ' cascata
    CALL Ombra()
    PRINT "vede:"; area       ' dopo l'ombra: torna la sua
END SUB

SUB Ombra()
    DYNAMIC area AS STRING = "temporanea"   ' shadowing
END SUB


## Parità per backend (asserita)
| backend | comando |
|---|---|
| C99Native | `GMB_MINI_ONLY="controls/dynamic_scope_parity.bas" tools/sh/test_minitests_c99native` → PASS=1 |
| VM/C | `tools/sh/test_minitests_parity.sh "controls/dynamic_scope_parity.bas"` (3 backend) → Superato |
| PCodeVM | idem (incluso nelle 3 backend) |
| IL | idem (incluso nelle 3 backend) |

## Limiti noti (dichiarati)

### GET-Guida-Utente

*Fonte: `GET-Guida-Utente`*


## Sintassi

@ riga, colonna SAY espressione [PICTURE "fmt"] [COLOR col]
@ riga, colonna GET variabile [clausole...]
READ


## Parametri di @GET
| clausola | significato |
|---|---|
| `riga, colonna` | posizione del campo (celle in TUI; convertite in pixel in grafica) |
| `variabile` | target: variabile locale/globale **oppure** nome `DYNAMIC` |
| `PICTURE "fmt"` | formato di visualizzazione/edit (es. `"@!"`, `"999999"`) |
| `VALID espressione` | **dopo** INVIO: se `.F.` il campo resta attivo (ri-prompt) e il valore non viene applicato |
| `WHEN espressione` | **prima** di dare il fuoco: se `.F.` il campo è saltato (nessun tasto consumato, variabile invariata) |
| `RANGE lo, hi` | limiti numerici (forma Clipper). **Alternativa** a `MIN`/`MAX`: non combinarle |
| `MIN n` / `MAX n` / `STEP n` | limiti e passo (estensione GMBasic). Usare **al posto** di `RANGE`, non insieme |
| `COLOR col` | colore del campo |
| `TYPE "tipo"` | widget tipizzato: `"Spinner"`, `"CheckBox"`, `"DatePicker"`, `"ListBox"`, `"ComboBox"` (reso come widget in **grafica**; in TUI il campo resta editabile come testo) |
| `ITEMS {…}` | voci per `ListBox`/`ComboBox`; il valore restituito è la **voce** (testo) |
| `SIZE w[, h]` | larghezza (e altezza per le liste) in celle |
| `CAPTION "testo"` | etichetta del widget |
| `MESSAGE "t"` / `ERROR "t"` | testo di aiuto (TUI, riga inferiore) / testo mostrato quando `VALID` rifiuta il valore |

@ 2, 20 GET codice VALID Len(codice) >= 2
@ 4, 20 GET conferma WHEN Len(codice) >= 2


## MESSAGE / ERROR

## Valore iniziale (prefill)

## Target `DYNAMIC`

SUB Chiedi()
    DYNAMIC nome AS STRING = ""
    @ 2, 10 GET nome
    READ
    PRINT "nome="; nome
END SUB


## Esempi

@ 2, 2 SAY "Nome (prefill, min 5):"
@ 2, 26 GET nome VALID Len(nome) >= 5
@ 4, 2 SAY "Codice (min 2):"
@ 4, 20 GET codice VALID Len(codice) >= 2
@ 6, 2 SAY "Conferma (solo con codice):"
@ 6, 32 GET conferma WHEN Len(codice) >= 2
READ


## Parità backend (misurata)
| backend | comando |
|---|---|
| IL | `GMB_TUI_SCRIPT=… dotnet test_ilrun/bin/Debug/net10.0/test_ilrun.dll --engine=il <file>` |
| P-Code | `GMB_TUI_SCRIPT=… dotnet test_pcode/bin/Debug/net10.0/test_pcode.dll <file>` |
| VM/C | `gmbchunk <file> /tmp/x.chunk` + `GMB_TUI_SCRIPT=… gmb_vm_host /tmp/x.chunk` |
| C99Native | `GMB_TUI_SCRIPT=… tools/sh/gmb_c99native_run.sh <file>` |
| caso | input | esito |
|---|---|---|
| `SIZE 8 COLOR "green" CAPTION "voce"` + prefill | `c⏎` | `v=abc` |
| `RANGE 0, 9 STEP 1` su DOUBLE | `5⏎` | `ok=1` |
| `MIN 0 MAX 9 STEP 1` su DOUBLE | `5⏎` | `ok=1` |
| MiniTest / sample | input | output |
|---|---|---|
| `shared/minitests/controls/get_prefill_parity.bas` | `cd⏎` | `a=abcd` |
| `shared/minitests/controls/get_valid_parity.bas` | `a⏎b⏎` | `gv=ab` |
| `shared/minitests/controls/get_when_parity.bas` | `ab⏎` | `a=ab`, `b=orig` |
| `shared/minitests/controls/dynamic_scope_parity.bas` | — | `cascata=uno`, `B=due`, `A=uno` |
| `shared/samples/Clipper/ClipperGetFullDemo.bas` | `⏎a⏎b⏎S⏎` | `nome=Mario`, `codice=ab`, `conferma=noS` |
| `shared/samples/Clipper/ClipperValidDemo.bas` | `ab⏎X⏎` | `codice=ab`, `conferma=X` |
| `shared/minitests/controls/get_message_parity.bas` | `a⏎b⏎` | `v=ab` (MESSAGE/ERROR accettati; testo reso in TUI) |

## Riferimenti

## Eseguire i MiniTest interattivi (riproducibilità)
| backend | comando (esempio `get_valid_parity.bas`, script `a⏎b⏎`) |
|---|---|
| IL | `GMB_TUI_SCRIPT=$'a\rb\r' dotnet test_ilrun/bin/Debug/net10.0/test_ilrun.dll --engine=il shared/minitests/controls/get_valid_parity.bas` |
| P-Code | `GMB_TUI_SCRIPT=$'a\rb\r' dotnet test_pcode/bin/Debug/net10.0/test_pcode.dll shared/minitests/controls/get_valid_parity.bas` |
| VM/C | `./native/gmb_compiler_c/build/gmbchunk <file> /tmp/x.chunk` + `GMB_TUI_SCRIPT=$'a\rb\r' ./native/gmb_vm_c/build/gmb_vm_host /tmp/x.chunk` |
| C99Native | `GMB_TUI_SCRIPT=$'a\rb\r' tools/sh/gmb_c99native_run.sh <file>` |
| MiniTest | `GMB_TUI_SCRIPT` | output atteso |
|---|---|---|
| `get_prefill_parity.bas` | `cd⏎` | `a=abcd` |
| `get_valid_parity.bas` | `a⏎b⏎` | `gv=ab` |
| `get_when_parity.bas` | `ab⏎` | `a=ab`, `b=orig` |
| `get_message_parity.bas` | `a⏎b⏎` | `v=ab` |
| `get_numeric_parity.bas` | `5⏎` | `qta=[ 5 ]`, `qtb=[ 42 ]` |
| `dynamic_scope_parity.bas` | *(nessuno)* | `cascata=uno`, `B=due`, `A=uno` |

### GET-Tipi-Guida-Utente

*Fonte: `GET-Tipi-Guida-Utente`*


## Sintassi

@ r, c GET var TYPE "Spinner"    RANGE 0, 99 STEP 1 SIZE 8
@ r, c GET var TYPE "DatePicker" SIZE 14
@ r, c GET var TYPE "listbox"    ITEMS {"Rosso","Verde","Blu"} SIZE 14, 4
@ r, c GET var TYPE "checkbox"   CAPTION "Si"
@ r, c GET var PICTURE "@!"      SIZE 16


## Tipi supportati
| TYPE | widget | valore restituito | chiavi |
|---|---|---|---|
| (assente) | textbox | testo (o numero convertito) | `PICTURE`, `SIZE` |
| `Spinner` | spinner | numero | `RANGE lo, hi` (o `MIN`/`MAX`), `STEP` |
| `DatePicker` | datepicker | stringa data `gg/mm/aaaa` | `SIZE` |
| `CheckBox` | checkbox | 0 / 1 | `CAPTION` |
| `ListBox` / `ComboBox` | listbox | **label** dell'item scelto | `ITEMS {…}`, `SIZE w, h` |

## Tastiera e mouse

## Parametri

## Clauses VALID e WHEN (code block Clipper/xBase)
| clausola | quando | effetto |
|---|---|---|
| `WHEN expr` | prima di dare il fuoco al campo | se `.F.` il campo viene **saltato** (nessun input richiesto) |
| `VALID expr` | a ogni conferma (INVIO) del campo | se `.F.` il campo **resta attivo** (ri-prompt), il valore rifiutato non viene applicato |

@ 2, 20 GET codice VALID Len(codice) >= 2
@ 4, 20 GET conferma WHEN Len(codice) >= 2
READ


## Disponibilità e parità (asserita)
| backend | comando di verifica | esito |
|---|---|---|
| C99Native (grafica) | `GMB_FORM_AUTOKEY="10,AB;…" gmb_c99native_run.sh hl_types.bas` | ✅ `nome=AB`, `qta=3`, `colore=Blu`, `attivo=1` |
| C99Native (testo) | `GMB_TUI_SCRIPT=$'ab\r' gmb_c99native_run.sh hl_text_read.bas` | ✅ `nome=ab` |
| VM/C | `GMB_TUI_SCRIPT=$'ab\r' gmb_vm_host t.chunk` | ✅ `nome=ab` |
| PCodeVM | `GMB_TUI_SCRIPT=$'ab\r' test_pcode` | ✅ `nome=ab` |
| IL | `GMB_TUI_SCRIPT=$'ab\r' test_ilrun --engine=il` | ✅ `nome=ab` |

### VALID/WHEN — parità misurata
| backend | comando | esito |
|---|---|---|
| C99Native | `GMB_TUI_SCRIPT=$'a\rb\r' gmb_c99native_run.sh get_valid_parity.bas` | ✅ `gv=ab` |
| VM/C | `GMB_TUI_SCRIPT=$'a\rb\r' gmb_vm_host t.chunk` | ✅ `gv=ab` |
| P-Code | `GMB_TUI_SCRIPT=$'a\rb\r' test_pcode get_valid_parity.bas` | ✅ `gv=ab` |
| IL | `GMB_TUI_SCRIPT=$'a\rb\r' test_ilrun --engine=il get_valid_parity.bas` | ✅ `gv=ab` |

<!-- END SURFACE -->
