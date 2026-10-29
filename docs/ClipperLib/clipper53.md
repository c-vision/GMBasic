# Clipper 5.3 Emulation — Technical Reference

`ClipperLib.Clipper` reimplements the Clipper 5.3 language on top of the modern,
unified ClipperLib engine. Every legacy loop (GET/READ, FORM, MEMOEDIT, INKEY)
runs on the **same `EventLoop` + `BlinkClock` + `IInputSource`** that drives the
Controls framework and the `IApp`/`AppRunner` apps, and every screen operation
writes to the shared `ScreenBuffer` rendered through the transport-specific
renderer (console ANSI, telnet diff).

This means: an app can mix `SAY`/`GET`/`READ` with `Control` widgets and `IApp`
screens in the same frame, with one blinking cursor and one input source.

```
EventLoop (UiKey / UiMouse / UiResize)
   ├─ IInputSource (console RawInputReader | telnet TelnetInputSource | test KeySource)
   ├─ BlinkClock (BLINK_MS = 400) → ClipperState.BlinkOn + Control.BlinkOn
   ├─ legacy loops: ClipperRead.Read(), ClipperInput.Accept(), MEMOEDIT
   ├─ Controls EventHandler / FocusManager
   └─ IApp sessions (ModalHost nesting: IApp ⇄ ClipperForm)
```

## Namespaces

| Namespace | Content |
|---|---|
| `ClipperLib.Clipper` | Clipper 5.3 functions (`ClipperOutput`, `ClipperInput`, `ClipperRead`, `ClipperSet`, `ClipperDates`, `ClipperStrings`, `ClipperNumbers`, `ClipperScrn`, `ClipperPrinter`, `ClipperMouse`, `ClipperGlobal`, `FormatPicture`, `Get`, `GetBuilder`, `KeyConstants`) |
| `ClipperLib.App` | `EventLoop`, `ModalHost`, `ConsoleRenderer`, `IInputSource`, `UiEvent`, `MouseAction`, `AppRunner` |

## `ClipperSet` — SET functions

All SET functions are getter/setter overloads: called without an argument they
return the current value, called with one they set it (and return it).

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

```csharp
ClipperRead.AddGet(new Get { Row = 5, Col = 10, Value = nome, Picture = "@! XXXXXXXXXX",
    Color = "W/B", FocusColor = "W+/B", ErrorColor = "R+/W",
    Valid = s => s.Length >= 3, Apply = s => nome = s });
ClipperRead.AddGet(GetBuilder.ForValue(5, 30, ref qty).Range(1, 999).Width(5).Build());
ClipperRead.DrawGets();   // draw the fields and their delimiters
var exit = ClipperRead.Read();  // block until ESC (6) or final Enter (0)
```

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

After `Read()` returns — whether via Enter or ESC — every `Get` in `ClipperGlobal.State.Gets`
still holds the **typed** text in its `Value` property (even on ESC, where `Apply` was not
called). This is the documented way to collect results: read `Gets[i].Value` directly, or use
the `Apply` callback for the Enter-confirmed path. `RunGet()` in the demos reads the values
from the GETs so the results are visible even when the user exits with ESC.

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

Validation happens on **every redraw**, so the field color updates live while the
user types (the "email" pattern):

1. `DrawGet` computes `ClipperRead.IsValid(g)` = `Valid(g.Value)` **and** `Range` bounds.
2. If invalid and `ErrorColor` is set, the field is drawn in the error color; otherwise `FocusColor` (active) or `Color`.
3. On `Enter`, the same predicate decides whether to advance: an invalid field refuses with a beep.
4. `AcceptKey` is applied **before** insertion: a rejected character is discarded, `Rejected` fires, and it never appears in `Value` — no correction step needed afterwards.

## PICTURE — `FormatPicture`

`FormatPicture.Apply(value, picture)` formats any value (string, `double`, `int`, `DateTime`, `bool`) and is used by `SayPic`, `DevOutPic`, `TRANSFORM()`, and the GET system.

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

In a GET field the template is respected **while typing**, not corrected later:

- The typed character must match the placeholder under the cursor (`FormatPicture.TemplateAccepts`).
- `9`/`0` accept digits only, `#` adds `-`/`+`, `A` letters, `N` letters+digits, `L`/`Y` T-F/Y-N, `X`/`!` anything.
- Typing past the last placeholder slot is rejected (the field length is bounded by the template).
- `@A` and `@!` are enforced before insertion; `AcceptKey` adds application-level rules on top.
- Templates without recognizable placeholders (e.g. `@D` using the date format) impose no constraint.

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

`Date()`, `Time()`, `Seconds()`, `Year(d)`, `Month(d)`, `Day(d)`, `Dow(d)` (1=Sunday…7=Saturday),
`CDow(d)`, `CMonth(d)`, `Dtoc(d)` (in `SET DATE FORMAT`), `Dtos(d)` (`yyyymmdd`),
`Ctod(s)` → `DateTime?` (`null` if invalid; honors `SET DATE FORMAT`, `SET CENTURY`, `SET EPOCH`).

## `ClipperStrings` — string functions

`At`/`Rat`, `Substr`, `Stuff`, `StrTran`, `Left`, `Right`, `Upper`, `Lower`,
`IsAlpha`, `IsDigit`, `IsLower`, `IsUpper`, `Len`, `Trim`/`RTrim`/`LTrim`/`AllTrim`,
`PadL`/`PadC`/`PadR`, `Replicate`, `Space`, `Transform` (alias of `FormatPicture.Apply`),
`Val` (leading-numeric parse), `Str`, `StrZero`, `Asc`, `Chr`, `Empty`, `ValType`, `Type`.

## `ClipperNumbers` — numeric functions

`Abs`, `Int` (truncation toward zero), `Round`, `Sqrt`, `Exp`, `Log`, `Log10`,
`Max`/`Min` (params and pairs), `Mod`, `Sign`.

## `ClipperScrn` — screen functions

`MaxCol()`, `MaxRow()`, `Scroll(top, left, bottom, right, rows)` (negative = down,
iterates bottom-up so sources are not overwritten), `SaveScreen([top, left, bottom, right])`
→ string, `RestScreen(s[, top, left])`, `TColor()`, `SetBlink(bool?)`, `Prompt(row, col, text)`,
`Message()`.

## `ClipperPrinter` — printer functions

`SetPrinter(bool?)` / `SetDevice(string?)` (delegate to `ClipperSet`), `Prow()`, `Pcol()`,
`Eject()` (form feed + clear buffer), `PrinterText()` (raw accumulated output).
`QOut`/`DevOut` echo to the printer buffer while `SET PRINTER` is on.

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

`MouseAction` events arrive as INKEY codes `<= K_MOUSE_BASE` (`-1000`); use
`MouseAction.FromInkeyCode(code)` to decode them (`Click`, `RightClick`, `MiddleClick`,
`Drag`, `Release`, `Move`).

## Key constants

`K_LEFT=19`, `K_RIGHT=4`, `K_UP=5`, `K_DOWN=24`, `K_ENTER=13`, `K_ESC=27`, `K_TAB=9`,
`K_BTAB=15`, `K_BS=127`, `K_INS=22`, `K_DEL=7`, `K_HOME=1`, `K_END=6`, `K_PGUP=18`,
`K_PGDN=3`, `K_F1..F10=28..37`, `K_MOUSE_BASE=-1000`, `BLINK_MS=400`.
Border presets: `BORDER_SINGLE`, `BORDER_DOUBLE`, `BORDER_SINGLE2` (9-char strings).

## Headless test harness

The interactive functions are fully testable in-process: set a synthetic key
source and a no-op render hook, then drive `Read()`/`InKey()`:

```csharp
ClipperGlobal.SetScreenSize(24, 80);
ClipperOutput.ClearScreen();
ClipperInput.SetKeySource(() => keys.Dequeue());   // throws when exhausted
ClipperInput.SetRenderHook(_ => { });
using (Headless.Keys('1', '2', 'a', '3', KeyConstants.K_ENTER))
{
    Assert.Equal(0, ClipperRead.Read());
}
Assert.Equal("123", ClipperGlobal.State.Gets[0].Value);  // 'a' rejected by the template
```

The suite gates on `CLIPPERLIB_RUN_HEADLESS=1`:

```bash
CLIPPERLIB_RUN_HEADLESS=1 dotnet test
```

`Clipper53FunctionsTests`, `Clipper53MoreFunctionsTests`, `ClipperPictureInputTests`
cover the function surface; `Clipper53ExamplesSmokeTests` replays the interactive
demos (`DemoClipper53`, `DemoPictureValidation`) end-to-end with synthetic input,
including stray mouse clicks and invalid keys in the submenus (they must be ignored
without re-printing the menu).
