// FILE: Interpreter.Linker.Visitors.cs

namespace Basic;

/// <summary>
/// Contiene le classi AstVisitor interne utilizzate durante le varie fasi del Linker.
/// </summary>
public static partial class Linker
{
    /// <summary>
    /// [FASE 2.A] Un AstVisitor specializzato che attraversa l'intero AST di un programma
    /// con l'unico scopo di trovare e registrare tutte le dichiarazioni `GLOBAL`.
    /// Opera prima del Binder principale per garantire che tutti i simboli globali
    /// esistano nella SymbolTable prima che qualsiasi corpo di procedura venga analizzato.
    /// </summary>
    private class GlobalDeclarationVisitor(BasicProgram program)
    {
        private string _currentNamespace = SymbolTable.GlobalScopeName;

        public void VisitModule(BasicModule module)
        {
            // Imposta il namespace di default del modulo prima di iniziare.
            _currentNamespace = program.GetNamespaceForModule(module.FilePath);

            // Visita sia le dichiarazioni top-level che gli statement
            foreach (var declaration in module.Declarations)
            {
                Visit(declaration);
            }

            foreach (var statementsList in module.Statements.Values)
            {
                foreach (var statement in statementsList)
                {
                    Visit(statement);
                }
            }
        }

        private void Visit(AstNode node)
        {
            // Scende ricorsivamente nei nodi contenitore.
            switch (node)
            {
                // Nodo target: DeclarationStmt con GLOBAL.
                case DeclarationStmt { Keyword.Kind: TokenKind.Global } declStmt:
                    RegisterGlobalDeclarations(declStmt);
                    // Non scendiamo oltre perché le inizializzazioni non ci interessano in questa fase.
                    break;

                // Nodi contenitore da attraversare.
                case ProcedureDefinition proc:
                    foreach (var stmt in proc.Body) Visit(stmt);
                    break;
                case IfStmt ifStmt:
                    foreach (var clause in ifStmt.IfClauses)
                    foreach (var stmt in clause.Statements)
                        Visit(stmt);
                    if (ifStmt.ElseClause != null)
                        foreach (var stmt in ifStmt.ElseClause.Statements)
                            Visit(stmt);
                    break;
                case SelectStmt selectStmt:
                    foreach (var clause in selectStmt.CaseClauses)
                    foreach (var stmt in clause.Statements)
                        Visit(stmt);
                    if (selectStmt.ElseClause != null)
                        foreach (var stmt in selectStmt.ElseClause.Statements)
                            Visit(stmt);
                    break;
                case TryCatchStmt tryStmt:
                    foreach (var stmt in tryStmt.TryBody) Visit(stmt);
                    foreach (var clause in tryStmt.CatchClauses)
                    foreach (var stmt in clause.Body)
                        Visit(stmt);
                    if (tryStmt.Finally != null)
                        foreach (var stmt in tryStmt.Finally.Body)
                            Visit(stmt);
                    break;
                // Ignora tutti gli altri nodi.
            }
        }

        private void RegisterGlobalDeclarations(DeclarationStmt declStmt)
        {
            program.Symbols.ResetScope();
            if (_currentNamespace != SymbolTable.GlobalScopeName)
            {
                program.Symbols.EnterScope(_currentNamespace, SymbolTable.ScopeType.Namespace);
            }

            // Invoca il metodo di registrazione esistente, che ora opererà
            // nello scope corretto (Globale o Namespace).
            RegisterGlobalVariables(program, declStmt, bindExpressions: true, isStrict: false);
        }
    }

    /// <summary>
    /// FASE 3.5: Esegue la validazione finale dei tipi sull'AST completamente riscritto.
    /// Questo passaggio è cruciale per intercettare errori semantici che emergono solo dopo
    /// che tutte le risoluzioni e le inferenze sono state completate, come l'accesso a campi
    /// su variabili risolte come primitive (es. Double invece di UDT) o assegnazioni incompatibili.
    /// </summary>
    /// <param name="program">L'istanza del programma da validare.</param>
    /// <exception cref="BasicException">Lanciata se viene rilevata un'incongruenza di tipo bloccante.</exception>
    public static void ValidateTypes(BasicProgram program)
    {
        foreach (var module in program.LoadedModules.Values)
        {
            var visitor = new TypeMismatchVisitor(program, module.IsStrict, module.FilePath);

            // Validazione dichiarazioni top-level
            foreach (var declaration in module.Declarations)
            {
                visitor.Visit(declaration);
            }

            // Validazione statement
            foreach (var statementsList in module.Statements.Values)
            {
                foreach (var statement in statementsList)
                {
                    visitor.Visit(statement);
                }
            }
        }
    }

    /// <summary>
    /// Visitor interno che attraversa l'AST per verificare la coerenza dei tipi.
    /// A differenza della versione di analisi (REPL), questa classe lancia eccezioni
    /// bloccanti (BasicException) non appena rileva un errore.
    /// </summary>
    private class TypeMismatchVisitor(BasicProgram program, bool isStrict, string modulePath)
    {
        // Contesto di riscrittura minimale necessario per l'inferenza dei tipi
        private readonly AstRewriteContext _context = new(
            modulePath,
            null,
            new DbfContext(),
            false, // ClipperMode non rilevante per la validazione statica finale
            isStrict,
            0 // Inizializza CurrentScopeId a 0 (Global Scope)
        );

        /// <summary>
        /// Avvia la visita di un nodo utilizzando il contesto di riscrittura predefinito del visitor.
        /// </summary>
        /// <param name="node">Il nodo AST da visitare.</param>
        public void Visit(AstNode? node)
        {
            Visit(node, _context);
        }

        /// <summary>
        /// Metodo centrale di dispatching per la visita ricorsiva dei nodi AST.
        /// Gestisce il cambio di contesto per le definizioni di procedura e delega
        /// le validazioni specifiche ai metodi helper.
        /// </summary>
        /// <param name="node">Il nodo corrente da visitare.</param>
        /// <param name="context">Il contesto di riscrittura corrente.</param>
        private void Visit(AstNode? node, AstRewriteContext context)
        {
            if (node == null) return;

            // Gestione del cambio di contesto per le procedure
            if (node is ProcedureDefinition proc)
            {
                var newContext = context with { CurrentProcedure = proc };
                foreach (var p in proc.Parameters) Visit(p, newContext);
                foreach (var s in proc.Body) Visit(s, newContext);
                return;
            }

            // --- VALIDAZIONI SPECIFICHE ---

            // 1. Controllo Assegnazioni (Type Mismatch)
            switch (node)
            {
                case DeclarationStmt declStmt:
                    foreach (var decl in declStmt.Declarations)
                        if (decl.InitialValue != null)
                            ValidateAssignment(decl.InitialValue, decl.DataType, context, decl.NameSuffixed);
                    break;
                case LetStmt letStmt:
                    foreach (var assignment in letStmt.Assignments)
                        ValidateAssignment(assignment.Value, null, context, targetNode: assignment.Target);
                    break;
                case AssignmentExpr ae:
                    ValidateAssignment(ae.Value, null, context, targetNode: ae.Target);
                    break;
                case ReturnStmt rs:
                    ValidateReturn(rs, context);
                    break;
            }

            // 2. Controllo Accesso ai Campi (Struttura non valida)
            // Questo intercetta il caso "Persone(0).Nome" dove Persone è Double
            if (node is FieldAccessExpr fae)
            {
                ValidateFieldAccess(fae, context);
            }
            else if (node is FieldTarget ft)
            {
                ValidateFieldTarget(ft, context);
            }

            // --- DISCESA RICORSIVA ---
            // Utilizziamo un approccio semplificato per visitare i figli, 
            // delegando ai metodi specifici o iterando sulle proprietà note.
            // Nota: Per brevità, qui copriamo i casi principali di attraversamento.

            switch (node)
            {
                // Statements
                case LetStmt l:
                    foreach (var a in l.Assignments)
                    {
                        Visit(a.Target, context);
                        Visit(a.Value, context);
                    }

                    break;
                case IfStmt i:
                    foreach (var c in i.IfClauses)
                    {
                        Visit(c.Condition, context);
                        foreach (var s in c.Statements) Visit(s, context);
                    }

                    if (i.ElseClause != null)
                        foreach (var s in i.ElseClause.Statements)
                            Visit(s, context);
                    break;
                case ForStmt f:
                    Visit(f.Variable, context);
                    Visit(f.Start, context);
                    Visit(f.End, context);
                    Visit(f.Step, context);
                    break;
                case SelectStmt s:
                    Visit(s.Expression, context);
                    foreach (var c in s.CaseClauses)
                    {
                        foreach (var t in c.Tests) VisitCaseTest(t, context);
                        foreach (var st in c.Statements) Visit(st, context);
                    }

                    if (s.ElseClause != null)
                        foreach (var st in s.ElseClause.Statements)
                            Visit(st, context);
                    break;
                case UsingStmt u:
                    Visit(u.ResourceInit, context);
                    foreach (var st in u.Body) Visit(st, context);
                    break;
                // Expressions & Targets
                case BinaryExpr b:
                    Visit(b.Left, context);
                    Visit(b.Right, context);
                    break;
                case UnaryExpr u: Visit(u.Right, context); break;
                case GroupingExpr g: Visit(g.Expression, context); break;
                case FunctionExpr f:
                    foreach (var a in f.Args) Visit(a, context);
                    break;
                case MethodCallExpr m:
                    Visit(m.Instance, context);
                    foreach (var a in m.Args) Visit(a, context);
                    break;
                case BuiltinFunctionCallExpr b:
                    foreach (var a in b.Args) Visit(a, context);
                    break;
                case ArrayAccessExpr a:
                    foreach (var idx in a.Indices) Visit(idx, context);
                    break;
                case IndexedAccessExpr i:
                    Visit(i.Base, context);
                    foreach (var idx in i.Indices) Visit(idx, context);
                    break;
                case FieldAccessExpr fa:
                    Visit(fa.Structure, context); break; // Già validato, ma scendiamo per visitare la struttura
                case FieldTarget ft: Visit(ft.Structure, context); break; // Già validato, ma scendiamo
                case ArrayElementTarget at:
                    foreach (var idx in at.Indices) Visit(idx, context);
                    break;
                case IndexedTarget it:
                    Visit(it.Base, context);
                    foreach (var idx in it.Indices) Visit(idx, context);
                    break;

                // Altri nodi contenitore (Try, While, etc.) dovrebbero essere aggiunti qui per completezza
                case TryCatchStmt t:
                    foreach (var s in t.TryBody) Visit(s, context);
                    foreach (var c in t.CatchClauses)
                    foreach (var s in c.Body)
                        Visit(s, context);
                    if (t.Finally != null)
                        foreach (var s in t.Finally.Body)
                            Visit(s, context);
                    break;
            }
        }

        /// <summary>
        /// Visita i nodi specifici delle clausole di test in un blocco SELECT CASE.
        /// </summary>
        /// <param name="test">Il nodo di test (Valore, Range o Confronto).</param>
        /// <param name="context">Il contesto corrente.</param>
        private void VisitCaseTest(CaseTest test, AstRewriteContext context)
        {
            switch (test)
            {
                case CaseValue cv: Visit(cv.Value, context); break;
                case CaseRange cr:
                    Visit(cr.Start, context);
                    Visit(cr.End, context);
                    break;
                case CaseComparison cc: Visit(cc.Value, context); break;
            }
        }

        /// <summary>
        /// Verifica che l'accesso ai campi (es. 'Var.Campo') avvenga su una struttura valida.
        /// Lancia un'eccezione se la variabile base è stata risolta in un tipo primitivo
        /// (es. Double, String) invece che in un UDT o una Classe.
        /// </summary>
        /// <param name="fae">L'espressione di accesso al campo.</param>
        /// <param name="context">Il contesto corrente per l'inferenza dei tipi.</param>
        /// <exception cref="BasicException">Lanciata se il tipo base non supporta l'accesso ai membri.</exception>
        private void ValidateFieldAccess(FieldAccessExpr fae, AstRewriteContext context)
        {
            // Verifica che la struttura base sia un tipo valido per avere campi (UDT, Classe, JSON, Built-in)
            // Ignoriamo VariableExpr con ID < 0 (late binding) o keyword speciali.
            if (fae.Structure is VariableExpr ve && (ve.SymbolId < 0 || ve.NameCleaned.Lexeme == "_dbRecord")) return;

            var structType = LinkerInferExpressionType(fae.Structure, context, program);
            var resolvedStructType = ResolveDataType(structType, program, modulePath);

            // 1. JSON, ANY e InferredType sono sempre validi (binding dinamico o non risolvibile staticamente).
            // ANY: IL emette JsonFieldGet/Set (EmitFieldAccess) e PCode/VM-C fanno la lookup sul dizionario;
            // un elemento di un JSON (`c = g["k"]`) e' ANY e `c.campo` e' la forma d'uso normale.
            if (resolvedStructType is JsonType or AnyType or InferredType) return;

            // 2. UDT e Classi sono validi contenitori
            if (resolvedStructType is UserDefinedType or ClassType) return;

            // 3. Tipi Built-in con Schema (es. HttpOptions, DisplayMode, Trading Types)
            // Questi tipi non sono UDT, ma hanno una struttura definita in StdLib.
            if (TypeSchemaRegistry.TryGetValue(resolvedStructType.GetType(), out var schema))
            {
                // Validazione stretta: se è un tipo built-in noto, il campo DEVE esistere nello schema.
                if (!schema!.Fields.ContainsKey(fae.Field.Lexeme))
                {
                    throw new BasicException(fae.Field,
                        $"Il campo '{fae.Field.Lexeme}' non esiste nel tipo di sistema '{resolvedStructType}'.");
                }

                return;
            }

            // 4. '.VALUE' su una base che non è UDT/Classe/tipo di sistema (es. `d.VALUE` con
            // `d AS DOUBLE?`) è l'unwrap di un nullable, non un vero field access: RewriteFieldAccess
            // lo riscrive in NullableValueExpr. L'informazione "nullable" di una variabile non
            // sopravvive al binding (il simbolo viene registrato con il tipo sottostante), quindi qui
            // non possiamo verificarlo staticamente: se il tipo non è un contenitore di campi valido
            // E il nome del membro è la keyword VALUE, lasciamo passare (i veri campi UDT/CLASS
            // chiamati "Value" sono già stati accettati al punto 2, prima di arrivare qui).
            if (fae.Field.Kind == TokenKind.Value) return;

            // 5. Se arriviamo qui, è un tipo primitivo (Double, String) o un tipo senza campi.
            throw new BasicException(fae.Field,
                $"Impossibile accedere al membro '{fae.Field.Lexeme}' su un valore di tipo '{resolvedStructType}'. " +
                "Solo Tipi Definiti dall'Utente (TYPE), Classi, JSON e tipi di sistema supportano l'accesso ai membri con il punto.");
        }

        /// <summary>
        /// Verifica che l'assegnazione a un campo (es. 'LET Var.Campo = ...') avvenga su una struttura valida.
        /// Analogo a ValidateFieldAccess ma per i target di assegnazione (L-Value).
        /// </summary>
        /// <param name="ft">Il target di accesso al campo.</param>
        /// <param name="context">Il contesto corrente per l'inferenza dei tipi.</param>
        /// <exception cref="BasicException">Lanciata se il tipo base non supporta l'accesso ai membri.</exception>
        private void ValidateFieldTarget(FieldTarget ft, AstRewriteContext context)
        {
            // Logica analoga a FieldAccessExpr, ma per le assegnazioni (L-Value)
            if (ft.Structure is VariableTarget vt && (vt.SymbolId < 0 || vt.NameCleaned.Lexeme == "_dbRecord")) return;

            // Dobbiamo convertire il Target in Expr per l'inferenza
            Expr structExpr;
            try
            {
                structExpr = ConvertTargetToExpr(ft.Structure!);
            }
            catch
            {
                return;
            }

            var structType = LinkerInferExpressionType(structExpr, context, program);
            var resolvedStructType = ResolveDataType(structType, program, modulePath);

            // 1. JSON, ANY e InferredType sono sempre validi (binding dinamico: un elemento di un JSON e' ANY)
            if (resolvedStructType is JsonType or AnyType or InferredType) return;

            // 2. UDT e Classi sono validi
            if (resolvedStructType is UserDefinedType or ClassType) return;

            // 2.2. Nullable: assegna al membro del tipo sottostante (es. `c.Nome` con c AS Contatto?)
            if (resolvedStructType is NullableType nullableStructType)
            {
                if (nullableStructType.ElementType is UserDefinedType or ClassType or JsonType)
                    return;
            }

            // 2.5. Le interfacce sono valide se il membro è una proprietà
            if (resolvedStructType is InterfaceType interfaceType)
            {
                var member = interfaceType.Definition.Members.FirstOrDefault(m =>
                    m.Name.Lexeme.Equals(ft.OriginalToken.Lexeme, StringComparison.OrdinalIgnoreCase));
                if (member is PropertyMember)
                {
                    // È una proprietà dell'interfaccia, quindi è un target valido.
                    return;
                }
            }

            // 3. Tipi Built-in con Schema
            if (TypeSchemaRegistry.TryGetValue(resolvedStructType.GetType(), out var schema))
            {
                if (!schema!.Fields.ContainsKey(ft.OriginalToken.Lexeme))
                {
                    throw new BasicException(ft.OriginalToken,
                        $"Il campo '{ft.OriginalToken.Lexeme}' non esiste nel tipo di sistema '{resolvedStructType}'.");
                }

                return;
            }

            // 4. Errore per tipi primitivi
            throw new BasicException(ft.OriginalToken,
                $"Impossibile assegnare al membro '{ft.OriginalToken.Lexeme}' perché la struttura base " +
                $"è di tipo '{resolvedStructType}', che non supporta i campi.");
        }

        /// <summary>
        /// Verifica la compatibilità dei tipi in un'operazione di assegnazione o inizializzazione.
        /// Controlla se il tipo del valore può essere coercito nel tipo della destinazione.
        /// VERSIONE CORRETTA: Logga solo in caso di errore, come richiesto.
        /// </summary>
        private void ValidateAssignment(Expr valueExpr, DataType? declaredTargetType, AstRewriteContext context,
            Token? tokenForError = null, AstNode? targetNode = null)
        {
            DataType? targetType = declaredTargetType;
            if (targetType == null && targetNode is Target t)
            {
                targetType = InferTargetType(t, context, program);
            }

            if (targetType is null or InferredType)
            {
                return;
            }

            var valueContext = context with { ExpectedType = targetType };
            var valueType = LinkerInferExpressionType(valueExpr, valueContext, program);

            if (valueType is not InferredType && targetType is not InferredType)
            {
                var resolvedValueType = ResolveDataType(valueType, program, modulePath);
                var resolvedTargetType = ResolveDataType(targetType, program, modulePath);

                // Controllo rank array: se entrambi sono ArrayType, verifica che il numero
                // di dimensioni (rank) corrisponda, esaminando i nodi dichiarazione.
                if (resolvedValueType is ArrayType && resolvedTargetType is ArrayType)
                {
                    var srcRank = InferArrayRank(valueExpr, program);
                    var tgtRank = InferArrayRank(targetNode, program);
                    if (srcRank >= 0 && tgtRank >= 0 && srcRank != tgtRank)
                    {
                        var msg = $"Impossibile assegnare un array {srcRank}D a un array {tgtRank}D.";
                        EmitTypeError(msg, tokenForError, targetNode, valueExpr);
                        return;
                    }
                }

                if (!CanCoerce(resolvedValueType, resolvedTargetType, valueContext, program))
                {
                    var msg =
                        $"Impossibile convertire implicitamente il tipo '{resolvedValueType}' in '{resolvedTargetType}'.";

                    EmitTypeError(msg, tokenForError, targetNode, valueExpr);
                }
            }
        }

        private static int InferArrayRank(AstNode? node, BasicProgram program)
        {
            if (node is VariableExpr ve)
            {
                if (ve.SymbolId >= 0)
                {
                    var sym = program.Symbols.GetSymbol(ve.SymbolId);
                    if (sym.DeclarationNode is VariableDeclaration vd)
                        return vd.Dimensions.Length;
                }
                return -1;
            }
            if (node is Target t)
            {
                // Try to get symbol from target
                int symbolId = t switch
                {
                    VariableTarget vt => vt.SymbolId,
                    ArrayElementTarget aet => aet.SymbolId,
                    _ => -1
                };
                if (symbolId >= 0)
                {
                    var sym = program.Symbols.GetSymbol(symbolId);
                    if (sym.DeclarationNode is VariableDeclaration vd)
                        return vd.Dimensions.Length;
                }
                return -1;
            }
            return -1;
        }

        private void EmitTypeError(string msg, Token? tokenForError, AstNode? targetNode, Expr valueExpr)
        {
            if (tokenForError != null)
            {
                throw CreateError(tokenForError, msg, ErrorCode.TypeMismatch);
            }

            if (targetNode is Target t1)
            {
                var tToken = t1 switch
                {
                    VariableTarget vt => vt.NameSuffixed,
                    ArrayElementTarget aet => aet.NameSuffixed,
                    FieldTarget ft => ft.OriginalToken,
                    _ => null
                };

                if (tToken != null)
                    throw CreateError(tToken, msg, ErrorCode.TypeMismatch);

                throw new BasicException(ErrorCode.TypeMismatch, t1.Position, msg);
            }

            throw CreateError(valueExpr, msg, ErrorCode.TypeMismatch);
        }

        /// <summary>
        /// Verifica che il tipo restituito da un'istruzione RETURN sia compatibile con
        /// il tipo di ritorno dichiarato della funzione corrente.
        /// </summary>
        /// <param name="returnStmt">L'istruzione RETURN da validare.</param>
        /// <param name="context">Il contesto corrente (necessario per accedere alla definizione della procedura).</param>
        /// <exception cref="BasicException">Lanciata se il tipo restituito non corrisponde alla firma della funzione.</exception>
        private void ValidateReturn(ReturnStmt returnStmt, AstRewriteContext context)
        {
            if (returnStmt.Value == null || context.CurrentProcedure is not FunctionDefinition funcDef) return;

            var returnValueType = LinkerInferExpressionType(returnStmt.Value, context, program);
            var targetType = funcDef.ReturnType;

            if (returnValueType is not InferredType && targetType is not InferredType)
            {
                var resolvedReturnValueType = ResolveDataType(returnValueType, program, modulePath);
                var resolvedTargetType = ResolveDataType(targetType, program, modulePath);

                if (!CanCoerce(resolvedReturnValueType, resolvedTargetType, context, program))
                {
                    throw new BasicException(returnStmt.Token,
                        $"Il tipo restituito '{resolvedReturnValueType}' non corrisponde al tipo dichiarato della funzione '{resolvedTargetType}'.");
                }
            }
        }
    }
}