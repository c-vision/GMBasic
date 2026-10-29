# ClipperLib Controls Documentation

## Namespaces

- `ClipperLib.Clipper` — Clipper 5.3 emulation (SAY, GET, READ, BOX, INKEY, ACHOICE, MEMOEDIT, PICTURE, SET/DATE/STRING/NUMERIC/SCREEN/PRINTER/MOUSE functions). Full reference: `docs/ClipperLib/clipper53.md`
- `ClipperLib.Controls` — 78+ TUI controls and helpers
- `ClipperLib.TBrowse` — TBrowse grid control

---

## Base Class

### `Control` (abstract)

Every interactive TUI widget inherits from `Control`.

**Properties:**

| Property | Type | Default | Description |
|---|---|---|---|
| `Id` | `string` | `""` | Unique identifier for event routing |
| `X` | `int` | `0` | Column position (left) |
| `Y` | `int` | `0` | Row position (top) |
| `Width` | `int` | `0` | Width in terminal columns |
| `Height` | `int` | `0` | Height in terminal rows |
| `Focused` | `bool` | `false` (`virtual`) | Whether control has keyboard focus |
| `Visible` | `bool` | `true` | Whether control is rendered |
| `Enabled` | `bool` | `true` | Whether control accepts input |
| `TabStop` | `bool` | `true` | Whether Tab/Shift+Tab can focus this control |
| `TabIndex` | `int` | `0` | Tab order index |
| `Style` | `ControlStyle` | `ControlStyle.Default()` | Visual style (colors, fill) |
| `Uppercase` | `bool` | `false` | Auto-convert text to uppercase |
| `HasError` | `bool` | `false` | Error state — activates error colors |

**Static Property:**

| Property | Type | Default | Description |
|---|---|---|---|
| `BlinkOn` | `bool` | `true` | Global cursor blink toggle |

**Methods:**

| Method | Returns | Description |
|---|---|---|
| `Draw(ScreenBuffer)` | `abstract void` | Render control to buffer |
| `HandleKey(int key)` | `abstract bool` | Process keyboard event |
| `HandleClick(x, y)` | `virtual bool` | Legacy click handler |
| `HandleMouse(MouseAction)` | `virtual bool` | Full mouse handler (click, scroll, drag) |
| `HandleInput(int key)` | `bool` | Unified entry: routes mouse via `MouseAction.FromInkeyCode`, keyboard via `HandleKey` |
| `Navigate(UiEvent)` | `virtual bool` | Unified event handler (crossterm-style) |
| `WithStyle(ControlStyle)` | `Control` | Set style |
| `WithColors(fg, bg)` | `Control` | Set normal colors |
| `WithFocusColors(fg, bg)` | `Control` | Set focus colors |
| `WithErrorColors(fg, bg)` | `Control` | Set error colors |
| `WithSelectedColors(fg, bg)` | `Control` | Set selection colors |
| `WithFillChar(char)` | `Control` | Set fill character |
| `WithUppercase(bool)` | `Control` | Enable/disable uppercase |

**Computed Property:**

| Property | Type | Description |
|---|---|---|
| `Rect` | `ScreenRect` | Returns `new(X, Y, Width, max(1, Height))` |

**Example:**

```csharp
var input = new TextInput { X = 2, Y = 5, Width = 30, Height = 1 }
    .WithColors(CellColor.White, CellColor.Blue)
    .WithFocusColors(CellColor.Black, CellColor.Yellow);
```

---

### `ControlStyle`

Centralized visual style. Every control gets its default style from `ControlStyle.Default()`, which derives from the global `Theme` if set.

**Properties:**

| Property | Type | Default | Role |
|---|---|---|---|
| `Fg` | `CellColor?` | — | Normal text color |
| `Bg` | `CellColor?` | — | Normal background color |
| `FgFocus` | `CellColor?` | — | Focused text color |
| `BgFocus` | `CellColor?` | — | Focused background color |
| `FgError` | `CellColor?` | — | Error text color |
| `BgError` | `CellColor?` | — | Error background color |
| `SelFg` | `CellColor?` | — | Selected/highlighted text color |
| `SelBg` | `CellColor?` | — | Selected/highlighted background color |
| `FillChar` | `char` | `' '` | Background fill character |

**Static Methods:**

| Method | Returns | Description |
|---|---|---|
| `Default()` | `ControlStyle` | Default style (or from active Theme) |
| `FromTheme(Theme)` | `ControlStyle` | Build style from a Theme palette |
| `SetTheme(Theme)` | `void` | Set global theme |

**Instance Methods:**

| Method | Returns | Description |
|---|---|---|
| `Clone()` | `ControlStyle` | Deep copy |
| `WithColors(fg, bg)` | `ControlStyle` | Set normal colors |
| `WithFocusColors(fg, bg)` | `ControlStyle` | Set focus colors |
| `WithErrorColors(fg, bg)` | `ControlStyle` | Set error colors |
| `WithSelectedColors(fg, bg)` | `ControlStyle` | Set selection colors |
| `WithFill(char)` | `ControlStyle` | Set fill character |
| `Effective(focused, error, selected)` | `(CellColor, CellColor)` | Resolve effective colors for state |

**Example:**

```csharp
var style = new ControlStyle()
    .WithColors(CellColor.White, CellColor.Blue)
    .WithFocusColors(CellColor.Black, CellColor.Cyan)
    .WithErrorColors(CellColor.White, CellColor.Red)
    .WithFill('░');

var (fg, bg) = style.Effective(focused: true);
```

---

## Input Controls

### `TextInput`

Single-line text entry with cursor, optional picture formatting, validation, and password mode.

**Properties:**

| Property | Type | Default | Description |
|---|---|---|---|
| `Text` | `string` | `""` | Text content |
| `Cursor` | `int` | `0` | Cursor position within text |
| `MaxLength` | `int` | `50` | Maximum characters allowed |
| `Picture` | `string?` | `null` | PICTURE format template (Clipper-compatible) |
| `Validator` | `Func<string, bool>?` | `null` | Validation function |
| `PasswordMode` | `bool` | `false` | Mask input with `*` |
| `ReadOnly` | `bool` | `false` | Disable editing |

**Computed Property:**

| Property | Description |
|---|---|
| `IsValid` | Returns `Validator == null \|\| Validator(Text)` |

**Fluent Methods:**

| Method | Returns | Description |
|---|---|---|
| `WithText(string)` | `TextInput` | Set text, cursor at end |
| `WithMaxLength(int)` | `TextInput` | Set max length |
| `WithPasswordMode(bool)` | `TextInput` | Toggle password mode |
| `WithValidator(Func<string,bool>?)` | `TextInput` | Set validator |

**Key Bindings:**

| Key | Action |
|---|---|
| Printable (32–126) | Insert character at cursor |
| `BS` | Delete character before cursor |
| `DEL` | Delete character at cursor |
| `LEFT` | Move cursor left |
| `RIGHT` | Move cursor right |
| `HOME` | Move cursor to start |
| `END` | Move cursor to end |

**Example:**

```csharp
var input = new TextInput
{
    X = 5, Y = 8, Width = 30, Height = 1
}
.WithText("hello")
.WithMaxLength(100)
.WithValidator(s => s.Length >= 3);
```

---

### `TextArea`

Multi-line text editor with line management.

**Properties:**

| Property | Type | Default | Description |
|---|---|---|---|
| `Lines` | `List<string>` | `[""]` | Text lines |
| `CursorLine` | `int` | `0` | Current line index |
| `CursorCol` | `int` | `0` | Column within current line |
| `ScrollOffset` | `int` | `0` | Vertical scroll offset |

**Fluent Methods:**

| Method | Returns | Description |
|---|---|---|
| `WithLines(List<string>)` | `TextArea` | Set lines, reset cursor |

**Key Bindings:**

| Key | Action |
|---|---|
| Printable | Insert character at cursor |
| `ENTER` | Split line at cursor |
| `BS` | Delete char before cursor; join with previous line at start |
| `UP` | Move cursor up |
| `DOWN` | Move cursor down |
| `LEFT` | Move cursor left |
| `RIGHT` | Move cursor right |
| `HOME` | Move to start of line |
| `END` | Move to end of line |

**Example:**

```csharp
var editor = new TextArea
{
    X = 2, Y = 3, Width = 60, Height = 20
}
.WithLines(["First line", "Second line", "Third line"]);
```

---

### `Button`

Clickable button with centered label.

**Properties:**

| Property | Type | Default | Description |
|---|---|---|---|
| `Label` | `string` | `"OK"` | Button text |
| `OnClick` | `Action?` | `null` | Click callback |

**Constructor:**

- Sets `Width = 10, Height = 1`

**Fluent Methods:**

| Method | Returns | Description |
|---|---|---|
| `WithLabel(string)` | `Button` | Set label |
| `WithOnClick(Action)` | `Button` | Set click handler |

**Key Bindings:**

| Key | Action |
|---|---|
| `ENTER` or `SPACE` | Invoke `OnClick` |

**Example:**

```csharp
var btn = new Button { X = 10, Y = 15 }
    .WithLabel("Save")
    .WithOnClick(() => Console.Beep());
```

---

### `CheckBox`

Toggleable checkbox with label: `[X] Label` / `[ ] Label`.

**Properties:**

| Property | Type | Default | Description |
|---|---|---|---|
| `Checked` | `bool` | `false` | Checked state |
| `Label` | `string` | `""` | Checkbox label |

**Fluent Methods:**

| Method | Returns | Description |
|---|---|---|
| `WithChecked(bool)` | `CheckBox` | Set checked state |
| `WithLabel(string)` | `CheckBox` | Set label |

**Key Bindings:**

| Key | Action |
|---|---|
| `SPACE` or `ENTER` | Toggle checked state |

**Example:**

```csharp
var cb = new CheckBox { X = 2, Y = 4, Width = 20, Height = 1 }
    .WithLabel("Enable feature")
    .WithChecked(true);
```

---

### `RadioGroup`

Radio button group: `(•) Option` / `( ) Option`.

**Properties:**

| Property | Type | Default | Description |
|---|---|---|---|
| `Options` | `List<string>` | `[]` | Radio options |
| `Selected` | `int` | `0` | Selected index |

**Fluent Methods:**

| Method | Returns | Description |
|---|---|---|
| `WithOptions(List<string>)` | `RadioGroup` | Set options |

**Key Bindings:**

| Key | Action |
|---|---|
| `UP` | Select previous option |
| `DOWN` | Select next option |

**Example:**

```csharp
var rg = new RadioGroup { X = 2, Y = 5, Width = 15, Height = 3 }
    .WithOptions(["Small", "Medium", "Large"]);
```

---

### `ComboBox`

Drop-down selection list with text display.

**Properties:**

| Property | Type | Default | Description |
|---|---|---|---|
| `Items` | `List<string>` | `[]` | Drop-down items |
| `Selected` | `int?` | `null` | Selected index |
| `Open` | `bool` | `false` | Drop-down open state |
| `Highlight` | `int` | `0` | Highlighted item in open list |
| `DropDownHeight` | `int` | `5` | Visible items in drop-down |

**Fluent Methods:**

| Method | Returns | Description |
|---|---|---|
| `WithItems(List<string>)` | `ComboBox` | Set items |

**Key Bindings (closed):**

| Key | Action |
|---|---|
| `DOWN` | Open drop-down and move to next |
| `ENTER` | Open drop-down |

**Key Bindings (open):**

| Key | Action |
|---|---|
| `UP` / `DOWN` | Navigate items |
| `ENTER` | Select highlighted item |
| `ESC` | Close without selecting |
| Printable | Filter items by typed text |

**Example:**

```csharp
var combo = new ComboBox { X = 2, Y = 3, Width = 20, Height = 1 }
    .WithItems(["Option A", "Option B", "Option C"]);
```

---

### `NumericUpDown`

Integer up/down control with step and range.

**Properties:**

| Property | Type | Default | Description |
|---|---|---|---|
| `Value` | `int` | `0` | Current value |
| `Min` | `int` | `int.MinValue` | Minimum value |
| `Max` | `int` | `int.MaxValue` | Maximum value |
| `Step` | `int` | `1` | Step increment |

**Fluent Methods:**

| Method | Returns | Description |
|---|---|---|
| `WithRange(int, int)` | `NumericUpDown` | Set min/max range |
| `WithValue(int)` | `NumericUpDown` | Set value (clamped) |

**Key Bindings:**

| Key | Action |
|---|---|
| `UP` | Increment by step |
| `DOWN` | Decrement by step |
| `PGUP` | Add 10 |
| `PGDN` | Subtract 10 |
| Digits (0–9) | Direct entry |

**Example:**

```csharp
var nud = new NumericUpDown { X = 2, Y = 4, Width = 8, Height = 1 }
    .WithRange(0, 100)
    .WithValue(42);
```

---

### `Slider`

Horizontal slider with draggable thumb.

**Properties:**

| Property | Type | Default | Description |
|---|---|---|---|
| `Min` | `int` | `0` | Minimum value |
| `Max` | `int` | `100` | Maximum value |
| `Value` | `int` | `0` | Current value |
| `Step` | `int` | `1` | Step increment |

**Fluent Methods:**

| Method | Returns | Description |
|---|---|---|
| `WithRange(int, int)` | `Slider` | Set min/max |
| `WithValue(int)` | `Slider` | Set value (clamped) |

**Key Bindings:**

| Key | Action |
|---|---|
| `LEFT` / `UP` | Decrease value |
| `RIGHT` / `DOWN` | Increase value |
| `HOME` | Set to minimum |
| `END` | Set to maximum |

**Mouse:** Drag thumb directly.

**Example:**

```csharp
var slider = new Slider { X = 2, Y = 6, Width = 20, Height = 1 }
    .WithRange(0, 50)
    .WithValue(25);
```

---

### `StarRating`

Star rating with ★ / ☆ rendering.

**Properties:**

| Property | Type | Default | Description |
|---|---|---|---|
| `Value` | `int` | `0` | Current rating |
| `Max` | `int` | `5` | Maximum stars |

**Fluent Methods:**

| Method | Returns | Description |
|---|---|---|
| `WithMax(int)` | `StarRating` | Set max stars |
| `WithValue(int)` | `StarRating` | Set value |

**Key Bindings:**

| Key | Action |
|---|---|
| `LEFT` / `UP` | Decrease |
| `RIGHT` / `DOWN` | Increase |

**Example:**

```csharp
var rating = new StarRating { X = 2, Y = 3, Width = 10, Height = 1 }
    .WithMax(10)
    .WithValue(7);
```

---

### `Spinner`

Animated spinner/indicator with frame presets.

**Properties:**

| Property | Type | Default | Description |
|---|---|---|---|
| `Frames` | `List<string>` | `["|","/","-","\\"]` | Animation frames |
| `Index` | `int` | `0` | Current frame index |

**Fluent Methods:**

| Method | Returns | Description |
|---|---|---|
| `WithFrames(List<string>)` | `Spinner` | Set custom frames |
| `WithPreset(string)` | `Spinner` | Set preset by name |

**Presets:**

`line`, `dots`, `bar`, `braille`, `boxes`, `circle`, `clock`

**Methods:**

| Method | Description |
|---|---|
| `Tick()` | Advance to next frame |
| `SetPreset(string)` | Load frame set by name |

**Usage:**

```csharp
var spinner = new Spinner { X = 2, Y = 5, Width = 3, Height = 1 }
    .WithPreset("braille");

// In update loop:
spinner.Tick();
```

---

### `DatePicker`

Calendar date picker with popup.

**Properties:**

| Property | Type | Default | Description |
|---|---|---|---|
| `Day` | `int` | `1` | Day |
| `Month` | `int` | `1` | Month |
| `Year` | `int` | `2025` | Year |
| `Open` | `bool` | `false` | Calendar popup state |
| `PopupHeight` | `int` | `10` | Height when open |
| `MinYear` | `int` | `0` | Minimum selectable year |
| `MaxYear` | `int` | `0` | Maximum selectable year |

**Events:**

| Event | Description |
|---|---|
| `OnDateSelected` | `event Action<int, int, int>` — day, month, year |

**Fluent Methods:**

| Method | Returns | Description |
|---|---|---|
| `WithDate(d, m, y)` | `DatePicker` | Set date |
| `WithYearRange(min, max)` | `DatePicker` | Set year range |

**Key Bindings (closed):**

| Key | Action |
|---|---|
| `ENTER` or `SPACE` | Open calendar popup |

**Key Bindings (open):**

| Key | Action |
|---|---|
| `LEFT` / `RIGHT` | Previous/next day |
| `UP` / `DOWN` | Previous/next week |
| `PGUP` | Previous month |
| `PGDN` | Next month |
| `HOME` | First of month |
| `END` | Last of month |
| `ENTER` | Confirm selection |
| `ESC` | Cancel |

**Example:**

```csharp
var dp = new DatePicker { X = 2, Y = 3, Width = 12, Height = 1 }
    .WithDate(15, 6, 2026)
    .WithYearRange(2000, 2030);
dp.OnDateSelected += (d, m, y) => Console.WriteLine($"{d}/{m}/{y}");
```

---

### `ColorPicker`

Color palette picker with 256‑color support.

**Properties:**

| Property | Type | Default | Description |
|---|---|---|---|
| `Palette` | `List<CellColor>` | `[]` | Color palette |
| `Columns` | `int` | `16` | Display columns |
| `SelectedIndex` | `int` | `0` | Selected color index |
| `TopRow` | `int` | `0` | Scroll offset |

**Constructor:**

- Builds a default 256-color palette if called without setup

**Fluent Methods:**

| Method | Returns | Description |
|---|---|---|
| `WithColumns(int)` | `ColorPicker` | Set columns |
| `WithPalette(List<CellColor>)` | `ColorPicker` | Set palette |

**Key Bindings:**

| Key | Action |
|---|---|
| `UP` / `DOWN` / `LEFT` / `RIGHT` | Navigate palette |

**Example:**

```csharp
var cp = new ColorPicker { X = 2, Y = 2, Width = 32, Height = 8 }
    .WithColumns(8);
```

---

### `ToggleButton`

Toggle button: `[X] Caption` / `[ ] Caption`.

**Properties:**

| Property | Type | Default | Description |
|---|---|---|---|
| `Caption` | `string` | `""` | Button caption |
| `Pressed` | `bool` | `false` | Toggle state |

**Fluent Methods:**

| Method | Returns | Description |
|---|---|---|
| `WithCaption(string)` | `ToggleButton` | Set caption |
| `WithPressed(bool)` | `ToggleButton` | Set state |

**Key Bindings:**

| Key | Action |
|---|---|
| `SPACE` or `ENTER` | Toggle pressed state |

**Example:**

```csharp
var tb = new ToggleButton { X = 2, Y = 3, Width = 16, Height = 1 }
    .WithCaption("Dark Mode")
    .WithPressed(true);
```

---

### `ImageButton`

Button with icon prefix.

**Properties:**

| Property | Type | Default | Description |
|---|---|---|---|
| `Icon` | `string` | `""` | Icon character(s) |
| `Label` | `string` | `""` | Button label |
| `OnClick` | `Action?` | `null` | Click callback |

**Fluent Methods:**

| Method | Returns | Description |
|---|---|---|
| `WithIcon(string)` | `ImageButton` | Set icon |
| `WithLabel(string)` | `ImageButton` | Set label |
| `WithOnClick(Action)` | `ImageButton` | Set click handler |

**Example:**

```csharp
var ibtn = new ImageButton { X = 2, Y = 5, Width = 12, Height = 1 }
    .WithIcon("🔍")
    .WithLabel("Search")
    .WithOnClick(() => Console.Beep());
```

---

### `Form`

Form with labeled fields, inline editing.

**Types:**

```csharp
public class FormField
{
    public string Label { get; set; } = "";
    public string Value { get; set; } = "";
    public bool Editing { get; set; }
}
```

**Properties:**

| Property | Type | Default | Description |
|---|---|---|---|
| `Fields` | `List<FormField>` | `[]` | Form fields |
| `SelectedIndex` | `int` | `0` | Selected field index |

**Fluent Methods:**

| Method | Returns | Description |
|---|---|---|
| `WithFields(List<FormField>)` | `Form` | Set fields |

**Key Bindings:**

| Key | Action |
|---|---|
| `UP` / `DOWN` | Navigate between fields |
| `ENTER` | Edit selected field value |
| `TAB` | Confirm edit and move to next |

**Example:**

```csharp
var form = new Form { X = 2, Y = 2, Width = 40, Height = 4 }
    .WithFields([
        new FormField { Label = "Name", Value = "John" },
        new FormField { Label = "Email", Value = "john@example.com" }
    ]);
```

---

## List Controls

### `ListBox`

Selectable list with optional multi-select checkboxes.

**Properties:**

| Property | Type | Default | Description |
|---|---|---|---|
| `Items` | `List<string>` | `[]` | List items |
| `Selected` | `int?` | `null` | Selected index |
| `Top` | `int` | `0` | Scroll offset |
| `MultiSelect` | `bool` | `false` | Enable checkboxes |
| `Checked` | `HashSet<int>` | `[]` | Checked indices |

**Fluent Methods:**

| Method | Returns | Description |
|---|---|---|
| `WithItems(List)` | `ListBox` | Set items |
| `WithMultiSelect(bool)` | `ListBox` | Toggle multi-select |

**Rendering:** `>` prefix for selected item, `[X]`/`[ ]` checkboxes when `MultiSelect` is true.

**Key Bindings:**

| Key | Action |
|---|---|
| `UP` / `DOWN` | Navigate items |
| `PGUP` / `PGDN` | Page scroll |
| `HOME` / `END` | First/last item |
| `SPACE` | Toggle checkbox (multi-select) |

**Mouse:** Click to select, scroll wheel to navigate.

**Example:**

```csharp
var lb = new ListBox { X = 2, Y = 2, Width = 20, Height = 6 }
    .WithItems(["Apples", "Oranges", "Bananas", "Grapes"])
    .WithMultiSelect(true);
```

---

### `ReorderListBox`

ListBox with reorder support (Ctrl+UP/DOWN).

**Properties:**

| Property | Type | Default | Description |
|---|---|---|---|
| `Items` | `List<string>` | `[]` | List items |
| `Selected` | `int?` | `null` | Selected index |
| `Top` | `int` | `0` | Scroll offset |
| `MultiSelect` | `bool` | `false` | Enable checkboxes |
| `Checked` | `List<bool>` | `[]` | Checked states |
| `CtrlHeld` | `bool` | `false` | Ctrl held for reorder |

**Fluent Methods:**

| Method | Returns | Description |
|---|---|---|
| `WithItems(List)` | `ReorderListBox` | Set items |
| `WithMultiSelect(bool)` | `ReorderListBox` | Toggle multi-select |

**Key Bindings (same as ListBox +):**

| Key | Action |
|---|---|
| `Ctrl+UP` | Move selected item up |
| `Ctrl+DOWN` | Move selected item down |

**Example:**

```csharp
var rlb = new ReorderListBox { X = 2, Y = 2, Width = 20, Height = 6 }
    .WithItems(["Item A", "Item B", "Item C"])
    .WithMultiSelect(true);
```

---

### `DataList`

List with multi-select and incremental search.

**Properties:**

| Property | Type | Default | Description |
|---|---|---|---|
| `Items` | `List<string>` | `[]` | List items |
| `SelectedIndex` | `int` | `0` | Selected index |
| `ScrollOffset` | `int` | `0` | Scroll offset |
| `MultiSelect` | `bool` | `false` | Enable multi-select |
| `SelectedItems` | `HashSet<int>` | `[]` | Selected indices |
| `Searchable` | `bool` | `false` | Enable incremental search |
| `SearchBuffer` | `string` | `""` | Accumulated search text |

**Fluent Methods:**

| Method | Returns | Description |
|---|---|---|
| `WithItems(List)` | `DataList` | Set items |
| `WithMultiSelect(bool)` | `DataList` | Toggle multi-select |
| `WithSearchable(bool)` | `DataList` | Toggle search |

**Key Bindings:**

| Key | Action |
|---|---|
| `UP` / `DOWN` | Navigate |
| `PGUP` / `PGDN` | Page scroll |
| `HOME` / `END` | First/last |
| `SPACE` | Toggle selection (multi-select) |
| Printable (searchable) | Incremental search |

**Example:**

```csharp
var dl = new DataList { X = 2, Y = 2, Width = 20, Height = 6 }
    .WithItems(["Alpha", "Beta", "Gamma", "Delta"])
    .WithMultiSelect(true)
    .WithSearchable(true);
```

---

### `Tab`

Horizontal tab bar.

**Properties:**

| Property | Type | Default | Description |
|---|---|---|---|
| `Tabs` | `List<string>` | `[]` | Tab titles |
| `Active` | `int` | `0` | Active tab index |

**Fluent Methods:**

| Method | Returns | Description |
|---|---|---|
| `WithTabs(List)` | `Tab` | Set tabs |

**Key Bindings:**

| Key | Action |
|---|---|
| `LEFT` / `RIGHT` | Switch tab |
| `HOME` | First tab |
| `END` | Last tab |

**Example:**

```csharp
var tab = new Tab { X = 2, Y = 1, Width = 40, Height = 1 }
    .WithTabs(["General", "Advanced", "About"]);
```

---

### `Menu`

Drop-down menu with nesting and separators.

**Type:**

```csharp
public class MenuItem
{
    public string Label { get; set; } = "";
    public List<MenuItem> SubItems { get; set; } = [];
    public bool Separator { get; set; }
    public Action? Action { get; set; }
}
```

**Properties:**

| Property | Type | Default | Description |
|---|---|---|---|
| `Items` | `List<MenuItem>` | `[]` | Menu items |
| `SelectedIndex` | `int` | `0` | Selected index |
| `IsOpen` | `bool` | `false` | Menu open state |

**Constructor:** Sets `Height = 3`.

**Fluent Methods:**

| Method | Returns | Description |
|---|---|---|
| `WithItems(List<MenuItem>)` | `Menu` | Set menu items |

**Key Bindings:**

| Key | Action |
|---|---|
| `UP` / `DOWN` | Navigate items |
| `ENTER` | Activate item or open submenu |
| `ESC` | Close menu |

**Example:**

```csharp
var menu = new Menu { X = 2, Y = 1, Width = 15 }
    .WithItems([
        new MenuItem { Label = "New", Action = () => { } },
        new MenuItem { Separator = true },
        new MenuItem { Label = "Exit", Action = () => Environment.Exit(0) }
    ]);
```

---

### `TreeView`

Tree with expand/collapse nodes, indent display.

**Type:**

```csharp
public class TreeNode
{
    public string Label { get; set; } = "";
    public int Depth { get; set; }
    public bool Expanded { get; set; } = true;
    public bool HasChildren { get; set; }
}
```

**Properties:**

| Property | Type | Default | Description |
|---|---|---|---|
| `Nodes` | `List<TreeNode>` | `[]` | All tree nodes (flat list) |
| `SelectedIndex` | `int` | `0` | Selected index in flat list |
| `ScrollOffset` | `int` | `0` | Scroll offset |

**Fluent Methods:**

| Method | Returns | Description |
|---|---|---|
| `WithNodes(List<TreeNode>)` | `TreeView` | Set nodes |

**Key Bindings:**

| Key | Action |
|---|---|
| `UP` / `DOWN` | Navigate |
| `LEFT` | Collapse node |
| `RIGHT` | Expand node |
| `HOME` / `END` | First/last |

**Example:**

```csharp
var tv = new TreeView { X = 2, Y = 2, Width = 30, Height = 8 }
    .WithNodes([
        new TreeNode { Label = "Root", Depth = 0, HasChildren = true, Expanded = true },
        new TreeNode { Label = "Child 1", Depth = 1 },
        new TreeNode { Label = "Child 2", Depth = 1, HasChildren = true, Expanded = false },
        new TreeNode { Label = "Grandchild", Depth = 2 },
    ]);
```

---

## Data Controls

### `TBrowse` (in `ClipperLib.TBrowse`)

Full-featured browse/grid control (Clipper 5.3 TBrowse compatible).

**Types:**

```csharp
public class BrowseColumn
{
    public string Header { get; set; }
    public int Width { get; set; }
    public BrowseAlign Align { get; set; } = BrowseAlign.Left;
    public bool Editable { get; set; }
    public bool Visible { get; set; } = true;
    public BrowseDataType DataType { get; set; } = BrowseDataType.Text;
    public List<string>? Choices { get; set; }
}
```

**Properties:**

| Property | Type | Default | Description |
|---|---|---|---|
| `Columns` | `List<BrowseColumn>` | `[]` | Column definitions |
| `DataSource` | `IBrowseDataSource` | — | Data source |
| `FrozenCols` | `int` | `0` | Frozen left columns |
| `CurrentRow` | `int` | `0` | Current row |
| `CurrentCol` | `int` | `0` | Current column |
| `TopRow` | `int` | `0` | Top visible row |
| `LeftCol` | `int` | `0` | Left visible column |
| `ShowHeader` | `bool` | `true` | Show header row |
| `ShowBorder` | `bool` | `true` | Show border |
| `HighlightRow` | `bool` | `true` | Highlight entire row |
| `MouseEnabled` | `bool` | `false` | Mouse navigation |
| `HeaderColors` | `BrowseColorPair` | — | Header color pair |
| `RowColors` | `BrowseColorPair` | — | Normal row colors |
| `HighlightColors` | `BrowseColorPair` | — | Highlighted row colors |

**Key Navigation Methods:**

| Method | Returns | Description |
|---|---|---|
| `MoveUp()` | `bool` | Move up one row |
| `MoveDown()` | `bool` | Move down one row |
| `MoveLeft()` | `bool` | Move left one column |
| `MoveRight()` | `bool` | Move right one column |
| `PageUp()` | `bool` | Move up one page |
| `PageDown()` | `bool` | Move down one page |
| `GoTop()` | `void` | Go to first row |
| `GoBottom()` | `void` | Go to last row |
| `HandleKey(int)` | `BrowseAction` | Process key, returns `BrowseAction` |

**`BrowseAction` enum:** `None`, `Selected`, `Cancelled`, `Refreshed`, `Edit`

**`IBrowseDataSource` Interface:**

```csharp
public interface IBrowseDataSource
{
    string GetValue(int row, int col);
    void SetValue(int row, int col, string val);
    int RowCount { get; }
    int Skip(int delta);
    void GoTop();
    void GoBottom();
    object? Bookmark(bool create);
    void GoTo(object? bookmark);
}
```

**Provided implementation:** `VecDataSource` (list-of-lists)

**Example:**

```csharp
var data = new List<List<string>>
{
    ["Alice", "30", "Engineer"],
    ["Bob", "25", "Designer"],
    ["Charlie", "35", "Manager"]
};

var browse = new TBrowse
{
    Area = new ScreenRect(2, 2, 40, 10),
    Columns =
    [
        new BrowseColumn("Name", 15) { Editable = true },
        new BrowseColumn("Age", 6) { Align = BrowseAlign.Right },
        new BrowseColumn("Role", 15),
    ],
    DataSource = new VecDataSource(data),
    ShowHeader = true,
    ShowBorder = true
};
```

---

### `Grid`

Simple tabular grid.

**Properties:**

| Property | Type | Default | Description |
|---|---|---|---|
| `Cols` | `int` | `0` | Number of columns |
| `Cells` | `List<string>` | `[]` | Cell values (row-major) |
| `CursorRow` | `int` | `0` | Row cursor |
| `CursorCol` | `int` | `0` | Column cursor |
| `Selected` | `(int, int)?` | `null` | Selected cell |

**Fluent Methods:**

| Method | Returns | Description |
|---|---|---|
| `WithCols(int)` | `Grid` | Set columns |
| `WithCells(List<string>)` | `Grid` | Set cells |

**Key Bindings:**

| Key | Action |
|---|---|
| `UP` / `DOWN` / `LEFT` / `RIGHT` | Navigate cells |

**Example:**

```csharp
var grid = new Grid { X = 2, Y = 2, Width = 40, Height = 6 }
    .WithCols(3)
    .WithCells(["A1", "B1", "C1", "A2", "B2", "C2"]);
```

---

### `PropertyGrid`

Key-value property editor with inline editing.

**Properties:**

| Property | Type | Default | Description |
|---|---|---|---|
| `Items` | `List<(string name, string value)>` | `[]` | Property list |
| `SelectedIndex` | `int` | `0` | Selected property |
| `ScrollOffset` | `int` | `0` | Scroll offset |

**Fluent Methods:**

| Method | Returns | Description |
|---|---|---|
| `WithItems(List<(string,string)>)` | `PropertyGrid` | Set items |

**Key Bindings:**

| Key | Action |
|---|---|
| `UP` / `DOWN` | Navigate |
| `ENTER` | Start editing value |
| `ENTER` (editing) | Confirm edit |
| `ESC` (editing) | Cancel edit |

**Example:**

```csharp
var pg = new PropertyGrid { X = 2, Y = 2, Width = 40, Height = 6 }
    .WithItems([
        ("Name", "MyApp"),
        ("Version", "1.0.0"),
        ("Author", "Developer")
    ]);
```

---

### `HexView`

Hexadecimal dump viewer with optional editing.

**Config:**

```csharp
public class HexViewConfig
{
    public int BytesPerGroup { get; set; } = 2;
    public string GroupSeparator { get; set; } = " ";
    public bool ShowAddress { get; set; } = true;
    public AddressFormat AddressFormat { get; set; } = AddressFormat.Hex;
    public int StartAddr { get; set; }
    public bool ShowAscii { get; set; } = true;
}
```

**Properties:**

| Property | Type | Default | Description |
|---|---|---|---|
| `Data` | `byte[]` | `[]` | Data to display |
| `BytesPerRow` | `int` | `16` | Bytes per line |
| `Offset` | `int` | `0` | Scroll offset |
| `Config` | `HexViewConfig` | `new()` | Display configuration |
| `State` | `DisplayState` | `Enabled` | Disabled/Enabled/Editable |
| `EditingIndex` | `int?` | `null` | Currently editing byte |
| `EditingHighNibble` | `bool` | `false` | Nibble being edited |

**Fluent Methods:**

| Method | Returns | Description |
|---|---|---|
| `WithData(byte[])` | `HexView` | Set data |
| `WithConfig(HexViewConfig)` | `HexView` | Set config |

**Key Bindings:**

| Key | Action |
|---|---|
| `UP` / `DOWN` | Scroll |
| `LEFT` / `RIGHT` | Navigate bytes |
| `HOME` / `END` | First/last byte |
| Hex digit (Editable) | Edit nibble |

**Example:**

```csharp
var hex = new HexView { X = 2, Y = 2, Width = 60, Height = 10 }
    .WithData(File.ReadAllBytes("data.bin"))
    .WithConfig(new HexViewConfig { BytesPerGroup = 4, ShowAscii = true });
```

---

## Navigation & Display Controls

### `Label`

Static text label with alignment.

**Properties:**

| Property | Type | Default | Description |
|---|---|---|---|
| `Text` | `string` | `""` | Label text |
| `Align` | `Alignment` | `Left` | Horizontal alignment |

**Alignment Values:** `Left`, `Center`, `Right`

**Fluent Methods:**

| Method | Returns | Description |
|---|---|---|
| `WithText(string)` | `Label` | Set text |
| `WithAlign(Alignment)` | `Label` | Set alignment |

**Example:**

```csharp
var label = new Label { X = 2, Y = 1, Width = 20, Height = 1 }
    .WithText("Welcome to ClipperLib")
    .WithAlign(Alignment.Center);
```

---

### `HLine` (Horizontal Line)

Horizontal separator line with optional centered label.

**Properties:**

| Property | Type | Default | Description |
|---|---|---|---|
| `Label` | `string` | `""` | Centered label text |

**Fluent Methods:**

| Method | Returns | Description |
|---|---|---|
| `WithLabel(string)` | `HLine` | Set label |

**Example:**

```csharp
var hline = new HLine { X = 2, Y = 4, Width = 40, Height = 1 }
    .WithLabel("Section Break");
```

---

### `VLine` (Vertical Line)

Vertical separator line.

No additional properties beyond `Control`.

**Example:**

```csharp
var vline = new VLine { X = 20, Y = 2, Width = 1, Height = 10 };
```

---

### `ProgressBar`

Progress bar with optional percentage display.

**Properties:**

| Property | Type | Default | Description |
|---|---|---|---|
| `Min` | `int` | `0` | Minimum value |
| `Max` | `int` | `100` | Maximum value |
| `Value` | `int` | `0` | Current value |
| `ShowPercent` | `bool` | `true` | Show percentage text |

**Fluent Methods:**

| Method | Returns | Description |
|---|---|---|
| `WithRange(int, int)` | `ProgressBar` | Set range |
| `WithValue(int)` | `ProgressBar` | Set value (clamped) |

**Example:**

```csharp
var pb = new ProgressBar { X = 2, Y = 8, Width = 30, Height = 1 }
    .WithRange(0, 100)
    .WithValue(67);

// Output: [████████████████████····] 67%
```

---

### `StatusBar`

Full-width status bar at the bottom of the screen.

**Properties:**

| Property | Type | Default | Description |
|---|---|---|---|
| `Text` | `string` | `""` | Status text |

**Constructor:** Sets `Height = 1` and default style for status bar appearance.

**Fluent Methods:**

| Method | Returns | Description |
|---|---|---|
| `WithText(string)` | `StatusBar` | Set text |

**Example:**

```csharp
var sb = new StatusBar { X = 0, Y = 24, Width = 80 }
    .WithText("Ready | Line: 42 | Col: 7");
```

---

### `Panel`

Bordered container for grouping controls.

**Border Types:** `None`, `Single`, `Double`, `Rounded`

**Properties:**

| Property | Type | Default | Description |
|---|---|---|---|
| `Title` | `string?` | `null` | Panel title |
| `Border` | `PanelBorder` | `Single` | Border style |
| `Draggable` | `bool` | `true` | Can be dragged by title bar |
| `IsDragging` | `bool` | `false` | Drag state |
| `DragOffsetX/Y` | `int` | `0` | Mouse offset during drag |

**Fluent Methods:**

| Method | Returns | Description |
|---|---|---|
| `WithTitle(string?)` | `Panel` | Set title |
| `WithBorder(PanelBorder)` | `Panel` | Set border style |

**Computed Property:**

| Property | Description |
|---|---|
| `InnerArea` | `ScreenRect` inside the border |

**Example:**

```csharp
var panel = new Panel { X = 2, Y = 2, Width = 40, Height = 12 }
    .WithTitle("Settings Panel")
    .WithBorder(PanelBorder.Rounded);

// Use panel.InnerArea to layout child controls
```

---

### `Window`

Title bar window with close button and drag support.

**Properties:**

| Property | Type | Default | Description |
|---|---|---|---|
| `Title` | `string` | `""` | Window title |
| `Border` | `PanelBorder` | `Single` | Border style |
| `Closable` | `bool` | `true` | Show close button (×) |
| `Draggable` | `bool` | `true` | Can be dragged by title bar |
| `IsDragging` | `bool` | `false` | Drag state |

**Fluent Methods:**

| Method | Returns | Description |
|---|---|---|
| `WithTitle(string)` | `Window` | Set title |
| `WithBorder(PanelBorder)` | `Window` | Set border style |
| `WithClosable(bool)` | `Window` | Toggle close button |

**Computed Property:** `InnerArea` — content area inside border.

**Example:**

```csharp
var win = new Window { X = 5, Y = 3, Width = 50, Height = 20 }
    .WithTitle("Document Editor")
    .WithBorder(PanelBorder.Double);
```

---

### `ScrollView`

Scrollable content view.

**Properties:**

| Property | Type | Default | Description |
|---|---|---|---|
| `Content` | `List<string>` | `[]` | Content lines |
| `Offset` | `int` | `0` | Scroll offset |

**Fluent Methods:**

| Method | Returns | Description |
|---|---|---|
| `WithContent(List<string>)` | `ScrollView` | Set content |

**Methods:**

| Method | Description |
|---|---|
| `Push(string line)` | Add line to content |
| `ScrollFraction` | `double` — current scroll position (0–1) |

**Key Bindings:**

| Key | Action |
|---|---|
| `UP` / `DOWN` | Scroll |
| `PGUP` / `PGDN` | Page scroll |
| `HOME` / `END` | Top/bottom |

**Example:**

```csharp
var sv = new ScrollView { X = 2, Y = 2, Width = 40, Height = 10 }
    .WithContent(File.ReadAllLines("log.txt").ToList());
```

---

### `LogView`

Colored log viewer with level filtering.

**Levels:** `Trace`, `Debug`, `Info`, `Warn`, `Error`

**Types:**

```csharp
public class LogRecord
{
    public LogLevel Level { get; set; }
    public string Target { get; set; } = "";
    public string Message { get; set; } = "";
}
```

**Properties:**

| Property | Type | Default | Description |
|---|---|---|---|
| `Records` | `List<LogRecord>` | `[]` | All records |
| `Capacity` | `int` | `1000` | Max records (oldest trimmed) |
| `MinLevel` | `LogLevel` | `Trace` | Minimum level to display |
| `Offset` | `int` | `0` | Scroll offset |

**Fluent Methods:**

| Method | Returns | Description |
|---|---|---|
| `WithMinLevel(LogLevel)` | `LogView` | Set minimum level |
| `WithCapacity(int)` | `LogView` | Set capacity |

**Methods:**

| Method | Description |
|---|---|
| `Push(LogLevel, target, message)` | Add log record |
| `ScrollFraction` | `double` — current scroll position |

**Example:**

```csharp
var logview = new LogView { X = 2, Y = 2, Width = 60, Height = 15 }
    .WithMinLevel(LogLevel.Info)
    .WithCapacity(500);

logview.Push(LogLevel.Info, "System", "Application started");
logview.Push(LogLevel.Error, "Database", "Connection failed");
```

---

### `Dashboard`

Metrics dashboard with bar charts.

**Type:**

```csharp
public class Metric
{
    public string Label { get; set; } = "";
    public float Value { get; set; }
    public float Max { get; set; } = 100;
    public string Unit { get; set; } = "";
}
```

**Properties:**

| Property | Type | Default | Description |
|---|---|---|---|
| `Metrics` | `List<Metric>` | `[]` | Metrics to display |
| `SelectedIndex` | `int` | `0` | Selected metric |
| `BarWidth` | `int` | `10` | Bar segment width |

**Fluent Methods:**

| Method | Returns | Description |
|---|---|---|
| `WithMetrics(List<Metric>)` | `Dashboard` | Set metrics |

**Example:**

```csharp
var dash = new Dashboard { X = 2, Y = 2, Width = 30, Height = 5 }
    .WithMetrics([
        new Metric { Label = "CPU", Value = 45, Max = 100, Unit = "%" },
        new Metric { Label = "RAM", Value = 6.2f, Max = 16, Unit = "GB" }
    ]);
```

---

### `ScrollBar`

Vertical or horizontal scroll bar with draggable thumb.

**Properties:**

| Property | Type | Default | Description |
|---|---|---|---|
| `Orientation` | `Orientation` | `Vertical` | `Vertical` or `Horizontal` |
| `Min` | `int` | `0` | Minimum position |
| `Max` | `int` | `100` | Maximum position |
| `Position` | `int` | `0` | Current position |
| `Page` | `int` | `10` | Page size (thumb proportion) |

**Fluent Methods:**

| Method | Returns | Description |
|---|---|---|
| `WithRange(int, int)` | `ScrollBar` | Set range |
| `WithPage(int)` | `ScrollBar` | Set page size |
| `WithPosition(int)` | `ScrollBar` | Set position (clamped) |

**Mouse:** Click track to page, drag thumb.

**Example:**

```csharp
var scrollbar = new ScrollBar { X = 40, Y = 2, Width = 1, Height = 10 }
    .WithRange(0, 200)
    .WithPage(10)
    .WithPosition(50);
```

---

## Dialog Controls

### `AlertBox`

Modal dialog with message and OK button.

**Properties:**

| Property | Type | Default | Description |
|---|---|---|---|
| `Message` | `string` | `""` | Alert message |
| `Title` | `string` | `""` | Dialog title |
| `OKButton` | `Button` | `"OK"` | OK button |
| `Border` | `PanelBorder` | `Single` | Border style |
| `Draggable` | `bool` | `true` | Drag support |
| `IsDragging` | `bool` | `false` | Drag state |

**Static Factory:**

| Method | Description |
|---|---|
| `Centered(termW, termH, title, message)` | Create centered alert box |

**Key Bindings:**

| Key | Action |
|---|---|
| `ENTER` / `ESC` / `SPACE` | Dismiss |

**Example:**

```csharp
var alert = AlertBox.Centered(80, 24, "Notice", "Operation completed successfully.");
```

---

### `ConfirmationBox`

Modal confirmation dialog with up to 3 buttons.

**Choice enum:** `Confirm`, `Cancel`, `Retry`

**Properties:**

| Property | Type | Default | Description |
|---|---|---|---|
| `Message` | `string` | `""` | Confirmation message |
| `Title` | `string?` | `null` | Dialog title |
| `Border` | `PanelBorder` | `Double` | Border style |
| `ConfirmButton` | `Button` | `"OK"` | Confirm button |
| `CancelButton` | `Button` | `"Cancel"` | Cancel button |
| `RetryButton` | `Button?` | `null` | Optional retry button |
| `FocusedChoice` | `ConfirmationChoice` | `Confirm` | Currently focused button |
| `Draggable` | `bool` | `true` | Drag support |

**Fluent Methods:**

| Method | Returns | Description |
|---|---|---|
| `WithTitle(string?)` | `ConfirmationBox` | Set title |
| `WithMessage(string)` | `ConfirmationBox` | Set message |
| `WithButtons(confirm, cancel, retry?)` | `ConfirmationBox` | Set button labels |

**Static Factories:**

| Method | Description |
|---|---|
| `CenteredConfirmCancel(w, h, title, msg)` | Full-params centered |
| `CenteredConfirmCancel(termW, termH, title, msg)` | Terminal-aware centered |

**Key Bindings:**

| Key | Action |
|---|---|
| `TAB` / `LEFT` / `RIGHT` | Cycle buttons |
| `ENTER` | Activate focused button |

**Example:**

```csharp
var confirm = ConfirmationBox.CenteredConfirmCancel(80, 24, "Save?", "Save changes before closing?");
```

---

### `FileDialog`

File/directory listing dialog.

**Type:**

```csharp
public class FileEntry
{
    public string Name { get; set; } = "";
    public string FullPath { get; set; } = "";
    public bool IsDir { get; set; }
}
```

**Properties:**

| Property | Type | Default | Description |
|---|---|---|---|
| `Entries` | `List<FileEntry>` | `[]` | File entries |
| `SelectedIndex` | `int` | `0` | Selected index |
| `ScrollOffset` | `int` | `0` | Scroll offset |

**Constructor:** Sets `Height = 8`.

**Fluent Methods:**

| Method | Returns | Description |
|---|---|---|
| `WithEntries(List<FileEntry>)` | `FileDialog` | Set entries |

**Example:**

```csharp
var fd = new FileDialog { X = 10, Y = 5, Width = 40 }
    .WithEntries([
        new FileEntry { Name = "Documents", FullPath = "/home/user/docs", IsDir = true },
        new FileEntry { Name = "readme.txt", FullPath = "/home/user/readme.txt", IsDir = false }
    ]);
```

---

### `FileExplorer`

Full file system browser with navigation, icons, and search.

**Properties:**

| Property | Type | Default | Description |
|---|---|---|---|
| `CurrentPath` | `string` | `""` | Current directory |
| `ShowHidden` | `bool` | `false` | Show hidden files |
| `ShowIcons` | `bool` | `true` | Show file type icons |
| `LastAction` | `FileExplorerAction` | `None` | Last user action |
| `SelectedPath` | `string?` | `null` | Selected file path |

**Fluent Methods:**

| Method | Returns | Description |
|---|---|---|
| `WithPath(string)` | `FileExplorer` | Set initial path |
| `WithShowIcons(bool)` | `FileExplorer` | Toggle icons |
| `WithShowHidden(bool)` | `FileExplorer` | Toggle hidden files |

**Key Bindings:**

| Key | Action |
|---|---|
| `UP` / `DOWN` | Navigate |
| `ENTER` | Enter directory / confirm selection |
| `BS` / `BACKSPACE` | Go to parent directory |
| `/` | Search mode |
| `HOME` / `END` | First/last entry |

**Example:**

```csharp
var explorer = new FileExplorer { X = 2, Y = 2, Width = 50, Height = 20 }
    .WithPath(Environment.CurrentDirectory)
    .WithShowIcons(true);
```

---

### `PrinterDialog`

Printer selection dialog with copies and duplex options.

**Properties:**

| Property | Type | Default | Description |
|---|---|---|---|
| `Printers` | `List<string>` | Built-in list | Printer names |
| `SelectedIndex` | `int` | `0` | Selected printer |
| `Copies` | `ushort` | `1` | Number of copies |
| `Duplex` | `bool` | `false` | Duplex printing |
| `Confirmed` | `bool` | `false` | User confirmed |
| `Border` | `PanelBorder` | `Double` | Border style |
| `Title` | `string?` | `"Select Printer"` | Dialog title |
| `OKButton` | `Button` | `"OK"` | OK button |
| `CancelButton` | `Button` | `"Cancel"` | Cancel button |

**Fluent Methods:**

| Method | Returns | Description |
|---|---|---|
| `WithTitle(string?)` | `PrinterDialog` | Set title |
| `WithPrinters(List<string>)` | `PrinterDialog` | Set printer list |
| `WithButtons(ok, cancel)` | `PrinterDialog` | Set button labels |

**Example:**

```csharp
var pd = new PrinterDialog { X = 10, Y = 5, Width = 40, Height = 14 }
    .WithTitle("Print Document");
```

---

## Buffer Widgets (`BufferWidgets.cs`)

Simple drawing primitives for `ScreenBuffer` — no focus/keyboard handling.

### `BorderBox`

Single-line border box with optional centered title.

| Field | Type | Default | Description |
|---|---|---|---|
| `BorderColor` | `CellColor` | `Cyan` | Border color |
| `Title` | `string?` | `null` | Centered title text |

**Example:**

```csharp
var border = new BorderBox { Title = "Output", BorderColor = CellColor.Green };
border.Draw(buffer, new Rect(2, 2, 40, 20));
```

### `Paragraph`

Word-wrapped styled text block.

| Field | Type | Default | Description |
|---|---|---|---|
| `Spans` | `List<RichSpan>` | `[]` | Styled text spans |

**Example:**

```csharp
var p = new Paragraph();
p.Spans.Add(new RichSpan { Text = "Hello ", Fg = CellColor.White });
p.Spans.Add(new RichSpan { Text = "World", Fg = CellColor.Yellow });
p.Draw(buffer, new Rect(4, 4, 30, 5));
```

### `List`

Scrollable list with selection highlight.

| Field | Type | Default | Description |
|---|---|---|---|
| `Items` | `List<string>` | `[]` | Display items |
| `Selected` | `int` | `-1` | Selected index |
| `SelFg` | `CellColor` | `Black` | Selection text color |
| `SelBg` | `CellColor` | `Cyan` | Selection background |

### `Tabs`

Horizontal tab bar with selection highlight.

| Field | Type | Default | Description |
|---|---|---|---|
| `Titles` | `List<string>` | `[]` | Tab titles |
| `Selected` | `int` | `0` | Selected tab |
| `SelFg` | `CellColor` | `Black` | Active text color |
| `SelBg` | `CellColor` | `Cyan` | Active background |

### `Table`

Bordered table with headers and data rows.

| Field | Type | Default | Description |
|---|---|---|---|
| `Headers` | `List<string>` | `[]` | Column headers |
| `Rows` | `List<List<string>>` | `[]` | Data rows |
| `BorderColor` | `CellColor` | `Cyan` | Border color |

---

## Canvas Controls

### `BrailleCanvas`

High-resolution canvas using Unicode Braille characters (2×4 dots per cell).

**Properties:**

| Property | Type | Description |
|---|---|---|
| `Width` | `int` | Dot width |
| `Height` | `int` | Dot height |

**Methods:**

| Method | Description |
|---|---|
| `SetDot(x, y)` | Turn on a dot |
| `ClearDot(x, y)` | Turn off a dot |
| `ToggleDot(x, y)` | Toggle a dot |
| `GetDot(x, y)` | Check dot state |
| `Clear()` | Clear all dots |
| `RenderToBuffer(buffer, ox, oy)` | Render to ScreenBuffer |
| `DrawLine(x1,y1,x2,y2)` | Bresenham line |
| `DrawCircle(cx, cy, r)` | Midpoint circle |
| `DrawRect(x, y, w, h)` | Rectangle outline |
| `BrailleChar(byte dots)` | `static` — encode dots as Braille char |

**Example:**

```csharp
var canvas = new BrailleCanvas(80, 48);
canvas.DrawLine(0, 0, 40, 24);
canvas.DrawCircle(40, 24, 10);
canvas.RenderToBuffer(buffer, 0, 0);
```

### `HalfBlockCanvas`

Canvas using half-block characters (▀▄) — 2 pixels per cell vertically.

**Properties:**

| Property | Type | Description |
|---|---|---|
| `Width` | `int` | Pixel width |
| `Height` | `int` | Pixel height |

**Methods:**

| Method | Description |
|---|---|
| `Clear(char fill)` | Clear with fill char |
| `Clear(CellColor? bg)` | Clear with background color |
| `Set(x, y, CellColor)` | Set pixel color |
| `Print(x, pixelY, text, fg, bg)` | Render text at pixel position |
| `RenderToBuffer(buffer, ox, oy)` | Render to ScreenBuffer |

**Example:**

```csharp
var hb = new HalfBlockCanvas(80, 48, ' ');
hb.Set(10, 10, CellColor.Red);
hb.RenderToBuffer(buffer, 0, 0);
```

---

## Other Controls

### `QrWidget`

QR code generator and renderer.

**Constructor:**

```csharp
public QrWidget(ScreenRect area, string text, EccLevel level = EccLevel.M, bool border = true);
```

**ECC Levels:** `L` (7%), `M` (15%), `Q` (25%), `H` (30%)

**Methods:**

| Method | Returns | Description |
|---|---|---|
| `SetText(string)` | `void` | Set QR content |
| `Size()` | `(int w, int h)` | QR code dimensions |
| `Draw(buffer, x, y)` | `bool` | Render to buffer |

**Example:**

```csharp
var qr = new QrWidget(new ScreenRect(0, 0, 30, 30), "https://example.com", EccLevel.M);
qr.Draw(buffer, 2, 2);
```

### `ImageWidget`

Image display widget — supports Kitty and Sixel protocols.

**Properties:**

| Property | Type | Default | Description |
|---|---|---|---|
| `ImgW` | `int` | `0` | Image width |
| `ImgH` | `int` | `0` | Image height |
| `Pixels` | `CellColor?[,]` | `null` | Pixel color array |
| `PixelAt` | `Func<int,int,CellColor?>` | `null` | Lazy pixel provider |
| `Protocol` | `GraphicsProtocol` | `None` | Kitty/Sixel |
| `LastClickPixelX/Y` | `int` | `-1` | Last click pixel coordinates |

**Methods:**

| Method | Description |
|---|---|
| `SetPixels(CellColor?[,])` | Set pixel data |
| `RenderProtocol()` | Render protocol string |

**Example:**

```csharp
var img = new ImageWidget { X = 2, Y = 2, Width = 20, Height = 20, Protocol = GraphicsProtocol.Kitty };
img.SetPixels(pixelData);
```

### `RichText / RichSpan`

Styled text spans with fg/bg colors and bold/italic/underline attributes.

**RichSpan Fields:**

| Field | Type | Default | Description |
|---|---|---|---|
| `Text` | `string` | `""` | Span text |
| `Fg` | `CellColor?` | `null` | Foreground color |
| `Bg` | `CellColor?` | `null` | Background color |
| `Bold` | `bool` | `false` | Bold |
| `Italic` | `bool` | `false` | Italic |
| `Underline` | `bool` | `false` | Underline |

### `AnsiStr`

ANSI string handling utilities.

### `Charts`

Chart rendering utilities.

### `CodeArea`

Code editor area with syntax highlighting and line numbers.

**Properties:**

| Property | Type | Default | Description |
|---|---|---|---|
| `Syntax` | `Syntax` | `new()` | Syntax definition |
| `ShowLineNumbers` | `bool` | `true` | Show gutter |
| `GutterColor` | `CellColor` | `Grey` | Gutter text color |

**Syntax class:**

| Method | Description |
|---|---|
| `AddSymbol(char, CellColor)` | Define color for a character |
| `Colorize(string)` | Apply syntax colors to line |

### `Repl<T>`

Interactive REPL with command registration, history, tab-completion.

**Result enum:** `Continue`, `Exit`, `ClearScreen`

**Delegate:** `CommandHandler<T>` = `ReplResult handler(T state, string[] args)`

**Properties:**

| Property | Type | Default | Description |
|---|---|---|---|
| `State` | `T` | (constructor) | Mutable state |
| `Prompt` | `string` | `"> "` | Input prompt |
| `History` | `List<string>` | `[]` | Command history |

**Methods:**

| Method | Description |
|---|---|
| `Register(name, desc, handler)` | Register command |
| `RegisterCompleter(ICompleter)` | Tab-completion source |
| `LoadHistory(path)` | Load from file |
| `SaveHistory(path)` | Save to file |
| `Eval(line, out error)` | Evaluate a line |
| `Run()` | Start interactive loop |
| `Complete(line, pos)` | Get completions |

**Example:**

```csharp
var repl = new Repl<string>("initial", "cli> ");
repl.Register("hello", "Say hello", (state, args) => { Console.WriteLine("Hi!"); return ReplResult.Continue; });
repl.Run();
```

---

## Types

### `CellColor` (struct, `ClipperLib.Clipper`)

Terminal color with 24-bit RGB.

**Fields:**

| Field | Type | Description |
|---|---|---|
| `R, G, B` | `byte` | RGB components |
| `IsDefault` | `bool` | True if unset |

**Static Colors:** `Black`, `White`, `Red`, `Green`, `Blue`, `Yellow`, `Cyan`, `Magenta`, `Grey` + dark variants + `Default`

### `ScreenBuffer` (`ClipperLib.Clipper`)

Rectangular buffer of `ScreenCell` values for double-buffered rendering.

**Properties:**

| Property | Type | Description |
|---|---|---|
| `Width` | `int` | Buffer width |
| `Height` | `int` | Buffer height |
| `Cells` | `ScreenCell[,]` | Cell grid |

**Methods:**

| Method | Description |
|---|---|
| `Clear(fg?, bg?)` | Clear entire buffer |
| `SetChar(x,y,ch,fg?,bg?)` | Set single cell |
| `SetString(x,y,s,fg?,bg?)` | Set string (stops at buffer edge) |
| `SetStringN(x,y,s,maxLen,fg?,bg?)` | Set string with max length |
| `FillRect(t,l,b,r,ch,fg?,bg?)` | Fill rectangular area |
| `Clone()` | Deep copy |
| `Resize(w,h)` | Resize buffer |

### `ScreenRect` (struct, `ClipperLib.Clipper`)

Rectangle with position and size.

**Properties:** `X, Y, W, H`, `Right`, `Bottom`

**Methods:** `Contains(x,y)`, `Intersect(other)`

### `ScreenCell` (struct, `ClipperLib.Clipper`)

Single cell: `char Char`, `CellColor Fg`, `CellColor Bg`

### `ScreenPoint` (struct, `ClipperLib.Clipper`)

Point: `int X`, `int Y`

### `ScreenPoint` (struct `ClipperLib.Types`)

Independent point type: `int X`, `int Y`

---

## Themes

### `Theme` class

Color palette for windows, panels, and controls.

**Properties:**

| Property | Type | Description |
|---|---|---|
| `DesktopBg` | `CellColor` | Desktop background |
| `WindowBg` | `CellColor` | Window background |
| `TitleFocus` | `CellColor` | Focused title bar bg |
| `TitleBlur` | `CellColor` | Blurred title bar bg |
| `TitleFg` | `CellColor` | Title text |
| `Border` | `CellColor` | Border lines |
| `CtrlFg` | `CellColor` | Control text |
| `CloseFg` | `CellColor` | Close button |
| `Text` | `CellColor` | Body text |
| `Dim` | `CellColor` | Dimmed text |
| `Accent` | `CellColor` | Accent color |
| `ActiveBg` | `CellColor` | Active element bg |
| `Shadow` | `CellColor` | Shadow color |

**Static Factories:**

| Method | Description |
|---|---|
| `Midnight()` | Dark blue theme |
| `Nord()` | Nord palette |
| `Dracula()` | Dracula palette |
| `Turbo()` | Turbo Vision (blue) |
| `Named(string)` | `"midnight"`, `"nord"`, `"dracula"`, else `Turbo()` |
| `Default()` | `Turbo()` |

**Usage:**

```csharp
ControlStyle.SetTheme(Theme.Dracula());
// New controls now use Dracula colors by default
```

---

## Colors Utility (`Colors.cs`)

### `Rgb` struct

`double R, G, B` (0–1 range). Methods: `ToCellColor()`, `ToU8()`.

### `Gradient` class

Linear gradient with stops and blend modes (`Linear`, `LinearRgb`, `Oklab`, `Hsv`, `Hsl`).

**Methods:** `At(double t)` → `CellColor`, `Samples(int n)` → `CellColor[]`

### `ColorLevel` enum

Color fidelity levels for terminal detection.

---

 ## Clipper 5.3 Emulation (`ClipperLib.Clipper`)

> **Full technical reference:** `docs/ClipperLib/clipper53.md` (every function, the
> PICTURE grammar, the GET/READ editing model, validation, and the headless test harness).
> The legacy loops run on the same unified `EventLoop`/`BlinkClock` as the Controls
> framework and the `IApp` apps.

### `ClipperSet` — SET functions

Getter/setter overloads (call with no argument to read the current value).

`SetConfirm`, `SetWrap`, `SetDelimiters`, `SetEscape`, `SetBell`, `SetTypeahead`,
`SetIntensity`, `SetDateFormat`, `SetCentury`, `SetEpoch`, `SetDecimals`, `SetFixed`,
`SetPrinter`, `SetDevice`, `SetMessage`/`Message`.

### `ClipperOutput`

SAY/BOX/CLEAR/LINE output.

**Methods:** `SetColor`, `SetColorTo`, `Row`, `Col`, `SetPos`/`DevPos`, `Say`, `SayPic`,
`SayColor`, `DevOut`, `DevOutPic`, `DispOut`, `QOut`, `QQOut`, `TextBlock`, `Box`,
`DispBox`, `ClearScreen`, `ClearRegion`, `Beep`, `SetCursor`/`GetCursor`, `Render`,
`RenderToBuffer`.

### `ClipperInput`

INKEY/READKEY/SET KEY input.

**Methods:** `InKey()`, `InKeyTimeout(ms)`, `InKeyTimeoutEx(ms)`, `NextKey()`, `LastKey()`,
`ReadKey()`, `Keyboard(string)`, `SetKey(n, Action)`, `Accept(ref buf)`, `Wait(secs?)`,
`SetKeySource(Func<int>)`, `ClearKeySource()`, `SetRenderHook(Action<ScreenBuffer>)`,
`DisableMouseCapture()`.

### `ClipperRead` — GET/READ

**Methods:** `AddGet(Get)`, `Read()`, `ResetGets()`, `DrawGets()`, `GetActive()`,
`GetAt(col, row)`, `ReadVar()`, `Updated()`, `ReadExit()`, `ApplyAll()`, `IsValid(Get)`.

**`Get` Properties:** `Row`, `Col`, `Value`, `Width`, `Fill`, `Picture`, `Valid`, `Range`,
`AcceptKey`, `Rejected`, `Color`, `FocusColor`, `ErrorColor`, `When`, `Apply`, `Cur`,
`Sel`, `Changed`, `Updated`, `ScrollPos`.

The PICTURE is enforced **at input time**: the typed character must match the template
placeholder under the cursor (`9`→digits, `#`→digits/sign, `A`→letters, `N`→letters+digits,
`L`/`Y`→T-F/Y-N, `X`/`!`→any) and is rejected once the template slots are full.
`AcceptKey` blocks application-level invalid keys *before* insertion; `Rejected` fires on
every keypress rejection; `Valid`/`Range` (see `IsValid`) drive the live error color
(`ErrorColor`, the "email" pattern) and refuse Enter on invalid fields.

**`GetBuilder`:** `Picture`, `Color`, `FocusColor`, `ErrorColor`, `Message`, `Valid`,
`AcceptKey`, `Rejected`, `When`, `Range(lo, hi)`, `Width`, `Fill` + `ForValue(row, col, ref int)`
and `ForString(row, col, ref string)`.

### `ClipperAChoice`

**Method:** `AChoice(top, left, bottom, right, items, precheck, msg)` → 1‑based index or 0 for ESC

### `ClipperMemoEdit`

**Method:** `MemoEdit(text, row, col, width, height, editMode, keyProc, userFunc)` → edited text

### `ClipperDates` — DATE/DTOC/CTOD

`Date()`, `Time()`, `Seconds()`, `Year(d)`, `Month(d)`, `Day(d)`, `Dow(d)`, `CDow(d)`,
`CMonth(d)`, `Dtoc(d)`, `Dtos(d)`, `Ctod(s)` → `DateTime?`.

### `ClipperStrings` — string functions

`At`, `Rat`, `Substr`, `Stuff`, `StrTran`, `Left`, `Right`, `Upper`, `Lower`, `IsAlpha`,
`IsDigit`, `IsLower`, `IsUpper`, `Len`, `Trim`/`RTrim`/`LTrim`/`AllTrim`,
`PadL`/`PadC`/`PadR`, `Replicate`, `Space`, `Transform`, `Val`, `Str`, `StrZero`, `Asc`,
`Chr`, `Empty`, `ValType`, `Type`.

### `ClipperNumbers` — numeric functions

`Abs`, `Int`, `Round`, `Sqrt`, `Exp`, `Log`, `Log10`, `Max`, `Min`, `Mod`, `Sign`.

### `ClipperScrn` — screen functions

`MaxCol()`, `MaxRow()`, `Scroll(top, left, bottom, right, rows)`, `SaveScreen([region])`,
`RestScreen(s[, top, left])`, `TColor()`, `SetBlink(bool?)`, `Prompt(row, col, text)`,
`Message()`.

### `ClipperPrinter` — printer functions

`SetPrinter(bool?)`, `SetDevice(string?)`, `Prow()`, `Pcol()`, `Eject()`, `PrinterText()`.

### `ClipperMouse` — mouse state

`IsMouse()`, `MRow()`, `MCol()`, `MLeft/MRight/MMiddle(+Press/+Less/+Shift/+Control/+M)`,
`MouStat(button, out row, out col, out mods)`, `MPosition(row?, col?)`,
`MouseCursor(row?, col?, type?)`, `MRelease()`, `UpdateFromEvent(MouseAction)`.

### `FormatPicture`

PICTURE grammar: `@`-functions `A B C D E F I J K L<c> N R S<n> T X Z ( ) $ !`
and template symbols `A N X 9 # L Y ! $ * . , 0`.

**Method:** `Apply(value, picture)` → formatted string (string / `double` / `int` /
`DateTime` / `bool` overloads).

**Examples:** `@R (999) 999` → `(123) 456`; `@$ 999,999.99` → `$  1,200.00`;
`@( 999,999.99` → `( 1,234.50 )`; `@Z 9999` → blanks for zero; `99/99/9999` date templates;
`@E` British dates; `@!` / `@A` / `@N` / `@J` text transforms.

### Key Constants

| Constant | Value | Description |
|---|---|---|
| `K_LEFT` | 19 | Left arrow |
| `K_RIGHT` | 4 | Right arrow |
| `K_UP` | 5 | Up arrow |
| `K_DOWN` | 24 | Down arrow |
| `K_ENTER` | 13 | Enter |
| `K_ESC` | 27 | Escape |
| `K_TAB` | 9 | Tab |
| `K_BTAB` | 15 | Shift+Tab |
| `K_BS` | 127 | Backspace |
| `K_INS` | 22 | Insert |
| `K_DEL` | 7 | Delete |
| `K_HOME` | 1 | Home |
| `K_END` | 6 | End |
| `K_PGUP` | 18 | Page Up |
| `K_PGDN` | 3 | Page Down |
| `K_F1..F10` | 28..37 | Function keys |

**Mouse:** `K_MOUSE_BASE = -1000`

---

## Event System

### `UiEvent` (discriminated union)

| Variant | Description |
|---|---|
| `UiKey(int key, KeyModifiers mods)` | Keyboard event |
| `UiMouse(MouseAction action)` | Mouse event |
| `UiResize(int width, int height)` | Terminal resize (NAWS / window resize) |
| `UiPaste(string text)` | Pasted text |
| `UiNone` | No event / timeout |

### Unified event engine

A single event-driven engine powers the app framework (`AppRunner`), the Controls
`EventHandler`, the legacy Clipper loops (GET/READ, `ClipperForm.Run`, MEMOEDIT)
and the telnet server. All loops consume the same `EventLoop` + `BlinkClock`.

| Component | Namespace | Role |
|---|---|---|
| `IInputSource` | `ClipperLib.App` | Transport contract: `UiEvent Read(int timeoutMs)`. Implementations: `ClipperInputSource` (console, honours `KeySource`/`Typeahead` first), `ConsoleInputSource`, `TelnetInputSource` |
| `EventLoop` | `ClipperLib.App` | Shared host loop: `UiEvent Next(int timeoutMs)` (resets blink on real input) + `bool BlinkTick()` (advances the clock, true when a redraw is needed) |
| `BlinkClock` | `ClipperLib.App` | Single blink timer (period `KeyConstants.BLINK_MS`); syncs both `ClipperState.BlinkOn` (legacy) and `Control.BlinkOn` (Controls) |
| `ClipperInputSource` | `ClipperLib.App` | Legacy adapter: console keys/mouse as `UiEvent`; used by `InKey`/`InKeyTimeout`/`InKeyTimeoutEx` (now thin wrappers) and the GET/FORM/MEMO loops |
| `InkeyAdapter` | `ClipperLib.Clipper` | Bidirectional `UiEvent ⇄ int` INKEY conversion (`ToInkey` / `ToEvent`) |
| `ModalHost` | `ClipperLib.App` | Session nesting: runs an `IApp` (`RunApp`) or a modal function (`RunModal`) on the same loop; `ModalHost.Current` is the ambient host, so a `ClipperForm.Run()` started from an `IApp` shares its `EventLoop` |
| `ConsoleRenderer.PositionCursor` | `ClipperLib.App` | Places/shows-hides the system cursor from `IApp.Caret` / `IApp.CursorVisible` after each frame |

The legacy API (`ClipperInput.InKey*`, `SetKeySource`, `Keyboard`, `SetKey`,
`SetRenderHook`, `Wait`, `Accept`) is unchanged; internally it delegates to the
unified engine, so headless tests driven via `KeySource` keep working.

### `MouseAction`

| Property | Type | Description |
|---|---|---|
| `Kind` | `MouseActionKind` | Click/DoubleClick/ScrollUp/ScrollDown/Drag/RightClick/Release |
| `X` | `int` | Column |
| `Y` | `int` | Row |
| `FromInkeyCode(int)` | `static MouseAction?` | Decode INKEY mouse code |

### `EventHandler`

Central event dispatcher with focus management.

**Properties:**

| Property | Type | Description |
|---|---|---|
| `FocusedControl` | `Control?` | Currently focused control |

**Methods:**

| Method | Description |
|---|---|
| `Register(string id, Control ctrl)` | Register control by ID |
| `SetFocusById(string id)` | Set focus to control |
| `Dispatch(UiEvent e)` | Route event, returns true if handled |
| `EventToNavigation(UiEvent)` | `static` — convert to navigation action |
| `IsQuitEvent(UiEvent)` | `static` — check for ESC/Ctrl+Q |
| `ReadEvent()` | `static` — read terminal event |
| `ReadEventTimeout(ms)` | `static` — read with timeout |

### `FocusManager`

Tab-order and point-to-focus management.

**Methods:** `Register(id, area)`, `Register(id, Control)`, `SetFocusById(id)`, `SetFocusByPoint(x, y)`, `Next()`, `Previous()`, `HandleKey(key)`, `HandleEvent(UiEvent)`, `Clear()`

### `NavigationAction` enum

`None`, `Next`, `Previous`, `First`, `Last`, `PageUp`, `PageDown`, `Accept`, `Cancel`, `ClickAt`, `ScrollUp`, `ScrollDown`

### `IMouseTarget` interface

`Contains(x,y)`, `OnClick(x,y)`, `OnDoubleClick(x,y)`, `OnScroll(direction)`, `OnDrag(x,y)`, `OnRightClick(x,y)`

---

## Helper Utilities

### `BoxDrawing`

Box-drawing character sets for `Single`, `Double`, `Rounded`, `Thick`, `Dashed` styles.

### `Validator` validators

| Class | Description |
|---|---|
| `IntValidator(min, max)` | Integer range validation |
| `FloatValidator(min, max)` | Float range validation |
| `FnValidator(fn, message?)` | Custom function validation |
| `RequiredValidator` | Non-empty validation |
| `OneOfValidator(values)` | Must be one of values |

### `FuzzyPattern`

Fuzzy string matching for search/filter.

### `KeyMap<A>`

Generic key-to-action mapping.

### `DialogBuilder`

Dialogue-style questionnaire system.

**Question kinds:** `Input`, `Password`, `Confirm`, `Int`, `Float`, `Select`, `MultiSelect`, `Expand`

### `TilingModel` / `Tiler<T>`

Window tiling layout engines with split, resize, and direction navigation.

### `WindowManager`

Full window management with tile grid, snapping, floating, minimize, maximized states, z-order, drag, and resize.

**Types:** `ManagedWindow`, `TileGrid`, `SnapZone`, `WindowState`

### `Dashboard`

See Data Controls section above.

### `LogView`

See Navigation & Display Controls section above.

### `ScrollView`

See Navigation & Display Controls section above.

### `ProgressBuilder` / `BarBuilder` / `Bar`

Progress bar building utilities with custom fill chars, brackets, and styles.

**BarBuilder Methods:**
| Method | Description |
|---|---|
| `WithWidth(int)` | Set total width |
| `WithFilledChar(char)` | Set filled character (default `█`) |
| `WithEmptyChar(char)` | Set empty character (default `░`) |
| `WithBrackets(l, r)` | Set brackets (default `[`, `]`) |
| `WithStyle(BarFillStyle)` | Set fill style |

**Bar Methods:**
| Method | Description |
|---|---|
| `Render(fraction)` | Render bar without percent |
| `RenderWithPercent(fraction)` | Render bar with percentage |

### `StyleBox`

Box styling with fg/bg, padding, border, alignment.

**Methods:** `WithFg`, `WithBg`, `WithPadding`, `WithBorder`, `WithAlign`

### `SpreadsheetModel`

Spreadsheet data model with column definitions and cell cursor.

### `TableModel`

Table view model with columns, alignment, sort, and reorder.

**Interface:** `ITableViewItem<H>` — `ColumnCount`, `GetValue(col)`, `IsSelected`

### `FirstFit` / `KnuthPlass`

Paragraph layout engines (Knuth-Plass line breaking).

### `Graphics`

Graphics protocol detection and rendering for Kitty/Sixel.

---

## Total: 78+ Control Classes + Clipper Emulation

All controls live in the `ClipperLib.Controls` namespace and share the common `Control` base class. Controls render to `ScreenBuffer` and handle `UiEvent` input. Use `EventHandler` for centralized dispatching with focus management.
