// FILE: StdLib.Controls.cs
// Namespace unico CONTROLS: UN verbo per controllo, reso dal tipo di console attivo.
// - SCREEN 0 (o default)  -> TEXT  : renderer testuale (gmb_tui_*)
// - SCREEN n!=0           -> GRAPHICS: renderer pixel (gmb_form_*)
// Graphics-only (CANDLESTICK/CHART) -> no-op in TEXT (fase successiva).
// Richiede `IMPORT CONTROLS` (non default-attivo).
// Pipeline UNICA C#/C: la modalità è data da SCREEN n (parser.GuiMode), come il nativo (gui_mode).

/* ==== GMB-STDLIB-AUDIT-BEGIN ====
 * FILE: Controls — namespace 'CONTROLS'
 * 3 funzioni | 0 comandi | 0 tipi | 0 costanti
 *
 * FUNZIONI (3):
 *   CTRLGETPROP
 *   CTRLGETPROPS
 *   handle
    ==== GMB-STDLIB-AUDIT-END ==== */


using System.Collections.Immutable;

namespace Basic;

public sealed partial class StdLib
{
    /// <summary>Verbo canonico -> (parser testo, parser pixel). null = non disponibile in quella console.</summary>
    private static readonly (string Verb, System.Func<Parser, string, Stmt>? Text, System.Func<Parser, string, Stmt>? Pixel)[] ControlsTable =
    {
        ("CHECKBOX",   Parser.ParseCheckBoxStandalone,  Parser.ParsePixelControlStandalone),
        ("RADIO",      Parser.ParseRadioGroupStandalone, Parser.ParsePixelControlStandalone),
        ("SLIDER",     Parser.ParseSliderStandalone,    Parser.ParsePixelControlStandalone),
        ("NUMERICUPDOWN", Parser.ParseNumericUpDownStandalone, Parser.ParsePixelControlStandalone),
        ("STARRATING", Parser.ParseStarRatingStandalone, Parser.ParsePixelControlStandalone),
        ("SCROLLBAR", Parser.ParseScrollBarStandalone, Parser.ParsePixelControlStandalone),
        ("SCROLLVIEW", Parser.ParseScrollViewStandalone, Parser.ParsePixelControlStandalone),
        ("LABEL", Parser.ParseLabelStandalone, Parser.ParsePixelControlStandalone),
        ("DATALIST", Parser.ParseDataListStandalone, Parser.ParsePixelControlStandalone),
        ("PROPERTYGRID", Parser.ParsePropertyGridStandalone, Parser.ParsePixelControlStandalone),
        ("SPLITTER", Parser.ParseSplitterStandalone, Parser.ParsePixelControlStandalone),
        ("CONTEXTMENU", Parser.ParseContextMenuStandalone, Parser.ParsePixelControlStandalone),
        ("LOGVIEW", Parser.ParseLogViewStandalone, Parser.ParsePixelControlStandalone),
        ("HEXVIEW", Parser.ParseHexViewStandalone, Parser.ParsePixelControlStandalone),
        ("BIGTEXT", Parser.ParseBigTextStandalone, Parser.ParsePixelControlStandalone),
        ("CODEAREA", Parser.ParseCodeAreaStandalone, Parser.ParsePixelControlStandalone),
        ("REORDERLISTBOX", Parser.ParseReorderListBoxStandalone, Parser.ParsePixelControlStandalone),
        ("ALERTBOX", Parser.ParseAlertBoxStandalone, Parser.ParsePixelControlStandalone),
        ("CONFIRMATIONBOX", Parser.ParseConfirmationBoxStandalone, Parser.ParsePixelControlStandalone),
        ("FILEEXPLORER", Parser.ParseFileExplorerStandalone, Parser.ParsePixelControlStandalone),
        ("DASHBOARD", Parser.ParseDashboardStandalone, Parser.ParsePixelControlStandalone),
        ("COMBOBOX",   Parser.ParseComboBoxStandalone,  Parser.ParsePixelControlStandalone),
        ("DROPDOWN",   Parser.ParseComboBoxStandalone,  Parser.ParsePixelControlStandalone),
        ("TBROWSE",    Parser.ParseTBrowseStandalone,   Parser.ParsePixelControlStandalone),
        ("LISTBOX",    Parser.ParseListBoxStandalone,   Parser.ParsePixelControlStandalone),
        ("TOGGLE",     Parser.ParseToggleStandalone,    Parser.ParsePixelControlStandalone),
        ("SPINNER",    Parser.ParseSpinnerStandalone,   Parser.ParsePixelControlStandalone),
        ("TAB",        Parser.ParseTabStandalone,       Parser.ParsePixelControlStandalone),
        ("GRID",       Parser.ParseGridStandalone,      Parser.ParsePixelControlStandalone),
        ("STATUSBAR",  Parser.ParseStatusBarStandalone, Parser.ParsePixelControlStandalone),
        ("DATEPICKER", Parser.ParseDatePickerStandalone,Parser.ParsePixelControlStandalone),
        ("COLORPICKER",Parser.ParseColorPickerStandalone,Parser.ParsePixelControlStandalone),
        ("TREEVIEW",   Parser.ParseTreeViewStandalone,  Parser.ParsePixelControlStandalone),
        ("MENU",       Parser.ParseMenuStandalone,      Parser.ParsePixelControlStandalone),
        ("FILEDIALOG", Parser.ParseFileDialogStandalone,Parser.ParsePixelControlStandalone),
        ("PRINTERDIALOG", Parser.ParsePrinterDialogStandalone, Parser.ParsePixelControlStandalone),
        // Solo-pixel (nessun parser testuale): resa pixel in entrambe le modalità.
        ("BUTTON",     null, Parser.ParsePixelControlStandalone),
        ("TEXTBOX",  null, Parser.ParsePixelControlStandalone),
        ("PROGRESS",   null, Parser.ParsePixelControlStandalone),
        // FunctionKeyBar: nessun parser testuale; resa a celle in TEXT e pixel (barra) in GRAPHICS.
        ("FUNCTIONKEYBAR", null, Parser.ParsePixelControlStandalone),
        // Solo-grafica: no-op su TEXT (tabella capacita'), resa pixel in GRAPHICS.
        ("CANDLESTICK", null, Parser.ParsePixelControlStandalone),
        // Chart a linee: controllo a celle (TEXT) e pixel (GRAPHICS), motore gmb_chart.
        ("CHART",       null, Parser.ParsePixelControlStandalone),
        ("CANVAS",      null, Parser.ParsePixelControlStandalone),
        ("IMAGE",       null, Parser.ParsePixelControlStandalone),
        ("IMAGEBUTTON", null, Parser.ParsePixelControlStandalone),
        ("QR",          null, Parser.ParsePixelControlStandalone),
        ("RICHTEXT",    null, Parser.ParsePixelControlStandalone),
        ("HALFBLOCK",   null, Parser.ParsePixelControlStandalone),
        ("TILING",      null, Parser.ParsePixelControlStandalone),
        ("PANEL",       null, Parser.ParsePixelControlStandalone),
    };

    private void CreateControlsNamespace()
    {
        const string ns = "CONTROLS";
        BuiltinNamespaceNames.Add(ns);
        SymbolTable.BuiltinNamespaceNames.Add(ns);

        foreach (var (verb, text, pixel) in ControlsTable)
        {
            RegisterCommandByName(verb, ns, (p, n) =>
            {
                // Famiglia UNICA: i controlli condivisi usano sempre il percorso gmb_control_t
                // (che rende celle in TEXT e pixel ricco in GRAPHICS via la finestra). Il ramo
                // 'pixel' resta solo per i controlli pixel-only (text == null).
                var parser = text ?? pixel;
                if (parser == null)
                    throw new System.InvalidOperationException($"Il controllo {verb} non è disponibile in questa console.");
                return parser(p, n);
            });
        }

        // --- Interfaccia di controllo (metodi/comandi/eventi/polling) ---
        RegisterCommandByName("CONTROL", ns, Parser.ParseControlMethodStandalone);
        RegisterCommandByName("FIRE", ns, Parser.ParseFireStandalone);
        const SymbolFlags publicFlag = SymbolFlags.IsPublic | SymbolFlags.IsFunction;
        const SymbolFlags optFlag = SymbolFlags.IsOptional;
        var procs = new System.Collections.Generic.List<ProcedureDefinition>();
        // Polling di stato (NON consuma): CLICKED(id)/CHANGED(id)/... + globali MOUSECLICKED()/KEYPRESSED().
        foreach (var f in new[] { "CLICKED", "DOUBLECLICKED", "CHANGED", "GOTFOCUSED", "LOSTFOCUSED" })
            procs.Add(new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.Identifier, f),
                [new Parameter(-1, T(TokenKind.Identifier, "handle"), new DoubleType(), optFlag, null, false)],
                new DoubleType(), TokenKind.Identifier));
        foreach (var f in new[] { "MOUSECLICKED", "MOUSEDBLCLICKED", "MOUSEPRESSED", "KEYPRESSED" })
            procs.Add(new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.Identifier, f), [], new DoubleType(), TokenKind.Identifier));
        // §11.1: accessor proprietà controllo (usati dallo zucchero `istanza.Prop`).
        procs.Add(new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.Identifier, "CTRLGETPROP"),
            [new Parameter(-1, T(TokenKind.Identifier, "name"), new StringType(), SymbolFlags.None, null, false),
             new Parameter(-1, T(TokenKind.Identifier, "prop"), new StringType(), SymbolFlags.None, null, false)],
            new DoubleType(), TokenKind.Identifier));
        procs.Add(new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.Identifier, "CTRLGETPROPS"),
            [new Parameter(-1, T(TokenKind.Identifier, "name"), new StringType(), SymbolFlags.None, null, false),
             new Parameter(-1, T(TokenKind.Identifier, "prop"), new StringType(), SymbolFlags.None, null, false)],
            new StringType(), TokenKind.Identifier));
        RegisterBuiltinNamespace(ns, procs.ToImmutableArray(), BuildControlBaseTypes());
    }

    /// <summary>Base formale `Control` (namespace CONTROLS): `CLASS X INHERITS Control` risolve.
    /// La superficie dei membri (§11.1-11.3) viene estesa nei task successivi; qui la base è
    /// registrata come classe pubblica così l'ereditarietà non è più "rilassata".</summary>
    private static Dictionary<string, DataType> BuildControlBaseTypes()
    {
        var controlBase = new ClassDefinition(
            -1,
            T(TokenKind.Identifier, "Control"),
            SymbolFlags.IsPublic | SymbolFlags.IsClass,
            null, null,
            ImmutableArray<Token>.Empty,
            ImmutableArray<ClassMember>.Empty);
        return new Dictionary<string, DataType>(StringComparer.OrdinalIgnoreCase)
        {
            { "Control", new ClassType(controlBase) }
        };
    }
}
