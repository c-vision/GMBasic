// FILE: Interpreter.Parser.Ast.Dump.cs

using System.Globalization;
using System.Text;

// ReSharper disable UnusedMember.Local

// ReSharper disable UnusedVariable

// ReSharper disable InconsistentNaming
// ReSharper disable InvalidXmlDocComment
namespace Basic;

/// <summary>
///     Contiene i metodi per il dump dell'AST.
/// </summary>
public sealed partial class Parser
{
    // ====================================================================================================================
    // --- METODI PUBBLICI E DI CONTROLLO PRINCIPALE ---
    // ====================================================================================================================

    /// <summary>
    /// Genera un dump gerarchico completo del programma, con opzioni per filtrare i moduli da visualizzare.
    /// È il punto di ingresso pubblico per l'analisi dell'AST. Non usa più la ResolutionMap.
    /// </summary>
    public static void DumpASTHierarchical(BasicProgram program, bool showAllModules = false,
        bool showUsedBuiltins = false, bool useConsole = false)
    {
        var mainModulePath = program.LoadedModules.FirstOrDefault(kvp => kvp.Value == program.MainModule).Key ??
                             "MainProgram";

        // 1. DUMP DEL MODULO PRINCIPALE (SEMPRE)
        WriteOutput("======================================================================", useConsole);
        WriteOutput($"// AST DUMP FOR MODULE: {mainModulePath}", useConsole);
        WriteOutput("======================================================================", useConsole);
        DumpASTHierarchicalInternal(program, program.MainModule, useConsole);

        if (!showAllModules)
        {
            return;
        }

        // --- SE showAllModules è TRUE, PROCEDIAMO ---

        // Costruisce la lista dei namespace usati direttamente dagli import del modulo principale e dai default.
        var usedNamespaces = new HashSet<string>(program.Settings.DefaultNamespaces, StringComparer.OrdinalIgnoreCase);
        if (program.MainModule != null)
        {
            foreach (var import in program.MainModule.Imports)
            {
                usedNamespaces.Add(import.NamespaceName.Lexeme);
            }
        }

        // 2. DUMP DEGLI ALTRI MODULI UTENTE
        foreach (var (path, module) in program.LoadedModules.OrderBy(kvp => kvp.Key))
        {
            if (path.Equals(mainModulePath, StringComparison.OrdinalIgnoreCase) ||
                path.StartsWith("virtual::", StringComparison.OrdinalIgnoreCase))
            {
                continue;
            }

            WriteOutput("\n======================================================================", useConsole);
            WriteOutput($"// AST DUMP FOR USER MODULE: {path}", useConsole);
            WriteOutput("======================================================================", useConsole);
            DumpASTHierarchicalInternal(program, module, useConsole);
        }

        // 3. DUMP DEI MODULI BUILT-IN (solo se richiesto)
        if (!showUsedBuiltins) return;
        foreach (var (path, module) in program.LoadedModules.OrderBy(kvp => kvp.Key))
        {
            if (!path.StartsWith("virtual::", StringComparison.OrdinalIgnoreCase)) continue;
            var namespaceName = path.Split("::")[1];

            if (!usedNamespaces.Contains(namespaceName)) continue;

            WriteOutput("\n======================================================================", useConsole);
            WriteOutput($"// AST DUMP FOR BUILT-IN MODULE: {namespaceName}", useConsole);
            WriteOutput("======================================================================", useConsole);
            DumpASTHierarchicalInternal(program, module, useConsole);
        }
    }

    /// <summary>
    /// Esegue il dump strutturato dell'Abstract Syntax Tree per un singolo modulo,
    /// iterando su tutte le sue sezioni principali: dichiarazioni globali, definizioni di tipi e classi,
    /// corpo del programma e definizioni di procedure.
    /// </summary>
    /// <param name="module">Il modulo specifico di cui eseguire il dump.</param>
    /// <param name="useConsole">Se true, l'output viene indirizzato alla console; altrimenti, a un listener di debug.</param>
    /// <param name="title">Un titolo opzionale per il dump.</param>
    /// <summary>
    /// Esegue il dump strutturato dell'Abstract Syntax Tree per un singolo modulo.
    /// QUESTA VERSIONE È STATA AGGIORNATA per leggere le definizioni dalla nuova proprietà `Declarations`.
    /// </summary>
    private static void DumpASTHierarchicalInternal(BasicProgram program, BasicModule? module, bool useConsole = false,
        string title = "")
    {
        if (module == null)
        {
            WriteOutput($"=== DUMP AST GERARCHICO {title} (Modulo non disponibile/nullo) ===", useConsole);
            return;
        }

        WriteOutput($"=== DUMP AST GERARCHICO {title} ===", useConsole);

        if (module.Declarations.Any())
        {
            WriteOutput("== DICHIARAZIONI DEL MODULO ==", useConsole);
            foreach (var declNode in module.Declarations)
            {
                switch (declNode)
                {
                    case UserDefinedType udt:
                        DumpTypeDefinition(program, udt, "", useConsole);
                        break;
                    case InterfaceDefinition interfaceDef:
                        DumpInterfaceDefinition(program, interfaceDef, "", useConsole);
                        break;
                    case ClassDefinition classDef:
                        DumpClassDefinition(program, classDef, "", useConsole);
                        break;
                    case EnumDefinition enumDef:
                        DumpEnumDefinition(program, enumDef, "", useConsole);
                        break;
                    case DelegateDefinition delegateDef:
                        DumpDelegateDefinition(program, delegateDef, "", useConsole);
                        break;
                    case ProcedureDefinition procDef:
                        // Gestisce FunctionDefinition, GeneratorDefinition, ExternalProcedureDefinition
                        DumpProcedureDefinition(program, procDef, "", useConsole);
                        break;
                    case DeclarationStmt declStmt: // Per DIM/GLOBAL/STATIC a livello di modulo
                        DumpStatement(declStmt, "", program, useConsole);
                        break;
                    case ConstStmt constStmt: // Per CONST a livello di modulo
                        DumpStatement(constStmt, "", program, useConsole);
                        break;
                    case VariableDeclaration varDecl:
                        // Una VariableDeclaration a livello di modulo è implicitamente un'istruzione DIM
                        var implicitDimToken = new Token(TokenKind.Dim, "DIM", null, varDecl.Position);
                        DumpStatement(new DeclarationStmt(implicitDimToken, [varDecl]), "", program, useConsole);
                        break;
                    case VariableTarget vt:
                        WriteOutput($"-> VariableTarget {TargetToString(vt, program)} [Implicit]", useConsole);
                        break;
                    default:
                        switch (declNode)
                        {
                            // Se ci sono altri tipi di nodi, li stampiamo in modo generico
                            case Stmt s:
                                DumpStatement(s, "", program, useConsole);
                                break;
                            // Aggiunto controllo per ignorare i token placeholder senza stampare un errore
                            case Token: /* Non fare nulla, è un placeholder */
                                break;
                            default:
                                WriteOutput($"  -> Nodo di dichiarazione non gestito: {declNode.GetType().Name}",
                                    useConsole);
                                break;
                        }

                        break;
                }
            }

            WriteOutput("", useConsole);
        }

        if (module.Statements.Count != 0)
        {
            WriteOutput("== CORPO PRINCIPALE ==", useConsole);
            foreach (var (lineNumber, stmts) in module.Statements.OrderBy(kvp => kvp.Key))
            {
                WriteOutput($"LINE {lineNumber}:", useConsole);
                foreach (var stmt in stmts) DumpStatement(stmt, "  ", program, useConsole);
            }
        }

        WriteOutput("=== FINE DUMP AST ===", useConsole);
    }

    /// <summary>
    /// Helper per formattare l'intestazione di un nodo AST, includendo ScopeId e DeclaredNamespace.
    /// </summary>
    private static string FormatNodeHeader(AstNode node, string nodeTypeName, string mainText)
    {
        var sb = new StringBuilder();
        sb.Append($"-> {nodeTypeName}");

        // Aggiunge informazioni di scope e namespace
        sb.Append($" [Scope:{node.ScopeId}]");
        if (node is Decl d && !string.IsNullOrEmpty(d.DeclaredNamespace))
        {
            sb.Append($" [NS:{d.DeclaredNamespace}]");
        }

        if (!string.IsNullOrEmpty(mainText))
        {
            sb.Append(": ");
            sb.Append(mainText);
        }

        return sb.ToString();
    }

    /// <summary>
    /// Nuovo metodo helper per il dump di una ProcedureDefinition generica.
    /// </summary>
    private static void DumpProcedureDefinition(BasicProgram program, ProcedureDefinition procedure, string indent,
        bool useConsole)
    {
        string header;
        switch (procedure)
        {
            case NativeProcedureDefinition nativeDef:
                var returnTypeStrNative =
                    nativeDef.ReturnType is not DoubleType || nativeDef.Name.Lexeme.EndsWith('$')
                        ? $" AS {DataTypeToString(nativeDef.ReturnType)}"
                        : "";
                var paramsStrNative = string.Join(", ", nativeDef.Parameters.Select(p => p.ToString()));
                header =
                    $"{nativeDef.Name.Lexeme}({paramsStrNative}){returnTypeStrNative} -> BUILTIN({nativeDef.BuiltinKind})";
                break;
            case FunctionDefinition funcDef:
                header =
                    $"{(funcDef.ReturnType is VoidType ? "SUB" : "FUNCTION")} {funcDef.OriginalName.Lexeme} AS {DataTypeToString(funcDef.ReturnType)}";
                break;
            case GeneratorDefinition genDef:
                header = $"GENERATOR {genDef.OriginalName.Lexeme} AS {DataTypeToString(genDef.YieldType)}";
                break;
            default:
                header = $"{procedure.Name.Lexeme}";
                break;
        }

        WriteOutput($"{indent}{FormatNodeHeader(procedure, procedure.GetType().Name, header)}", useConsole);

        if (procedure.Parameters.Any())
            WriteOutput(
                $"{indent}  Parameters: {string.Join(", ", procedure.Parameters.Select(p => p.ToString()))}",
                useConsole);

        if (procedure.Body.Any())
        {
            WriteOutput($"{indent}  Body:", useConsole);
            foreach (var stmt in procedure.Body) DumpStatement(stmt, indent + "    ", program, useConsole);
        }
    }

    // ====================================================================================================================
    // --- DISPATCHER PRINCIPALE E GESTORI DI BLOCCHI COMPLESSI ---
    // ====================================================================================================================

    /// <summary>
    /// Funge da dispatcher principale per il dump di un'istruzione (`Stmt`).
    /// Analizza il tipo di istruzione e delega a un metodo di gestione specializzato
    /// per blocchi complessi (es. IF, SELECT) o a dispatcher di categoria (es. comandi di I/O, grafica).
    /// Le istruzioni semplici vengono gestite direttamente tramite un'espressione switch.
    /// CORRETTO: Assicura che FormatNodeHeader sia usato per tutti i nodi per stampare ScopeId.
    /// </summary>
    /// <param name="stmt">L'istruzione (`Stmt`) dell'AST da analizzare e stampare.</param>
    /// <param name="indent">La stringa di indentazione da applicare per riflettere la gerarchia dell'albero.</param>
    /// <param name="useConsole">Specifica se l'output deve essere scritto sulla console o su un listener di debug.</param>
    private static void DumpStatement(Stmt stmt, string indent, BasicProgram program, bool useConsole)
    {
        // NOTA DI MODIFICA: Il dispatcher è stato mantenuto, ma ora ci assicuriamo che 
        // i metodi chiamati (es. DumpIfStatement) siano responsabili di chiamare FormatNodeHeader.
        // La modifica principale qui è per i casi gestiti direttamente.

        switch (stmt)
        {
            case IfStmt ifStmt:
                DumpIfStatement(program, ifStmt, indent, useConsole);
                break;
            case SelectStmt selectStmt:
                DumpSelectStatement(program, selectStmt, indent, useConsole);
                break;
            case GoSelectStmt goSelectStmt:
                DumpGoSelectStatement(program, goSelectStmt, indent, useConsole);
                break;
            case TryCatchStmt tryStmt:
                DumpTryCatchStatement(program, tryStmt, indent, useConsole);
                break;
            case LocalProcedureStmt localProcStmt:
            {
                var proc = localProcStmt.Procedure;
                var procType = proc switch
                {
                    FunctionDefinition => "FUNCTION", GeneratorDefinition => "GENERATOR",
                    _ => "PROCEDURE"
                };
                var returnType = proc switch
                {
                    FunctionDefinition fd => $" AS {DataTypeToString(fd.ReturnType)}",
                    GeneratorDefinition gd => $" AS {DataTypeToString(gd.YieldType)}",
                    _ => ""
                };
                // Usiamo FormatNodeHeader per stampare l'intestazione.
                WriteOutput(
                    $"{indent}{FormatNodeHeader(stmt, "LocalProcedureStmt", $"LOCAL {procType} {proc.Name.Lexeme}{returnType}")}",
                    useConsole);

                if (proc.Parameters.Any())
                    WriteOutput(
                        $"{indent}    Parameters: {string.Join(", ", proc.Parameters.Select(p => p.ToString()))}",
                        useConsole);
                WriteOutput($"{indent}    Body:", useConsole);
                foreach (var bodyStmt in proc.Body) DumpStatement(bodyStmt, indent + "      ", program, useConsole);
                break;
            }
            case RaiseEventStmt s:
                WriteOutput($"{indent}{FormatNodeHeader(s, "RaiseEventStmt", s.ToString())}", useConsole);
                break;
            case AddHandlerStmt s:
                WriteOutput($"{indent}{FormatNodeHeader(s, "AddHandlerStmt", s.ToString())}", useConsole);
                break;
            case RemoveHandlerStmt s:
                WriteOutput($"{indent}{FormatNodeHeader(s, "RemoveHandlerStmt", s.ToString())}", useConsole);
                break;
            default:
            {
                // I dispatcher di categoria devono essere responsabili della chiamata a FormatNodeHeader.
                // Questa logica rimane per centralizzare il fallback.
                if (IsConsoleStmt(stmt))
                {
                    DumpConsoleStatement(program, stmt, indent, useConsole);
                    return;
                }

                if (IsMouseStmt(stmt))
                {
                    DumpMouseStatement(program, stmt, indent, useConsole);
                    return;
                }

                if (IsGraphicsStmt(stmt))
                {
                    DumpGraphicsStatement(program, stmt, indent, useConsole);
                    return;
                }

                if (IsFileIOStmt(stmt))
                {
                    DumpFileIOStatement(program, stmt, indent, useConsole);
                    return;
                }

                if (IsFileSystemStmt(stmt))
                {
                    DumpFileSystemStatement(program, stmt, indent, useConsole);
                    return;
                }

                if (IsConcurrencyStmt(stmt))
                {
                    DumpConcurrencyStatement(program, stmt, indent, useConsole);
                    return;
                }

                if (IsDatabaseStmt(stmt))
                {
                    DumpDatabaseStatement(program, stmt, indent, useConsole);
                    return;
                }

                if (IsArrayStmt(stmt))
                {
                    DumpArrayStatement(program, stmt, indent, useConsole);
                    return;
                }

                if (IsLedgerStmt(stmt))
                {
                    DumpLedgerStatement(program, stmt, indent, useConsole);
                    return;
                }

                if (IsNetworkStmt(stmt))
                {
                    DumpNetworkStatement(program, stmt, indent, useConsole);
                    return;
                }

                if (IsPlottingStmt(stmt))
                {
                    DumpPlottingStatement(program, stmt, indent, useConsole);
                    return;
                }

                // Se non è una categoria speciale, usa lo switch generico
                var output = stmt switch
                {
                    LetStmt s => string.Join(", ",
                        s.Assignments.Select(a =>
                            $"{TargetToString(a.Target, program)} = {ExpressionToString(a.Value, program)}")),
                    DeclarationStmt s => $"{s.Keyword.Lexeme} {string.Join(", ", s.Declarations)}",
                    RedimStmt s => s.ToString(),
                    DataStmt s => string.Join(", ", s.Values.Select(v => ExpressionToString(v, program))),
                    ReadStmt s => string.Join(", ", s.Variables.Select(t => TargetToString(t, program))),
                    RestoreStmt s => s.Target != null ? s.Target.Lexeme : "",
                    SwapStmt s => $"{TargetToString(s.Left, program)}, {TargetToString(s.Right, program)}",
                    EndStmt => "",
                    EndProcedureStmt s => s.Token.Lexeme,
                    GotoStmt s => s.Target.Kind == TokenKind.Number ? s.Target.Literal?.ToString() : s.Target.Lexeme,
                    GosubStmt s => s.Target.Lexeme,
                    ReturnStmt s => s.Value != null ? ExpressionToString(s.Value, program) : "",
                    OnErrorStmt s => s.ProcedureCall != null 
                        ? $"ERROR {s.ProcedureCall}" 
                        : $"ERROR {s.Keyword?.Lexeme} {s.Target?.Lexeme}",
                    RethrowStmt => "THROW",
                    OnErrorResumeStmt => "ERROR RESUME NEXT",
                    ResumeStmt s => $"RESUME{(s.Target != null ? " " + s.Target.Lexeme : "")}",
                    ErrorStmt s =>
                        $"{ExpressionToString(s.ErrorCode, program)}{(s.Description != null ? ", " + ExpressionToString(s.Description, program) : "")}",
                    OnKeyStmt s => FormatOnKey(program, s),
                    OnMouseStmt s => $"ON MOUSE {s.EventKind.ToString().ToUpper()} {FormatAction(s.Action, program)}",
                    OnWindowStmt s => $"ON WINDOW {s.EventKind.ToString().ToUpper()} {FormatAction(s.Action, program)}",
                    LabelStmt s => $"{s.Label.Lexeme}:",
                    EnterLoopStmt s =>
                        $"ENTER {s.StartToken.Lexeme} (Continue: {s.ContinueLabel.Lexeme}, Exit: {s.ExitLabel.Lexeme})",
                    ExitLoopStmt s => $"EXIT LOOP ({s.EndToken.Lexeme})",
                    ForStmt s =>
                        $"FOR {s.Variable.NameSuffixed.Lexeme} = {ExpressionToString(s.Start, program)} TO {ExpressionToString(s.End, program)}{(s.Step != null ? " STEP " + ExpressionToString(s.Step, program) : "")}",
                    NextStmt s => $"NEXT {s.Variable.Lexeme}",
                    ForCheckAndJumpStmt s => $"CHECK {s.Variable.Lexeme} -> {s.ExitLabel.Lexeme}",
                    ConditionalJumpStmt s =>
                        $"IF {(s.JumpWhenTrue ? "" : "NOT ")}{ExpressionToString(s.Condition, program)} JUMP -> {s.TargetLabel.Lexeme}",
                    JumpStmt s => $"JUMP -> {s.TargetLabel.Lexeme}",
                    ContinueStmt s => $"CONTINUE -> {s.TargetLabel.Lexeme}",
                    ExitStmt s =>
                        $"EXIT {s.TargetLoop.Lexeme}{(s.TargetLabel != null ? " -> " + s.TargetLabel.Lexeme : "")}",
                    RemStmt s => s.ToString(),
                    ClipStmt s =>
                        $".{s.Command.Lexeme} {(s.Argument != null ? ExpressionToString(s.Argument, program) : s.Destination != null ? TargetToString(s.Destination, program) : "")}",
                    RandomizeStmt s => s.Seed != null ? ExpressionToString(s.Seed, program) : "",
                    KeyGenStmt s =>
                        $"{ExpressionToString(s.Algorithm, program)}, Private={TargetToString(s.PrivateKeyHandle, program)}, Public={TargetToString(s.PublicKeyHandle, program)}",
                    KeyCloseStmt s => ExpressionToString(s.KeyHandle, program),
                    GraphicsPrintExprStmt s => $"(Internal): {ExpressionToString(s.Expression, program)}",
                    GraphicsTabZoneStmt => "(Internal)",
                    GraphicsNewlineStmt => "(Internal)",
                    GraphicsTabStmt s => $"(Internal): {ExpressionToString(s.ColumnExpression, program)}",
                    EvaluateIndicesStmt s => $"(VM-Internal): {s.IndexExpressions.Length} indici",
                    StopStmt => "STOP",
                    RunStmt s => $"RUN {ExpressionToString(s.Command, program)}",
                    HttpListenerLoopStmt => "(VM-Internal)",
                    HttpConnectionHandlerStmt => "(VM-Internal)",
                    YieldStmt s => $"YIELD {ExpressionToString(s.ValueToYield, program)}",
                    OnJumpStmt onJump =>
                        $"ON {ExpressionToString(onJump.IndexExpression, program)} {onJump.Type} {string.Join(", ", onJump.TargetLabels.Select(t => t.Lexeme))}",
                    ExpressionStmt expr => ExpressionToString(expr.Expression, program),
                    ImportStmt import => import.ToString(),
                    RequireStmt req => req.ToString(),
                    JsonSaveStmt save => save.ToString(),
                    JsonWriteValueStmt write =>
                        $"WRITEVALUE {ExpressionToString(write.Key, program)}, {ExpressionToString(write.Value, program)}{(write.Section != null ? ", " + ExpressionToString(write.Section, program) : "")}",
                    JsonWriteSectionStmt writeSect =>
                        $"WRITESECTION {ExpressionToString(writeSect.Source, program)}{(writeSect.Section != null ? ", " + ExpressionToString(writeSect.Section, program) : "")}",
                    ConstStmt constStmt => $"CONST {string.Join(", ", constStmt.Declarations)}",
                    AmbiguousMethodStmt amb =>
                        $"{amb.CommandToken.Lexeme}({string.Join(", ", amb.Arguments.Select(v => ExpressionToString(v, program)))})",
                    MacroStmt macro => $"&{ExpressionToString(macro.Expression, program)}",
                    LocalTypeStmt localType => $"LOCAL {localType.Definition}",
                    _ => stmt.ToString() ?? stmt.GetType().Name
                };

                WriteOutput($"{indent}{FormatNodeHeader(stmt, stmt.GetType().Name, output ?? string.Empty)}",
                    useConsole);

                break;
            }
        }
    }

    /// [HELPER DEFINITIVO] Formatta la rappresentazione di un Target, leggendo
    /// il tipo direttamente dalla proprietà ResolvedType del nodo AST.
    /// Se il target è un accesso a elemento, mostra il tipo dell'elemento,
    /// non il tipo della collezione, per TUTTI i tipi di collezione.
    /// </summary>
    private static string FormatTargetWithType(BasicProgram program, Target target)
    {
        var resolvedType = GetResolvedType(target);

        if (resolvedType == null) return TargetToString(target, program); // Fallback se il tipo non è stato risolto.
        var finalTypeToDisplay = resolvedType;

        // Se il target è un accesso a un elemento (array o indicizzato)
        // e il tipo risolto è un tipo di collezione, estraiamo il tipo dell'elemento/valore.
        if (target is ArrayElementTarget or IndexedTarget)
        {
            finalTypeToDisplay = resolvedType switch
            {
                // Collezioni con un solo tipo di elemento
                ArrayType at => at.ElementType,
                ListType lt => lt.ElementType,
                HashSetType hst => hst.ElementType,
                QueueType qt => qt.ElementType,
                StackType st => st.ElementType,
                BinaryIndexedTreeType bitt => bitt.ElementType,
                LinkedListType llt => llt.ElementType,
                DoublyLinkedListType dllt => dllt.ElementType, // Aggiunto
                CircularLinkedListType cllt => cllt.ElementType,
                BinaryHeapType bht => bht.ElementType,
                BagType bt => bt.ElementType,
                GraphType gt => gt.NodeType,
                TimelineType tlt => tlt.ValueType,
                HashRingType hrt => hrt.NodeType, // Aggiunto
                RingBufferType rbt => rbt.ElementType, // Aggiunto

                // Collezioni chiave-valore (un accesso indicizzato imposta il valore)
                DictType dt => dt.ValueType,
                SortedDictType sdt => sdt.ValueType,
                TreeType tt => tt.ValueType,
                AVLTreeType avlt => avlt.ValueType,
                RedBlackTreeType rbt => rbt.ValueType,
                CacheType ct => ct.ValueType,
                BPlusTreeType bpt => bpt.ValueType, // Aggiunto

                // Tipi non indicizzabili per assegnazione (Trie, SparseSet) o fallback
                _ => resolvedType
            };
        }

        return $"{TargetToString(target, program)} AS {DataTypeToString(finalTypeToDisplay)}";

        // Funzione interna per estrarre il tipo risolto, se presente.
        static DataType? GetResolvedType(Target t) => t switch
        {
            VariableTarget vt => vt.ResolvedType,
            ArrayElementTarget aet => aet.ResolvedType,
            FieldTarget ft => ft.ResolvedType,
            IndexedTarget it => it.ResolvedType,
            _ => null
        };
    }

    /// <summary>
    /// Crea una rappresentazione testuale formattata per un nodo Target (L-value),
    /// includendo sempre i SymbolId per una diagnostica chiara e non ambigua.
    /// Per i campi (`FieldTarget`), include sia l'ID della struttura base che l'ID del campo.
    /// </summary>
    /// <param name="target">Il nodo Target da convertire in stringa.</param>
    /// <returns>Una stringa che rappresenta il target in modo leggibile e completo di ID.</returns>
    private static string TargetToString(Target target, BasicProgram program)
    {
        return target switch
        {
            VariableTarget vt => vt.NameSuffixed.Lexeme == vt.NameCleaned.Lexeme
                ? $"{vt.NameSuffixed.Lexeme}#{vt.SymbolId}"
                : $"{vt.NameSuffixed.Lexeme} (as {vt.NameCleaned.Lexeme})#{vt.SymbolId}",
            ArrayElementTarget aet => aet.NameSuffixed.Lexeme == aet.NameCleaned.Lexeme
                ? $"{aet.NameSuffixed.Lexeme}#{aet.SymbolId}({string.Join(", ", aet.Indices.Select(e => ExpressionToString(e, program)))})"
                : $"{aet.NameSuffixed.Lexeme} (as {aet.NameCleaned.Lexeme})#{aet.SymbolId}({string.Join(", ", aet.Indices.Select(e => ExpressionToString(e, program)))})",
            FieldTarget ft => $"{TargetToString(ft.Structure!, program)}.{ft.OriginalToken.Lexeme}#{ft.FieldSymbolId}",
            IndexedTarget it =>
                $"{ExpressionToString(it.Base, program)}[{string.Join(", ", it.Indices.Select(e => ExpressionToString(e, program)))}]",
            _ => target.ToString() ?? target.GetType().Name
        };
    }

    /// <summary>
    /// Controlla se un'istruzione appartiene alla categoria dei comandi di database (ISAM, DBF, SPEED).
    /// </summary>
    /// <param name="stmt">L'istruzione da esaminare.</param>
    /// <returns><c>true</c> se l'istruzione è un comando di database; altrimenti, <c>false</c>.</returns>
    private static bool IsDatabaseStmt(Stmt stmt) =>
        stmt is IsamUseStmt or IsamSelectWorkAreaStmt or IsamRecordStmt or IsamSkipStmt or IsamSeekStmt or
            IsamDeleteStmt or IsamRecallStmt or IsamPackStmt or IsamReindexStmt or IsamRepairStmt or IsamBackupStmt or IsamAppendStmt or IsamPutStmt or
            IsamLockStmt or IsamUnlockStmt or IsamSetOrderStmt or IsamSetRelationStmt or IsamCreateIndexStmt or
            IsamDropIndexStmt or IsamValidateStmt or IsamCopyToStmt or IsamJoinWithStmt or IsamSortToStmt or IsamSetFilterStmt or
            DbfUseStmt or DbfSelectWorkAreaStmt or DbfGetRecordStmt or DbfReplaceStmt or DbfDeleteStmt or DbfRecallStmt
            or DbfReportStmt or DbfAggregateStmt or DbfSortStmt or DbfUpdateStmt or DbfTotalOnStmt or DbfPackStmt or
            DbfZapStmt or DbfGoStmt or DbfSkipStmt or DbfSeekStmt or DbfCloseStmt or DbfSetDeletedStmt or
            DbfSetFilterStmt or DbfCopyToStmt or DbfReindexStmt or DbfAppendRecordStmt or DbfAppendFromStmt or
            DbfJoinWithStmt or DbfCreateViewStmt or DbfCopyStructureStmt or DbfSetOrderStmt or DbfSetRelationStmt or DbfIndexOnStmt or
            DbfSetExactStmt or DbfSetDefaultStmt or DbfSetPathStmt or DbfSetExclusiveStmt or DbfSetSchemaStmt or
            DbfAppendBlankStmt or DbfValidateStmt or SpeedOpenStmt or SpeedUseStmt or SpeedCreateIndexStmt or
            SpeedDropIndexStmt or SpeedGetStmt or SpeedPutStmt or SpeedDeleteStmt or SpeedSeekStmt or SpeedGoStmt or
            SpeedSkipStmt or SpeedSetOrderStmt or SpeedSetFilterStmt or SpeedSetRelationStmt or SpeedSelectStmt or
            SpeedCreateViewStmt or SpeedRefreshViewStmt or SpeedDropViewStmt or BeginTransactionStmt or CommitStmt or
            RollbackStmt or ExportToFileStmt or ImportFromFileStmt or ConvertDbfToIsamStmt;

    /// <summary>
    /// Gestisce la formattazione e la stampa di tutti i tipi di istruzioni relative ai database.
    /// Utilizza un'espressione switch per fornire una rappresentazione dettagliata e leggibile
    /// per ogni comando specifico dei motori ISAM, DBF e SPEED.
    /// CORRETTO: Utilizza FormatNodeHeader per stampare ScopeId e Namespace.
    /// </summary>
    /// <param name="stmt">L'istruzione di database da stampare.</param>
    /// <param name="indent">La stringa di indentazione per la formattazione gerarchica.</param>
    /// <param name="useConsole">Specifica il target di output (console o debug).</param>
    private static void DumpDatabaseStatement(BasicProgram program, Stmt stmt, string indent, bool useConsole)
    {
        var output = stmt switch
        {
            // --- Comandi Comuni ---
            BeginTransactionStmt s => $"BEGIN TRANSACTION [Level={s.Level}]",
            CommitStmt => "COMMIT",
            RollbackStmt => "ROLLBACK",
            ExportToFileStmt s =>
                $"EXPORT {s.EngineType.Lexeme} {ExpressionToString(s.Area, program)} TO {ExpressionToString(s.FilePath, program)} AS {s.Format.Lexeme}",
            ImportFromFileStmt s =>
                $"IMPORT {s.EngineType.Lexeme} {ExpressionToString(s.Area, program)} FROM {ExpressionToString(s.FilePath, program)} AS {s.Format.Lexeme}",
            ConvertDbfToIsamStmt s =>
                $"CONVERT DBF {ExpressionToString(s.SourceFile, program)} TO ISAM {ExpressionToString(s.DestinationFile, program)} TYPE {s.OriginalTypeName.Lexeme} KEY {s.OriginalPkFieldName.Lexeme} LEN {ExpressionToString(s.KeyLength, program)} [Unsafe={s.IsUnsafe}]",

            // --- Comandi ISAM ---
            IsamUseStmt s =>
                $"USE {ExpressionToString(s.FileName, program)} OF {s.OriginalTypeName.Lexeme}#{s.TypeSymbolId} ALIAS {s.OriginalAliasToken.Lexeme} [Locking={s.Locking}, Unsafe={s.IsUnsafe}, IndexedBy={(s.IndexedBy != null ? "Yes" : "No")}]",
            IsamSelectWorkAreaStmt s => $"SELECT {ExpressionToString(s.WorkArea, program)}",
            IsamRecordStmt s => $"GO #{ExpressionToString(s.FileNumber, program)}, {s.Location.Lexeme}",
            IsamSkipStmt s =>
                $"SKIP #{ExpressionToString(s.FileNumber, program)}, Count={ExpressionToString(s.RecordCount ?? new NumberExpr(1, s.Token), program)}",
            IsamSeekStmt s =>
                $"SEEK #{ExpressionToString(s.FileNumber, program)}, {ExpressionToString(s.PositionExpr, program)}{(s.IndexName != null ? $" USING {ExpressionToString(s.IndexName, program)}" : "")}",
            IsamDeleteStmt s => $"DELETE #{ExpressionToString(s.FileNumber, program)}{FormatScope(s.Scope)}",
            IsamRecallStmt s => $"RECALL #{ExpressionToString(s.FileNumber, program)}{FormatScope(s.Scope)}",
            IsamPackStmt s => $"PACK #{ExpressionToString(s.FileNumber, program)}",
            IsamReindexStmt s => $"REINDEX #{ExpressionToString(s.FileNumber, program)}",
            IsamRepairStmt s => $"REPAIR #{ExpressionToString(s.FileNumber, program)}",
            IsamBackupStmt s =>
                $"BACKUP #{ExpressionToString(s.FileNumber, program)} TO {ExpressionToString(s.DestinationFile, program)}",
            IsamAppendStmt s => s.ToString(),
            IsamPutStmt s => s.ToString(),
            IsamLockStmt s => $"LOCK #{ExpressionToString(s.FileNumber, program)} [IsRecordLock={s.IsRecordLock}]",
            IsamUnlockStmt s => $"UNLOCK #{ExpressionToString(s.FileNumber, program)} [IsRecordLock={s.IsRecordLock}]",
            IsamSetOrderStmt s =>
                $"SET ORDER ON #{ExpressionToString(s.FileNumber, program)}, {ExpressionToString(s.IndexName, program)}",
            IsamSetRelationStmt s =>
                $"SET RELATION ON #{ExpressionToString(s.FileNumber, program)}{(s.KeyExpression != null ? $" TO {ExpressionToString(s.KeyExpression, program)} INTO {s.ChildAlias?.Lexeme}" : " OFF")}",
            IsamCreateIndexStmt s =>
                $"CREATE INDEX {ExpressionToString(s.IndexName, program)} ON #{ExpressionToString(s.FileNumber, program)} KEY ({ExpressionToString(s.KeyExpression, program)}) LEN {ExpressionToString(s.KeyLength, program)}",
            IsamDropIndexStmt s =>
                $"DROP INDEX {ExpressionToString(s.IndexName, program)} ON #{ExpressionToString(s.FileNumber, program)}",
            IsamValidateStmt s => $"VALIDATE #{ExpressionToString(s.FileNumber, program)}",
            IsamCopyToStmt s =>
                $"COPY TO {ExpressionToString(s.DestinationFile, program)} ON #{ExpressionToString(s.FileNumber, program)}{FormatScope(s.Scope)}",
            IsamJoinWithStmt s =>
                $"JOIN WITH {s.SecondaryAliasToken.Lexeme} TO {ExpressionToString(s.DestinationFile, program)} ON #{ExpressionToString(s.FileNumber, program)} FOR {ExpressionToString(s.Condition.Condition, program)}",
            IsamSortToStmt s =>
                $"SORT TO {ExpressionToString(s.DestinationFile, program)} ON #{ExpressionToString(s.FileNumber, program)} ON {string.Join(", ", s.SortFields.Select(f => f.ToString()))}{FormatScope(s.Scope)}",
            IsamSetFilterStmt s =>
                $"SET FILTER ON #{ExpressionToString(s.FileNumber, program)} TO {(s.Condition != null ? ExpressionToString(s.Condition, program) : "(none)")}",

            // --- Comandi DBF ---
            DbfUseStmt s =>
                $"USE {ExpressionToString(s.FileName, program)} ALIAS {s.OriginalAliasToken?.Lexeme ?? "(default)"} AS {s.OriginalTypeName?.Lexeme ?? "UDT"}#{s.TypeSymbolId} [Indexes={s.IndexPaths.Length}, Locking={s.Locking}, Unsafe={s.IsUnsafe}, Map={s.FieldMappings.Count}]",
            DbfSelectWorkAreaStmt s => $"SELECT {ExpressionToString(s.WorkArea, program)}",
            DbfGetRecordStmt s => s.Destination != null ? $"GET {TargetToString(s.Destination, program)}" : "GET",
            DbfReplaceStmt s =>
                $"REPLACE {string.Join(", ", s.Replacements.Select(r => $"{TargetToString(r.Target, program)} WITH {ExpressionToString(r.Value, program)}"))}{FormatScope(s.Scope)}",
            DbfDeleteStmt s => $"DELETE{FormatScope(s.Scope)}",
            DbfRecallStmt s => $"RECALL{FormatScope(s.Scope)}",
            DbfSortStmt s =>
                $"SORT TO {ExpressionToString(s.FileName, program)} ON {string.Join(", ", s.SortFields.Select(sf => sf.ToString()))}{FormatScope(s.Scope)}",
            DbfUpdateStmt s =>
                $"UPDATE ON {ExpressionToString(s.KeyExpression, program)} FROM {s.OriginalAliasToken.Lexeme} REPLACE {string.Join(", ", s.Replacements.Select(r => $"{r.NameSuffixed.Lexeme} WITH {ExpressionToString(r.Value, program)}"))}",
            DbfTotalOnStmt s =>
                $"TOTAL ON {ExpressionToString(s.KeyExpression, program)} TO {ExpressionToString(s.FileName, program)}{(s.Fields != null ? $" {s.Fields}" : "")}{FormatScope(s.Scope)}",
            DbfAggregateStmt s =>
                $"AGGREGATE ON {ExpressionToString(s.KeyExpression, program)} TO {ExpressionToString(s.FileName, program)}{FormatScope(s.Scope)} COMPUTE {ExpressionToString(s.AggregationsExpr, program)}",
            DbfReportStmt s =>
                $"REPORT FORM {ExpressionToString(s.FormFile, program)}{FormatScope(s.Scope)}{(s.ToPrint ? " TO PRINT" : (s.Destination != null ? $" TO FILE {ExpressionToString(s.Destination, program)}" : ""))}",
            DbfPackStmt => "PACK",
            DbfZapStmt => "ZAP",
            DbfGoStmt s => $"GO {s.Destination}",
            DbfSkipStmt s => $"SKIP [Count={ExpressionToString(s.RecordCount ?? new NumberExpr(1, s.Token), program)}]",
            DbfSeekStmt s => $"SEEK {ExpressionToString(s.Value, program)}",
            DbfCloseStmt s => s.WorkAreas.IsEmpty ? "CLOSE DATABASES" : $"CLOSE {string.Join(", ", s.WorkAreas)}",
            DbfSetDeletedStmt s => $"SET DELETED {s.State.Lexeme}",
            DbfSetFilterStmt s =>
                $"SET FILTER TO {(s.Condition != null ? ExpressionToString(s.Condition, program) : "(none)")}",
            DbfCopyToStmt s => $"COPY TO {ExpressionToString(s.FileName, program)}{FormatScope(s.Scope)}",
            DbfReindexStmt => "REINDEX",
            DbfAppendRecordStmt s => $"APPEND RECORD {ExpressionToString(s.Source, program)}",
            DbfAppendFromStmt s => $"APPEND FROM {ExpressionToString(s.FileName, program)}{FormatScope(s.Scope)}",
            DbfJoinWithStmt s =>
                $"JOIN WITH {s.OriginalAliasToken.Lexeme} TO {ExpressionToString(s.FileName, program)} FOR {ExpressionToString(s.Condition.Condition, program)}{(s.Fields != null ? $" {s.Fields}" : "")}",
            DbfCreateViewStmt s =>
                $"CREATE VIEW {s.ViewNameToken.Lexeme} AS {s.SelectQuery?.ToString() ?? ExpressionToString(s.QueryExpression!, program)}",
            DbfCopyStructureStmt s => $"COPY STRUCTURE TO {ExpressionToString(s.FileName, program)}",
            DbfSetOrderStmt s => $"SET ORDER TO {ExpressionToString(s.Value, program)}",
            DbfSetRelationStmt s =>
                $"SET RELATION{(s.KeyExpression != null ? $" TO {ExpressionToString(s.KeyExpression, program)} INTO {s.ChildAlias?.Lexeme}" : " TO")}",
            DbfIndexOnStmt s =>
                $"INDEX ON {ExpressionToString(s.KeyExpression, program)} TO {ExpressionToString(s.FileName, program)}",
            DbfSetExactStmt s => $"SET EXACT {s.State.Lexeme}",
            DbfSetDefaultStmt s => $"SET DEFAULT TO {ExpressionToString(s.Path, program)}",
            DbfSetPathStmt s => $"SET PATH TO {ExpressionToString(s.PathList, program)}",
            DbfSetExclusiveStmt s => $"SET EXCLUSIVE {s.State.Lexeme}",
            DbfSetSchemaStmt s => $"SET SCHEMA STRICT {s.State.Lexeme}",
            DbfAppendBlankStmt => "APPEND BLANK",
            DbfValidateStmt s =>
                $"VALIDATE [Area={ExpressionToString(s.Area ?? new StringExpr("CURRENT", s.Token), program)}]",

            // --- Comandi Speed ---
            SpeedOpenStmt s =>
                $"SPEED OPEN {ExpressionToString(s.DatabasePath, program)} ALIAS {s.OriginalAliasToken.Lexeme}",
            SpeedUseStmt s =>
                $"SPEED USE {ExpressionToString(s.TableName, program)} FROM {ExpressionToString(s.DatabaseReference, program)} OF {s.OriginalTypeName.Lexeme}#{s.TypeSymbolId} ALIAS {s.OriginalAliasToken.Lexeme}{(s.IndexedBy != null ? $" {s.IndexedBy}" : "")}{(s.IsUnsafe ? " UNSAFE" : "")}",
            SpeedCreateIndexStmt s =>
                $"SPEED CREATE INDEX {ExpressionToString(s.IndexName, program)} ON {ExpressionToString(s.Alias, program)} KEY ({ExpressionToString(s.KeyExpression, program)})",
            SpeedDropIndexStmt s =>
                $"SPEED DROP INDEX {ExpressionToString(s.IndexName, program)} ON {ExpressionToString(s.Alias, program)}",
            SpeedGetStmt s =>
                $"SPEED GET {ExpressionToString(s.Alias, program)}, {TargetToString(s.Destination, program)}",
            SpeedPutStmt s =>
                $"SPEED PUT {ExpressionToString(s.Alias, program)}{(s.Source != null ? ", " + ExpressionToString(s.Source, program) : "")}",
            SpeedDeleteStmt s =>
                $"SPEED DELETE {ExpressionToString(s.Alias, program)}{(s.Source != null ? ", " + ExpressionToString(s.Source, program) : "")}",
            SpeedSeekStmt s =>
                $"SPEED SEEK {ExpressionToString(s.Alias, program)}, {ExpressionToString(s.KeyValue, program)}{(s.IndexName != null ? $" USING {ExpressionToString(s.IndexName, program)}" : "")}",
            SpeedGoStmt s => $"SPEED GO {ExpressionToString(s.Alias, program)}, {s.Location.Lexeme}",
            SpeedSkipStmt s =>
                $"SPEED SKIP {ExpressionToString(s.Alias, program)}, {ExpressionToString(s.Count ?? new NumberExpr(1, s.Token), program)}",
            SpeedSetOrderStmt s =>
                $"SPEED SET ORDER TO {ExpressionToString(s.IndexName, program)} ON {ExpressionToString(s.Alias, program)}",
            SpeedSetFilterStmt s =>
                $"SPEED SET FILTER ON {ExpressionToString(s.Alias, program)} TO {(s.Condition != null ? ExpressionToString(s.Condition, program) : "(none)")}",
            SpeedSetRelationStmt s =>
                $"SPEED SET RELATION ON {ExpressionToString(s.ParentAlias, program)}{(s.KeyExpression != null ? $" TO {ExpressionToString(s.KeyExpression, program)} INTO {ExpressionToString(s.ChildAlias!, program)}" : " OFF")}",
            SpeedSelectStmt s => s.QueryExpression != null
                ? $"SPEED SELECT (Dynamic Query): {ExpressionToString(s.QueryExpression, program)}"
                : $"SPEED SELECT {(s.IntoTarget != null ? $"INTO {TargetToString(s.IntoTarget, program)}" : "")} ... (static query)",
            SpeedCreateViewStmt s =>
                $"SPEED CREATE VIEW {s.OriginalViewName.Lexeme}#{s.ViewSymbolId} OF {s.OriginalTypeName.Lexeme}#{s.OfTypeSymbolId} AS ...",
            SpeedRefreshViewStmt s => $"SPEED REFRESH VIEW {s.ViewName.Lexeme}",
            SpeedDropViewStmt s => $"SPEED DROP VIEW {s.ViewName.Lexeme}",

            _ => stmt.ToString() ?? stmt.GetType().Name
        };

        // Usa FormatNodeHeader per stampare l'intestazione completa
        WriteOutput($"{indent}{FormatNodeHeader(stmt, stmt.GetType().Name, output)}", useConsole);
        return; // Rimosso 'return' ridondante e 'WriteOutput' precedente

        // Helper locale per formattare le clausole di scopo in modo coerente
        string FormatScope(DbfScopeClause? scope) => scope switch
        {
            DbfForClause f => $" FOR ({ExpressionToString(f.Condition, program)})",
            DbfWhileClause w => $" WHILE ({ExpressionToString(w.Condition, program)})",
            DbfNextClause n => $" NEXT {ExpressionToString(n.Count, program)}",
            DbfRecordClause r => $" RECORD {ExpressionToString(r.RecordNumber, program)}",
            DbfAllClause => " ALL",
            DbfRestClause => " REST",
            null => "",
            _ => " [UNKNOWN SCOPE]"
        };
    }

    /// <summary>
    /// Gestisce il dump di un'istruzione `IF...THEN...ELSEIF...ELSE...END IF` complessa.
    /// CORRETTO: Stampa le informazioni di scope per ogni clausola (IfClause, ElseClause).
    /// </summary>
    /// <param name="ifStmt">L'istruzione IF da stampare.</param>
    /// <param name="indent">La stringa di indentazione.</param>
    /// <param name="useConsole">Specifica il target di output.</param>
    /// <param name="isRecursiveCall">Flag interno per evitare doppie intestazioni (non utilizzato qui, ma mantenuto per compatibilità).</param>
    private static void DumpIfStatement(BasicProgram program, IfStmt ifStmt, string indent, bool useConsole,
        bool isRecursiveCall = false)
    {
        if (!isRecursiveCall) // Stampa l'intestazione del blocco IF principale
            WriteOutput($"{indent}{FormatNodeHeader(ifStmt, "IfStmt", "")}", useConsole);

        var nextIndent = indent + "  ";

        foreach (var (clause, index) in ifStmt.IfClauses.Select((c, i) => (c, i)))
        {
            // Stampa l'intestazione del nodo IfClause con il suo ScopeId
            var clauseHeader = $"{(index == 0 ? "IF" : "ELSEIF")} ({ExpressionToString(clause.Condition, program)})";
            WriteOutput($"{nextIndent}{FormatNodeHeader(clause, "IfClause", clauseHeader)}", useConsole);

            WriteOutput($"{nextIndent}  THEN:", useConsole);
            if (clause.Statements.Any())
                foreach (var thenStmt in clause.Statements)
                    DumpStatement(thenStmt, nextIndent + "    ", program, useConsole);
            else WriteOutput(nextIndent + "    (vuoto)", useConsole);
        }

        if (ifStmt.ElseClause == null) return;

        // Stampa l'intestazione del nodo ElseClause con il suo ScopeId
        WriteOutput($"{nextIndent}{FormatNodeHeader(ifStmt.ElseClause, "ElseClause", "ELSE:")}", useConsole);

        if (ifStmt.ElseClause.Statements.Any())
            foreach (var elseStmt in ifStmt.ElseClause.Statements)
                DumpStatement(elseStmt, nextIndent + "  ", program, useConsole);
        else WriteOutput(nextIndent + "  (vuoto)", useConsole);
    }

    /// <summary>
    /// Gestisce il dump di un'istruzione `SELECT CASE...CASE...CASE ELSE...END SELECT`.
    /// Stampa l'espressione di selezione e itera su tutte le clausole `CASE` e `CASE ELSE`,
    /// mostrando le condizioni di test, i loro ScopeId, e i rispettivi corpi di istruzioni.
    /// CORRETTO: Stampa le informazioni di scope per ogni clausola (CaseClause, CaseElseClause).
    /// </summary>
    /// <param name="selectStmt">L'istanza di `SelectStmt` da analizzare.</param>
    /// <param name="indent">La stringa di indentazione di base per il blocco SELECT.</param>
    /// <param name="useConsole">Specifica il target di output.</param>
    /// <param name="isRecursiveCall">Flag interno per evitare doppie intestazioni (non utilizzato qui, ma mantenuto per compatibilità).</param>
    private static void DumpSelectStatement(BasicProgram program, SelectStmt selectStmt, string indent, bool useConsole,
        bool isRecursiveCall = false)
    {
        if (!isRecursiveCall)
            WriteOutput(
                $"{indent}{FormatNodeHeader(selectStmt, "SelectStmt", $"({ExpressionToString(selectStmt.Expression, program)})")}",
                useConsole);

        var nextIndent = indent + "  ";

        foreach (var caseClause in selectStmt.CaseClauses)
        {
            // Genera una descrizione testuale dettagliata per ogni test della clausola
            var testsDescription = caseClause.Tests.Select(test => CaseTestToString(test, program));
            var clauseHeader = $"CASE {string.Join(", ", testsDescription)}:";

            WriteOutput($"{nextIndent}{FormatNodeHeader(caseClause, "CaseClause", clauseHeader)}", useConsole);

            foreach (var caseStmt in caseClause.Statements)
                DumpStatement(caseStmt, nextIndent + "  ", program, useConsole);
        }

        if (selectStmt.ElseClause == null) return;

        WriteOutput($"{nextIndent}{FormatNodeHeader(selectStmt.ElseClause, "CaseElseClause", "CASE ELSE:")}",
            useConsole);

        foreach (var elseStmt in selectStmt.ElseClause.Statements)
            DumpStatement(elseStmt, nextIndent + "  ", program, useConsole);
    }

    // Metodo helper per convertire un CaseTest in stringa (necessario per DumpSelectStatement)
    private static string CaseTestToString(CaseTest test, BasicProgram program)
    {
        return test switch
        {
            CaseValue cv => ExpressionToString(cv.Value, program),
            CaseRange cr => $"{ExpressionToString(cr.Start, program)} TO {ExpressionToString(cr.End, program)}",
            CaseComparison cc => $"IS {cc.Operator.Lexeme} {ExpressionToString(cc.Value, program)}",
            CaseTypePattern ctp =>
                $"IS {ctp.TargetType}{(ctp.VariableName != null ? " " + ctp.VariableName.Lexeme : "")}" +
                $"{(ctp.WhenCondition != null ? " WHEN " + ExpressionToString(ctp.WhenCondition, program) : "")}",
            CaseValuePattern cvp =>
                $"{ExpressionToString(cvp.PatternValue, program)}{(cvp.VariableName != null ? " AS " + cvp.VariableName.Lexeme : "")}" +
                $"{(cvp.WhenCondition != null ? " WHEN " + ExpressionToString(cvp.WhenCondition, program) : "")}",
            _ => test.ToString() ?? ""
        };
    }

    /// <summary>
    /// Gestisce il dump di un'istruzione `TRY...CATCH...FINALLY...END TRY` per la gestione delle eccezioni.
    /// Stampa in modo ricorsivo i corpi dei blocchi `TRY`, di tutte le clausole `CATCH` (con le loro condizioni e ScopeId)
    /// e del blocco `FINALLY` opzionale (con il suo ScopeId).
    /// CORRETTO: Stampa le informazioni di scope per ogni clausola (CatchClause, FinallyClause).
    /// </summary>
    /// <param name="tryStmt">L'istanza di `TryCatchStmt` da analizzare.</param>
    /// <param name="indent">La stringa di indentazione di base per il blocco TRY.</param>
    /// <param name="useConsole">Specifica il target di output.</param>
    /// <param name="isRecursiveCall">Flag interno per evitare doppie intestazioni (non utilizzato qui, ma mantenuto per compatibilità).</param>
    private static void DumpTryCatchStatement(BasicProgram program, TryCatchStmt tryStmt, string indent,
        bool useConsole,
        bool isRecursiveCall = false)
    {
        if (!isRecursiveCall)
            WriteOutput($"{indent}{FormatNodeHeader(tryStmt, "TryCatchStmt", "")}", useConsole);

        var nextIndent = indent + "  ";

        WriteOutput($"{nextIndent}TRY:", useConsole);
        foreach (var tryBodyStmt in tryStmt.TryBody) DumpStatement(tryBodyStmt, nextIndent + "  ", program, useConsole);

        foreach (var catchClause in tryStmt.CatchClauses)
        {
            var catchVar = catchClause.Variable != null
                ? $" {catchClause.Variable.NameSuffixed.Lexeme}#{catchClause.Variable.SymbolId}"
                : "";
            var whenCond = catchClause.WhenCondition != null
                ? $" WHEN {ExpressionToString(catchClause.WhenCondition, program)}"
                : "";

            // Stampa l'intestazione del nodo CatchClause con il suo ScopeId
            var clauseHeader = $"CATCH{catchVar}{whenCond}:";
            WriteOutput($"{nextIndent}{FormatNodeHeader(catchClause, "CatchClause", clauseHeader)}", useConsole);

            foreach (var catchStmt in catchClause.Body)
                DumpStatement(catchStmt, nextIndent + "  ", program, useConsole);
        }

        if (tryStmt.Finally == null) return;

        // Stampa l'intestazione del nodo FinallyClause con il suo ScopeId
        WriteOutput($"{nextIndent}{FormatNodeHeader(tryStmt.Finally, "FinallyClause", "FINALLY:")}", useConsole);

        foreach (var finallyStmt in tryStmt.Finally.Body)
            DumpStatement(finallyStmt, nextIndent + "  ", program, useConsole);
    }

    // ====================================================================================================================
    // --- DISPATCHER HELPERS E GESTORI DI CATEGORIE ---
    // ====================================================================================================================

    /// <summary>
    /// Controlla se un'istruzione appartiene alla categoria dei comandi per la manipolazione di array (es. `AADD`, `ASORT`).
    /// </summary>
    /// <param name="stmt">L'istruzione da esaminare.</param>
    /// <returns><c>true</c> se l'istruzione è un comando di array; altrimenti, <c>false</c>.</returns>
    private static bool IsArrayStmt(Stmt stmt) =>
        stmt is AAddStmt or AInsStmt or ADelStmt or ASortStmt or AEvalStmt or AFillStmt or ACopyStmt;

    /// <summary>
    /// Gestisce la formattazione e la stampa per tutti i comandi di manipolazione degli array.
    /// </summary>
    /// <param name="stmt">L'istruzione di array da stampare.</param>
    /// <param name="indent">La stringa di indentazione per la formattazione.</param>
    /// <param name="useConsole">Specifica il target di output.</param>
    private static void DumpArrayStatement(BasicProgram program, Stmt stmt, string indent, bool useConsole)
    {
        var (typeName, content) = stmt switch
        {
            AAddStmt s => ("AAddStmt",
                $"AADD({TargetToString(s.Array, program)}, {ExpressionToString(s.Value, program)})"),
            AInsStmt s => ("AInsStmt",
                $"AINS({TargetToString(s.Array, program)}, {ExpressionToString(s.PositionExpr, program)})"),
            ADelStmt s => ("ADelStmt",
                $"ADEL({TargetToString(s.Array, program)}, {ExpressionToString(s.PositionExpr, program)})"),
            ASortStmt s => ("ASortStmt",
                $"ASORT({TargetToString(s.Array, program)}{(s.CompareFunction is not null
                    ? ", " + ExpressionToString(s.CompareFunction, program) : "")}{(s.Order is not null
                    ? ", " + ExpressionToString(s.Order, program) : "")})"),
            AEvalStmt s => ("AEvalStmt",
                $"AEVAL({TargetToString(s.Array, program)}, {ExpressionToString(s.ActionFunction, program)})"),
            AFillStmt s => ("AFillStmt",
                $"AFILL({TargetToString(s.Array, program)}, {ExpressionToString(s.Value, program)}{(s.Start is not null
                    ? ", " + ExpressionToString(s.Start, program) : "")}{(s.Count is not null
                    ? ", " + ExpressionToString(s.Count, program) : "")})"),
            ACopyStmt s => ("ACopyStmt",
                $"ACOPY({TargetToString(s.Source, program)}, {TargetToString(s.Destination, program)}{(s.SourceStart is not null
                    ? ", " + ExpressionToString(s.SourceStart, program) : "")}{(s.Count is not null
                    ? ", " + ExpressionToString(s.Count, program) : "")}{(s.DestStart is not null
                    ? ", " + ExpressionToString(s.DestStart, program) : "")})"),
            _ => (stmt.GetType().Name, stmt.ToString() ?? "") // Fallback
        };

        WriteOutput($"{indent}{FormatNodeHeader(stmt, typeName, content)}", useConsole);
    }

    /// <summary>
    /// Controlla se un'istruzione appartiene alla categoria dei comandi di interazione con la console (es. `PRINT`, `LOCATE`).
    /// </summary>
    /// <param name="stmt">L'istruzione da esaminare.</param>
    /// <returns><c>true</c> se l'istruzione è un comando di console; altrimenti, <c>false</c>.</returns>
    private static bool IsConsoleStmt(Stmt stmt) => stmt is ClsStmt or ColorStmt or LocateStmt or PrintStmt or InputStmt
        or LineInputStmt or GetKeyboardStmt or InkeyStmt or WaitKeyStmt or SetAttributeStmt or CursorStateStmt
        or CursorShapeStmt or CCLSStmt or CLocateStmt or CPrintStmt or ConsoleSendKeyStmt
        or CSaveScreenStmt or CRestoreScreenStmt;

    /// <summary>
    /// Gestisce la formattazione e la stampa per tutti i comandi relativi alla console di testo.
    /// CORRETTO: Utilizza FormatNodeHeader per stampare ScopeId e Namespace.
    /// AGGIORNATO: Supporta ConsoleSendKeyStmt e i campi TimeoutExpr.
    /// </summary>
    /// <param name="stmt">L'istruzione di console da stampare.</param>
    /// <param name="indent">La stringa di indentazione per la formattazione.</param>
    /// <param name="useConsole">Specifica il target di output.</param>
    private static void DumpConsoleStatement(BasicProgram program, Stmt stmt, string indent, bool useConsole)
    {
        var (typeName, content) = stmt switch
        {
            ClsStmt s => ("ClsStmt", s.Color != null ? ExpressionToString(s.Color, program) : ""),
            ColorStmt s => ("ColorStmt", FormatColorArgs(s)),
            LocateStmt s => ("LocateStmt",
                $"{ExpressionToString(s.LineExpr, program)}, {ExpressionToString(s.ColumnExpr, program)}"),
            PrintStmt s => ("PrintStmt", string.Join("", s.Arguments)),
            InputStmt s => ("InputStmt",
                $"{(s.Prompt != null ? ExpressionToString(s.Prompt, program) + "; " : "")}{string.Join(", ", s.Variables)}{(s.TimeoutExpr != null ? ", Timeout=" + ExpressionToString(s.TimeoutExpr, program) : "")}{(s.MaskExpr != null ? ", MASK=" + ExpressionToString(s.MaskExpr, program) : "")}"),
            LineInputStmt s => ("LineInputStmt",
                $"{(s.Prompt != null ? ExpressionToString(s.Prompt, program) + "; " : "")}{TargetToString(s.Variable, program)}{(s.TimeoutExpr != null ? ", Timeout=" + ExpressionToString(s.TimeoutExpr, program) : "")}{(s.MaskExpr != null ? ", MASK=" + ExpressionToString(s.MaskExpr, program) : "")}"),
            GetKeyboardStmt s => ("GetKeyboardStmt", TargetToString(s.Variable, program)),
            InkeyStmt s => ("InkeyStmt", FormatInkeyArgs(s)),
            WaitKeyStmt => ("WaitKeyStmt", ""),
            SetAttributeStmt s => ("SetAttributeStmt", $"{s.Attribute.Lexeme} {s.State.Lexeme}"),
            CursorStateStmt s => ("CursorStateStmt", $"CURSOR {s.State.Lexeme}"),
            CursorShapeStmt s => ("CursorShapeStmt", $"CURSOR SHAPE {ExpressionToString(s.Shape, program)}"),
            ConsoleSendKeyStmt s => ("ConsoleSendKeyStmt", ExpressionToString(s.KeysExpression, program)),
            CCLSStmt s => ("CCLSStmt", s.Color != null ? ExpressionToString(s.Color, program) : ""),
            CLocateStmt s => ("CLocateStmt",
                $"{ExpressionToString(s.LineExpr, program)}, {ExpressionToString(s.ColumnExpr, program)}"),
            CPrintStmt s => ("CPrintStmt", string.Join("", s.Arguments)),
            CSaveScreenStmt s => ("CSaveScreenStmt", ExpressionToString(s.Name, program)),
            CRestoreScreenStmt s => ("CRestoreScreenStmt", ExpressionToString(s.Name, program)),
            _ => (stmt.GetType().Name, stmt.ToString() ?? "")
        };

        WriteOutput($"{indent}{FormatNodeHeader(stmt, typeName, content)}", useConsole);
        return;

        // --- Helper Locali per la formattazione ---
        string FormatColorArgs(ColorStmt s)
        {
            var parts = new List<string>();
            if (s.Foreground != null)
            {
                parts.Add($"Foreground={ExpressionToString(s.Foreground, program)}");
            }

            if (s.Background != null)
            {
                parts.Add($"Background={ExpressionToString(s.Background, program)}");
            }

            return parts.Count > 0 ? string.Join(", ", parts) : "(no arguments)";
        }

        string FormatInkeyArgs(InkeyStmt s)
        {
            var parts = new List<string> { TargetToString(s.Variable, program) };
            if (s.TimeoutExpr != null) parts.Add(ExpressionToString(s.TimeoutExpr, program));
            return string.Join(", ", parts);
        }
    }

    /// <summary>
    /// Controlla se un'istruzione appartiene alla categoria dei comandi grafici e audio (es. `PSET`, `LINE`, `SOUND`).
    /// </summary>
    /// <param name="stmt">L'istruzione da esaminare.</param>
    /// <returns><c>true</c> se l'istruzione è un comando grafico o audio; altrimenti, <c>false</c>.</returns>
    private static bool IsGraphicsStmt(Stmt stmt) => stmt is ScreenStmt or TextModeStmt or PSetStmt or PresetStmt or LineStmt
        or CircleStmt or PaintStmt or DrawStmt or PCopyStmt or FontStmt or PaletteStmt or ScreenSetStmt or ImageGetStmt
        or ImagePutStmt or ImagePutsStmt or SaveScreenStmt or RestoreScreenStmt or PointStmt or BSaveStmt or BLoadStmt
        or ClearRectStmt or LoadTextureStmt or DrawTextureStmt or SpriteCommandStmt or SyncStmt or VectorCommandStmt
        or MatrixCommandStmt or SoundStmt or PlayStmt or AudioControlStmt or SetFilterStmt;

    /// <summary>
    /// Gestisce la formattazione e la stampa per tutti i comandi grafici e audio.
    /// CORRETTO: Utilizza FormatNodeHeader per includere lo ScopeId e i metadati.
    /// </summary>
    private static void DumpGraphicsStatement(BasicProgram program, Stmt stmt, string indent, bool useConsole)
    {
        string content;
        var typeName = stmt.GetType().Name;

        switch (stmt)
        {
            case ScreenStmt s:
                var sParts = new List<string> { ExpressionToString(s.Mode, program) };
                if (s.ActivePage != null) sParts.Add(ExpressionToString(s.ActivePage, program));
                if (s.VisiblePage != null) sParts.Add(ExpressionToString(s.VisiblePage, program));
                if (s.CloseRepl != null) sParts.Add(ExpressionToString(s.CloseRepl, program));
                if (s.WindowModeExpr != null) sParts.Add(ExpressionToString(s.WindowModeExpr, program));
                content = string.Join(", ", sParts);
                break;
            case TextModeStmt tms:
                content = ExpressionToString(tms.Mode, program);
                break;
            case PSetStmt s:
                var psParts = new List<string>
                    { $"({ExpressionToString(s.X, program)}, {ExpressionToString(s.Y, program)})" };
                if (s.Color != null) psParts.Add(ExpressionToString(s.Color, program));
                content = string.Join(", ", psParts);
                break;
            case PresetStmt s:
                content = $"({ExpressionToString(s.X, program)}, {ExpressionToString(s.Y, program)})";
                break;
            case LineStmt s:
                var start =
                    $"({(s.X1 != null ? ExpressionToString(s.X1, program) : "")}, {(s.Y1 != null ? ExpressionToString(s.Y1, program) : "")})";
                var end = $"({ExpressionToString(s.X2, program)}, {ExpressionToString(s.Y2, program)})";
                var lParts = new List<string> { $"{start}-{end}" };
                if (s.Color != null) lParts.Add(ExpressionToString(s.Color, program));
                if (s.Style != null) lParts.Add(ExpressionToString(s.Style, program));
                content = string.Join(", ", lParts);
                break;
            case CircleStmt s:
                var cParts = new List<string>
                {
                    $"({ExpressionToString(s.X, program)}, {ExpressionToString(s.Y, program)}), {ExpressionToString(s.Radius, program)}"
                };
                if (s.Color != null) cParts.Add(ExpressionToString(s.Color, program));
                if (s.Start != null) cParts.Add(ExpressionToString(s.Start, program));
                if (s.End != null) cParts.Add(ExpressionToString(s.End, program));
                if (s.Aspect != null) cParts.Add(ExpressionToString(s.Aspect, program));
                content = string.Join(", ", cParts);
                break;
            case PaintStmt s:
                var pParts = new List<string>
                    { $"({ExpressionToString(s.X, program)}, {ExpressionToString(s.Y, program)})" };
                if (s.FillColor != null) pParts.Add(ExpressionToString(s.FillColor, program));
                if (s.BorderColor != null) pParts.Add(ExpressionToString(s.BorderColor, program));
                content = string.Join(", ", pParts);
                break;
            case DrawStmt s:
                content = ExpressionToString(s.CommandString, program);
                break;
            case PCopyStmt s:
                content =
                    $"{ExpressionToString(s.SourcePage, program)}, {ExpressionToString(s.DestinationPage, program)}";
                break;
            case FontStmt s:
                content = $"{ExpressionToString(s.FontPath, program)}, {ExpressionToString(s.Size, program)}";
                break;
            case PaletteStmt s:
                content =
                    $"{ExpressionToString(s.Index, program)}, {ExpressionToString(s.R, program)}, {ExpressionToString(s.G, program)}, {ExpressionToString(s.B, program)}";
                break;
            case ScreenSetStmt s:
                var ssParts = new List<string>();
                if (s.VisiblePage != null) ssParts.Add(ExpressionToString(s.VisiblePage, program));
                if (s.ActivePage != null) ssParts.Add(ExpressionToString(s.ActivePage, program));
                content = string.Join(", ", ssParts);
                break;
            case ImageGetStmt s:
                content =
                    $"({ExpressionToString(s.X1, program)},{ExpressionToString(s.Y1, program)})-({ExpressionToString(s.X2, program)},{ExpressionToString(s.Y2, program)}), {TargetToString(s.Array, program)}";
                break;
            case ImagePutStmt s:
                var ipParts = new List<string>
                {
                    $"({ExpressionToString(s.X, program)},{ExpressionToString(s.Y, program)})",
                    TargetToString(s.Array, program)
                };
                if (s.Mode != null) ipParts.Add(ExpressionToString(s.Mode, program));
                content = string.Join(", ", ipParts);
                break;
            case ImagePutsStmt s:
                var ipsParts = new List<string>
                    { TargetToString(s.SpriteArray, program), TargetToString(s.PositionsArray, program) };
                if (s.Mode != null) ipsParts.Add(ExpressionToString(s.Mode, program));
                content = string.Join(", ", ipsParts);
                break;
            case SaveScreenStmt s:
                content = ExpressionToString(s.Name, program);
                break;
            case RestoreScreenStmt s:
                content = ExpressionToString(s.Name, program);
                break;
            case PointStmt s:
                var ptParts = new List<string>
                    { $"({ExpressionToString(s.X, program)}, {ExpressionToString(s.Y, program)})" };
                if (s.Color != null) ptParts.Add(ExpressionToString(s.Color, program));
                content = string.Join(", ", ptParts);
                break;
            case BSaveStmt s:
                content =
                    $"({ExpressionToString(s.X1, program)},{ExpressionToString(s.Y1, program)})-({ExpressionToString(s.X2, program)},{ExpressionToString(s.Y2, program)}), {TargetToString(s.Buffer, program)}";
                break;
            case BLoadStmt s:
                content =
                    $"({ExpressionToString(s.X, program)},{ExpressionToString(s.Y, program)}), {ExpressionToString(s.Buffer, program)}";
                break;
            case ClearRectStmt s:
                content =
                    $"({ExpressionToString(s.X1, program)},{ExpressionToString(s.Y1, program)})-({ExpressionToString(s.X2, program)},{ExpressionToString(s.Y2, program)})";
                break;
            case LoadTextureStmt s:
                content = $"{ExpressionToString(s.FileName, program)} IN {TargetToString(s.Destination, program)}";
                break;
            case DrawTextureStmt s:
                content =
                    $"{ExpressionToString(s.TextureId, program)}, ({ExpressionToString(s.X, program)},{ExpressionToString(s.Y, program)})";
                break;
            case SyncStmt:
                content = "";
                break;
            case SpriteCommandStmt s:
                content =
                    $"{s.Command.Lexeme} {string.Join(", ", s.Arguments.Select(a => ExpressionToString(a, program)))}";
                break;
            case VectorCommandStmt s:
                content =
                    $"{s.Command.Lexeme}#{s.CommandSymbolId} {string.Join(", ", s.Arguments.Select(a => ExpressionToString(a, program)))}";
                break;
            case MatrixCommandStmt s:
                content =
                    $"{s.Command.Lexeme}#{s.CommandSymbolId} {string.Join(", ", s.Arguments.Select(a => ExpressionToString(a, program)))}";
                break;
            case SoundStmt s:
                var sndParts = new List<string>
                    { ExpressionToString(s.Frequency, program), ExpressionToString(s.Duration, program) };
                if (s.Volume != null) sndParts.Add(ExpressionToString(s.Volume, program));
                content = string.Join(", ", sndParts);
                break;
            case PlayStmt s:
                content = s.Action switch
                {
                    StandardPlayAction a => $"Standard {ExpressionToString(a.MusicString, program)}",
                    LoopPlayAction a =>
                        $"Loop {ExpressionToString(a.MusicString, program)} ON {ExpressionToString(a.Channel, program)}{(a.Times != null ? " TIMES " + ExpressionToString(a.Times, program) : "")}",
                    NamedPlayAction a =>
                        $"Named {ExpressionToString(a.MusicString, program)} ON {ExpressionToString(a.Channel, program)}",
                    ConcurrentPlayAction a =>
                        $"Concurrent {string.Join(", ", a.MusicStrings.Select(v => ExpressionToString(v, program)))}",
                    QueuePlayAction a => $"Queue {ExpressionToString(a.MusicString, program)}",
                    _ => "Unknown Action"
                };
                break;
            case AudioControlStmt s:
                var acArg = s.Argument switch
                {
                    null when s.Action.Kind == TokenKind.Stop => " ALL", null => "",
                    _ => " " + ExpressionToString(s.Argument, program)
                };
                content = $"{s.Command.Lexeme} {s.Action.Lexeme}{acArg}";
                break;
            default:
                content = stmt.ToString() ?? "";
                break;
        }

        WriteOutput($"{indent}{FormatNodeHeader(stmt, typeName, content)}", useConsole);
    }

    /// <summary>
    /// Controlla se un'istruzione appartiene alla categoria dei comandi di plotting (`CHART`).
    /// </summary>
    /// <param name="stmt">L'istruzione da esaminare.</param>
    /// <returns><c>true</c> se l'istruzione è un comando `CHART`; altrimenti, <c>false</c>.</returns>
    private static bool IsPlottingStmt(Stmt stmt) =>
        stmt is ChartPlotStmt or ChartFinancialStmt or ChartClearStmt or ChartCloseStmt or ChartRenderStmt;

    /// <summary>
    /// Gestisce la formattazione e la stampa per tutti i comandi del sottosistema di plotting (`CHART`).
    /// CORRETTO: Utilizza FormatNodeHeader e formattazione esplicita.
    /// </summary>
    /// <param name="stmt">L'istruzione `CHART` da stampare.</param>
    /// <param name="indent">La stringa di indentazione per la formattazione.</param>
    /// <param name="useConsole">Specifica il target di output.</param>
    private static void DumpPlottingStatement(BasicProgram program, Stmt stmt, string indent, bool useConsole)
    {
        var (typeName, content) = stmt switch
        {
            ChartPlotStmt s => ("ChartPlotStmt",
                $"PLOT {string.Join(", ", s.Args.Select(v => ExpressionToString(v, program)))}"),
            ChartFinancialStmt s => ("ChartFinancialStmt",
                $"FINANCIAL {string.Join(", ", s.Args.Select(v => ExpressionToString(v, program)))}"),
            ChartClearStmt s => ("ChartClearStmt", $"CLEAR {ExpressionToString(s.Handle, program)}"),
            ChartCloseStmt s => ("ChartCloseStmt", $"CLOSE {ExpressionToString(s.Handle, program)}"),
            ChartRenderStmt s => ("ChartRenderStmt",
                $"{(s.IsSave ? "SAVE" : "SHOW")} {string.Join(", ", s.Args.Select(v => ExpressionToString(v, program)))}"),
            _ => (stmt.GetType().Name, stmt.ToString() ?? "") // Fallback
        };
        WriteOutput($"{indent}{FormatNodeHeader(stmt, typeName, content)}", useConsole);
    }

    /// <summary>
    /// Controlla se un'istruzione appartiene alla categoria dei comandi di I/O su file (es. `OPEN`, `PRINT #`, `LOCK`).
    /// Questo metodo è stato esteso per includere i nuovi comandi di Memory Map e Watcher.
    /// </summary>
    /// <param name="stmt">L'istruzione da esaminare.</param>
    /// <returns><c>true</c> se l'istruzione è un comando di I/O su file; altrimenti, <c>false</c>.</returns>
    private static bool IsFileIOStmt(Stmt stmt) => stmt is OpenStmt or CloseStmt or PrintFileStmt or WriteFileStmt
        or InputFileStmt or LineInputFileStmt or GetFileStmt or PutFileStmt or SeekStmt or LockStmt or UnlockStmt
        or MemoryMapStmt or MemoryMapPokeStmt or MemoryMapWriteBytesStmt
        or IoWatchStmt or IoUnwatchStmt;

    /// <summary>
    /// Gestisce la formattazione e la stampa per tutti i comandi di I/O su file.
    /// Questo metodo è stato esteso per includere i nuovi comandi di Memory Map e Watcher.
    /// </summary>
    /// <param name="program">L'istanza di BasicProgram per la risoluzione dei simboli e delle espressioni.</param>
    /// <param name="stmt">L'istruzione di I/O su file da stampare.</param>
    /// <param name="indent">La stringa di indentazione per la formattazione.</param>
    /// <param name="useConsole">Specifica il target di output.</param>
    private static void DumpFileIOStatement(BasicProgram program, Stmt stmt, string indent, bool useConsole)
    {
        var (typeName, content) = stmt switch
        {
            OpenStmt s => ("OpenStmt", s.ToString()), // Il ToString() di OpenStmt è già molto dettagliato
            CloseStmt s => ("CloseStmt",
                string.Join(", ", s.FileNumbers.Select(f => "#" + ExpressionToString(f, program)))),
            PrintFileStmt s => ("PrintFileStmt",
                $"#{ExpressionToString(s.FileNumber, program)}, {string.Join("", s.Arguments)}"),
            WriteFileStmt s => ("WriteFileStmt",
                $"#{ExpressionToString(s.FileNumber, program)}, {string.Join(", ", s.Arguments.Select(v => ExpressionToString(v, program)))}"),
            InputFileStmt s => ("InputFileStmt",
                $"#{ExpressionToString(s.FileNumber, program)}, {string.Join(", ", s.Variables.Select(t => TargetToString(t, program)))}"),
            LineInputFileStmt s =>
                ("LineInputFileStmt",
                    $"#{ExpressionToString(s.FileNumber, program)}, {TargetToString(s.Variable, program)}"),
            GetFileStmt s =>
                ("GetFileStmt", $"#{ExpressionToString(s.FileNumber, program)}, {(s.RecordNumber != null
                    ? ExpressionToString(s.RecordNumber, program) + ", " : "")}{TargetToString(s.Destination, program)}"),
            PutFileStmt s =>
                ("PutFileStmt", $"#{ExpressionToString(s.FileNumber, program)}, {(s.RecordNumber != null
                    ? ExpressionToString(s.RecordNumber, program) + ", " : "")}{ExpressionToString(s.Source, program)}"),
            SeekStmt s => ("SeekStmt",
                $"#{ExpressionToString(s.FileNumber, program)}, {ExpressionToString(s.PositionExpr, program)}"),
            LockStmt s => ("LockStmt",
                $"#{ExpressionToString(s.FileNumber, program)}{(s.RecordNumber != null ? ", " + ExpressionToString(s.RecordNumber, program) : "")}"),
            UnlockStmt s => ("UnlockStmt",
                $"#{ExpressionToString(s.FileNumber, program)}{(s.RecordNumber != null ? ", " + ExpressionToString(s.RecordNumber, program) : "")}"),
            MemoryMapStmt s => ("MemoryMapStmt",
                $"{ExpressionToString(s.FilePath, program)} AS {TargetToString(s.Handle, program)}"),
            MemoryMapPokeStmt s => ("MemoryMapPokeStmt",
                $"{ExpressionToString(s.Handle, program)}, {ExpressionToString(s.Offset, program)}, {ExpressionToString(s.Value, program)}"),
            MemoryMapWriteBytesStmt s => ("MemoryMapWriteBytesStmt",
                $"{ExpressionToString(s.Handle, program)}, {ExpressionToString(s.Offset, program)}, {ExpressionToString(s.SourceArray, program)}{(s.Count != null ? ", " + ExpressionToString(s.Count, program) : "")}"),
            IoWatchStmt s => ("IoWatchStmt",
                $"{ExpressionToString(s.Path, program)}, {ExpressionToString(s.EventsMask, program)}, Callback={s.Callback.Lexeme}"),
            IoUnwatchStmt s => ("IoUnwatchStmt",
                $"{ExpressionToString(s.Path, program)}"),

            _ => (stmt.GetType().Name, stmt.ToString() ?? "")
        };
        WriteOutput($"{indent}{FormatNodeHeader(stmt, typeName, content)}", useConsole);
    }

    /// <summary>
    /// Controlla se un'istruzione appartiene alla categoria dei comandi di gestione del file system (es. `KILL`, `MKDIR`).
    /// </summary>
    /// <param name="stmt">L'istruzione da esaminare.</param>
    /// <returns><c>true</c> se l'istruzione è un comando di file system; altrimenti, <c>false</c>.</returns>
    private static bool IsFileSystemStmt(Stmt stmt) => stmt is KillStmt or RenameStmt or MkDirStmt or RmDirStmt
        or ChDirStmt or CopyStmt or MoveStmt or XCopyStmt;

    /// <summary>
    /// Gestisce la formattazione e la stampa per tutti i comandi del file system.
    /// </summary>
    /// <param name="stmt">L'istruzione di file system da stampare.</param>
    /// <param name="indent">La stringa di indentazione per la formattazione.</param>
    /// <param name="useConsole">Specifica il target di output.</param>
    private static void DumpFileSystemStatement(BasicProgram program, Stmt stmt, string indent, bool useConsole)
    {
        var (typeName, content) = stmt switch
        {
            KillStmt s => ("KillStmt", ExpressionToString(s.FileName, program)),
            RenameStmt s => ("RenameStmt",
                $"{ExpressionToString(s.OldFileName, program)} AS {ExpressionToString(s.NewFileName, program)}"),
            MkDirStmt s => ("MkDirStmt", ExpressionToString(s.DirectoryPath, program)),
            RmDirStmt s => ("RmDirStmt",
                $"{ExpressionToString(s.DirectoryPath, program)}{(s.RecursiveExpr != null ? ", " + ExpressionToString(s.RecursiveExpr, program) : "")}"),
            ChDirStmt s => ("ChDirStmt", ExpressionToString(s.DirectoryPath, program)),
            CopyStmt s => ("CopyStmt",
                $"{ExpressionToString(s.SourceFile, program)} TO {ExpressionToString(s.DestinationFile, program)}"),
            MoveStmt s => ("MoveStmt",
                $"{ExpressionToString(s.SourceFile, program)} TO {ExpressionToString(s.DestinationFile, program)}"),
            XCopyStmt s => ("XCopyStmt",
                $"{ExpressionToString(s.SourceDirectory, program)} TO {ExpressionToString(s.DestinationDirectory, program)}"),
            _ => (stmt.GetType().Name, stmt.ToString() ?? "")
        };
        WriteOutput($"{indent}{FormatNodeHeader(stmt, typeName, content)}", useConsole);
    }

    /// <summary>
    /// Gestisce il dump di un'istruzione `GO SELECT...CASE...DEFAULT...END SELECT` per la concorrenza.
    /// Mostra ogni clausola `CASE` con la sua operazione di canale e il corpo di istruzioni associato.
    /// CORRETTO: Utilizza FormatNodeHeader per stampare lo ScopeId del blocco principale.
    /// </summary>
    /// <param name="selectStmt">L'istanza di `GoSelectStmt` da analizzare.</param>
    /// <param name="indent">La stringa di indentazione di base per il blocco.</param>
    /// <param name="useConsole">Specifica il target di output.</param>
    private static void DumpGoSelectStatement(BasicProgram program, GoSelectStmt selectStmt, string indent,
        bool useConsole)
    {
        // Usa FormatNodeHeader per l'intestazione del blocco.
        WriteOutput($"{indent}{FormatNodeHeader(selectStmt, "GoSelectStmt", "")}", useConsole);

        var nextIndent = indent + "  ";

        foreach (var caseClause in selectStmt.Cases)
        {
            // GoSelectCase non è un AstNode, quindi non ha uno ScopeId da stampare.
            // Stampiamo l'istruzione di comunicazione che contiene.
            WriteOutput($"{nextIndent}CASE:", useConsole);
            DumpStatement(caseClause.CommunicationStmt, nextIndent + "  ", program, useConsole);

            // Stampa il corpo della clausola
            foreach (var caseStmt in caseClause.Body)
                DumpStatement(caseStmt, nextIndent + "    ", program, useConsole);
        }

        // Verifica se DefaultCase ha un valore, e in caso affermativo, se l'array non è vuoto.
        if (!selectStmt.DefaultCase.HasValue || !selectStmt.DefaultCase.Value.Any()) return;
        WriteOutput($"{nextIndent}DEFAULT:", useConsole);
        foreach (var defaultStmt in selectStmt.DefaultCase.Value)
            DumpStatement(defaultStmt, nextIndent + "  ", program, useConsole);
    }

    /// <summary>
    /// Controlla se un'istruzione appartiene alla categoria dei comandi del sottosistema Ledger.
    /// </summary>
    /// <param name="stmt">L'istruzione da esaminare.</param>
    /// <returns><c>true</c> se l'istruzione è un comando Ledger; altrimenti, <c>false</c>.</returns>
    private static bool IsLedgerStmt(Stmt stmt) => stmt is LedgerOpenStmt or LedgerAddStmt or LedgerRegisterStmt
        or LedgerBalanceStmt or LedgerEditStmt or LedgerDeleteStmt or LedgerSaveStmt or LedgerCloseStmt
        or LedgerPackStmt or LedgerAccountsStmt or LedgerValidateStmt;

    /// <summary>
    /// Gestisce la formattazione e la stampa per tutti i comandi del sottosistema Ledger.
    /// CORRETTO: Utilizza FormatNodeHeader per stampare ScopeId e Namespace.
    /// </summary>
    /// <param name="stmt">L'istruzione Ledger da stampare.</param>
    /// <param name="indent">La stringa di indentazione per la formattazione.</param>
    /// <param name="useConsole">Specifica il target di output.</param>
    private static void DumpLedgerStatement(BasicProgram program, Stmt stmt, string indent, bool useConsole)
    {
        var (typeName, content) = stmt switch
        {
            LedgerOpenStmt s => ("LedgerOpenStmt",
                $"{ExpressionToString(s.FilePath, program)} AS {TargetToString(s.Handle, program)}"),
            LedgerAddStmt s => ("LedgerAddStmt",
                $"Handle={ExpressionToString(s.Handle, program)}, Date={ExpressionToString(s.Date, program)}, " +
                $"Desc={ExpressionToString(s.Description, program)}, ID->{TargetToString(s.NewIdVariable, program)}"),
            LedgerRegisterStmt s => ("LedgerRegisterStmt",
                $"Handle={ExpressionToString(s.Handle, program)}, Into={TargetToString(s.DestinationArray, program)}, " +
                $"Query={(s.Query != null ? ExpressionToString(s.Query, program) : "N/A")}"),
            LedgerBalanceStmt s => ("LedgerBalanceStmt",
                $"Handle={ExpressionToString(s.Handle, program)}, Into={TargetToString(s.DestinationArray, program)}, " +
                $"Query={(s.Query != null ? ExpressionToString(s.Query, program) : "N/A")}"),
            LedgerEditStmt s => ("LedgerEditStmt",
                $"Handle={ExpressionToString(s.Handle, program)}, ID={ExpressionToString(s.TransactionId, program)}"),
            LedgerDeleteStmt s => ("LedgerDeleteStmt",
                $"Handle={ExpressionToString(s.Handle, program)}, ID={ExpressionToString(s.TransactionId, program)}"),
            LedgerSaveStmt s => ("LedgerSaveStmt", $"Handle={ExpressionToString(s.Handle, program)}"),
            LedgerCloseStmt s => ("LedgerCloseStmt", $"Handle={ExpressionToString(s.Handle, program)}"),
            LedgerPackStmt s => ("LedgerPackStmt", $"Handle={ExpressionToString(s.Handle, program)}"),
            LedgerAccountsStmt s => ("LedgerAccountsStmt",
                $"Handle={ExpressionToString(s.Handle, program)}, Into={TargetToString(s.DestinationArray, program)}"),
            LedgerValidateStmt s => ("LedgerValidateStmt",
                $"Handle={ExpressionToString(s.Handle, program)}, Into={TargetToString(s.DestinationVariable, program)}"),
            LedgerGetStmt s => ("LedgerGetStmt",
                $"Handle={ExpressionToString(s.Handle, program)}, ID={(s.TransactionId is null ? "" : ExpressionToString(s.TransactionId, program))}, Into={TargetToString(s.DestinationArray, program)}"),
            _ => (stmt.GetType().Name, stmt.ToString() ?? "")
        };
        WriteOutput($"{indent}{FormatNodeHeader(stmt, typeName, content)}", useConsole);
    }

    /// <summary>
    /// Controlla se un'istruzione appartiene alla categoria dei comandi di concorrenza (es. `GO SUB`, `WAIT`, `SEND`).
    /// </summary>
    /// <param name="stmt">L'istruzione da esaminare.</param>
    /// <returns><c>true</c> se l'istruzione è un comando di concorrenza; altrimenti, <c>false</c>.</returns>
    private static bool IsConcurrencyStmt(Stmt stmt) => stmt is GoProcedureStmt or WaitStmt or DoneStmt or SendStmt
        or ReceiveStmt or SleepStmt or GoExitStmt;

    /// <summary>
    /// Gestisce la formattazione e la stampa per tutti i comandi relativi alla concorrenza.
    /// </summary>
    /// <param name="stmt">L'istruzione di concorrenza da stampare.</param>
    /// <param name="indent">La stringa di indentazione per la formattazione.</param>
    /// <param name="useConsole">Specifica il target di output.</param>
    private static void DumpConcurrencyStatement(BasicProgram program, Stmt stmt, string indent, bool useConsole)
    {
        var (typeName, content) = stmt switch
        {
            GoProcedureStmt s => ("GoProcedureStmt",
                $"GO {ExpressionToString(s.Call, program)}{(s.WaitGroup != null ? " IN " + ExpressionToString(s.WaitGroup, program) : "")}"),
            WaitStmt s => ("WaitStmt", $"WAIT {ExpressionToString(s.WaitGroup, program)}"),
            DoneStmt s => ("DoneStmt", $"DONE {ExpressionToString(s.WaitGroup, program)}"),
            SendStmt s => ("SendStmt",
                $"SEND {ExpressionToString(s.Value, program)} TO {ExpressionToString(s.Channel, program)}"),
            ReceiveStmt s => ("ReceiveStmt",
                s.OkFlag is null
                    ? $"RECEIVE {TargetToString(s.Destination, program)} FROM {ExpressionToString(s.Channel, program)}"
                    : $"RECEIVE {TargetToString(s.Destination, program)} FROM {ExpressionToString(s.Channel, program)} OK {TargetToString(s.OkFlag, program)}"),
            CloseChannelStmt s => ("CloseChannelStmt",
                $"CHANNEL CLOSE {ExpressionToString(s.Channel, program)}"),
            TimeoutStmt s => ("TimeoutStmt",
                $"TIMEOUT {ExpressionToString(s.Duration, program)}"),
            SleepStmt s => ("SleepStmt", $"SLEEP {ExpressionToString(s.Duration, program)}"),
            GoExitStmt => ("GoExitStmt", ""),
            _ => (stmt.GetType().Name, stmt.ToString() ?? "")
        };
        WriteOutput($"{indent}{FormatNodeHeader(stmt, typeName, content)}", useConsole);
    }

    /// <summary>
    /// Controlla se un'istruzione appartiene alla categoria dei comandi di networking (TCP, UDP, SSL, HTTP).
    /// </summary>
    /// <param name="stmt">L'istruzione da esaminare.</param>
    /// <returns><c>true</c> se l'istruzione è un comando di networking; altrimenti, <c>false</c>.</returns>
    private static bool IsNetworkStmt(Stmt stmt) => stmt is TcpOpenStmt or TcpConnectStmt or TcpAcceptStmt
        or TcpSendStmt or TcpRecvStmt or TcpCloseStmt or SslOpenStmt or SslConnectStmt or UdpOpenStmt or UdpSendStmt
        or UdpRecvStmt or UdpCloseStmt or ShutdownStmt or SetOptionStmt or HttpFetchStmt or HttpServerStmt
        or HttpRouteStmt or HttpUseStmt or HttpUseNextStmt or HttpConfigStmt;

    /// <summary>
    /// Gestisce la formattazione e la stampa per tutti i comandi di networking.
    /// CORRETTO: Utilizza FormatNodeHeader per stampare ScopeId e Namespace.
    /// </summary>
    /// <param name="stmt">L'istruzione di networking da stampare.</param>
    /// <param name="indent">La stringa di indentazione per la formattazione.</param>
    /// <param name="useConsole">Specifica il target di output.</param>
    private static void DumpNetworkStatement(BasicProgram program, Stmt stmt, string indent, bool useConsole)
    {
        var (typeName, content) = stmt switch
        {
            TcpOpenStmt s => ("TcpOpenStmt",
                $"Port={ExpressionToString(s.Port, program)}, Handle={TargetToString(s.Handle, program)}" +
                (s.Engine != null ? $", Engine={ExpressionToString(s.Engine, program)}" : "")),
            TcpConnectStmt s => ("TcpConnectStmt",
                $"Host={ExpressionToString(s.Host, program)}, Port={ExpressionToString(s.Port, program)}, Handle={TargetToString(s.Handle, program)}" +
                (s.Timeout != null ? $", Timeout={ExpressionToString(s.Timeout, program)}" : "") +
                (s.Engine != null ? $", Engine={ExpressionToString(s.Engine, program)}" : "")),
            TcpAcceptStmt s => ("TcpAcceptStmt",
                $"Server={ExpressionToString(s.ServerHandle, program)}, Client={TargetToString(s.ClientHandle, program)}" +
                (s.Timeout != null ? $", Timeout={ExpressionToString(s.Timeout, program)}" : "")),
            TcpSendStmt s => ("TcpSendStmt",
                $"Handle={ExpressionToString(s.ClientHandle, program)}, Data={ExpressionToString(s.Data, program)}"),
            TcpRecvStmt s => ("TcpRecvStmt",
                $"Handle={ExpressionToString(s.ClientHandle, program)}, Buffer={TargetToString(s.Buffer, program)}, BytesRead={TargetToString(s.BytesReceived, program)}" +
                (s.Timeout != null ? $", Timeout={ExpressionToString(s.Timeout, program)}" : "")),
            TcpCloseStmt s => ("TcpCloseStmt", $"Handle={ExpressionToString(s.Handle, program)}"),
            SslOpenStmt s => ("SslOpenStmt",
                $"Port={ExpressionToString(s.Port, program)}, Handle={TargetToString(s.Handle, program)}, Cert={ExpressionToString(s.CertificatePath, program)}" +
                (s.CertificatePassword != null
                    ? $", Password={ExpressionToString(s.CertificatePassword, program)}"
                    : "")),
            SslConnectStmt s => ("SslConnectStmt",
                $"Host={ExpressionToString(s.Host, program)}" +
                (s.Port != null ? $", Port={ExpressionToString(s.Port, program)}" : "") +
                $", Handle={TargetToString(s.Handle, program)}" +
                (s.Timeout != null ? $", Timeout={ExpressionToString(s.Timeout, program)}" : "")),
            UdpOpenStmt s => ("UdpOpenStmt",
                $"{(s.Port != null ? "Port=" + ExpressionToString(s.Port, program) + ", " : "")}Handle={TargetToString(s.Handle, program)}" +
                (s.Engine != null ? $", Engine={ExpressionToString(s.Engine, program)}" : "")),
            UdpSendStmt s => ("UdpSendStmt",
                $"Handle={ExpressionToString(s.Handle, program)}, Host={ExpressionToString(s.Host, program)}, Port={ExpressionToString(s.Port, program)}, Data={ExpressionToString(s.Data, program)}"),
            UdpRecvStmt s => ("UdpRecvStmt",
                $"Handle={ExpressionToString(s.Handle, program)}, Buffer={TargetToString(s.Buffer, program)}, RemoteIP={TargetToString(s.RemoteIp, program)}, RemotePort={TargetToString(s.RemotePort, program)}" +
                (s.Timeout != null ? $", Timeout={ExpressionToString(s.Timeout, program)}" : "")),
            UdpCloseStmt s => ("UdpCloseStmt", $"Handle={ExpressionToString(s.Handle, program)}"),
            ShutdownStmt s => ("ShutdownStmt",
                $"Handle={ExpressionToString(s.Handle, program)}, Mode={ExpressionToString(s.Mode, program)}"),
            SetOptionStmt s => ("SetOptionStmt",
                $"Handle={ExpressionToString(s.Handle, program)}, Opt={ExpressionToString(s.Option, program)}, Val={ExpressionToString(s.Value, program)}"),
            HttpFetchStmt s => ("HttpFetchStmt", $"{ExpressionToString(s.Url, program)}" +
                                                 (s.Options != null
                                                     ? $", Options={ExpressionToString(s.Options, program)}"
                                                     : "") +
                                                 $", Callback={s.CallbackName.Lexeme}"),
            HttpServerStmt s => ("HttpServerStmt", s.IsStopCommand
                ? "STOP"
                : $"Port={ExpressionToString(s.Port!, program)}" +
                  (s.Callback != null ? $", Callback={s.Callback.Lexeme}" : "") +
                  (s.CertificatePath != null ? $", Cert={ExpressionToString(s.CertificatePath, program)}" : "") +
                  (s.CertificatePassword != null ? ", Password=***" : "")),
            HttpRouteStmt s => ("HttpRouteStmt",
                $"Method={ExpressionToString(s.Method, program)}, Path={ExpressionToString(s.Path, program)}, Callback={s.Callback.Lexeme}"),
            HttpUseStmt s => ("HttpUseStmt",
                $"Path={(s.Path != null ? ExpressionToString(s.Path, program) : "DEFAULT")}, Callback={s.Callback.Lexeme}"),
            HttpUseNextStmt s => ("HttpUseNextStmt", ExpressionToString(s.NextHandle, program)),
            HttpConfigStmt s => ("HttpConfigStmt", ExpressionToString(s.ConfigObject, program)),
            _ => (stmt.GetType().Name, stmt.ToString() ?? "")
        };
        WriteOutput($"{indent}{FormatNodeHeader(stmt, typeName, content)}", useConsole);
    }

    /// <summary>
    /// Controlla se un'istruzione appartiene alla categoria dei comandi Mouse.
    /// </summary>
    private static bool IsMouseStmt(Stmt stmt) =>
        stmt is MouseConfigStmt or MouseVisibilityStmt or MouseSetStmt
            or MouseLimitStmt or MouseCursorStmt or MouseWaitStmt;

    /// <summary>
    /// Gestisce la formattazione e la stampa per i comandi del Mouse.
    /// </summary>
    private static void DumpMouseStatement(BasicProgram program, Stmt stmt, string indent, bool useConsole)
    {
        var (typeName, content) = stmt switch
        {
            MouseConfigStmt s => ("MouseConfigStmt", $"MOUSE {(s.Enabled ? "ON" : "OFF")}"),
            MouseVisibilityStmt s => ("MouseVisibilityStmt", $"MOUSE {(s.Visible ? "SHOW" : "HIDE")}"),
            MouseSetStmt s => ("MouseSetStmt",
                $"MOUSE SET {ExpressionToString(s.X, program)}, {ExpressionToString(s.Y, program)}"),
            MouseLimitStmt s => ("MouseLimitStmt", s.X1 != null
                // Usa '!' per asserire che se X1 esiste, anche gli altri parametri esistono (garantito dal Parser)
                ? $"MOUSE LIMIT {ExpressionToString(s.X1, program)}, {ExpressionToString(s.Y1!, program)}, {ExpressionToString(s.X2!, program)}, {ExpressionToString(s.Y2!, program)}"
                : "MOUSE UNLIMIT"),
            MouseCursorStmt s => ("MouseCursorStmt",
                $"MOUSE CURSOR {ExpressionToString(s.CursorId, program)}"),
            MouseWaitStmt => ("MouseWaitStmt", "WAIT MOUSE"),
            _ => (stmt.GetType().Name, stmt.ToString() ?? "")
        };

        WriteOutput($"{indent}{FormatNodeHeader(stmt, typeName, content)}", useConsole);
    }

    // ====================================================================================================================
    // --- HELPER DI FORMATTAZIONE PER STRINGHE ---
    // ====================================================================================================================

    /// <summary>
    /// Helper per formattare l'azione di un evento (GOTO, GOSUB, CALL, STOP) in una stringa leggibile.
    /// Utilizzato per la generazione del dump dell'AST per i comandi ON KEY, ON MOUSE e ON WINDOW.
    /// </summary>
    /// <param name="action">L'oggetto azione da formattare.</param>
    /// <param name="program">Il programma corrente, usato per la formattazione delle espressioni di chiamata.</param>
    /// <returns>Una stringa che descrive l'azione (es. "GOTO Label", "CALL MySub").</returns>
    private static string FormatAction(OnKeyAction action, BasicProgram program)
    {
        return action switch
        {
            GotoAction g => $"GOTO {g.TargetLabel.Lexeme}",
            GosubAction g => $"GOSUB {g.TargetLabel.Lexeme}",
            CallAction c => $"CALL {ExpressionToString(c.Call, program)}",
            StopAction => "STOP",
            _ => "UNKNOWN"
        };
    }

    /// <summary>
    /// Crea una rappresentazione testuale formattata per un'istruzione `OnKeyStmt`,
    /// dettagliando la chiave associata e l'azione da eseguire (GOTO, GOSUB, CALL, STOP).
    /// </summary>
    /// <param name="stmt">L'istruzione `OnKeyStmt` da formattare.</param>
    /// <returns>Una stringa che descrive l'istruzione in modo leggibile.</returns>
    private static string FormatOnKey(BasicProgram program, OnKeyStmt stmt)
    {
        var keyExpr = stmt.KeyExpression != null ? $"({ExpressionToString(stmt.KeyExpression, program)})" : "";
        return $"OnKeyStmt: ON KEY{keyExpr} {FormatAction(stmt.Action, program)}";
    }

    /// <summary>
    /// Helper per scrivere una stringa su `Console.Out` o su un listener di debug,
    /// in base a un flag booleano. Centralizza la logica di output.
    /// </summary>
    /// <param name="message">Il messaggio da scrivere.</param>
    /// <param name="useConsole">Se `true`, scrive sulla console; altrimenti, su un listener di debug.</param>
    private static void WriteOutput(string message, bool useConsole)
    {
        if (useConsole)
        {
            Console.WriteLine(message);
        }
        //System.Diagnostics.Debug.WriteLine(message);
    }

    /// <summary>
    /// Converte ricorsivamente un nodo di espressione (`Expr`) in una rappresentazione testuale dettagliata.
    /// Aggiunge informazioni di namespace per le chiamate a funzione risolte per migliorare la leggibilità del dump.
    /// </summary>
    /// <param name="expr">L'espressione dell'AST da convertire.</param>
    /// <param name="program">L'istanza del programma, necessaria per accedere alla SymbolTable.</param>
    /// <returns>Una stringa formattata che rappresenta l'espressione.</returns>
    private static string ExpressionToString(Expr expr, BasicProgram program)
    {
        return FormatExpr(expr, program);

        // Usa una funzione locale ricorsiva per gestire l'annidamento.
        string FormatExpr(Expr e, BasicProgram prog)
        {
            string FormatArrayAccess(ArrayAccessExpr a, BasicProgram p)
            {
                var s = a.NameSuffixed.Lexeme == a.NameCleaned.Lexeme
                    ? $"{a.NameSuffixed.Lexeme}#{a.SymbolId}({string.Join(", ", a.Indices.Select(i => FormatExpr(i, p)))})"
                    : $"{a.NameSuffixed.Lexeme} (as {a.NameCleaned.Lexeme})#{a.SymbolId}({string.Join(", ", a.Indices.Select(i => FormatExpr(i, p)))})";
                return a.IsSafe ? s + "?" : s;
            }

            return e switch
            {
                BinaryExpr binary =>
                    $"({FormatExpr(binary.Left, prog)} {binary.Op.Lexeme} {FormatExpr(binary.Right, prog)})",
                UnaryExpr unary => $"({unary.Op.Lexeme}{FormatExpr(unary.Right, prog)})",
                NumberExpr number => number.Value.ToString(CultureInfo.InvariantCulture),
                StringExpr str => $"\"{str.Value}\"",
                CheckedStringExpr chk => $"CHECKED \"{chk.Value}\"",
                BooleanExpr boolean => boolean.Value ? "TRUE" : "FALSE",

                VariableExpr variable => variable.NameSuffixed.Lexeme == variable.NameCleaned.Lexeme
                    ? $"{variable.NameSuffixed.Lexeme}#{variable.SymbolId}"
                    : $"{variable.NameSuffixed.Lexeme} (as {variable.NameCleaned.Lexeme})#{variable.SymbolId}",

                ArrayAccessExpr array => FormatArrayAccess(array, prog),

                ArrayLiteralExpr arrayLit =>
                    $"{{ {string.Join(", ", arrayLit.Elements.Select(elem => {
                        var elemStr = FormatExpr(elem, prog);
                        // Aggiungi l'introspezione del tipo per i NumberExpr
                        if (elem is NumberExpr ne)
                        {
                            var val = ne.Value;
                            string typeHint;
                            if (val % 1 == 0) // È un intero
                            {
                                if (val is >= byte.MinValue and <= byte.MaxValue) typeHint = "BYTE";
                                else if (val is >= short.MinValue and <= short.MaxValue) typeHint = "INTEGER";
                                else if (val is >= int.MinValue and <= int.MaxValue) typeHint = "LONG";
                                else typeHint = "QUAD";
                            }
                            else
                            {
                                typeHint = "DOUBLE";
                            }
                            return $"{elemStr} [{typeHint}]";
                        }
                        return elemStr;
                    }))} }}",

                FieldAccessExpr field => field.FieldSymbolId >= 0
                    ? $"{FormatExpr(field.Structure, prog)}.{field.Field.Lexeme}#{field.FieldSymbolId}"
                    : $"{FormatExpr(field.Structure, prog)}.{field.Field.Lexeme}",

                BuiltinFunctionCallExpr bfc =>
                    $"{bfc.Namespace.Lexeme}.{bfc.OriginalToken.Lexeme}({string.Join(", ", bfc.Args.Select(arg => FormatExpr(arg, prog)))})",

                FunctionExpr func =>
                    func.SymbolId >= 0
                        ? $"{(prog.Symbols.GetNamespaceNameForSymbol(func.SymbolId) != SymbolTable.GlobalScopeName ? $"{prog.Symbols.GetNamespaceNameForSymbol(func.SymbolId)}." : "")}{func.OriginalToken.Lexeme}#{func.SymbolId}({string.Join(", ", func.Args.Select(arg => FormatExpr(arg, prog)))})"
                        : $"{func.OriginalToken.Lexeme}#{func.SymbolId}({string.Join(", ", func.Args.Select(arg => FormatExpr(arg, prog)))})",

                UnresolvedCallExpr uce =>
                    $"(Unresolved: {uce.Name.Lexeme}({string.Join(", ", uce.Arguments.Select(arg => FormatExpr(arg, prog)))}))",

                IndexedAccessExpr indexed =>
                    $"{FormatExpr(indexed.Base, prog)}[{string.Join(", ", indexed.Indices.Select(arg => FormatExpr(arg, prog)))}]{(indexed.IsSafe ? "?" : "")}",

                MethodCallExpr methodCall =>
                    $"{FormatExpr(methodCall.Instance, prog)}.{methodCall.MethodName.Lexeme}({string.Join(", ", methodCall.Args.Select(arg => FormatExpr(arg, prog)))})",

                GroupingExpr grouping => $"({FormatExpr(grouping.Expression, prog)})",

                PropertyAccessExpr propAccess =>
                    $"{FormatExpr(propAccess.Instance, prog)}.{propAccess.PropertyName.Lexeme}",

                DictionaryLiteralExpr dict =>
                    $"{{ {string.Join(", ", dict.Entries.Select(entry => $"{FormatExpr(entry.Key, prog)}: {FormatExpr(entry.Value, prog)}"))} }}",

                LambdaExpr lambda => lambda.ToString(),

                ListComprehensionExpr lce =>
                    $"[{FormatExpr(lce.OutputExpression, prog)} {string.Join(" ", lce.Clauses.Select(c => $"FOR {c.IteratorVariable.Lexeme}#{c.IteratorSymbolId} IN {FormatExpr(c.SourceExpression, prog)}{(c.ConditionExpression != null ? " IF " + FormatExpr(c.ConditionExpression, prog) : "")}"))}]",

                DictComprehensionExpr dce =>
                    $"{{ {FormatExpr(dce.KeyExpression, prog)}:{FormatExpr(dce.ValueExpression, prog)} {string.Join(" ", dce.Clauses.Select(c => $"FOR {c.IteratorVariable.Lexeme}#{c.IteratorSymbolId} IN {FormatExpr(c.SourceExpression, prog)}{(c.ConditionExpression != null ? " IF " + FormatExpr(c.ConditionExpression, prog) : "")}"))} }}",

                SetComprehensionExpr sce =>
                    $"{{ {FormatExpr(sce.ElementExpression, prog)} {string.Join(" ", sce.Clauses.Select(c => $"FOR {c.IteratorVariable.Lexeme}#{c.IteratorSymbolId} IN {FormatExpr(c.SourceExpression, prog)}{(c.ConditionExpression != null ? " IF " + FormatExpr(c.ConditionExpression, prog) : "")}"))} }}",
                
                GoFunctionExpr goFunc =>
                    $"GO FUNCTION {goFunc.OriginalToken.Lexeme}({string.Join(", ", goFunc.Args.Select(arg => FormatExpr(arg, prog)))})",

                NewExpr newExpr => newExpr.ToString(),
                QualifiedNameExpr qne => qne.FullName,
                BaseExpr => "BASE",
                MacroExpr macro => $"&({FormatExpr(macro.Expression, prog)})",
                EvalExpr eval => eval.Args.IsDefaultOrEmpty
                    ? $"EVAL({FormatExpr(eval.Target, prog)})"
                    : $"EVAL({FormatExpr(eval.Target, prog)}, {string.Join(", ", eval.Args.Select(a => FormatExpr(a, prog)))})",
                FunctionReferenceExpr fref => $"@(FuncRef: {fref.OriginalToken.Lexeme}#{fref.SymbolId})",
                HttpFetchExpr fetch => $"(FETCH {FormatExpr(fetch.Url, prog)})",

                _ => e.ToString() ?? e.GetType().Name
            };
        }
    }

    /// <summary>
    /// Converte un nodo `DataType` in una rappresentazione testuale leggibile,
    /// gestendo tipi semplici, composti (es. `LIST OF ...`), UDT e array.
    /// VERSIONE CORRETTA E COMPLETA.
    /// </summary>
    /// <param name="dt">Il tipo di dato da convertire.</param>
    /// <returns>Una stringa formattata che descrive il tipo di dato.</returns>
    private static string DataTypeToString(DataType dt)
    {
        return dt switch
        {
            // Tipi non risolti e speciali
            UnresolvedQualifiedType uqt => uqt.QualifiedName.FullName,
            InferredType => "<inferred>",

            // Tipi contenitore (ricorsivi)
            ArrayType at => $"ARRAY OF {DataTypeToString(at.ElementType)}",
            ListType lt => $"LIST OF {DataTypeToString(lt.ElementType)}",
            DictType dict => $"DICT OF {DataTypeToString(dict.KeyType)}, {DataTypeToString(dict.ValueType)}",
            HashSetType hst => $"HASHSET OF {DataTypeToString(hst.ElementType)}",
            QueueType qt => $"QUEUE OF {DataTypeToString(qt.ElementType)}",
            StackType st => $"STACK OF {DataTypeToString(st.ElementType)}",
            SortedDictType sdt => $"SORTED OF {DataTypeToString(sdt.KeyType)}, {DataTypeToString(sdt.ValueType)}",
            TreeType tt => $"TREE OF {DataTypeToString(tt.KeyType)}, {DataTypeToString(tt.ValueType)}",
            AVLTreeType avlt => $"AVLTREE OF {DataTypeToString(avlt.KeyType)}, {DataTypeToString(avlt.ValueType)}",
            RedBlackTreeType rbt => $"REDBLACK OF {DataTypeToString(rbt.KeyType)}, {DataTypeToString(rbt.ValueType)}",
            BinaryIndexedTreeType bitt => $"BITREE OF {DataTypeToString(bitt.ElementType)}",
            LinkedListType llt => $"LINKEDLIST OF {DataTypeToString(llt.ElementType)}",
            DoublyLinkedListType dllt => $"DOUBLYLINKEDLIST OF {DataTypeToString(dllt.ElementType)}",
            CircularLinkedListType cllt => $"CIRCULAR OF {DataTypeToString(cllt.ElementType)}",
            BinaryHeapType bht => $"{(bht.IsMaxHeap ? "MAXHEAP" : "HEAP")} OF {DataTypeToString(bht.ElementType)}",
            BagType bt => $"BAG OF {DataTypeToString(bt.ElementType)}",
            CacheType ct => $"CACHE OF {DataTypeToString(ct.KeyType)}, {DataTypeToString(ct.ValueType)}",
            GraphType gt => $"GRAPH OF {DataTypeToString(gt.NodeType)}",
            TimelineType tlt => $"TIMELINE OF {DataTypeToString(tlt.ValueType)}",
            HashRingType hrt => $"HASHRING OF {DataTypeToString(hrt.NodeType)}",
            RingBufferType rbt => $"RINGBUFFER OF {DataTypeToString(rbt.ElementType)}",
            BPlusTreeType bpt => $"BPLUSTREE OF {DataTypeToString(bpt.KeyType)}, {DataTypeToString(bpt.ValueType)}",
            ChannelType chanT => $"CHANNEL OF {DataTypeToString(chanT.ValueType)}",
            GeneratorType genT => $"GENERATOR OF {DataTypeToString(genT.YieldType)}",
            FunctionPointerType fpt =>
                $"CALLBACK ({string.Join(", ", fpt.ParameterTypes.Select(DataTypeToString))}) AS {DataTypeToString(fpt.ReturnType)}",
            KeyValuePairType kvpt =>
                $"KEYVALUEPAIR OF {DataTypeToString(kvpt.KeyType)}, {DataTypeToString(kvpt.ValueType)}",

            // Tipi definiti dall'utente
            UserDefinedType udt => udt.OriginalName.Lexeme,
            ClassType ct => ct.Definition.OriginalName.Lexeme,

            // Tipi primitivi e di sistema
            StringType st => st.ToString().ToUpper(), // Gestisce STRING * N
            SingleType => "SINGLE",
            DoubleType => "DOUBLE",
            CurrencyType => "CURRENCY",
            BitType => "BIT",
            ByteType => "BYTE",
            SByteType => "SBYTE",
            IntegerType => "INTEGER",
            UIntegerType => "UINTEGER",
            LongType => "LONG",
            ULongType => "ULONG",
            QuadType => "QUAD",
            UQuadType => "UQUAD",
            DateType => "DATE",
            TimeType => "TIME",
            DateTimeType => "DATETIME",
            TimeSpanType => "TIMESPAN",
            VoidType => "VOID",
            AnyType => "ANY",
            JsonType => "JSON",
            WaitGroupType => "WAITGROUP",
            FutureType => "FUTURE",
            DisplayModeType => "DISPLAYMODE",
            ErrType => "ERR",
            HttpOptionsType => "HTTPOPTIONS",
            HttpResponseType => "HTTPRESPONSE",
            HttpConfigType => "HTTPCONFIG",
            CookieJarType => "COOKIEJAR",
            HttpRequestType => "HTTPREQUEST",
            HttpPostedDataType => "HTTPPOSTEDDATA",
            FetchResultType => "FETCHRESULT",
            SqlResultSetType => "SQLRESULTSET",
            TrieType => "TRIE",
            SparseSetType => "SPARSESET",
            TBrowseType => "TBROWSE",
            TBColumnType => "TBCOLUMN",
            NextHandlerType => "NEXTHANDLER",
            AnovaResultType => "ANOVARESULT",
            ArimaResultType => "ARIMARESULT",
            RegressionResultType => "REGRESSIONRESULT",
            KMeansResultType => "KMEANSRESULT",
            HoltWintersResultType => "HOLTWINTERSRESULT",
            AdfTestResultType => "ADFTESTRESULT",
            PcaResultType => "PCARESULT",
            HmmResultType => "HMMRESULT",
            LuDecompositionType => "LUDECOMPOSITION",
            QrDecompositionType => "QRDECOMPOSITION",
            SvdDecompositionType => "SVDDECOMPOSITION",
            EvdDecompositionType => "EVDDECOMPOSITION",
            CholeskyDecompositionType => "CHOLESKYDECOMPOSITION",
            PortfolioResultType => "PORTFOLIORESULT",
            PortfolioConstraintsType => "PORTFOLIOCONSTRAINTS",
            GaParametersType => "GAPARAMETERS",
            BacktestResultType => "BACKTESTRESULT",
            PortfolioMetricsType => "PORTFOLIOMETRICS",
            RebalanceStrategyType => "REBALANCESTRATEGY",

            _ => dt.GetType().Name.Replace("Type", "").ToUpper()
        };
    }

    /// <summary>
    /// Gestisce il dump della definizione di un tipo definito dall'utente (`TYPE...END TYPE`).
    /// Stampa il nome, la visibilità, lo ScopeId, il SymbolId, e scende ricorsivamente nei membri.
    /// CORRETTO: Utilizza FormatNodeHeader e delega il dump dei membri.
    /// </summary>
    /// <param name="udt">L'istanza di `UserDefinedType` da analizzare.</param>
    /// <param name="indent">La stringa di indentazione per la formattazione.</param>
    /// <param name="useConsole">Specifica il target di output.</param>
    private static void DumpTypeDefinition(BasicProgram program, UserDefinedType udt, string indent, bool useConsole)
    {
        // Usa FormatNodeHeader per un'intestazione completa
        WriteOutput($"{indent}{FormatNodeHeader(udt, "UserDefinedType", udt.ToString())}", useConsole);

        var nextIndent = indent + "  ";

        if (udt.Fields.Any())
        {
            WriteOutput($"{nextIndent}Fields:", useConsole);
            // Stampa i campi con i loro ID
            foreach (var field in udt.Fields)
                WriteOutput($"{nextIndent}  -> {field}", useConsole);
        }

        if (udt.Properties.Any())
        {
            WriteOutput($"{nextIndent}Properties:", useConsole);
            // Delega a DumpPropertyDefinition per una stampa completa
            foreach (var prop in udt.Properties)
                DumpPropertyDefinition(program, prop, nextIndent + "  ", useConsole);
        }

        if (udt.Constructors.Any())
        {
            WriteOutput($"{nextIndent}Constructors:", useConsole);
            foreach (var ctor in udt.Constructors)
                DumpConstructorDefinition(program, ctor, nextIndent + "  ", useConsole);
        }

        if (udt.Destructors.Any())
        {
            WriteOutput($"{nextIndent}Destructors:", useConsole);
            foreach (var dtor in udt.Destructors)
                DumpDestructorDefinition(program, dtor, nextIndent + "  ", useConsole);
        }

        if (udt.Operators.Any())
        {
            WriteOutput($"{nextIndent}Operators:", useConsole);
            foreach (var op in udt.Operators)
                DumpOperatorDefinition(program, op, nextIndent + "  ", useConsole);
        }

        if (udt.Methods.Any())
        {
            WriteOutput($"{nextIndent}Methods:", useConsole);
            // Delega a DumpProcedureDefinition per una stampa ricorsiva e completa
            foreach (var method in udt.Methods)
                DumpProcedureDefinition(program, method, nextIndent + "  ", useConsole);
        }

        WriteOutput($"{indent}END TYPE", useConsole);
    }

    /// <summary>
    /// Gestisce il dump di una definizione di proprietà (`PROPERTY...END PROPERTY`).
    /// Stampa la firma della proprietà, il suo ScopeId, SymbolId, e i corpi degli accessori GET e SET.
    /// </summary>
    /// <param name="propDef">L'istanza di `PropertyDefinition` da analizzare.</param>
    /// <param name="indent">La stringa di indentazione per la formattazione.</param>
    /// <param name="useConsole">Specifica il target di output.</param>
    private static void DumpPropertyDefinition(BasicProgram program, PropertyDefinition propDef, string indent,
        bool useConsole)
    {
        WriteOutput($"{indent}{FormatNodeHeader(propDef, "PropertyDefinition", propDef.ToString())}", useConsole);
        var nextIndent = indent + "  ";

        if (propDef.Getter != null)
        {
            WriteOutput($"{nextIndent}GET:", useConsole);
            foreach (var stmt in propDef.Getter.Body)
            {
                DumpStatement(stmt, nextIndent + "  ", program, useConsole);
            }
        }

        if (propDef.Setter != null)
        {
            WriteOutput(
                $"{nextIndent}SET(value#{propDef.Setter.ValueParameter.SymbolId} AS {DataTypeToString(propDef.Setter.ValueParameter.DataType)}):",
                useConsole);
            foreach (var stmt in propDef.Setter.Body)
            {
                DumpStatement(stmt, nextIndent + "  ", program, useConsole);
            }
        }
    }

    /// <summary>
    /// Gestisce il dump di una definizione di operatore sovraccaricato (`OPERATOR...END OPERATOR`).
    /// Stampa la firma dell'operatore, il suo ScopeId, SymbolId, i parametri e il corpo.
    /// </summary>
    /// <param name="opDef">L'istanza di `OperatorDefinition` da analizzare.</param>
    /// <param name="indent">La stringa di indentazione per la formattazione.</param>
    /// <param name="useConsole">Specifica il target di output.</param>
    private static void DumpOperatorDefinition(BasicProgram program, OperatorDefinition opDef, string indent,
        bool useConsole)
    {
        var header = $"OPERATOR {opDef.OperatorToken.Lexeme} AS {DataTypeToString(opDef.ReturnType)}";
        WriteOutput($"{indent}{FormatNodeHeader(opDef, "OperatorDefinition", header)}", useConsole);
        var nextIndent = indent + "  ";

        if (opDef.Parameters.Any())
        {
            WriteOutput($"{nextIndent}Parameters: {string.Join(", ", opDef.Parameters.Select(p => p.ToString()))}",
                useConsole);
        }

        WriteOutput($"{nextIndent}Body:", useConsole);
        foreach (var stmt in opDef.Body)
        {
            DumpStatement(stmt, nextIndent + "  ", program, useConsole);
        }
    }

    /// <summary>
    /// Gestisce il dump di una definizione di costruttore (`SUB NEW...END SUB`).
    /// Stampa i parametri e il corpo del costruttore.
    /// </summary>
    /// <param name="ctorDef">L'istanza di `ConstructorDefinition` da analizzare.</param>
    /// <param name="indent">La stringa di indentazione per la formattazione.</param>
    /// <param name="useConsole">Specifica il target di output.</param>
    private static void DumpConstructorDefinition(BasicProgram program, ConstructorDefinition ctorDef, string indent,
        bool useConsole)
    {
        // I costruttori non sono AstNode, quindi non hanno un header formattato standard.
        WriteOutput($"{indent}-> ConstructorDefinition: SUB NEW", useConsole);
        var nextIndent = indent + "  ";

        if (ctorDef.Parameters.Any())
        {
            WriteOutput($"{nextIndent}Parameters: {string.Join(", ", ctorDef.Parameters.Select(p => p.ToString()))}",
                useConsole);
        }

        WriteOutput($"{nextIndent}Body:", useConsole);
        foreach (var stmt in ctorDef.Body)
        {
            DumpStatement(stmt, nextIndent + "  ", program, useConsole);
        }
    }

    /// <summary>
    /// Gestisce il dump di una definizione di distruttore (`SUB DESTRUCTOR...END SUB`).
    /// Stampa il corpo del distruttore.
    /// </summary>
    /// <param name="dtorDef">L'istanza di `DestructorDefinition` da analizzare.</param>
    /// <param name="indent">La stringa di indentazione per la formattazione.</param>
    /// <param name="useConsole">Specifica il target di output.</param>
    private static void DumpDestructorDefinition(BasicProgram program, DestructorDefinition dtorDef, string indent,
        bool useConsole)
    {
        // I distruttori non sono AstNode.
        WriteOutput($"{indent}-> DestructorDefinition: SUB DESTRUCTOR", useConsole);
        var nextIndent = indent + "  ";

        WriteOutput($"{nextIndent}Body:", useConsole);
        foreach (var stmt in dtorDef.Body)
        {
            DumpStatement(stmt, nextIndent + "  ", program, useConsole);
        }
    }

    /// <summary>
    /// Gestisce il dump della definizione di una classe (`CLASS...END CLASS`).
    /// Stampa il nome, la visibilità, lo ScopeId, il SymbolId, l'eventuale classe base
    /// e scende ricorsivamente nei membri.
    /// CORRETTO: Utilizza FormatNodeHeader e delega il dump dei membri.
    /// </summary>
    /// <param name="classDef">L'istanza di `ClassDefinition` da analizzare.</param>
    /// <param name="indent">La stringa di indentazione per la formattazione.</param>
    /// <param name="useConsole">Specifica il target di output.</param>
    private static void DumpClassDefinition(BasicProgram program, ClassDefinition classDef, string indent,
        bool useConsole)
    {
        // Usa FormatNodeHeader per un'intestazione completa
        WriteOutput($"{indent}{FormatNodeHeader(classDef, "ClassDefinition", classDef.ToString())}", useConsole);

        var nextIndent = indent + "  ";

        if (classDef.Members.Any())
        {
            WriteOutput($"{nextIndent}Members:", useConsole);
            foreach (var member in classDef.Members)
            {
                // Stampa il modificatore di accesso
                WriteOutput($"{nextIndent}  [{member.Access.ToString().ToUpper()}]", useConsole);

                // Delega al dumper appropriato per il tipo di membro
                switch (member)
                {
                    case FieldMember fm:
                        // Un FieldMember contiene un DeclarationStmt
                        DumpStatement(
                            new DeclarationStmt(new Token(TokenKind.Dim, "DIM", null, SourcePosition.Unknown),
                                [fm.Declaration]), nextIndent + "    ", program, useConsole);
                        break;
                    case PropertyMember pm:
                        DumpPropertyDefinition(program, pm.Property, nextIndent + "    ", useConsole);
                        break;
                    case MethodMember mm:
                        DumpProcedureDefinition(program, mm.Procedure, nextIndent + "    ", useConsole);
                        break;
                    case ConstructorMember cm:
                        DumpConstructorDefinition(program, cm.Constructor, nextIndent + "    ", useConsole);
                        break;
                    case DestructorMember dm:
                        DumpDestructorDefinition(program, dm.Destructor, nextIndent + "    ", useConsole);
                        break;
                    case OperatorMember om:
                        DumpOperatorDefinition(program, om.Operator, nextIndent + "    ", useConsole);
                        break;
                    case EventMember em:
                        WriteOutput($"{nextIndent}    EVENT {em.Event.OriginalName.Lexeme}", useConsole);
                        break;
                    default:
                        WriteOutput($"{nextIndent}    Unknown Member Type: {member.GetType().Name}", useConsole);
                        break;
                }
            }
        }

        WriteOutput($"{indent}END CLASS", useConsole);
    }

    /// <summary>
    /// Gestisce il dump della definizione di un'interfaccia (`INTERFACE...END INTERFACE`).
    /// </summary>
    private static void DumpInterfaceDefinition(BasicProgram program, InterfaceDefinition interfaceDef, string indent,
        bool useConsole)
    {
        WriteOutput($"{indent}{FormatNodeHeader(interfaceDef, "InterfaceDefinition", interfaceDef.ToString())}",
            useConsole);

        var nextIndent = indent + "  ";

        if (interfaceDef.Members.Any())
        {
            WriteOutput($"{nextIndent}Members:", useConsole);
            foreach (var member in interfaceDef.Members)
            {
                WriteOutput($"{nextIndent}  [{member.Access.ToString().ToUpper()}]", useConsole);
                switch (member)
                {
                    case MethodMember mm:
                        DumpProcedureDefinition(program, mm.Procedure, nextIndent + "    ", useConsole);
                        break;
                    case PropertyMember pm:
                        DumpPropertyDefinition(program, pm.Property, nextIndent + "    ", useConsole);
                        break;
                    case EventMember em:
                        WriteOutput($"{nextIndent}    EVENT {em.Event.OriginalName.Lexeme}", useConsole);
                        break;
                    default:
                        WriteOutput($"{nextIndent}    Unknown Interface Member Type: {member.GetType().Name}",
                            useConsole);
                        break;
                }
            }
        }

        WriteOutput($"{indent}END INTERFACE", useConsole);
    }

    /// <summary>
    /// Gestisce il dump di una definizione di tipo delegato (`DELEGATE...`).
    /// </summary>
    private static void DumpDelegateDefinition(BasicProgram program, DelegateDefinition delegateDef, string indent,
        bool useConsole)
    {
        WriteOutput($"{indent}{FormatNodeHeader(delegateDef, "DelegateDefinition", delegateDef.ToString())}",
            useConsole);
    }

    /// <summary>
    /// Gestisce il dump della definizione di un'enumerazione (`ENUM...END ENUM`).
    /// Stampa il nome, la visibilità, lo ScopeId, il SymbolId, il tipo sottostante
    /// e scende ricorsivamente nei membri.
    /// </summary>
    /// <param name="enumDef">L'istanza di `EnumDefinition` da analizzare.</param>
    /// <param name="indent">La stringa di indentazione per la formattazione.</param>
    /// <param name="useConsole">Specifica il target di output.</param>
    private static void DumpEnumDefinition(BasicProgram program, EnumDefinition enumDef, string indent, bool useConsole)
    {
        // Usa FormatNodeHeader per un'intestazione completa
        WriteOutput($"{indent}{FormatNodeHeader(enumDef, "EnumDefinition", enumDef.ToString())}", useConsole);

        var nextIndent = indent + "  ";

        if (enumDef.Members.Any())
        {
            WriteOutput($"{nextIndent}Members:", useConsole);
            // Stampa i membri con i loro ID e valori
            foreach (var member in enumDef.Members)
            {
                // Un EnumMember è un Decl, quindi possiamo usare FormatNodeHeader per stampare il suo ScopeId
                var memberOutput = member.ValueExpr != null
                    ? $"{member.Name.Lexeme}#{member.SymbolId} = {ExpressionToString(member.ValueExpr, program)}"
                    : $"{member.Name.Lexeme}#{member.SymbolId}";
                WriteOutput($"{nextIndent}  {FormatNodeHeader(member, "EnumMember", memberOutput)}", useConsole);
            }
        }
        else
        {
            WriteOutput($"{nextIndent}(Nessun membro)", useConsole);
        }

        WriteOutput($"{indent}END ENUM", useConsole);
    }
}