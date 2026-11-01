// FILE: Interpreter.Ast.Controls.cs

namespace Basic;

/// <summary>CHECKBOX var, row, col, "caption"</summary>
[JsonDerivedType(typeof(CheckBoxStmt), nameof(CheckBoxStmt))]
public sealed record ControlProps(Expr? Name, Expr? Tag, Expr? Tooltip, Expr? Visible, Expr? Enabled, Expr? TabIndex, Expr? TabStop, bool Focus = false, Expr? Style = null, Expr? Color = null, Expr? ColorFocus = null, Expr? ColorSelect = null, Expr? ColorError = null, Expr? HasError = null, Expr? Uppercase = null);

public sealed record CheckBoxStmt(Target Variable, Expr Row, Expr Col, Expr Caption, Token Token, Target? Id = null, ControlProps? Props = null, List<ControlEventStmt>? Events = null) : Stmt(Token.Position)
{
    public override string ToString() => $"CHECKBOX {Variable}, {Row}, {Col}, {Caption}";
    public override AstNode DeepClone() => new CheckBoxStmt((Target)Variable.DeepClone(), (Expr)Row.DeepClone(), (Expr)Col.DeepClone(), (Expr)Caption.DeepClone(), (Token)Token.DeepClone());
}

/// <summary>LISTBOX var, row, col, width, height, items()</summary>
[JsonDerivedType(typeof(ListBoxStmt), nameof(ListBoxStmt))]
public sealed record ListBoxStmt(Target Variable, Expr Row, Expr Col, Expr Width, Expr Height, Expr Items, Token Token, Target? Id = null, ControlProps? Props = null, List<ControlEventStmt>? Events = null) : Stmt(Token.Position)
{
    public override string ToString() => $"LISTBOX {Variable}, {Row}, {Col}, {Width}, {Height}, {Items}";
    public override AstNode DeepClone() => new ListBoxStmt((Target)Variable.DeepClone(), (Expr)Row.DeepClone(), (Expr)Col.DeepClone(), (Expr)Width.DeepClone(), (Expr)Height.DeepClone(), (Expr)Items.DeepClone(), (Token)Token.DeepClone());
}

/// <summary>COMBOBOX var, row, col, width, items()</summary>
[JsonDerivedType(typeof(ComboBoxStmt), nameof(ComboBoxStmt))]
public sealed record ComboBoxStmt(Target Variable, Expr Row, Expr Col, Expr Width, Expr Items, Token Token, Target? Id = null, ControlProps? Props = null, List<ControlEventStmt>? Events = null) : Stmt(Token.Position)
{
    public override string ToString() => $"COMBOBOX {Variable}, {Row}, {Col}, {Width}, {Items}";
    public override AstNode DeepClone() => new ComboBoxStmt((Target)Variable.DeepClone(), (Expr)Row.DeepClone(), (Expr)Col.DeepClone(), (Expr)Width.DeepClone(), (Expr)Items.DeepClone(), (Token)Token.DeepClone());
}

/// <summary>MENU var, row, col, items()</summary>
[JsonDerivedType(typeof(MenuStmt), nameof(MenuStmt))]
public sealed record MenuStmt(Target? Variable, Expr Row, Expr Col, Expr Items, Token Token, Target? Id = null, ControlProps? Props = null, List<ControlEventStmt>? Events = null) : Stmt(Token.Position)
{
    public override string ToString() => $"MENU {Variable}, {Row}, {Col}, {Items}";
    public override AstNode DeepClone() => new MenuStmt((Target?)Variable?.DeepClone(), (Expr)Row.DeepClone(), (Expr)Col.DeepClone(), (Expr)Items.DeepClone(), (Token)Token.DeepClone());
}

/// <summary>STATUSBAR "text"</summary>
[JsonDerivedType(typeof(StatusBarStmt), nameof(StatusBarStmt))]
public sealed record StatusBarStmt(Expr Text, Token Token, Target? Id = null, ControlProps? Props = null, List<ControlEventStmt>? Events = null) : Stmt(Token.Position)
{
    public override string ToString() => $"STATUSBAR {Text}";
    public override AstNode DeepClone() => new StatusBarStmt((Expr)Text.DeepClone(), (Token)Token.DeepClone());
}

/// <summary>FILEDIALOG var, "mask", "title"</summary>
[JsonDerivedType(typeof(FileDialogStmt), nameof(FileDialogStmt))]
public sealed record FileDialogStmt(Target Variable, Expr Mask, Expr Title, Token Token, Target? Id = null, ControlProps? Props = null, List<ControlEventStmt>? Events = null) : Stmt(Token.Position)
{
    public override string ToString() => $"FILEDIALOG {Variable}, {Mask}, {Title}";
    public override AstNode DeepClone() => new FileDialogStmt((Target)Variable.DeepClone(), (Expr)Mask.DeepClone(), (Expr)Title.DeepClone(), (Token)Token.DeepClone());
}

/// <summary>PRINTERDIALOG printerVar, copiesVar, duplexVar[, "title"]</summary>
[JsonDerivedType(typeof(PrinterDialogStmt), nameof(PrinterDialogStmt))]
public sealed record PrinterDialogStmt(Target PrinterVar, Target CopiesVar, Target DuplexVar, Expr? Title, Token Token, Target? Id = null, ControlProps? Props = null, List<ControlEventStmt>? Events = null) : Stmt(Token.Position)
{
    public override string ToString() => $"PRINTERDIALOG {PrinterVar}, {CopiesVar}, {DuplexVar}";
    public override AstNode DeepClone() => new PrinterDialogStmt((Target)PrinterVar.DeepClone(), (Target)CopiesVar.DeepClone(), (Target)DuplexVar.DeepClone(), (Expr?)Title?.DeepClone(), (Token)Token.DeepClone());
}

/// <summary>TOGGLE var, row, col, "caption"</summary>
[JsonDerivedType(typeof(ToggleStmt), nameof(ToggleStmt))]
public sealed record ToggleStmt(Target Variable, Expr Row, Expr Col, Expr Caption, Token Token, Target? Id = null, ControlProps? Props = null, List<ControlEventStmt>? Events = null) : Stmt(Token.Position)
{
    public override string ToString() => $"TOGGLE {Variable}, {Row}, {Col}, {Caption}";
    public override AstNode DeepClone() => new ToggleStmt((Target)Variable.DeepClone(), (Expr)Row.DeepClone(), (Expr)Col.DeepClone(), (Expr)Caption.DeepClone(), (Token)Token.DeepClone());
}

/// <summary>RADIOGROUP var, row, col, items()</summary>
[JsonDerivedType(typeof(RadioGroupStmt), nameof(RadioGroupStmt))]
public sealed record RadioGroupStmt(Target Variable, Expr Row, Expr Col, Expr Items, Token Token, Target? Id = null, ControlProps? Props = null, List<ControlEventStmt>? Events = null) : Stmt(Token.Position)
{
    public override string ToString() => $"RADIOGROUP {Variable}, {Row}, {Col}, {Items}";
    public override AstNode DeepClone() => new RadioGroupStmt((Target)Variable.DeepClone(), (Expr)Row.DeepClone(), (Expr)Col.DeepClone(), (Expr)Items.DeepClone(), (Token)Token.DeepClone());
}

/// <summary>SLIDER var, row, col, length, min, max, step</summary>
[JsonDerivedType(typeof(SliderStmt), nameof(SliderStmt))]
public sealed record SliderStmt(Target Variable, Expr Row, Expr Col, Expr Length, Expr Min, Expr Max, Expr Step, Token Token, Target? Id = null, ControlProps? Props = null, List<ControlEventStmt>? Events = null) : Stmt(Token.Position)
{
    public override string ToString() => $"SLIDER {Variable}, {Row}, {Col}, {Length}, {Min}, {Max}, {Step}";
    public override AstNode DeepClone() => new SliderStmt((Target)Variable.DeepClone(), (Expr)Row.DeepClone(), (Expr)Col.DeepClone(), (Expr)Length.DeepClone(), (Expr)Min.DeepClone(), (Expr)Max.DeepClone(), (Expr)Step.DeepClone(), (Token)Token.DeepClone());
}

/// <summary>NUMERICUPDOWN var, row, col, length, min, max, step (come SLIDER, con spinner up/down)</summary>
[JsonDerivedType(typeof(NumericUpDownStmt), nameof(NumericUpDownStmt))]
public sealed record NumericUpDownStmt(Target Variable, Expr Row, Expr Col, Expr Length, Expr Min, Expr Max, Expr Step, Token Token, Target? Id = null, ControlProps? Props = null, List<ControlEventStmt>? Events = null) : Stmt(Token.Position)
{
    public override string ToString() => $"NUMERICUPDOWN {Variable}, {Row}, {Col}, {Length}, {Min}, {Max}, {Step}";
    public override AstNode DeepClone() => new NumericUpDownStmt((Target)Variable.DeepClone(), (Expr)Row.DeepClone(), (Expr)Col.DeepClone(), (Expr)Length.DeepClone(), (Expr)Min.DeepClone(), (Expr)Max.DeepClone(), (Expr)Step.DeepClone(), (Token)Token.DeepClone());
}

[JsonDerivedType(typeof(StarRatingStmt), nameof(StarRatingStmt))]
public sealed record StarRatingStmt(Target? Variable, Expr Row, Expr Col, Expr Length, Expr Max, Expr Value, Token Token, Target? Id = null, ControlProps? Props = null, List<ControlEventStmt>? Events = null) : Stmt(Token.Position)
{
    public override string ToString() => $"STARRATING {Variable}, {Row}, {Col}, {Length}, {Max}, {Value}";
    public override AstNode DeepClone() => new StarRatingStmt((Target?)Variable?.DeepClone(), (Expr)Row.DeepClone(), (Expr)Col.DeepClone(), (Expr)Length.DeepClone(), (Expr)Max.DeepClone(), (Expr)Value.DeepClone(), (Token)Token.DeepClone());
}

[JsonDerivedType(typeof(ScrollBarStmt), nameof(ScrollBarStmt))]
public sealed record ScrollBarStmt(Target Variable, Expr Row, Expr Col, Expr Length, Expr Min, Expr Max, Expr Value, Token Token, Target? Id = null, ControlProps? Props = null, List<ControlEventStmt>? Events = null) : Stmt(Token.Position)
{
    public override string ToString() => $"SCROLLBAR {Variable}, {Row}, {Col}, {Length}, {Min}, {Max}, {Value}";
    public override AstNode DeepClone() => new ScrollBarStmt((Target)Variable.DeepClone(), (Expr)Row.DeepClone(), (Expr)Col.DeepClone(), (Expr)Length.DeepClone(), (Expr)Min.DeepClone(), (Expr)Max.DeepClone(), (Expr)Value.DeepClone(), (Token)Token.DeepClone());
}

[JsonDerivedType(typeof(ScrollViewStmt), nameof(ScrollViewStmt))]
public sealed record ScrollViewStmt(Target Variable, Expr Row, Expr Col, Expr Width, Expr Height, Expr Content, Token Token, Target? Id = null, ControlProps? Props = null, List<ControlEventStmt>? Events = null) : Stmt(Token.Position)
{
    public override string ToString() => $"SCROLLVIEW {Variable}, {Row}, {Col}, {Width}, {Height}";
    public override AstNode DeepClone() => new ScrollViewStmt((Target)Variable.DeepClone(), (Expr)Row.DeepClone(), (Expr)Col.DeepClone(), (Expr)Width.DeepClone(), (Expr)Height.DeepClone(), (Expr)Content.DeepClone(), (Token)Token.DeepClone());
}

[JsonDerivedType(typeof(LabelControlStmt), nameof(LabelControlStmt))]
public sealed record LabelControlStmt(Expr Row, Expr Col, Expr Width, Expr Text, Target? Variable, Token Token, Target? Id = null, ControlProps? Props = null, List<ControlEventStmt>? Events = null) : Stmt(Token.Position)
{
    public override string ToString() => $"LABEL {Row}, {Col}, {Width}";
    public override AstNode DeepClone() => new LabelControlStmt((Expr)Row.DeepClone(), (Expr)Col.DeepClone(), (Expr)Width.DeepClone(), (Expr)Text.DeepClone(), Variable == null ? null : (Target)Variable.DeepClone(), (Token)Token.DeepClone());
}

[JsonDerivedType(typeof(DataListStmt), nameof(DataListStmt))]
public sealed record DataListStmt(Target Variable, Expr Row, Expr Col, Expr Width, Expr Height, Expr Items, Token Token, Target? Id = null, ControlProps? Props = null, List<ControlEventStmt>? Events = null) : Stmt(Token.Position)
{
    public override string ToString() => $"DATALIST {Variable}, {Row}, {Col}, {Width}, {Height}";
    public override AstNode DeepClone() => new DataListStmt((Target)Variable.DeepClone(), (Expr)Row.DeepClone(), (Expr)Col.DeepClone(), (Expr)Width.DeepClone(), (Expr)Height.DeepClone(), (Expr)Items.DeepClone(), (Token)Token.DeepClone());
}

[JsonDerivedType(typeof(PropertyGridStmt), nameof(PropertyGridStmt))]
public sealed record PropertyGridStmt(Target Variable, Expr Row, Expr Col, Expr Width, Expr Height, Expr Items, Token Token, Target? Id = null, ControlProps? Props = null, List<ControlEventStmt>? Events = null) : Stmt(Token.Position)
{
    public override string ToString() => $"PROPERTYGRID {Variable}, {Row}, {Col}, {Width}, {Height}";
    public override AstNode DeepClone() => new PropertyGridStmt((Target)Variable.DeepClone(), (Expr)Row.DeepClone(), (Expr)Col.DeepClone(), (Expr)Width.DeepClone(), (Expr)Height.DeepClone(), (Expr)Items.DeepClone(), (Token)Token.DeepClone());
}

[JsonDerivedType(typeof(LogViewStmt), nameof(LogViewStmt))]
public sealed record LogViewStmt(Target? Variable, Expr Row, Expr Col, Expr Width, Expr Height, Expr Records, Token Token, Target? Id = null, ControlProps? Props = null, List<ControlEventStmt>? Events = null) : Stmt(Token.Position)
{
    public override string ToString() => $"LOGVIEW {Variable}, {Row}, {Col}, {Width}, {Height}";
    public override AstNode DeepClone() => new LogViewStmt((Target?)Variable?.DeepClone(), (Expr)Row.DeepClone(), (Expr)Col.DeepClone(), (Expr)Width.DeepClone(), (Expr)Height.DeepClone(), (Expr)Records.DeepClone(), (Token)Token.DeepClone());
}

[JsonDerivedType(typeof(HexViewStmt), nameof(HexViewStmt))]
public sealed record HexViewStmt(Target? Variable, Expr Row, Expr Col, Expr Width, Expr Height, Expr Data, Token Token, Target? Id = null, ControlProps? Props = null, List<ControlEventStmt>? Events = null) : Stmt(Token.Position)
{
    public override string ToString() => $"HEXVIEW {Variable}, {Row}, {Col}, {Width}, {Height}";
    public override AstNode DeepClone() => new HexViewStmt((Target?)Variable?.DeepClone(), (Expr)Row.DeepClone(), (Expr)Col.DeepClone(), (Expr)Width.DeepClone(), (Expr)Height.DeepClone(), (Expr)Data.DeepClone(), (Token)Token.DeepClone());
}

[JsonDerivedType(typeof(BigTextStmt), nameof(BigTextStmt))]
public sealed record BigTextStmt(Expr Row, Expr Col, Expr Text, Target? Variable, Token Token, Target? Id = null, ControlProps? Props = null, List<ControlEventStmt>? Events = null) : Stmt(Token.Position)
{
    public override string ToString() => $"BIGTEXT {Row}, {Col}";
    public override AstNode DeepClone() => new BigTextStmt((Expr)Row.DeepClone(), (Expr)Col.DeepClone(), (Expr)Text.DeepClone(), Variable == null ? null : (Target)Variable.DeepClone(), (Token)Token.DeepClone());
}

[JsonDerivedType(typeof(CodeAreaStmt), nameof(CodeAreaStmt))]
public sealed record CodeAreaStmt(Target Variable, Expr Row, Expr Col, Expr Width, Expr Height, Expr Lines, Token Token, Target? Id = null, ControlProps? Props = null, List<ControlEventStmt>? Events = null) : Stmt(Token.Position)
{
    public override string ToString() => $"CODEAREA {Variable}, {Row}, {Col}, {Width}, {Height}";
    public override AstNode DeepClone() => new CodeAreaStmt((Target)Variable.DeepClone(), (Expr)Row.DeepClone(), (Expr)Col.DeepClone(), (Expr)Width.DeepClone(), (Expr)Height.DeepClone(), (Expr)Lines.DeepClone(), (Token)Token.DeepClone());
}

[JsonDerivedType(typeof(ReorderListBoxStmt), nameof(ReorderListBoxStmt))]
public sealed record ReorderListBoxStmt(Target Variable, Expr Row, Expr Col, Expr Width, Expr Height, Expr Items, Token Token, Target? Id = null, ControlProps? Props = null, List<ControlEventStmt>? Events = null) : Stmt(Token.Position)
{
    public override string ToString() => $"REORDERLISTBOX {Variable}, {Row}, {Col}, {Width}, {Height}";
    public override AstNode DeepClone() => new ReorderListBoxStmt((Target)Variable.DeepClone(), (Expr)Row.DeepClone(), (Expr)Col.DeepClone(), (Expr)Width.DeepClone(), (Expr)Height.DeepClone(), (Expr)Items.DeepClone(), (Token)Token.DeepClone());
}

[JsonDerivedType(typeof(AlertBoxStmt), nameof(AlertBoxStmt))]
public sealed record AlertBoxStmt(Expr Row, Expr Col, Expr Width, Expr Height, Expr Text, Target? Variable, Token Token, Target? Id = null, ControlProps? Props = null, List<ControlEventStmt>? Events = null) : Stmt(Token.Position)
{
    public override string ToString() => $"ALERTBOX {Row}, {Col}, {Width}, {Height}";
    public override AstNode DeepClone() => new AlertBoxStmt((Expr)Row.DeepClone(), (Expr)Col.DeepClone(), (Expr)Width.DeepClone(), (Expr)Height.DeepClone(), (Expr)Text.DeepClone(), Variable == null ? null : (Target)Variable.DeepClone(), (Token)Token.DeepClone());
}

[JsonDerivedType(typeof(ConfirmationBoxStmt), nameof(ConfirmationBoxStmt))]
public sealed record ConfirmationBoxStmt(Target Variable, Expr Row, Expr Col, Expr Width, Expr Height, Expr Text, Token Token, Target? Id = null, ControlProps? Props = null, List<ControlEventStmt>? Events = null) : Stmt(Token.Position)
{
    public override string ToString() => $"CONFIRMATIONBOX {Variable}, {Row}, {Col}, {Width}, {Height}";
    public override AstNode DeepClone() => new ConfirmationBoxStmt((Target)Variable.DeepClone(), (Expr)Row.DeepClone(), (Expr)Col.DeepClone(), (Expr)Width.DeepClone(), (Expr)Height.DeepClone(), (Expr)Text.DeepClone(), (Token)Token.DeepClone());
}

[JsonDerivedType(typeof(FileExplorerStmt), nameof(FileExplorerStmt))]
public sealed record FileExplorerStmt(Target Variable, Expr Row, Expr Col, Expr Width, Expr Height, Expr Path, Token Token, Target? Id = null, ControlProps? Props = null, List<ControlEventStmt>? Events = null) : Stmt(Token.Position)
{
    public override string ToString() => $"FILEEXPLORER {Variable}, {Row}, {Col}, {Width}, {Height}";
    public override AstNode DeepClone() => new FileExplorerStmt((Target)Variable.DeepClone(), (Expr)Row.DeepClone(), (Expr)Col.DeepClone(), (Expr)Width.DeepClone(), (Expr)Height.DeepClone(), (Expr)Path.DeepClone(), (Token)Token.DeepClone());
}

[JsonDerivedType(typeof(DashboardStmt), nameof(DashboardStmt))]
public sealed record DashboardStmt(Target Variable, Expr Row, Expr Col, Expr Width, Expr Height, Expr Metrics, Token Token, Target? Id = null, ControlProps? Props = null, List<ControlEventStmt>? Events = null) : Stmt(Token.Position)
{
    public override string ToString() => $"DASHBOARD {Variable}, {Row}, {Col}, {Width}, {Height}";
    public override AstNode DeepClone() => new DashboardStmt((Target)Variable.DeepClone(), (Expr)Row.DeepClone(), (Expr)Col.DeepClone(), (Expr)Width.DeepClone(), (Expr)Height.DeepClone(), (Expr)Metrics.DeepClone(), (Token)Token.DeepClone());
}

[JsonDerivedType(typeof(SplitterStmt), nameof(SplitterStmt))]
public sealed record SplitterStmt(Target Variable, Expr Row, Expr Col, Expr Width, Expr Height, Expr Orient, Token Token, Target? Id = null, ControlProps? Props = null, List<ControlEventStmt>? Events = null) : Stmt(Token.Position)
{
    public override string ToString() => $"SPLITTER {Variable}, {Row}, {Col}, {Width}, {Height}, {Orient}";
    public override AstNode DeepClone() => new SplitterStmt((Target)Variable.DeepClone(), (Expr)Row.DeepClone(), (Expr)Col.DeepClone(), (Expr)Width.DeepClone(), (Expr)Height.DeepClone(), (Expr)Orient.DeepClone(), (Token)Token.DeepClone());
}

[JsonDerivedType(typeof(ContextMenuStmt), nameof(ContextMenuStmt))]
public sealed record ContextMenuStmt(Target Variable, Expr Row, Expr Col, Expr Width, Expr Height, Expr Items, Token Token, Target? Id = null, ControlProps? Props = null, List<ControlEventStmt>? Events = null) : Stmt(Token.Position)
{
    public override string ToString() => $"CONTEXTMENU {Variable}, {Row}, {Col}, {Width}, {Height}";
    public override AstNode DeepClone() => new ContextMenuStmt((Target)Variable.DeepClone(), (Expr)Row.DeepClone(), (Expr)Col.DeepClone(), (Expr)Width.DeepClone(), (Expr)Height.DeepClone(), (Expr)Items.DeepClone(), (Token)Token.DeepClone());
}

/// <summary>TAB var, row, col, width, height, tabs()</summary>
[JsonDerivedType(typeof(TabStmt), nameof(TabStmt))]
public sealed record TabStmt(Target Variable, Expr Row, Expr Col, Expr Width, Expr Height, Expr Tabs, Token Token, Target? Id = null, ControlProps? Props = null, List<ControlEventStmt>? Events = null) : Stmt(Token.Position)
{
    public override string ToString() => $"TAB {Variable}, {Row}, {Col}, {Width}, {Height}, {Tabs}";
    public override AstNode DeepClone() => new TabStmt((Target)Variable.DeepClone(), (Expr)Row.DeepClone(), (Expr)Col.DeepClone(), (Expr)Width.DeepClone(), (Expr)Height.DeepClone(), (Expr)Tabs.DeepClone(), (Token)Token.DeepClone());
}

/// <summary>GRID var, row, col, width, height, headers(), data()</summary>
[JsonDerivedType(typeof(GridStmt), nameof(GridStmt))]
public sealed record GridStmt(Target Variable, Expr Row, Expr Col, Expr Width, Expr Height, Expr Headers, Expr Data, Token Token, Target? Id = null, ControlProps? Props = null, List<ControlEventStmt>? Events = null) : Stmt(Token.Position)
{
    public override string ToString() => $"GRID {Variable}, {Row}, {Col}, {Width}, {Height}, {Headers}, {Data}";
    public override AstNode DeepClone() => new GridStmt((Target)Variable.DeepClone(), (Expr)Row.DeepClone(), (Expr)Col.DeepClone(), (Expr)Width.DeepClone(), (Expr)Height.DeepClone(), (Expr)Headers.DeepClone(), (Expr)Data.DeepClone(), (Token)Token.DeepClone());
}

/// <summary>DATEPICKER var, row, col</summary>
[JsonDerivedType(typeof(DatePickerStmt), nameof(DatePickerStmt))]
public sealed record DatePickerStmt(Target Variable, Expr Row, Expr Col, Token Token, Target? Id = null, ControlProps? Props = null, List<ControlEventStmt>? Events = null) : Stmt(Token.Position)
{
    public override string ToString() => $"DATEPICKER {Variable}, {Row}, {Col}";
    public override AstNode DeepClone() => new DatePickerStmt((Target)Variable.DeepClone(), (Expr)Row.DeepClone(), (Expr)Col.DeepClone(), (Token)Token.DeepClone());
}

/// <summary>COLORPICKER var, row, col, columns</summary>
[JsonDerivedType(typeof(ColorPickerStmt), nameof(ColorPickerStmt))]
public sealed record ColorPickerStmt(Target Variable, Expr Row, Expr Col, Expr Columns, Token Token, Target? Id = null, ControlProps? Props = null, List<ControlEventStmt>? Events = null) : Stmt(Token.Position)
{
    public override string ToString() => $"COLORPICKER {Variable}, {Row}, {Col}, {Columns}";
    public override AstNode DeepClone() => new ColorPickerStmt((Target)Variable.DeepClone(), (Expr)Row.DeepClone(), (Expr)Col.DeepClone(), (Expr)Columns.DeepClone(), (Token)Token.DeepClone());
}

/// <summary>TREEVIEW var, row, col, width, height, tree()</summary>
[JsonDerivedType(typeof(TreeViewStmt), nameof(TreeViewStmt))]
public sealed record TreeViewStmt(Target Variable, Expr Row, Expr Col, Expr Width, Expr Height, Expr Tree, Token Token, Target? Id = null, ControlProps? Props = null, List<ControlEventStmt>? Events = null) : Stmt(Token.Position)
{
    public override string ToString() => $"TREEVIEW {Variable}, {Row}, {Col}, {Width}, {Height}, {Tree}";
    public override AstNode DeepClone() => new TreeViewStmt((Target)Variable.DeepClone(), (Expr)Row.DeepClone(), (Expr)Col.DeepClone(), (Expr)Width.DeepClone(), (Expr)Height.DeepClone(), (Expr)Tree.DeepClone(), (Token)Token.DeepClone());
}

/// <summary>SPINNER row, col, "preset" [INTO v]</summary>
[JsonDerivedType(typeof(SpinnerStmt), nameof(SpinnerStmt))]
public sealed record SpinnerStmt(Expr Row, Expr Col, Expr Preset, Target? Variable, Token Token, Target? Id = null, ControlProps? Props = null, List<ControlEventStmt>? Events = null) : Stmt(Token.Position)
{
    public override string ToString() => $"SPINNER {Row}, {Col}, {Preset}{(Variable != null ? $" INTO {Variable}" : "")}";
    public override AstNode DeepClone() => new SpinnerStmt((Expr)Row.DeepClone(), (Expr)Col.DeepClone(), (Expr)Preset.DeepClone(), (Target?)Variable?.DeepClone(), (Token)Token.DeepClone());
}

/// <summary>TBROWSE var, row, col, width, height, columns(), data() [, options()]</summary>
[JsonDerivedType(typeof(TBrowseStmt), nameof(TBrowseStmt))]
public sealed record TBrowseStmt(Target Variable, Expr Row, Expr Col, Expr Width, Expr Height, Expr Columns, Expr Data, Expr? Options, Token Token, Target? Id = null, ControlProps? Props = null, List<ControlEventStmt>? Events = null) : Stmt(Token.Position)
{
    public override string ToString() => $"TBROWSE {Variable}, {Row}, {Col}, {Width}, {Height}, {Columns}, {Data}";
    public override AstNode DeepClone() => new TBrowseStmt((Target)Variable.DeepClone(), (Expr)Row.DeepClone(), (Expr)Col.DeepClone(), (Expr)Width.DeepClone(), (Expr)Height.DeepClone(), (Expr)Columns.DeepClone(), (Expr)Data.DeepClone(), (Expr?)Options?.DeepClone(), (Token)Token.DeepClone());
}

/// <summary>TBROWSE.CELL(row, col) — accesso cella dopo TBROWSE</summary>
[JsonDerivedType(typeof(TBrowseCellExpr), nameof(TBrowseCellExpr))]
public sealed record TBrowseCellExpr(Expr Row, Expr Col, Token Token) : Expr(Token.Position)
{
    public override string ToString() => $"TBROWSE.CELL({Row}, {Col})";
    public override AstNode DeepClone() => new TBrowseCellExpr((Expr)Row.DeepClone(), (Expr)Col.DeepClone(), (Token)Token.DeepClone());
}
