// FILE: Interpreter.Parser.Dumpers.cs
// ReSharper disable UnusedVariable
// ReSharper disable InconsistentNaming
// ReSharper disable InvalidXmlDocComment

using System.Collections.Immutable;
using System.Text;

// ReSharper disable UnusedMember.Local
// ReSharper disable SwitchStatementHandlesSomeKnownEnumValuesWithDefault

namespace Basic;

/// <summary>
/// Contiene i metodi di debug.
/// </summary>
public sealed partial class Parser
{
    /// <summary>
    /// Stampa una rappresentazione testuale dettagliata della SymbolTable del programma.
    /// Questo metodo funge da wrapper per il metodo `Dump` della SymbolTable, aggiungendo intestazioni per chiarezza.
    /// </summary>
    /// <param name="program">L'istanza di BasicProgram che contiene la SymbolTable da visualizzare.</param>
    /// <param name="userCodeOnly">Se true, il dump ometterà i simboli della libreria standard (built-in),
    /// mostrando solo il codice definito dall'utente.</param>
    public static void DumpSymbols(BasicProgram program, bool userCodeOnly = false)
    {
        Log("\n--- DUMP SYMBOL TABLE ---", true);
        program.Symbols.Dump(userCodeOnly: userCodeOnly);
        Log("--- FINE DUMP SYMBOL TABLE ---\n", true);
    }

    /// <summary>
    /// Metodo diagnostico che ricostruisce il codice BASIC sorgente (o equivalente)
    /// dal flusso di token. Utile per debuggare la logica del Lexer e le passate di pre-processore.
    /// </summary>
    /// <param name="tokens">La lista di token generata dal Lexer.</param>
    /// <returns>Una stringa che rappresenta il programma BASIC ricostruito.</returns>
    private static string ReconstructBasicFromTokens(IReadOnlyList<Token> tokens)
    {
        var sb = new StringBuilder();
        var currentLine = -1;
        var needsSpace = false;
        foreach (var token in tokens)
        {
            if (token.Kind == TokenKind.Eof)
                break;
            if (token.Kind == TokenKind.Newline)
            {
                sb.AppendLine();
                needsSpace = false;
                continue;
            }

            // Inizia una nuova riga quando il numero di riga del sorgente cambia
            if (token.Line != currentLine)
            {
                if (sb.Length > 0 && currentLine != -1)
                {
                    if (sb[^1] != '\n') sb.AppendLine();
                }

                sb.Append($"[{token.Line}] ");
                currentLine = token.Line;
                needsSpace = false;
            }

            // --- Logica di Spaziatura ---
            if (needsSpace && !IsDelimiter(token.Kind) && token.Kind is not TokenKind.LineNumber)
            {
                sb.Append(' ');
            }

            switch (token.Kind)
            {
                // --- Stampe letterali per stringhe ---
                case TokenKind.StringLiteral:
                    sb.Append('"').Append(token.Literal).Append('"');
                    break;
                // Stampe per numeri di riga (già formattati nel Lexer)
                case TokenKind.LineNumber:
                // Stampe per keyword e identificatori
                default:
                    sb.Append(token.Lexeme);
                    break;
            }

            // Aggiorna lo stato dello spazio successivo
            needsSpace = token.Kind is not (TokenKind.Dot or TokenKind.Comma or TokenKind.Semicolon
                or TokenKind.LeftParen or TokenKind.LeftBracket);
        }

        return sb.ToString();
    }

    // Helper per determinare se un token è un delimitatore (non richiede spazio prima).
    private static bool IsDelimiter(TokenKind kind)
    {
        return kind is TokenKind.Dot or TokenKind.Comma or TokenKind.Semicolon or TokenKind.RightParen
            or TokenKind.RightBracket or TokenKind.Colon;
    }

    /// <summary>
    /// [NUOVO HELPER] Stampa un dump dettagliato della SourceMap e del flusso di token
    /// per scopi di diagnostica e debug del Lexer.
    /// </summary>
    /// <param name="sourceMap">La SourceMap da stampare.</param>
    /// <param name="tokens">La lista di token generata dal Lexer.</param>
    /// <param name="filePath">Il percorso del file sorgente, usato per il titolo del dump.</param>
    /// <param name="tokensOnly">Se true, stampa solo il dump dei token, omettendo la SourceMap e la ricostruzione del codice.</param>
    public static void DumpSourceMapAndTokens(ImmutableArray<SourceLine> sourceMap, IReadOnlyList<Token> tokens,
        string filePath, bool printSourceMap = true, bool printTokens = true, bool printSourceCode = true)
    {
        if (printSourceMap)
        {
            Console.WriteLine("\n--- SOURCE MAP DUMP ---");
            Console.WriteLine("==========================================================");
            for (var i = 0; i < sourceMap.Length; i++)
            {
                Console.WriteLine($"Index={i}, OrigLine={sourceMap[i].OriginalLineNumber}, Text='{sourceMap[i].Text}'");
            }

            Console.WriteLine("==========================================================");
        }

        if (printTokens)
        {
            Console.WriteLine($"\n--- TOKEN DUMP for {Path.GetFileName(filePath)} ---");
            Console.WriteLine("==========================================================");
            Console.WriteLine("{0,-5} {1,-20} {2,-25} {3}", "Line", "Kind", "Lexeme", "Literal");
            Console.WriteLine("----------------------------------------------------------");
            foreach (var token in tokens)
            {
                Console.WriteLine("{0,-5} {1,-20} {2,-25} {3}",
                    token.Line,
                    token.Kind,
                    $"'{token.Lexeme}'",
                    token.Literal ?? "null");
            }

            Console.WriteLine("==========================================================");
        }

        if (!printSourceCode) return;
        Console.WriteLine($"\n--- SOURCE CODE for {Path.GetFileName(filePath)} ---");
        Console.WriteLine(ReconstructBasicFromTokens(tokens));
        Console.WriteLine("==========================================================");
    }

    /// <summary>
    /// Metodo helper di diagnostica per stampare unicamente la soruce map.
    /// </summary>
    /// <param name="preprocessedFile">L'oggetto contenente i token da stampare.</param>
    /// <param name="filePath">Il percorso del file, usato per il titolo del dump.</param>
    public static void DumpSourceMap(BasicProgram.PreprocessedFile preprocessedFile, string filePath) =>
        DumpSourceMapAndTokens(preprocessedFile.SourceMap, preprocessedFile.Tokens, filePath,
            printSourceMap: true, printTokens: false, printSourceCode: false);

    /// <summary>
    /// Metodo helper di diagnostica per stampare unicamente il flusso di token di un file pre-elaborato.
    /// </summary>
    /// <param name="preprocessedFile">L'oggetto contenente i token da stampare.</param>
    /// <param name="filePath">Il percorso del file, usato per il titolo del dump.</param>
    public static void DumpTokens(BasicProgram.PreprocessedFile preprocessedFile, string filePath) =>
        DumpSourceMapAndTokens(preprocessedFile.SourceMap, preprocessedFile.Tokens, filePath,
            printSourceMap: false, printTokens: true, printSourceCode: false);

    /// <summary>
    /// Metodo helper di diagnostica per stampare unicamente il codice sorgente ricostruito dai tokens.
    /// </summary>
    /// <param name="preprocessedFile">L'oggetto contenente i token da stampare.</param>
    /// <param name="filePath">Il percorso del file, usato per il titolo del dump.</param>
    public static void DumpSourceCode(BasicProgram.PreprocessedFile preprocessedFile, string filePath) =>
        DumpSourceMapAndTokens(preprocessedFile.SourceMap, preprocessedFile.Tokens, filePath,
            printSourceMap: false, printTokens: false, printSourceCode: true);

    /// <summary>
    /// Stampa il contenuto di una mappa di etichette, mostrando ogni etichetta e la sua posizione risolta
    /// (all'interno di una procedura o a livello globale). Utile per il debug del linking.
    /// </summary>
    /// <param name="labelMap">La mappa delle etichette da stampare.</param>
    /// <param name="contextMessage">Un messaggio di contesto per identificare l'origine della mappa (es. "Modulo Principale").</param>
    /// <param name="useConsole">Specifica il target di output.</param>
    public static void DumpLabelMap(IReadOnlyDictionary<string, LabelLocation> labelMap, string contextMessage,
        bool useConsole = true)
    {
        WriteOutput($"--- [LABEL MAP DUMP: {contextMessage}] ---", useConsole);
        if (labelMap.Count == 0)
        {
            WriteOutput("  (Mappa vuota o nulla)", useConsole);
            WriteOutput("---------------------------------", useConsole);
            return;
        }

        foreach (var (name, location) in labelMap.OrderBy(kvp => kvp.Key))
        {
            var locationStr = location.Procedure != null
                ? $"in {location.Procedure.Name.Lexeme}"
                : $"a riga {location.Line}";
            WriteOutput($"  - '{name}' -> {locationStr}", useConsole);
        }

        WriteOutput($"  Totale etichette: {labelMap.Count}", useConsole);
        WriteOutput("---------------------------------", useConsole);
    }

    /// <summary>
    /// Metodo di diagnostica per stampare lo stato corrente delle mappe della libreria standard in StdLib.
    /// </summary>
    private static void DumpStdLibState()
    {
        Log("\n--- DUMP StdLib.Instance (PRE-LINKER) ---", true);
        Log("--- BuiltinFunctionProvidersMap ---", true);
        Log("  (disponibile solo via IParserRegistry)", true);

        Log("\n--- UnifiedCommandRegistry ---", true);
        if (ParserRegistries.UnifiedCommandRegistry.Count != 0)
        {
            foreach (var kvp in ParserRegistries.UnifiedCommandRegistry.OrderBy(item => item.Key.ToString()))
            {
                var registrations = kvp.Value.Select(r => $"{r.ProvidingNamespace}");
                Log($"  - CMD {kvp.Key,-20}: {string.Join(", ", registrations)}", true);
            }
        }
        else
        {
            Log("  (Vuoto)", true);
        }

        Log("--- FINE DUMP ---\n", true);
    }

    /// <summary>
    /// Stampa una rappresentazione gerarchica dell'intero AST di un programma,
    /// evidenziando i SymbolId e gli ScopeId di ogni nodo.
    /// Questo è uno strumento di diagnostica fondamentale per verificare il risultato
    /// delle fasi di Binding e Risoluzione.
    /// </summary>
    /// <param name="program">Il programma contenente i moduli da stampare.</param>
    public static void DumpAstWithSymbolsAndScopes(BasicProgram program)
    {
        Console.WriteLine("\n======================================================================");
        Console.WriteLine("=== AST DUMP WITH SYMBOL & SCOPE IDs ===");
        Console.WriteLine("======================================================================");

        foreach (var module in program.LoadedModules.Values.OrderBy(m => m.FilePath))
        {
            Console.WriteLine($"\n--- MODULE: {Path.GetFileName(module.FilePath)} ---");

            Console.WriteLine("\n== DECLARATIONS ==");
            if (module.Declarations.Any())
            {
                foreach (var decl in module.Declarations)
                {
                    DumpNodeRecursive(decl, "");
                }
            }
            else
            {
                Console.WriteLine("(No top-level declarations)");
            }

            Console.WriteLine("\n== MAIN BODY STATEMENTS ==");
            if (module.Statements.Any())
            {
                foreach (var (line, stmts) in module.Statements.OrderBy(kvp => kvp.Key))
                {
                    Console.WriteLine($"LINE {line}:");
                    foreach (var stmt in stmts)
                    {
                        DumpNodeRecursive(stmt, "  ");
                    }
                }
            }
            else
            {
                Console.WriteLine("(No main body statements)");
            }
        }

        Console.WriteLine("\n======================================================================");
        Console.WriteLine("=== END OF AST DUMP ===");
        Console.WriteLine("======================================================================");
    }

    /// <summary>
    /// Helper ricorsivo per stampare un singolo nodo AST con i suoi dettagli di scope e simbolo.
    /// </summary>
    private static void DumpNodeRecursive(AstNode node, string indent)
    {
        var sb = new StringBuilder();
        sb.Append(indent);
        sb.Append($"-> {node.GetType().Name}");
        sb.Append($" [Scope:{node.ScopeId}]");

        // --- Stampa il Namespace Dichiarato se presente ---
        if (node is Decl { DeclaredNamespace: not null } dns)
        {
            sb.Append($" [NS:{dns.DeclaredNamespace}]");
        }
        
        // Aggiunge il SymbolId se il nodo ne ha uno significativo
        var symbolIdString = node switch
        {
            VariableExpr ve => $"#{ve.SymbolId}",
            VariableTarget vt => $"#{vt.SymbolId}",
            VariableDeclaration vd => $"#{vd.SymbolId}",
            ArrayAccessExpr aae => $"#{aae.SymbolId}",
            ArrayElementTarget aet => $"#{aet.SymbolId}",
            FunctionExpr fe => $"#{fe.SymbolId}",
            ProcedureDefinition pd => $"#{pd.SymbolId}",
            PropertyDefinition propd => $"#{propd.SymbolId}",
            TypeField tf => $"#{tf.SymbolId}",
            UserDefinedType udt => $"#{udt.SymbolId}",
            ClassDefinition cd => $"#{cd.SymbolId}",
            EnumDefinition ed => $"#{ed.SymbolId}",
            EnumMember em => $"#{em.SymbolId}",
            DelegateDefinition dd => $"#{dd.SymbolId}",
            _ => ""
        };

        sb.Append($" {node}");

        // Se abbiamo stampato un symbol ID tramite switch, non ripeterlo se è già nel ToString().
        if (!string.IsNullOrEmpty(symbolIdString) && !node.ToString().Contains(symbolIdString))
        {
            sb.Append($" {symbolIdString}");
        }

        Console.WriteLine(sb.ToString());

        // Attraversamento ricorsivo dei figli
        var nextIndent = indent + "  ";
        switch (node)
        {
            case LetStmt n:
                foreach (var a in n.Assignments)
                {
                    DumpNodeRecursive(a.Target, nextIndent);
                    DumpNodeRecursive(a.Value, nextIndent);
                }

                break;
            case VariableTarget:
                break;
            case DeclarationStmt n:
                foreach (var d in n.Declarations) DumpNodeRecursive(d, nextIndent);
                break;            
            case VariableDeclaration n:
                if (n.InitialValue != null) DumpNodeRecursive(n.InitialValue, nextIndent);
                foreach (var d in n.Dimensions) DumpNodeRecursive(d, nextIndent);
                break;
            case ConstStmt n:
                foreach (var d in n.Declarations) DumpNodeRecursive(d, nextIndent);
                break;
            case ConstDeclaration n: DumpNodeRecursive(n.InitialValue, nextIndent); break;
            case ExpressionStmt n: DumpNodeRecursive(n.Expression, nextIndent); break;
            case BinaryExpr n:
                DumpNodeRecursive(n.Left, nextIndent);
                DumpNodeRecursive(n.Right, nextIndent);
                break;
            case UnaryExpr n: DumpNodeRecursive(n.Right, nextIndent); break;
            case GroupingExpr n: DumpNodeRecursive(n.Expression, nextIndent); break;
            case IfStmt n:
                foreach (var c in n.IfClauses) DumpNodeRecursive(c, nextIndent);
                if (n.ElseClause != null) DumpNodeRecursive(n.ElseClause, nextIndent);
                break;
            case IfClause n:
                DumpNodeRecursive(n.Condition, nextIndent);
                foreach (var s in n.Statements) DumpNodeRecursive(s, nextIndent);
                break;
            case ElseClause n:
                foreach (var s in n.Statements) DumpNodeRecursive(s, nextIndent);
                break;
            case SelectStmt n:
                DumpNodeRecursive(n.Expression, nextIndent);
                foreach (var c in n.CaseClauses) DumpNodeRecursive(c, nextIndent);
                if (n.ElseClause != null) DumpNodeRecursive(n.ElseClause, nextIndent);
                break;
            case CaseClause n:
                foreach (var t in n.Tests) DumpNodeRecursive(t, nextIndent);
                foreach (var s in n.Statements) DumpNodeRecursive(s, nextIndent);
                break;
            case CaseElseClause n:
                foreach (var s in n.Statements) DumpNodeRecursive(s, nextIndent);
                break;
            case CaseValue n: DumpNodeRecursive(n.Value, nextIndent); break;
            case CaseRange n:
                DumpNodeRecursive(n.Start, nextIndent);
                DumpNodeRecursive(n.End, nextIndent);
                break;
            case CaseComparison n: DumpNodeRecursive(n.Value, nextIndent); break;
            case CaseTypePattern n:
                if (n.WhenCondition != null) DumpNodeRecursive(n.WhenCondition, nextIndent);
                break;
            case CaseValuePattern n:
                DumpNodeRecursive(n.PatternValue, nextIndent);
                if (n.WhenCondition != null) DumpNodeRecursive(n.WhenCondition, nextIndent);
                break;
            case ForStmt n:
                DumpNodeRecursive(n.Variable, nextIndent);
                DumpNodeRecursive(n.Start, nextIndent);
                DumpNodeRecursive(n.End, nextIndent);
                if (n.Step != null) DumpNodeRecursive(n.Step, nextIndent);
                break;
            case ForEachStmt n:
                foreach (var v in n.IteratorVariables) DumpNodeRecursive(v, nextIndent);
                DumpNodeRecursive(n.Collection, nextIndent);
                foreach (var s in n.Body) DumpNodeRecursive(s, nextIndent);
                break;
            case TryCatchStmt n:
                foreach (var s in n.TryBody) DumpNodeRecursive(s, nextIndent);
                foreach (var c in n.CatchClauses) DumpNodeRecursive(c, nextIndent);
                if (n.Finally != null) DumpNodeRecursive(n.Finally, nextIndent);
                break;
            case CatchClause n:
                if (n.Variable != null) DumpNodeRecursive(n.Variable, nextIndent);
                if (n.WhenCondition != null) DumpNodeRecursive(n.WhenCondition, nextIndent);
                foreach (var s in n.Body) DumpNodeRecursive(s, nextIndent);
                break;
            case FinallyClause n:
                foreach (var s in n.Body) DumpNodeRecursive(s, nextIndent);
                break;
            case ProcedureDefinition n:
                foreach (var p in n.Parameters) DumpNodeRecursive(p, nextIndent);
                foreach (var s in n.Body) DumpNodeRecursive(s, nextIndent);
                break;
            case Parameter n:
                if (n.DefaultValue != null) DumpNodeRecursive(n.DefaultValue, nextIndent);
                break;
            case UserDefinedType n:
                foreach (var f in n.Fields) DumpNodeRecursive(f, nextIndent);
                foreach (var p in n.Properties) DumpNodeRecursive(p, nextIndent);
                foreach (var m in n.Methods) DumpNodeRecursive(m, nextIndent);
                break;
            case EnumDefinition n:
                foreach (var m in n.Members) DumpNodeRecursive(m, nextIndent);
                break;
            case EnumMember n:
                if (n.ValueExpr != null) DumpNodeRecursive(n.ValueExpr, nextIndent);
                break;
            case DelegateDefinition n:
                foreach (var p in n.Parameters) DumpNodeRecursive(p, nextIndent);
                break;
            case ClassDefinition n:
                foreach (var m in n.Members)
                {
                    switch (m)
                    {
                        case FieldMember fm: DumpNodeRecursive(fm.Declaration, nextIndent); break;
                        case MethodMember mm: DumpNodeRecursive(mm.Procedure, nextIndent); break;
                        case PropertyMember pm: DumpNodeRecursive(pm.Property, nextIndent); break;
                        // Constructor/Destructor/Operator non sono AstNode diretti, ma contengono nodi.
                        // Per ora ci concentriamo sui membri principali.
                    }
                }
                break;
            case PropertyDefinition n:
                if (n.Getter != null)
                    foreach (var s in n.Getter.Body)
                        DumpNodeRecursive(s, nextIndent);
                if (n.Setter != null)
                    foreach (var s in n.Setter.Body)
                        DumpNodeRecursive(s, nextIndent);
                break;
            case ArrayAccessExpr n:
                foreach (var i in n.Indices) DumpNodeRecursive(i, nextIndent);
                break;
            case ArrayElementTarget n:
                foreach (var i in n.Indices) DumpNodeRecursive(i, nextIndent);
                break;
            case FieldAccessExpr n: DumpNodeRecursive(n.Structure, nextIndent); break;
            case FieldTarget n: DumpNodeRecursive(n.Structure!, nextIndent); break;
            case MethodCallExpr n:
                DumpNodeRecursive(n.Instance, nextIndent);
                foreach (var a in n.Args) DumpNodeRecursive(a, nextIndent);
                break;
            case PropertyAccessExpr n: DumpNodeRecursive(n.Instance, nextIndent); break;
            case NewExpr n:
                if (n.ConstructorArgs != null)
                    foreach (var a in n.ConstructorArgs)
                        DumpNodeRecursive(a, nextIndent);
                if (n.InitialValues != null)
                    foreach (var v in n.InitialValues)
                        DumpNodeRecursive(v, nextIndent);
                break;
            case LambdaExpr n:
                foreach (var p in n.Parameters) DumpNodeRecursive(p, nextIndent);
                if (n.ExpressionBody != null) DumpNodeRecursive(n.ExpressionBody, nextIndent);
                if (n.BlockBody != null)
                    foreach (var s in n.BlockBody)
                        DumpNodeRecursive(s, nextIndent);
                break;
            case MouseSetStmt n:
                DumpNodeRecursive(n.X, nextIndent);
                DumpNodeRecursive(n.Y, nextIndent);
                break;
            case MouseLimitStmt n:
                if (n.X1 != null) DumpNodeRecursive(n.X1, nextIndent);
                if (n.Y1 != null) DumpNodeRecursive(n.Y1, nextIndent);
                if (n.X2 != null) DumpNodeRecursive(n.X2, nextIndent);
                if (n.Y2 != null) DumpNodeRecursive(n.Y2, nextIndent);
                break;
            case MouseCursorStmt n:
                DumpNodeRecursive(n.CursorId, nextIndent);
                break;
            case DbfAggregateStmt n:
                DumpNodeRecursive(n.KeyExpression, nextIndent);
                DumpNodeRecursive(n.FileName, nextIndent);
                if (n.Scope != null) DumpNodeRecursive(n.Scope, nextIndent);
                DumpNodeRecursive(n.AggregationsExpr, nextIndent);
                break;
            case DbfReportStmt n:
                DumpNodeRecursive(n.FormFile, nextIndent);
                if (n.Scope != null) DumpNodeRecursive(n.Scope, nextIndent);
                if (n.Destination != null) DumpNodeRecursive(n.Destination, nextIndent);
                break;
        }
    }
}