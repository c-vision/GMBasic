using Basic;

namespace Compiler;

public sealed partial class Compiler
{
    // ═══════════════════════════════════════════════════════════════
    // Display-only (no return value)
    // ═══════════════════════════════════════════════════════════════

    private void EmitTuiProps(ControlProps? p, Token tok)
    {
        if (p == null) return;
        void V(Expr? e) { if (e == null) EmitByte((byte)OpCode.OP_PUSH_NIL); else VisitExpression(e); }
        V(p.Name); V(p.Tag); V(p.Tooltip); V(p.TabIndex); V(p.Visible); V(p.Enabled); V(p.TabStop); V(p.Focus ? new NumberExpr(1.0, tok) : null); V(p.Style); V(p.Color); V(p.ColorFocus); V(p.ColorSelect); V(p.ColorError);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.CONTROL_APPLY_PROPS); EmitByte((byte)13);
    }

    private void EmitTuiId(Target? id)
    {
        if (id == null) return;
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.TUI_LAST_HANDLE); EmitByte((byte)0);
        AssignVariable(id, isLet: false);
    }

    private void VisitStatusBarStatement(StatusBarStmt stmt)
    {
        VisitExpression(stmt.Text);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.TUI_STATUSBAR); EmitByte((byte)1);
        EmitTuiId(stmt.Id);
        EmitTuiProps(stmt.Props, stmt.Token);
        if (stmt.Events != null) foreach (var _e in stmt.Events) VisitControlEventStatement(_e);
    }

    private void VisitSpinnerStatement(SpinnerStmt stmt)
    {
        VisitExpression(stmt.Row);
        VisitExpression(stmt.Col);
        VisitExpression(stmt.Preset);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.TUI_SPINNER); EmitByte((byte)3);
        // INTO v: la VM pusha il frame corrente (Index) — assegnato alla variabile.
        // Senza INTO (display-only) il valore va scartato per non sbilanciare lo stack.
        if (stmt.Variable != null) AssignVariable(stmt.Variable, isLet: false);
        else EmitByte((byte)OpCode.OP_POP);
        EmitTuiId(stmt.Id);
        EmitTuiProps(stmt.Props, stmt.Token);
        if (stmt.Events != null) foreach (var _e in stmt.Events) VisitControlEventStatement(_e);
    }

    private void VisitCheckBoxStatement(CheckBoxStmt stmt)
    {
        VisitExpression(stmt.Row);
        VisitExpression(stmt.Col);
        VisitExpression(stmt.Caption);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.TUI_CHECKBOX); EmitByte((byte)3);
        AssignVariable(stmt.Variable, isLet: false);
        EmitTuiId(stmt.Id);
        EmitTuiProps(stmt.Props, stmt.Token);
        if (stmt.Events != null) foreach (var _e in stmt.Events) VisitControlEventStatement(_e);
    }

    private void VisitListBoxStatement(ListBoxStmt stmt)
    {
        VisitExpression(stmt.Row);
        VisitExpression(stmt.Col);
        VisitExpression(stmt.Width);
        VisitExpression(stmt.Height);
        VisitExpression(stmt.Items);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.TUI_LISTBOX); EmitByte((byte)5);
        AssignVariable(stmt.Variable, isLet: false);
        EmitTuiId(stmt.Id);
        EmitTuiProps(stmt.Props, stmt.Token);
        if (stmt.Events != null) foreach (var _e in stmt.Events) VisitControlEventStatement(_e);
    }

    private void VisitComboBoxStatement(ComboBoxStmt stmt)
    {
        VisitExpression(stmt.Row);
        VisitExpression(stmt.Col);
        VisitExpression(stmt.Width);
        VisitExpression(stmt.Items);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.TUI_COMBOBOX); EmitByte((byte)4);
        AssignVariable(stmt.Variable, isLet: false);
        EmitTuiId(stmt.Id);
        EmitTuiProps(stmt.Props, stmt.Token);
        if (stmt.Events != null) foreach (var _e in stmt.Events) VisitControlEventStatement(_e);
    }

    private void VisitToggleStatement(ToggleStmt stmt)
    {
        VisitExpression(stmt.Row);
        VisitExpression(stmt.Col);
        VisitExpression(stmt.Caption);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.TUI_TOGGLE); EmitByte((byte)3);
        AssignVariable(stmt.Variable, isLet: false);
        EmitTuiId(stmt.Id);
        EmitTuiProps(stmt.Props, stmt.Token);
        if (stmt.Events != null) foreach (var _e in stmt.Events) VisitControlEventStatement(_e);
    }

    private void VisitRadioGroupStatement(RadioGroupStmt stmt)
    {
        VisitExpression(stmt.Row);
        VisitExpression(stmt.Col);
        VisitExpression(stmt.Items);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.TUI_RADIOGROUP); EmitByte((byte)3);
        AssignVariable(stmt.Variable, isLet: false);
        EmitTuiId(stmt.Id);
        EmitTuiProps(stmt.Props, stmt.Token);
        if (stmt.Events != null) foreach (var _e in stmt.Events) VisitControlEventStatement(_e);
    }

    private void VisitSliderStatement(SliderStmt stmt)
    {
        VisitExpression(stmt.Row);
        VisitExpression(stmt.Col);
        VisitExpression(stmt.Length);
        VisitExpression(stmt.Min);
        VisitExpression(stmt.Max);
        VisitExpression(stmt.Step);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.TUI_SLIDER); EmitByte((byte)6);
        AssignVariable(stmt.Variable, isLet: false);
        EmitTuiId(stmt.Id);
        EmitTuiProps(stmt.Props, stmt.Token);
        if (stmt.Events != null) foreach (var _e in stmt.Events) VisitControlEventStatement(_e);
    }

    private void VisitNumericUpDownStatement(NumericUpDownStmt stmt)
    {
        VisitExpression(stmt.Row);
        VisitExpression(stmt.Col);
        VisitExpression(stmt.Length);
        VisitExpression(stmt.Min);
        VisitExpression(stmt.Max);
        VisitExpression(stmt.Step);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.TUI_NUMERICUPDOWN); EmitByte((byte)6);
        AssignVariable(stmt.Variable, isLet: false);
        EmitTuiId(stmt.Id);
        EmitTuiProps(stmt.Props, stmt.Token);
        if (stmt.Events != null) foreach (var _e in stmt.Events) VisitControlEventStatement(_e);
    }

    private void VisitStarRatingStatement(StarRatingStmt stmt)
    {
        VisitExpression(stmt.Row);
        VisitExpression(stmt.Col);
        VisitExpression(stmt.Length);
        VisitExpression(stmt.Max);
        VisitExpression(stmt.Value);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.TUI_STARRATING); EmitByte((byte)5);
        AssignVariable(stmt.Variable, isLet: false);
        EmitTuiId(stmt.Id);
        EmitTuiProps(stmt.Props, stmt.Token);
        if (stmt.Events != null) foreach (var _e in stmt.Events) VisitControlEventStatement(_e);
    }

    private void VisitScrollBarStatement(ScrollBarStmt stmt)
    {
        VisitExpression(stmt.Row);
        VisitExpression(stmt.Col);
        VisitExpression(stmt.Length);
        VisitExpression(stmt.Min);
        VisitExpression(stmt.Max);
        VisitExpression(stmt.Value);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.TUI_SCROLLBAR); EmitByte((byte)6);
        AssignVariable(stmt.Variable, isLet: false);
        EmitTuiId(stmt.Id);
        EmitTuiProps(stmt.Props, stmt.Token);
        if (stmt.Events != null) foreach (var _e in stmt.Events) VisitControlEventStatement(_e);
    }

    private void VisitScrollViewStatement(ScrollViewStmt stmt)
    {
        VisitExpression(stmt.Row);
        VisitExpression(stmt.Col);
        VisitExpression(stmt.Width);
        VisitExpression(stmt.Height);
        VisitExpression(stmt.Content);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.TUI_SCROLLVIEW); EmitByte((byte)5);
        AssignVariable(stmt.Variable, isLet: false);
        EmitTuiId(stmt.Id);
        EmitTuiProps(stmt.Props, stmt.Token);
        if (stmt.Events != null) foreach (var _e in stmt.Events) VisitControlEventStatement(_e);
    }

    private void VisitLabelStatement(LabelControlStmt stmt)
    {
        VisitExpression(stmt.Row);
        VisitExpression(stmt.Col);
        VisitExpression(stmt.Width);
        VisitExpression(stmt.Text);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.TUI_LABEL); EmitByte((byte)4);
        // INTO v opzionale (display-only): senza target scarta il valore per non sbilanciare lo stack.
        if (stmt.Variable != null) AssignVariable(stmt.Variable, isLet: false);
        else EmitByte((byte)OpCode.OP_POP);
        EmitTuiId(stmt.Id);
        EmitTuiProps(stmt.Props, stmt.Token);
        if (stmt.Events != null) foreach (var _e in stmt.Events) VisitControlEventStatement(_e);
    }

    private void VisitDataListStatement(DataListStmt stmt)
    {
        VisitExpression(stmt.Row);
        VisitExpression(stmt.Col);
        VisitExpression(stmt.Width);
        VisitExpression(stmt.Height);
        VisitExpression(stmt.Items);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.TUI_DATALIST); EmitByte((byte)5);
        AssignVariable(stmt.Variable, isLet: false);
        EmitTuiId(stmt.Id);
        EmitTuiProps(stmt.Props, stmt.Token);
        if (stmt.Events != null) foreach (var _e in stmt.Events) VisitControlEventStatement(_e);
    }

    private void VisitPropertyGridStatement(PropertyGridStmt stmt)
    {
        VisitExpression(stmt.Row);
        VisitExpression(stmt.Col);
        VisitExpression(stmt.Width);
        VisitExpression(stmt.Height);
        VisitExpression(stmt.Items);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.TUI_PROPERTYGRID); EmitByte((byte)5);
        AssignVariable(stmt.Variable, isLet: false);
        EmitTuiId(stmt.Id);
        EmitTuiProps(stmt.Props, stmt.Token);
        if (stmt.Events != null) foreach (var _e in stmt.Events) VisitControlEventStatement(_e);
    }

    private void VisitSplitterStatement(SplitterStmt stmt)
    {
        VisitExpression(stmt.Row);
        VisitExpression(stmt.Col);
        VisitExpression(stmt.Width);
        VisitExpression(stmt.Height);
        VisitExpression(stmt.Orient);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.TUI_SPLITTER); EmitByte((byte)5);
        AssignVariable(stmt.Variable, isLet: false);
        EmitTuiId(stmt.Id);
        EmitTuiProps(stmt.Props, stmt.Token);
        if (stmt.Events != null) foreach (var _e in stmt.Events) VisitControlEventStatement(_e);
    }

    private void VisitContextMenuStatement(ContextMenuStmt stmt)
    {
        VisitExpression(stmt.Row);
        VisitExpression(stmt.Col);
        VisitExpression(stmt.Width);
        VisitExpression(stmt.Height);
        VisitExpression(stmt.Items);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.TUI_CONTEXTMENU); EmitByte((byte)5);
        AssignVariable(stmt.Variable, isLet: false);
        EmitTuiId(stmt.Id);
        EmitTuiProps(stmt.Props, stmt.Token);
        if (stmt.Events != null) foreach (var _e in stmt.Events) VisitControlEventStatement(_e);
    }

    private void VisitLogViewStatement(LogViewStmt stmt)
    {
        VisitExpression(stmt.Row);
        VisitExpression(stmt.Col);
        VisitExpression(stmt.Width);
        VisitExpression(stmt.Height);
        VisitExpression(stmt.Records);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.TUI_LOGVIEW); EmitByte((byte)5);
        AssignVariable(stmt.Variable, isLet: false);
        EmitTuiId(stmt.Id);
        EmitTuiProps(stmt.Props, stmt.Token);
        if (stmt.Events != null) foreach (var _e in stmt.Events) VisitControlEventStatement(_e);
    }

    private void VisitHexViewStatement(HexViewStmt stmt)
    {
        VisitExpression(stmt.Row);
        VisitExpression(stmt.Col);
        VisitExpression(stmt.Width);
        VisitExpression(stmt.Height);
        VisitExpression(stmt.Data);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.TUI_HEXVIEW); EmitByte((byte)5);
        AssignVariable(stmt.Variable, isLet: false);
        EmitTuiId(stmt.Id);
        EmitTuiProps(stmt.Props, stmt.Token);
        if (stmt.Events != null) foreach (var _e in stmt.Events) VisitControlEventStatement(_e);
    }

    private void VisitBigTextStatement(BigTextStmt stmt)
    {
        VisitExpression(stmt.Row);
        VisitExpression(stmt.Col);
        VisitExpression(stmt.Text);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.TUI_BIGTEXT); EmitByte((byte)3);
        if (stmt.Variable != null) AssignVariable(stmt.Variable, isLet: false);
        else EmitByte((byte)OpCode.OP_POP);
        EmitTuiId(stmt.Id);
        EmitTuiProps(stmt.Props, stmt.Token);
        if (stmt.Events != null) foreach (var _e in stmt.Events) VisitControlEventStatement(_e);
    }

    private void VisitCodeAreaStatement(CodeAreaStmt stmt)
    {
        VisitExpression(stmt.Row);
        VisitExpression(stmt.Col);
        VisitExpression(stmt.Width);
        VisitExpression(stmt.Height);
        VisitExpression(stmt.Lines);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.TUI_CODEAREA); EmitByte((byte)5);
        AssignVariable(stmt.Variable, isLet: false);
        EmitTuiId(stmt.Id);
        EmitTuiProps(stmt.Props, stmt.Token);
        if (stmt.Events != null) foreach (var _e in stmt.Events) VisitControlEventStatement(_e);
    }

    private void VisitReorderListBoxStatement(ReorderListBoxStmt stmt)
    {
        VisitExpression(stmt.Row);
        VisitExpression(stmt.Col);
        VisitExpression(stmt.Width);
        VisitExpression(stmt.Height);
        VisitExpression(stmt.Items);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.TUI_REORDERLISTBOX); EmitByte((byte)5);
        AssignVariable(stmt.Variable, isLet: false);
        EmitTuiId(stmt.Id);
        EmitTuiProps(stmt.Props, stmt.Token);
        if (stmt.Events != null) foreach (var _e in stmt.Events) VisitControlEventStatement(_e);
    }

    private void VisitAlertBoxStatement(AlertBoxStmt stmt)
    {
        VisitExpression(stmt.Row);
        VisitExpression(stmt.Col);
        VisitExpression(stmt.Width);
        VisitExpression(stmt.Height);
        VisitExpression(stmt.Text);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.TUI_ALERTBOX); EmitByte((byte)5);
        if (stmt.Variable != null) AssignVariable(stmt.Variable, isLet: false);
        else EmitByte((byte)OpCode.OP_POP);
        EmitTuiId(stmt.Id);
        EmitTuiProps(stmt.Props, stmt.Token);
        if (stmt.Events != null) foreach (var _e in stmt.Events) VisitControlEventStatement(_e);
    }

    private void VisitConfirmationBoxStatement(ConfirmationBoxStmt stmt)
    {
        VisitExpression(stmt.Row);
        VisitExpression(stmt.Col);
        VisitExpression(stmt.Width);
        VisitExpression(stmt.Height);
        VisitExpression(stmt.Text);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.TUI_CONFIRMATIONBOX); EmitByte((byte)5);
        AssignVariable(stmt.Variable, isLet: false);
        EmitTuiId(stmt.Id);
        EmitTuiProps(stmt.Props, stmt.Token);
        if (stmt.Events != null) foreach (var _e in stmt.Events) VisitControlEventStatement(_e);
    }

    private void VisitFileExplorerStatement(FileExplorerStmt stmt)
    {
        VisitExpression(stmt.Row);
        VisitExpression(stmt.Col);
        VisitExpression(stmt.Width);
        VisitExpression(stmt.Height);
        VisitExpression(stmt.Path);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.TUI_FILEEXPLORER); EmitByte((byte)5);
        AssignVariable(stmt.Variable, isLet: false);
        EmitTuiId(stmt.Id);
        EmitTuiProps(stmt.Props, stmt.Token);
        if (stmt.Events != null) foreach (var _e in stmt.Events) VisitControlEventStatement(_e);
    }

    private void VisitDashboardStatement(DashboardStmt stmt)
    {
        VisitExpression(stmt.Row);
        VisitExpression(stmt.Col);
        VisitExpression(stmt.Width);
        VisitExpression(stmt.Height);
        VisitExpression(stmt.Metrics);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.TUI_DASHBOARD); EmitByte((byte)5);
        AssignVariable(stmt.Variable, isLet: false);
        EmitTuiId(stmt.Id);
        EmitTuiProps(stmt.Props, stmt.Token);
        if (stmt.Events != null) foreach (var _e in stmt.Events) VisitControlEventStatement(_e);
    }

    private void VisitTabStatement(TabStmt stmt)
    {
        VisitExpression(stmt.Row);
        VisitExpression(stmt.Col);
        VisitExpression(stmt.Width);
        VisitExpression(stmt.Height);
        VisitExpression(stmt.Tabs);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.TUI_TAB); EmitByte((byte)5);
        AssignVariable(stmt.Variable, isLet: false);
        EmitTuiId(stmt.Id);
        EmitTuiProps(stmt.Props, stmt.Token);
        if (stmt.Events != null) foreach (var _e in stmt.Events) VisitControlEventStatement(_e);
    }

    private void VisitGridStatement(GridStmt stmt)
    {
        VisitExpression(stmt.Row);
        VisitExpression(stmt.Col);
        VisitExpression(stmt.Width);
        VisitExpression(stmt.Height);
        VisitExpression(stmt.Headers);
        VisitExpression(stmt.Data);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.TUI_GRID); EmitByte((byte)6);
        AssignVariable(stmt.Variable, isLet: false);
        EmitTuiId(stmt.Id);
        EmitTuiProps(stmt.Props, stmt.Token);
        if (stmt.Events != null) foreach (var _e in stmt.Events) VisitControlEventStatement(_e);
    }

    private void VisitDatePickerStatement(DatePickerStmt stmt)
    {
        VisitExpression(stmt.Row);
        VisitExpression(stmt.Col);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.TUI_DATEPICKER); EmitByte((byte)2);
        AssignVariable(stmt.Variable, isLet: false);
        EmitTuiId(stmt.Id);
        EmitTuiProps(stmt.Props, stmt.Token);
        if (stmt.Events != null) foreach (var _e in stmt.Events) VisitControlEventStatement(_e);
    }

    private void VisitColorPickerStatement(ColorPickerStmt stmt)
    {
        VisitExpression(stmt.Row);
        VisitExpression(stmt.Col);
        VisitExpression(stmt.Columns);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.TUI_COLORPICKER); EmitByte((byte)3);
        AssignVariable(stmt.Variable, isLet: false);
        EmitTuiId(stmt.Id);
        EmitTuiProps(stmt.Props, stmt.Token);
        if (stmt.Events != null) foreach (var _e in stmt.Events) VisitControlEventStatement(_e);
    }

    private void VisitTreeViewStatement(TreeViewStmt stmt)
    {
        VisitExpression(stmt.Row);
        VisitExpression(stmt.Col);
        VisitExpression(stmt.Width);
        VisitExpression(stmt.Height);
        VisitExpression(stmt.Tree);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.TUI_TREEVIEW); EmitByte((byte)5);
        AssignVariable(stmt.Variable, isLet: false);
        EmitTuiId(stmt.Id);
        EmitTuiProps(stmt.Props, stmt.Token);
        if (stmt.Events != null) foreach (var _e in stmt.Events) VisitControlEventStatement(_e);
    }

    // ═══════════════════════════════════════════════════════════════
    // Complex controls
    // ═══════════════════════════════════════════════════════════════

    private void VisitMenuStatement(MenuStmt stmt)
    {
        VisitExpression(stmt.Row);
        VisitExpression(stmt.Col);
        VisitExpression(stmt.Items);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.TUI_MENU); EmitByte((byte)3);
        AssignVariable(stmt.Variable, isLet: false);
        EmitTuiId(stmt.Id);
        EmitTuiProps(stmt.Props, stmt.Token);
        if (stmt.Events != null) foreach (var _e in stmt.Events) VisitControlEventStatement(_e);
    }

    private void VisitFileDialogStatement(FileDialogStmt stmt)
    {
        VisitExpression(stmt.Mask);
        VisitExpression(stmt.Title);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.TUI_FILEDIALOG); EmitByte((byte)2);
        AssignVariable(stmt.Variable, isLet: false);
        EmitTuiId(stmt.Id);
        EmitTuiProps(stmt.Props, stmt.Token);
        if (stmt.Events != null) foreach (var _e in stmt.Events) VisitControlEventStatement(_e);
    }

    private void VisitPrinterDialogStatement(PrinterDialogStmt stmt)
    {
        if (stmt.Title != null) VisitExpression(stmt.Title); else EmitByte((byte)OpCode.OP_PUSH_NIL);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.TUI_PRINTER); EmitByte((byte)1);
        AssignVariable(stmt.PrinterVar, isLet: false);
        AssignVariable(stmt.CopiesVar, isLet: false);
        AssignVariable(stmt.DuplexVar, isLet: false);
        EmitTuiId(stmt.Id);
        EmitTuiProps(stmt.Props, stmt.Token);
        if (stmt.Events != null) foreach (var _e in stmt.Events) VisitControlEventStatement(_e);
    }

    private void VisitTBrowseStatement(TBrowseStmt stmt)
    {
        VisitExpression(stmt.Row);
        VisitExpression(stmt.Col);
        VisitExpression(stmt.Width);
        VisitExpression(stmt.Height);
        VisitExpression(stmt.Columns);
        VisitExpression(stmt.Data);
        if (stmt.Options != null) VisitExpression(stmt.Options); else EmitByte((byte)OpCode.OP_PUSH_NIL);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.TUI_TBROWSE); EmitByte((byte)7);
        AssignVariable(stmt.Variable, isLet: false);
        EmitTuiId(stmt.Id);
        EmitTuiProps(stmt.Props, stmt.Token);
        if (stmt.Events != null) foreach (var _e in stmt.Events) VisitControlEventStatement(_e);
    }


    private void VisitSetClipper(SetAttributeStmt stmt)
    {
        int subCode = stmt.Attribute.Kind switch
        {
            TokenKind.Bell => 0,
            TokenKind.Confirm => 1,
            TokenKind.Cursor => 2,
            TokenKind.Century => 3,
            TokenKind.Bold => 17,
            TokenKind.Underline => 18,
            TokenKind.Inverse => 19,
            TokenKind.Blink => 20,
            TokenKind.Shell => 21,   // SET SHELL ON|OFF — opt-in per SYSTEM.SHELL/EXEC
            _ => -1
        };
        EmitConstant(stmt.State.Kind == TokenKind.On ? 1.0 : 0.0);
        EmitConstant((double)subCode);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.SET_CLIPPER); EmitByte((byte)2);
    }

    private void VisitSetThemeStmt(SetThemeStmt stmt)
    {
        VisitExpression(stmt.ThemeName);
        EmitConstant(4.0);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.SET_CLIPPER); EmitByte((byte)2);
    }

    private void VisitSetDateFormatStmt(SetDateFormatStmt stmt)
    {
        VisitExpression(stmt.Format);
        EmitConstant(5.0);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.SET_CLIPPER); EmitByte((byte)2);
    }

}