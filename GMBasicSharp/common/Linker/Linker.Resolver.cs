// FILE: Interpreter.Linker.Resolver.cs

using System.Collections.Immutable;

// ReSharper disable RedundantArgumentDefaultValue

namespace Basic;

/// <summary>
/// Contiene la logica per la FASE 2.5 del processo di compilazione: ID Resolution.
/// </summary>
public static partial class Linker
{
    private const bool ResolverLog = false;

    /// <summary>
    /// [FASE 2.5] Punto di ingresso principale per la risoluzione degli identificatori (SymbolId).
    /// Questa fase è il "ponte" cruciale tra la costruzione della SymbolTable (Fase 2) e 
    /// l'analisi semantica/riscrittura (Fase 3).
    ///
    /// OBIETTIVO ARCHITETTURALE:
    /// L'obiettivo primario di questo metodo è trasformare l'AST da una rappresentazione "testuale"
    /// (dove i riferimenti sono basati su stringhe, es. VariableExpr con nome "X") a una rappresentazione
    /// "simbolica" (dove i riferimenti sono basati su ID numerici, es. VariableExpr con SymbolId 123).
    /// Questo passaggio è fondamentale per le prestazioni e la correttezza delle fasi successive,
    /// poiché elimina la necessità di costose ricerche di stringhe e disambigua i riferimenti
    /// basandosi sullo scope lessicale stabilito nella Fase 2.
    ///
    /// FLUSSO DI ESECUZIONE:
    /// 1.  Iterazione dei Moduli: Attraversa tutti i moduli caricati nel programma (`program.LoadedModules`).
    ///     L'ordine non è strettamente critico come nel Binder, poiché la SymbolTable è già popolata,
    ///     ma si mantiene un approccio deterministico.
    /// 2.  Isolamento delle Modifiche: Come per il Binder, viene creato un dizionario temporaneo `updatedModules`
    ///     per raccogliere le versioni trasformate dei moduli, garantendo che il processo sia atomico
    ///     per ogni unità di compilazione.
    /// 3.  Delega al Resolver di Modulo: Per ogni modulo, invoca `ResolveModule`. Questo metodo scenderà
    ///     ricorsivamente nell'albero sintattico (AST).
    /// 4.  Persistenza: Al termine dell'elaborazione di tutti i moduli, sostituisce le vecchie istanze
    ///     in `program.LoadedModules` con le nuove istanze contenenti gli ID risolti.
    ///
    /// STATO DI INGRESSO (PRE-CONDIZIONI):
    /// - La `SymbolTable` deve essere completamente popolata con tutte le dichiarazioni (Fase 2 completata).
    /// - Ogni nodo dell'AST (`AstNode`) deve avere una proprietà `ScopeId` valida (diversa da -1),
    ///   che indica lo scope lessicale in cui il nodo è stato definito o utilizzato.
    ///
    /// STATO DI USCITA (POST-CONDIZIONI):
    /// - Tutti i nodi espressione che fanno riferimento a simboli (es. `VariableExpr`, `ArrayAccessExpr`)
    ///   devono avere il loro `SymbolId` impostato a un valore valido (>= 0), a meno che il simbolo
    ///   non sia definito (nel qual caso potrebbe essere lanciato un errore o lasciato per la gestione
    ///   degli errori successiva, a seconda della politica).
    /// </summary>
    /// <param name="program">L'istanza del programma contenente la SymbolTable e i moduli da risolvere.</param>
    public static void ResolveSymbolIds(BasicProgram program)
    {
        Log("[RESOLVER] === INIZIO FASE 2.5: Risoluzione ID Simboli ===", ResolverLog);
        var updatedModules = new Dictionary<string, BasicModule>(StringComparer.OrdinalIgnoreCase);

        foreach (var (modulePath, originalModule) in program.LoadedModules)
        {
            Log($"[RESOLVER] Processo il modulo: {Path.GetFileName(modulePath)}", ResolverLog);
            updatedModules[modulePath] = ResolveModule(originalModule, program);
        }

        foreach (var (path, module) in updatedModules)
        {
            program.LoadedModules[path] = module;
        }

        Log("[RESOLVER] === FINE FASE 2.5: Tutti gli ID sono stati risolti. ===\n", ResolverLog);
    }

    /// <summary>
    /// Esegue la risoluzione dei simboli per un intero modulo.
    /// Questo metodo agisce come contenitore per la trasformazione dell'AST di un singolo file.
    ///
    /// STRATEGIA DI NAVIGAZIONE:
    /// A differenza del Binder (Fase 2), il Resolver NON ha bisogno di gestire lo stato attivo
    /// degli scope (EnterScope/ExitScope) nella SymbolTable, perché la Fase 2 ha già "timbrato"
    /// ogni nodo AST con il suo `ScopeId`. Questo rende il Resolver un componente puramente
    /// funzionale e stateless: per risolvere un nodo, guarda semplicemente il suo `ScopeId`
    /// e interroga la SymbolTable.
    ///
    /// COMPONENTI TRATTATI:
    /// 1.  Dichiarazioni (`module.Declarations`): Anche se le dichiarazioni sono state registrate
    ///     dal Binder, esse possono contenere espressioni (es. inizializzatori `DIM A = B + 1`)
    ///     che necessitano di risoluzione. Il metodo itera su queste dichiarazioni e invoca `ResolveDeclaration`.
    /// 2.  Istruzioni (`module.Statements`): Il corpo principale del codice (o le istruzioni top-level)
    ///     viene processato invocando `ResolveStatementList`.
    ///
    /// IMMUTABILITÀ:
    /// Come per tutte le fasi del compilatore, l'AST è trattato come immutabile. Questo metodo
    /// costruisce e restituisce una NUOVA istanza di `BasicModule` con le liste di dichiarazioni
    /// e istruzioni aggiornate.
    /// </summary>
    /// <param name="module">Il modulo originale proveniente dalla Fase 2.</param>
    /// <param name="program">L'istanza del programma per l'accesso alla SymbolTable.</param>
    /// <returns>Un nuovo `BasicModule` con un AST dove i riferimenti ai simboli sono risolti.</returns>
    private static BasicModule ResolveModule(BasicModule module, BasicProgram program)
    {
        // Risoluzione delle dichiarazioni globali/top-level (es. inizializzatori di costanti/variabili)
        var resolvedDeclarations = module.Declarations
            .Select(node => ResolveDeclaration(node, program))
            .ToImmutableArray();

        // Risoluzione degli statement a livello di modulo (codice numerato o main script)
        var resolvedStatements = module.Statements.ToDictionary(
            kvp => kvp.Key,
            kvp => ResolveStatementList([..kvp.Value], program).ToList()
        );

        var resolvedModule = new BasicModule(
            module.FilePath,
            module.IsStrict,
            module.SourceMap,
            resolvedStatements,
            module.Imports,
            resolvedDeclarations,
            module.LibraryName
        );
        resolvedModule.ExecutionOrder = module.ExecutionOrder;
        return resolvedModule;
    }

    /// <summary>
    /// Risolve i riferimenti ai simboli all'interno di un nodo di dichiarazione.
    /// Sebbene la dichiarazione stessa (il simbolo che definisce) sia già stata registrata dal Binder,
    /// molte dichiarazioni contengono "figli" che sono espressioni o blocchi di codice che devono
    /// essere risolti.
    ///
    /// GESTIONE DEI TIPI DI DICHIARAZIONE:
    /// - `ProcedureDefinition` (Function/Sub): Questa è la parte più complessa. Una procedura ha un corpo
    ///   di istruzioni (`Body`) e parametri. Il metodo deve scendere ricorsivamente nel corpo della
    ///   procedura per risolvere tutte le istruzioni al suo interno. È qui che avviene la risoluzione
    ///   del codice utente definito nelle funzioni.
    /// - `VariableDeclaration` / `ConstDeclaration`: Queste possono avere espressioni di inizializzazione
    ///   (es. `DIM X = Y * 2`). L'espressione `Y * 2` deve essere risolta per collegare `Y` al suo simbolo.
    /// - `ClassDefinition` / `UserDefinedType`: Simile alle procedure, contengono membri che possono avere
    ///   espressioni o corpi di metodi che necessitano di risoluzione.
    ///
    /// NOTE SULLO SCOPE:
    /// Anche qui, ci affidiamo agli `ScopeId` già presenti nei nodi figli. Non c'è bisogno di
    /// pushare/poppare scope nello stack della SymbolTable.
    /// </summary>
    /// <param name="node">Il nodo di dichiarazione da risolvere.</param>
    /// <param name="program">L'istanza del programma.</param>
    /// <returns>Una nuova versione del nodo di dichiarazione con i figli risolti.</returns>
    private static AstNode ResolveDeclaration(AstNode node, BasicProgram program)
    {
        AstNode resolvedNode;

        switch (node)
        {
            case ProcedureDefinition proc:
                // Entra nello scope della procedura prima di risolvere il corpo.
                // La Binder ha gia' creato questo scope (con nome, tipo e ownerSymbolId).
                // EnterScope lo ri-entra (non lo crea due volte) grazie al lookup
                // per nome+tipo+owner nei figli dello scope corrente.
                // Senza questo, le VariableExpr nel corpo hanno ScopeId del modulo
                // e FindSymbolIdInScopeChain non trova le variabili locali.
                program.Symbols.EnterScope(proc.OriginalName.Lexeme, SymbolTable.ScopeType.Procedure, proc.SymbolId);
                try
                {
                    var resolvedBody = ResolveStatementList(proc.Body, program);
                    var resolvedParams = proc.Parameters.Select(p =>
                        p.DefaultValue != null
                            ? p with { DefaultValue = ResolveExpression(p.DefaultValue, program) }
                            : p
                    ).ToImmutableArray();
                    resolvedNode = proc switch
                    {
                        FunctionDefinition fd => fd with { Body = resolvedBody, Parameters = resolvedParams },
                        GeneratorDefinition gd => gd with { Body = resolvedBody, Parameters = resolvedParams },
                        NativeProcedureDefinition npd => npd with { Parameters = resolvedParams },
                        ExternalProcedureDefinition epd => epd with { Parameters = resolvedParams },
                        _ => proc
                    };
                    if (proc.SymbolId >= 0)
                        program.Symbols.UpdateSymbolDeclaration(proc.SymbolId, resolvedNode);
                }
                finally
                {
                    program.Symbols.ExitScope();
                }
                break;

            case VariableDeclaration vd:
                // Risolve inizializzatore
                var resolvedInitialValue = vd.InitialValue != null
                    ? ResolveExpression(vd.InitialValue, program)
                    : null;

                // Risolve dimensioni (per array)
                var resolvedDimensions = vd.Dimensions.IsDefaultOrEmpty
                    ? ImmutableArray<Expr>.Empty
                    : [..vd.Dimensions.Select(d => ResolveExpression(d, program))];

                var resolvedLowerBounds = vd.LowerBounds.IsDefaultOrEmpty
                    ? ImmutableArray<Expr>.Empty
                    : [..vd.LowerBounds.Select(b => ResolveExpression(b, program))];

                resolvedNode = vd with
                {
                    InitialValue = resolvedInitialValue,
                    Dimensions = resolvedDimensions,
                    LowerBounds = resolvedLowerBounds
                };

                // Aggiorna la SymbolTable
                if (vd.SymbolId >= 0)
                    program.Symbols.UpdateSymbolDeclaration(vd.SymbolId, resolvedNode);
                break;

            case ConstDeclaration cd:
                resolvedNode = cd with { InitialValue = ResolveExpression(cd.InitialValue, program) };

                // Aggiorna la SymbolTable
                if (cd.SymbolId >= 0)
                    program.Symbols.UpdateSymbolDeclaration(cd.SymbolId, resolvedNode);
                break;

            case InterfaceDefinition idef:
                var resolvedInterfaceMembers =
                    idef.Members.Select(m => ResolveClassMember(m, program)).ToImmutableArray();
                resolvedNode = idef with { Members = resolvedInterfaceMembers };

                // Aggiorna la SymbolTable
                if (idef.SymbolId >= 0)
                    program.Symbols.UpdateSymbolDeclaration(idef.SymbolId, resolvedNode);
                break;

            case ClassDefinition cd:
                // Entra nello scope della classe prima di risolvere i membri,
                // cosi' i metodi trovano SELF e altri simboli di classe.
                program.Symbols.EnterScope(cd.Name.Lexeme, SymbolTable.ScopeType.Class, cd.SymbolId);
                try
                {
                    var resolvedMembers = cd.Members.Select(m => ResolveClassMember(m, program)).ToImmutableArray();
                    resolvedNode = cd with { Members = resolvedMembers };
                    if (cd.SymbolId >= 0)
                        program.Symbols.UpdateSymbolDeclaration(cd.SymbolId, resolvedNode);
                }
                finally
                {
                    program.Symbols.ExitScope();
                }
                break;

            case UserDefinedType udt:
                var modulePath = program.FileIdToPathMap.GetValueOrDefault(udt.Position.FileId, string.Empty);
                var resolvedFields = udt.Fields.Select(f =>
                {
                    var defVal = f.DefaultValue != null ? ResolveExpression(f.DefaultValue, program) : null;
                    var dims = f.Dimensions.IsDefaultOrEmpty
                        ? ImmutableArray<Expr>.Empty
                        : [..f.Dimensions.Select(d => ResolveExpression(d, program))];
                    var resolvedFieldType = ResolveDataType(f.DataType, program, modulePath);
                    return f with { DefaultValue = defVal, Dimensions = dims, DataType = resolvedFieldType };
                }).ToImmutableArray();

                // Entra nello scope del UDT prima di risolvere i metodi.
                // I metodi hanno SELF registrato in questo scope; senza EnterScope,
                // il Resolver non trova SELF perche' il metodo e' annidato.
                program.Symbols.EnterScope(udt.Name.Lexeme, SymbolTable.ScopeType.Type, udt.SymbolId);
                try
                {
                    var resolvedMethods = udt.Methods.Select(m => (ProcedureDefinition)ResolveDeclaration(m, program))
                        .ToImmutableArray();
                    var resolvedConstructors = udt.Constructors.Select(c =>
                        c with { Body = ResolveStatementList(c.Body, program) }).ToImmutableArray();
                    // BUG: (OperatorDefinition)ResolveDeclaration(o, program) faceva affidamento
                    // sullo switch generico di ResolveDeclaration, che non ha (e non aveva mai avuto)
                    // un case per OperatorDefinition — cadeva nel `default: resolvedNode = node;`,
                    // restituendo l'operatore INVARIATO. Il corpo dell'operatore non veniva MAI
                    // risolto: riferimenti a parametri come `a`/`b` restavano con SymbolId = -1,
                    // valutando a null a runtime e causando "Accesso a campi non supportato per
                    // il tipo 'null'" su `a.X`/`b.X`. Fix: risolvi il corpo direttamente, esattamente
                    // come già fatto sopra per i costruttori.
                    var resolvedOperators = udt.Operators.Select(o =>
                        o with { Body = ResolveStatementList(o.Body, program) }).ToImmutableArray();

                    resolvedNode = udt with
                    {
                        Fields = resolvedFields,
                        Methods = resolvedMethods,
                        Constructors = resolvedConstructors,
                        Operators = resolvedOperators
                    };
                }
                finally
                {
                    program.Symbols.ExitScope();
                }

                // Aggiorna la SymbolTable
                if (udt.SymbolId >= 0)
                    program.Symbols.UpdateSymbolDeclaration(udt.SymbolId, resolvedNode);
                break;

            default:
                resolvedNode = node;
                break;
        }

        return resolvedNode;
    }

    /// <summary>
    /// Risolve i riferimenti ai simboli all'interno della definizione di un membro di una classe.
    /// Questo metodo agisce come un dispatcher polimorfico per i vari tipi di <see cref="ClassMember"/>.
    ///
    /// RESPONSABILITÀ:
    /// 1.  Identificazione: Determina il tipo specifico del membro (Metodo, Campo, Proprietà, etc.).
    /// 2.  Delega della Risoluzione:
    ///     - Per <see cref="MethodMember"/>, delega la risoluzione della definizione della procedura (incluso il corpo) a <see cref="ResolveDeclaration"/>.
    ///     - Per <see cref="FieldMember"/>, delega la risoluzione della dichiarazione di variabile (incluso l'inizializzatore) a <see cref="ResolveDeclaration"/>.
    ///     - Per <see cref="ConstructorMember"/> e <see cref="DestructorMember"/>, naviga nel corpo del metodo speciale e invoca <see cref="ResolveStatementList"/> per risolvere le istruzioni.
    ///     - Per <see cref="PropertyMember"/>, invoca l'helper specializzato <see cref="ResolvePropertyMember"/> per gestire i blocchi GET/SET.
    ///     - Per <see cref="OperatorMember"/>, delega la risoluzione della definizione dell'operatore a <see cref="ResolveDeclaration"/>.
    /// 3.  Immutabilità: Restituisce una nuova istanza del membro con i sotto-nodi aggiornati e risolti, preservando l'integrità dell'AST originale.
    ///
    /// NOTE SULLA RICORSIONE:
    /// Poiché i membri di una classe contengono codice eseguibile (corpi dei metodi) o espressioni (inizializzatori),
    /// questo metodo innesca la discesa ricorsiva del Resolver all'interno della logica della classe.
    /// </summary>
    /// <param name="member">Il membro della classe da risolvere (es. un metodo o un campo).</param>
    /// <param name="program">L'istanza del programma contenente la SymbolTable.</param>
    /// <returns>Una nuova istanza di <see cref="ClassMember"/> con tutti i riferimenti ai simboli risolti.</returns>
    private static ClassMember ResolveClassMember(ClassMember member, BasicProgram program)
    {
        return member switch
        {
            MethodMember mm => mm with { Procedure = (ProcedureDefinition)ResolveDeclaration(mm.Procedure, program) },
            FieldMember fm => fm with
            {
                Declaration = (VariableDeclaration)ResolveDeclaration(fm.Declaration, program)
            },
            ConstructorMember cm => ResolveConstructorMember(cm, program),
            DestructorMember dm => dm with
            {
                Destructor = dm.Destructor with { Body = ResolveStatementList(dm.Destructor.Body, program) }
            },
            PropertyMember pm => ResolvePropertyMember(pm, program),
            OperatorMember om => om with { Operator = (OperatorDefinition)ResolveDeclaration(om.Operator, program) },
            EventMember em => em, // Nessuna espressione da risolvere nella definizione dell'evento in questa fase
            _ => member
        };
    }

    private static ConstructorMember ResolveConstructorMember(ConstructorMember cm, BasicProgram program)
    {
        // Enter scope so variable references in the body find parameters registered by the binder
        var pos2 = cm.Constructor.Parameters.FirstOrDefault()?.Position ?? cm.Name.Position;
        program.Symbols.EnterScope($"NEW_{pos2.Line}_{pos2.Column}", SymbolTable.ScopeType.Procedure);
        try
        {
            var resolvedBody = ResolveStatementList(cm.Constructor.Body, program);
            return cm with { Constructor = cm.Constructor with { Body = resolvedBody } };
        }
        finally
        {
            program.Symbols.ExitScope();
        }
    }

    /// <summary>
    /// Risolve una definizione di proprietà, inclusi i suoi accessori (Getter e Setter).
    ///
    /// RESPONSABILITÀ:
    /// 1.  Risoluzione Getter: Se presente, accede al blocco `Getter` della proprietà e risolve
    ///     la lista delle sue istruzioni (`Body`) invocando <see cref="ResolveStatementList"/>.
    /// 2.  Risoluzione Setter: Se presente, accede al blocco `Setter` della proprietà e risolve
    ///     la lista delle sue istruzioni (`Body`).
    ///     Nota: Il parametro implicito `value` del Setter è già stato registrato dal Binder (Fase 2)
    ///     nel corretto scope procedurale, quindi non richiede ulteriore gestione qui.
    /// 3.  Ricostruzione: Crea e restituisce una nuova istanza di <see cref="PropertyMember"/> contenente
    ///     la definizione della proprietà aggiornata con i corpi degli accessori completamente risolti.
    ///
    /// Questo metodo è fondamentale per garantire che il codice all'interno delle proprietà
    /// (che può essere complesso quanto una funzione) venga correttamente collegato ai simboli.
    /// </summary>
    /// <param name="pm">Il membro proprietà da risolvere.</param>
    /// <param name="program">L'istanza del programma.</param>
    /// <returns>Una nuova istanza di <see cref="PropertyMember"/> con i corpi degli accessori risolti.</returns>
    private static PropertyMember ResolvePropertyMember(PropertyMember pm, BasicProgram program)
    {
        var prop = pm.Property;
        var resolvedGetter = prop.Getter != null
            ? new PropertyGetter(Body: ResolveStatementList(prop.Getter.Body, program))
            : null;

        var resolvedSetter = prop.Setter != null
            ? prop.Setter with { Body = ResolveStatementList(prop.Setter.Body, program) }
            : null;

        return pm with { Property = prop with { Getter = resolvedGetter, Setter = resolvedSetter } };
    }

    /// <summary>
    /// Risolve una lista di istruzioni (Statement).
    /// Questo è il "motore" di iterazione per i blocchi di codice.
    /// </summary>
    /// <param name="statements">La lista di statement da risolvere.</param>
    /// <param name="program">L'istanza del programma.</param>
    /// <returns>Una nuova ImmutableArray con gli statement risolti.</returns>
    private static ImmutableArray<Stmt> ResolveStatementList(ImmutableArray<Stmt> statements, BasicProgram program)
    {
        var builder = ImmutableArray.CreateBuilder<Stmt>();
        bool isDead = false;
        foreach (var stmt in statements)
        {
            // Dead-code dopo RETURN/THROW: corretto per il flusso lineare, MA una LabelStmt
            // è un TARGET GOTO/GOSUB (handler ON KEY/MOUSE/WINDOW, GOSUB a label): il codice
            // dopo la label è raggiungibile → la label resetta la deadness.
            if (isDead && stmt is not LabelStmt) continue;
            if (stmt is LabelStmt) isDead = false;
            var resolved = ResolveStatement(stmt, program);
            var lowered = LowerStatement(resolved);
            if (lowered == null) continue;
            builder.Add(lowered);
            if (lowered is ReturnStmt or ThrowStmt)
                isDead = true;
        }
        return builder.ToImmutable();
    }

    /// <summary>Lowering: semplifica l'AST dopo la risoluzione.</summary>
    private static Stmt? LowerStatement(Stmt stmt)
    {
        // Identity: LET x = x → no-op
        if (stmt is LetStmt { Assignments: var assigns } && assigns.Length == 1)
        {
            var a = assigns[0];
            if (a.Value is VariableExpr ve && a.Target is VariableTarget vt && ve.SymbolId == vt.SymbolId)
                return null;
        }
        
        return stmt;
    }







    /// <summary>
    /// Attraversa e risolve ricorsivamente tutti i SymbolId in un singolo statement.
    /// Questo metodo agisce come un dispatcher esaustivo su tutti i tipi di `Stmt` definiti nell'AST.
    /// 
    /// RESPONSABILITÀ:
    /// 1. Identificazione: Determina il tipo esatto dello statement a runtime.
    /// 2. Risoluzione Espressioni: Per ogni proprietà di tipo `Expr`, invoca `ResolveExpression`.
    /// 3. Risoluzione Target: Per ogni proprietà di tipo `Target` (L-value), invoca `ResolveTarget`.
    /// 4. Ricorsione Strutturale: Per statement complessi (AudioAction, DbfScope, SpeedQueries),
    ///    delega a metodi helper specifici per garantire la risoluzione profonda.
    /// 5. Immutabilità: Utilizza l'espressione `with` per restituire una nuova istanza del nodo
    ///    con i figli risolti, mantenendo l'albero originale intatto se necessario (anche se in questa
    ///    fase stiamo sostituendo l'AST).
    /// 
    /// Questo metodo è "blindato": non contiene default impliciti che ignorano i dati. Se un nodo
    /// ha figli risolvibili, essi VENGONO risolti qui.
    /// </summary>
    /// <param name="stmt">Lo statement da risolvere.</param>
    /// <param name="program">L'istanza del programma per l'accesso alla SymbolTable.</param>
    /// <returns>Una nuova istanza dello statement con tutti i riferimenti simbolici risolti.</returns>
    /// <summary>Un gestore evento dichiarato come CALL (nome nudo o `Sub()`) DEVE essere una SUB;
    /// la forma legacy `… GOSUB &lt;label&gt;` / `… GOTO &lt;label&gt;` può puntare a una **label** (la leg C
    /// la accetta e la label viene validata nella fase di risoluzione delle etichette).</summary>
    private static bool RequiresSubHandler(string? actionKind) =>
        (actionKind ?? "CALL").ToUpperInvariant() is "CALL";

    private static bool IsEventSub(BasicProgram program, string name) =>
        program.Symbols.GetAllProcedures().Any(p =>
            p.OriginalName.Lexeme.Equals(name, StringComparison.OrdinalIgnoreCase)
            || p.OriginalName.Lexeme.Equals(name + "$", StringComparison.OrdinalIgnoreCase)
            || p.OriginalName.Lexeme.Equals(name.Replace("_", ""), StringComparison.OrdinalIgnoreCase));

    private static Stmt ResolveStatement(Stmt stmt, BasicProgram program)
    {
        switch (stmt)
        {
            case PixelControlStmt pcs:
                foreach (var cl in pcs.Clauses)
                    if (cl.Key.Equals("ON", StringComparison.OrdinalIgnoreCase)
                        && RequiresSubHandler(cl.ActionKind)
                        && !string.IsNullOrEmpty(cl.SubName ?? cl.LabelName)
                        && !IsEventSub(program, cl.SubName ?? cl.LabelName!))
                        throw new InvalidOperationException($"Evento controllo: il gestore '{cl.SubName ?? cl.LabelName}' deve essere una SUB esistente.");
                return pcs with { Into = pcs.Into != null ? ResolveTarget(pcs.Into, program) : null,
                                  Id = pcs.Id != null ? ResolveTarget(pcs.Id, program) : null };
            case FireEventStmt fes:
                return new FireEventStmt(ResolveExpression(fes.Handle, program), ResolveExpression(fes.Event, program), fes.Token) { ScopeId = fes.ScopeId };
            case ControlEventStmt ces:
                if (RequiresSubHandler(ces.ActionKind)
                    && !string.IsNullOrEmpty(ces.Target) && !IsEventSub(program, ces.Target))
                    throw new InvalidOperationException($"Evento controllo: il gestore '{ces.Target}' deve essere una SUB esistente.");
                return ces with { Id = ResolveExpression(ces.Id, program) };
            case ControlMethodStmt cms:
                // Anche gli ARGOMENTI vanno risolti: prima restavano VariableExpr con SymbolId -1 (solo Into veniva risolto). In un metodo di classe il -1 era trattato
                // come SELF (IL: `ldarg.0` al posto del parametro `g` di `Paint(g AS ANY)` -> `CONTROL DRAW g,...` passava `this`); i locali funzionavano solo per il
                // fallback per nome. Gli argomenti non risolti sfuggivano anche all'inferenza di tipo (i - 1 con tipo diverso dall'emesso).
                return cms with { Args = cms.Args.Select(a => ResolveExpression(a, program)).ToList(),
                                  Into = cms.Into != null ? ResolveTarget(cms.Into, program) : null };

            // ================================================================================
            // --- CORE LANGUAGE & FLOW CONTROL ---
            // ================================================================================
            case LetStmt s:
                return new LetStmt(
                    [
                        ..s.Assignments.Select(a => new Assignment(
                            ResolveTarget(a.Target, program),
                            ResolveExpression(a.Value, program)
                        ))
                    ],
                    s.LetToken
                ) { ScopeId = s.ScopeId };

            case DeclarationStmt s:
                return new DeclarationStmt(s.Keyword,
                    [..s.Declarations.Select(d => (VariableDeclaration)ResolveDeclaration(d, program))]
                ) { ScopeId = s.ScopeId };

            case ConstStmt s:
                return new ConstStmt(s.Keyword,
                    [..s.Declarations.Select(d => (ConstDeclaration)ResolveDeclaration(d, program))]
                ) { ScopeId = s.ScopeId };

            case RedimStmt s:
                return new RedimStmt(
                    ResolveTarget(s.ArrayTarget, program),
                    s.Preserve,
                    [..s.Dimensions.Select(d => ResolveExpression(d, program))],
                    s.Initializer != null ? ResolveExpression(s.Initializer, program) : null,
                    s.RedimToken
                ) { ScopeId = s.ScopeId };

            case IfStmt s:
                return new IfStmt(
                    [
                        ..s.IfClauses.Select(c => new IfClause(
                            ResolveExpression(c.Condition, program),
                            ResolveStatementList(c.Statements, program),
                            c.Token
                        ) { ScopeId = c.ScopeId })
                    ],
                    s.ElseClause != null
                        ? new ElseClause(
                            ResolveStatementList(s.ElseClause.Statements, program),
                            s.ElseClause.Token
                        ) { ScopeId = s.ElseClause.ScopeId }
                        : null,
                    s.Token
                ) { ScopeId = s.ScopeId };

            case ForStmt s:
                return new ForStmt(
                    (VariableTarget)ResolveTarget(s.Variable, program),
                    ResolveExpression(s.Start, program),
                    ResolveExpression(s.End, program),
                    s.Step != null ? ResolveExpression(s.Step, program) : null
                ) { ScopeId = s.ScopeId };
            case ForEachStmt s:
                return new ForEachStmt(
                    s.IteratorVariables,
                    s.DeclaredType,
                    ResolveExpression(s.Collection, program),
                    ResolveStatementList(s.Body, program),
                    s.ForToken
                ) { ScopeId = s.ScopeId };

            case SelectStmt s:
                return new SelectStmt(
                    ResolveExpression(s.Expression, program),
                    [
                        ..s.CaseClauses.Select(c => new CaseClause(
                            [..c.Tests.Select(t => ResolveCaseTest(t, program))],
                            ResolveStatementList(c.Statements, program),
                            c.Token
                        ) { ScopeId = c.ScopeId })
                    ],
                    s.ElseClause != null
                        ? new CaseElseClause(
                            ResolveStatementList(s.ElseClause.Statements, program),
                            s.ElseClause.Token
                        ) { ScopeId = s.ElseClause.ScopeId }
                        : null,
                    s.Token
                ) { ScopeId = s.ScopeId };

            case WithStmt s:
                return new WithStmt(
                    ResolveExpression(s.Expression, program),
                    ResolveStatementList(s.Body, program),
                    s.Token
                ) { ScopeId = s.ScopeId };

            case TryCatchStmt s:
                return new TryCatchStmt(
                    ResolveStatementList(s.TryBody, program),
                    [
                        ..s.CatchClauses.Select(c => new CatchClause(
                            c.Variable, // La variabile è già stata risolta nel Binder
                            c.WhenCondition != null ? ResolveExpression(c.WhenCondition, program) : null,
                            ResolveStatementList(c.Body, program),
                            c.Token
                        ) { ScopeId = c.ScopeId })
                    ],
                    s.Finally != null
                        ? new FinallyClause(
                            ResolveStatementList(s.Finally.Body, program),
                            s.Finally.Token
                        ) { ScopeId = s.Finally.ScopeId }
                        : null,
                    s.Token
                ) { ScopeId = s.ScopeId };

            case ExpressionStmt s:
                return new ExpressionStmt(ResolveExpression(s.Expression, program)) { ScopeId = s.ScopeId };

            case SemanticPatchStmt sp:
                // §4.10 write-side: risolvi il corpo del patch (l'apply avviene in ApplySemanticPatches).
                return sp with { Body = [.. sp.Body.Select(b => ResolveStatement(b, program))] };

            case ReturnStmt s:
                return new ReturnStmt(
                    s.Value != null ? ResolveExpression(s.Value, program) : null,
                    s.Token
                ) { ScopeId = s.ScopeId };

            case ConditionalJumpStmt s:
                return new ConditionalJumpStmt(
                    ResolveExpression(s.Condition, program),
                    s.InitialTargetLabel,
                    s.JumpWhenTrue
                ) { TargetLabel = s.TargetLabel, ScopeId = s.ScopeId };

            case OnJumpStmt s:
                return new OnJumpStmt(
                    ResolveExpression(s.IndexExpression, program),
                    s.Type,
                    s.TargetLabels,
                    s.Token
                ) { ScopeId = s.ScopeId };

            case ErrorStmt s:
                return new ErrorStmt(
                    ResolveExpression(s.ErrorCode, program),
                    s.Description != null ? ResolveExpression(s.Description, program) : null,
                    s.Token
                ) { ScopeId = s.ScopeId };

            case SwapStmt s:
                return new SwapStmt(
                    ResolveTarget(s.Left, program),
                    ResolveTarget(s.Right, program),
                    s.Token
                ) { ScopeId = s.ScopeId };

            case RandomizeStmt s:
                return new RandomizeStmt(
                    s.Seed != null ? ResolveExpression(s.Seed, program) : null,
                    s.Token
                ) { ScopeId = s.ScopeId };

            case SystemShellStmt s:
                return new SystemShellStmt(ResolveExpression(s.Command, program), s.Token) { ScopeId = s.ScopeId };

            case RunStmt s:
                return new RunStmt(ResolveExpression(s.Command, program), s.Token) { ScopeId = s.ScopeId };

            case BaseConstructorCallStmt s:
                return new BaseConstructorCallStmt(
                    [..s.Args.Select(a => ResolveExpression(a, program))],
                    s.BaseToken
                ) { ScopeId = s.ScopeId };

            case LocalTypeStmt s:
            {
                // Il Definition può NON essere un TYPE (a livello di MODULO il codice top-level
                // viene parsato dentro un Main implicito → `_functionContextStack` non vuoto):
                // il cast incondizionato a UserDefinedType lancia InvalidCastException su un
                // `DIM ... AS <UDT>`/VariableDeclaration (es. sample top-level con TYPE + USING).
                var resolvedDefinition = ResolveDeclaration(s.Definition, program);
                if (resolvedDefinition is not UserDefinedType udtLocal)
                    return resolvedDefinition is Stmt stmtLocal ? stmtLocal : s;
                return new LocalTypeStmt(udtLocal) { ScopeId = s.ScopeId };
            }

            case KeyGenStmt s:
                return new KeyGenStmt(
                    ResolveExpression(s.Algorithm, program),
                    ResolveTarget(s.PrivateKeyHandle, program),
                    ResolveTarget(s.PublicKeyHandle, program),
                    s.KeySize != null ? ResolveExpression(s.KeySize, program) : null,
                    s.Token
                ) { ScopeId = s.ScopeId };
            case KeyCloseStmt s:
                return new KeyCloseStmt(ResolveExpression(s.KeyHandle, program), s.Token) { ScopeId = s.ScopeId };

            case OnKeyStmt s:
                return new OnKeyStmt(
                    s.KeyExpression != null ? ResolveExpression(s.KeyExpression, program) : null,
                    ResolveOnKeyAction(s.Action, program),
                    s.OnToken
                ) { ScopeId = s.ScopeId };

            case MacroStmt s:
                return new MacroStmt(ResolveExpression(s.Expression, program), s.Token) { ScopeId = s.ScopeId };

            case YieldStmt s:
                return new YieldStmt(ResolveExpression(s.ValueToYield, program), s.Token) { ScopeId = s.ScopeId };

            case LocalProcedureStmt s:
                // Risolviamo il corpo della procedura locale
                return new LocalProcedureStmt((ProcedureDefinition)ResolveDeclaration(s.Procedure, program))
                    { ScopeId = s.ScopeId };

            case ForEachInitStmt s:
                return s with { CollectionExpr = ResolveExpression(s.CollectionExpr, program) };

            // ================================================================================
            // --- STATEMENT SENZA ESPRESSIONI O TARGET FIGLI DA RISOLVERE ---
            // ================================================================================
            case GotoStmt:
            case GosubStmt:
            case JumpStmt:
            case StopStmt:
            case EndStmt:
            case RemStmt:
            case OnErrorResumeStmt:
            case ResumeStmt:
            case LabelStmt:
            case EnterLoopStmt:
            case ExitLoopStmt:
            case ContinueStmt:
            case ExitStmt:
            case ForEachAdvanceAndAssignStmt:
            case ForCheckAndJumpStmt:
            case NextStmt:
            case WaitKeyStmt:
            case EvaluateIndicesStmt:
            case AmbiguousMethodStmt:
            case ImportStmt:
            case RequireStmt:
            case EndProcedureStmt:
            case UnloadResourceStmt:
            case DrawTextStmt:
                // Questi nodi non hanno espressioni o target figli da risolvere ricorsivamente.
                // Restituiamo il nodo originale, che ha già lo ScopeId timbrato dal Binder.
                return stmt;

            // ================================================================================
            // --- ERROR COMMANDS ---
            // ================================================================================
            case OnErrorStmt s:
                return s with 
                { 
                    ProcedureCall = s.ProcedureCall != null ? ResolveExpression(s.ProcedureCall, program) : null 
                };

            case RethrowStmt:
                return stmt; // Nessun SymbolId da risolvere

            case ThrowStmt s:
                return new ThrowStmt(ResolveExpression(s.Value, program), s.Token) { ScopeId = s.ScopeId };
            
            // ================================================================================
            // --- EVENT COMMANDS ---
            // ================================================================================
            case OnMouseStmt s:
                return new OnMouseStmt(
                    s.EventKind,
                    ResolveOnKeyAction(s.Action, program),
                    s.Token
                ) { ScopeId = s.ScopeId };

            case WindowPumpStmt wp:
                return new WindowPumpStmt(ResolveExpression(wp.Ms, program), wp.Token) { ScopeId = wp.ScopeId };
            case OnWindowStmt s:
            {
                string? wname = s.Action switch
                {
                    GosubAction g2 => g2.TargetLabel.Lexeme,
                    GotoAction go2 => go2.TargetLabel.Lexeme,
                    CallAction ca2 when ca2.Call is FunctionExpr fe2 => fe2.OriginalToken.Lexeme,
                    _ => null
                };
                /* `CALL` (nome nudo o `Sub()`) → SUB esistente. `GOSUB`/`GOTO` → label ammessa
                 * (parità con la leg C: `ON WINDOW CLOSE GOSUB WinClosed`, forma legacy Clipper). */
                if (wname != null && s.Action is CallAction && !IsEventSub(program, wname))
                    throw new InvalidOperationException($"Evento finestra: il gestore '{wname}' deve essere una SUB esistente.");
                return new OnWindowStmt(
                    s.EventKind,
                    ResolveOnKeyAction(s.Action, program),
                    s.Token,
                    s.Of != null ? ResolveExpression(s.Of, program) : null
                ) { ScopeId = s.ScopeId };
            }

            case OnSpriteCollisionStmt s:
                // ON SPRITE COLLISION(S): risolve il nome dell'handler (deve essere una
                // procedura esistente) — il dispatch avviene nel SYNC via ADD_HANDLER.
                return new OnSpriteCollisionStmt(s.HandlerName, s.IsPlural, s.Token) { ScopeId = s.ScopeId };

            case RaiseEventStmt s:
                return new RaiseEventStmt(
                    s.EventName, // Il nome dell'evento è un token, verrà risolto a runtime o validato dopo
                    [.. s.Arguments.Select(a => ResolveExpression(a, program))],
                    s.Token
                ) { ScopeId = s.ScopeId };

            case AddHandlerStmt s:
                return new AddHandlerStmt(
                    ResolveExpression(s.EventAccess, program),
                    ResolveExpression(s.HandlerExpr, program),
                    s.Token
                ) { ScopeId = s.ScopeId };

            case RemoveHandlerStmt s:
                return new RemoveHandlerStmt(
                    ResolveExpression(s.EventAccess, program),
                    ResolveExpression(s.HandlerExpr, program),
                    s.Token
                ) { ScopeId = s.ScopeId };

            // ================================================================================
            // --- MOUSE COMMANDS ---
            // ================================================================================
            case MouseConfigStmt:
            case MouseVisibilityStmt:
            case MouseWaitStmt:
                // Nessuna espressione figlia da risolvere
                return stmt;

            case MouseSetStmt s:
                return new MouseSetStmt(
                    ResolveExpression(s.X, program),
                    ResolveExpression(s.Y, program),
                    s.Token
                ) { ScopeId = s.ScopeId };

            case MouseLimitStmt s:
                return new MouseLimitStmt(
                    s.X1 != null ? ResolveExpression(s.X1, program) : null,
                    s.Y1 != null ? ResolveExpression(s.Y1, program) : null,
                    s.X2 != null ? ResolveExpression(s.X2, program) : null,
                    s.Y2 != null ? ResolveExpression(s.Y2, program) : null,
                    s.Token
                ) { ScopeId = s.ScopeId };

            case MouseCursorStmt s:
                return new MouseCursorStmt(
                    ResolveExpression(s.CursorId, program),
                    s.Token
                ) { ScopeId = s.ScopeId };

            // ================================================================================
            // --- CONSOLE & INPUT ---
            // ================================================================================
            case PrintStmt s:
                return new PrintStmt(
                    [
                        ..s.Arguments.Select(a => a.Expression != null
                            ? a with { Expression = ResolveExpression(a.Expression, program) }
                            : a)
                    ],
                    s.Token
                ) { ScopeId = s.ScopeId, SourceLineIndex = s.SourceLineIndex };

            case ConsoleSendKeyStmt s:
                return new ConsoleSendKeyStmt(
                    ResolveExpression(s.KeysExpression, program),
                    s.Token
                ) { ScopeId = s.ScopeId };

            case ConsoleScrollModeStmt s:
                return new ConsoleScrollModeStmt(
                    ResolveExpression(s.ModeExpression, program),
                    s.Token
                ) { ScopeId = s.ScopeId };

            case ConsoleCaptionStmt s:
                return new ConsoleCaptionStmt(
                    ResolveExpression(s.TitleExpression, program),
                    s.Token
                ) { ScopeId = s.ScopeId };

            case InputStmt s:
                return new InputStmt(
                    [..s.Variables.Select(v => ResolveTarget(v, program))],
                    s.Prompt != null ? ResolveExpression(s.Prompt, program) : null,
                    s.TimeoutExpr != null ? ResolveExpression(s.TimeoutExpr, program) : null,
                    s.MaskExpr != null ? ResolveExpression(s.MaskExpr, program) : null,
                    s.Token
                ) { ScopeId = s.ScopeId };

            case LineInputStmt s:
                return new LineInputStmt(
                    s.Prompt != null ? ResolveExpression(s.Prompt, program) : null,
                    ResolveTarget(s.Variable, program),
                    s.TimeoutExpr != null ? ResolveExpression(s.TimeoutExpr, program) : null,
                    s.MaskExpr != null ? ResolveExpression(s.MaskExpr, program) : null,
                    s.Token
                ) { ScopeId = s.ScopeId };

            case ReadStmt s:
                return new ReadStmt(
                    [..s.Variables.Select(v => ResolveTarget(v, program))],
                    s.Token
                ) { ScopeId = s.ScopeId };

            case DataStmt s:
                return new DataStmt(
                    [..s.Values.Select(v => ResolveExpression(v, program))],
                    s.Token
                ) { ScopeId = s.ScopeId };

            case RestoreStmt s:
                return s; // Target è un Token (label), non un'espressione

            case GetKeyboardStmt s:
                return new GetKeyboardStmt(ResolveTarget(s.Variable, program), s.Token) { ScopeId = s.ScopeId };

            case InkeyStmt s:
                return new InkeyStmt(
                    ResolveTarget(s.Variable, program),
                    s.TimeoutExpr != null ? ResolveExpression(s.TimeoutExpr, program) : null,
                    s.Token
                ) { ScopeId = s.ScopeId };

            case ClsStmt s:
                return new ClsStmt(
                    s.Color != null ? ResolveExpression(s.Color, program) : null,
                    s.Token
                ) { ScopeId = s.ScopeId };

            case ColorStmt s:
                return new ColorStmt(
                    s.Foreground != null ? ResolveExpression(s.Foreground, program) : null,
                    s.Background != null ? ResolveExpression(s.Background, program) : null,
                    s.Token
                ) { ScopeId = s.ScopeId };

            case LocateStmt s:
                return new LocateStmt(
                    ResolveExpression(s.LineExpr, program),
                    ResolveExpression(s.ColumnExpr, program),
                    s.Token
                ) { ScopeId = s.ScopeId };

            case CLocateStmt s:
                return new CLocateStmt(
                    ResolveExpression(s.LineExpr, program),
                    ResolveExpression(s.ColumnExpr, program),
                    s.Token
                ) { ScopeId = s.ScopeId };

            case CPrintStmt s:
                return new CPrintStmt(
                    [
                        ..s.Arguments.Select(a => a.Expression != null
                            ? a with { Expression = ResolveExpression(a.Expression, program) }
                            : a)
                    ],
                    s.Token
                ) { ScopeId = s.ScopeId, SourceLineIndex = s.SourceLineIndex };

            case CCLSStmt s:
                return new CCLSStmt(
                    s.Color != null ? ResolveExpression(s.Color, program) : null,
                    s.Token
                ) { ScopeId = s.ScopeId };

            case CSaveScreenStmt s:
                return new CSaveScreenStmt(ResolveExpression(s.Name, program), s.Token) { ScopeId = s.ScopeId };

            case CRestoreScreenStmt s:
                return new CRestoreScreenStmt(ResolveExpression(s.Name, program), s.Token) { ScopeId = s.ScopeId };

            case ClearRectStmt s:
                return new ClearRectStmt(
                    ResolveExpression(s.X1, program),
                    ResolveExpression(s.Y1, program),
                    ResolveExpression(s.X2, program),
                    ResolveExpression(s.Y2, program),
                    s.Token
                ) { ScopeId = s.ScopeId };

            case SetAttributeStmt:
            case SetClipperStmt:
            case SetThemeStmt:
            case SetDateFormatStmt:
            case CursorStateStmt:
                return stmt;

            case CursorShapeStmt s:
                return new CursorShapeStmt(ResolveExpression(s.Shape, program), s.Token) { ScopeId = s.ScopeId };

            case ClipStmt s:
                return new ClipStmt(
                    s.Command,
                    s.Argument != null ? ResolveExpression(s.Argument, program) : null,
                    s.Destination != null ? ResolveTarget(s.Destination, program) : null,
                    s.Token
                ) { ScopeId = s.ScopeId };

            // ================================================================================
            // --- FILE I/O ---
            // ================================================================================
            case OpenStmt s:
            {
                var resolvedFileName = ResolveExpression(s.FileName, program, false);
                var resolvedFileNumber = ResolveExpression(s.FileNumber, program, false);
                var resolvedRecordLength =
                    s.RecordLength != null ? ResolveExpression(s.RecordLength, program, false) : null;
                var resolvedEncoding =
                    s.EncodingExpr != null ? ResolveExpression(s.EncodingExpr, program, false) : null;

                return new OpenStmt(resolvedFileName, s.Mode, s.Locking, s.LockMode, s.Access, resolvedFileNumber,
                    s.FileContentType, resolvedRecordLength, s.IndexedBy, s.IndexPaths, resolvedEncoding, s.IsUnsafe,
                    s.Token) { ScopeId = s.ScopeId };
            }
            case CloseStmt s:
                return new CloseStmt(
                    [..s.FileNumbers.Select(f => ResolveExpression(f, program))],
                    s.Token,
                    s.IsIsam
                ) { ScopeId = s.ScopeId };
            case PrintFileStmt s:
                return new PrintFileStmt(
                    ResolveExpression(s.FileNumber, program),
                    [
                        ..s.Arguments.Select(a => a.Expression != null
                            ? a with { Expression = ResolveExpression(a.Expression, program) }
                            : a)
                    ],
                    s.Token
                ) { ScopeId = s.ScopeId };
            case WriteFileStmt s:
                return new WriteFileStmt(
                    ResolveExpression(s.FileNumber, program),
                    [..s.Arguments.Select(a => ResolveExpression(a, program))],
                    s.Token
                ) { ScopeId = s.ScopeId };
            case InputFileStmt s:
                return new InputFileStmt(
                    ResolveExpression(s.FileNumber, program),
                    [..s.Variables.Select(v => ResolveTarget(v, program))],
                    s.Token
                ) { ScopeId = s.ScopeId };
            case LineInputFileStmt s:
                return new LineInputFileStmt(
                    ResolveExpression(s.FileNumber, program),
                    ResolveTarget(s.Variable, program),
                    s.Token
                ) { ScopeId = s.ScopeId };
            case GetFileStmt s:
                return new GetFileStmt(
                    ResolveExpression(s.FileNumber, program),
                    s.RecordNumber != null ? ResolveExpression(s.RecordNumber, program) : null,
                    ResolveTarget(s.Destination, program),
                    s.Token
                ) { ScopeId = s.ScopeId, IsIsam = s.IsIsam };
            case PutFileStmt s:
                return new PutFileStmt(
                    ResolveExpression(s.FileNumber, program),
                    s.RecordNumber != null ? ResolveExpression(s.RecordNumber, program) : null,
                    ResolveExpression(s.Source, program),
                    s.IsUnsafe,
                    s.Token
                ) { ScopeId = s.ScopeId };
            case SeekStmt s:
                return new SeekStmt(
                    ResolveExpression(s.FileNumber, program),
                    ResolveExpression(s.PositionExpr, program),
                    s.Token
                ) { ScopeId = s.ScopeId };
            case LockStmt s:
                return new LockStmt(
                    ResolveExpression(s.FileNumber, program),
                    s.RecordNumber != null ? ResolveExpression(s.RecordNumber, program) : null,
                    s.Token
                ) { ScopeId = s.ScopeId };
            case UnlockStmt s:
                return new UnlockStmt(
                    ResolveExpression(s.FileNumber, program),
                    s.RecordNumber != null ? ResolveExpression(s.RecordNumber, program) : null,
                    s.Token
                ) { ScopeId = s.ScopeId };
            case IoWatchStmt s:
                return new IoWatchStmt(
                    ResolveExpression(s.Path, program),
                    ResolveExpression(s.EventsMask, program),
                    s.Callback,
                    s.Token
                ) { ScopeId = s.ScopeId };
            case IoUnwatchStmt s:
                return new IoUnwatchStmt(
                    ResolveExpression(s.Path, program),
                    s.Token
                ) { ScopeId = s.ScopeId };
            case KillStmt s:
                return new KillStmt(ResolveExpression(s.FileName, program), s.Token) { ScopeId = s.ScopeId };
            case RenameStmt s:
                return new RenameStmt(
                    ResolveExpression(s.OldFileName, program),
                    ResolveExpression(s.NewFileName, program),
                    s.Token
                ) { ScopeId = s.ScopeId };
            case MkDirStmt s:
                return new MkDirStmt(ResolveExpression(s.DirectoryPath, program), s.Token) { ScopeId = s.ScopeId };
            case RmDirStmt s:
                return new RmDirStmt(
                    ResolveExpression(s.DirectoryPath, program),
                    s.RecursiveExpr != null ? ResolveExpression(s.RecursiveExpr, program) : null,
                    s.Token
                ) { ScopeId = s.ScopeId };
            case ChDirStmt s:
                return new ChDirStmt(ResolveExpression(s.DirectoryPath, program), s.Token) { ScopeId = s.ScopeId };
            case CopyStmt s:
                return new CopyStmt(
                    ResolveExpression(s.SourceFile, program),
                    ResolveExpression(s.DestinationFile, program),
                    s.Token
                ) { ScopeId = s.ScopeId };
            case ZipStmt s:
                return new ZipStmt(ResolveExpression(s.Source, program), ResolveExpression(s.Destination, program), s.Token) { ScopeId = s.ScopeId };
            case UnzipStmt s:
                return new UnzipStmt(ResolveExpression(s.Archive, program), ResolveExpression(s.Destination, program), s.Token) { ScopeId = s.ScopeId };
            case MoveStmt s:
                return new MoveStmt(
                    ResolveExpression(s.SourceFile, program),
                    ResolveExpression(s.DestinationFile, program),
                    s.Token
                ) { ScopeId = s.ScopeId };
            case XCopyStmt s:
                return new XCopyStmt(
                    ResolveExpression(s.SourceDirectory, program),
                    ResolveExpression(s.DestinationDirectory, program),
                    s.RecursiveExpr != null ? ResolveExpression(s.RecursiveExpr, program) : null,
                    s.OverwriteExpr != null ? ResolveExpression(s.OverwriteExpr, program) : null,
                    s.Token
                ) { ScopeId = s.ScopeId };

            // ================================================================================
            // --- MEMORY MAP ---
            // ================================================================================

            case MemoryMapStmt s:
                return new MemoryMapStmt(
                    ResolveExpression(s.FilePath, program),
                    ResolveTarget(s.Handle, program),
                    s.Token
                ) { ScopeId = s.ScopeId };

            case MemoryMapPokeStmt s:
                return new MemoryMapPokeStmt(
                    ResolveExpression(s.Handle, program),
                    ResolveExpression(s.Offset, program),
                    ResolveExpression(s.Value, program),
                    s.Token
                ) { ScopeId = s.ScopeId };

            case MemoryMapWriteBytesStmt s:
                return new MemoryMapWriteBytesStmt(
                    ResolveExpression(s.Handle, program),
                    ResolveExpression(s.Offset, program),
                    ResolveExpression(s.SourceArray, program),
                    s.Count != null ? ResolveExpression(s.Count, program) : null,
                    s.Token
                ) { ScopeId = s.ScopeId };

            // ================================================================================
            // --- AUDIO & GRAPHICS ---
            // ================================================================================
            case SoundStmt s:
                return new SoundStmt(
                    ResolveExpression(s.Frequency, program),
                    ResolveExpression(s.Duration, program),
                    s.Volume != null ? ResolveExpression(s.Volume, program) : null,
                    s.Waveform != null ? ResolveExpression(s.Waveform, program) : null,
                    s.Attack != null ? ResolveExpression(s.Attack, program) : null,
                    s.Decay != null ? ResolveExpression(s.Decay, program) : null,
                    s.Sustain != null ? ResolveExpression(s.Sustain, program) : null,
                    s.Release != null ? ResolveExpression(s.Release, program) : null,
                    s.Voice != null ? ResolveExpression(s.Voice, program) : null,
                    s.PulseWidth != null ? ResolveExpression(s.PulseWidth, program) : null,
                    s.Token
                ) { ScopeId = s.ScopeId };

            case PlayStmt s:
                return new PlayStmt(ResolveAudioAction(s.Action, program), s.Token) { ScopeId = s.ScopeId };

            case AudioControlStmt s:
                return new AudioControlStmt(
                    s.Command,
                    s.Action,
                    s.Argument != null ? ResolveExpression(s.Argument, program) : null
                ) { ScopeId = s.ScopeId };

            case SoundLoadStmt sl:
                return new SoundLoadStmt(
                    ResolveExpression(sl.FilePath, program),
                    ResolveTarget(sl.Handle, program),
                    sl.Token) { ScopeId = sl.ScopeId };

            case SoundPlayStmt sp:
                return new SoundPlayStmt(
                    ResolveExpression(sp.Handle, program),
                    sp.Channel != null ? ResolveExpression(sp.Channel, program) : null,
                    sp.Loops != null ? ResolveExpression(sp.Loops, program) : null,
                    sp.Token) { ScopeId = sp.ScopeId };

            case SoundVolumeStmt sv:
                return new SoundVolumeStmt(
                    ResolveExpression(sv.Volume, program),
                    sv.Channel != null ? ResolveExpression(sv.Channel, program) : null,
                    sv.Token) { ScopeId = sv.ScopeId };

            case SetFilterStmt s:
                return new SetFilterStmt(
                    ResolveExpression(s.Voice, program),
                    ResolveExpression(s.Type, program),
                    ResolveExpression(s.Cutoff, program),
                    ResolveExpression(s.Resonance, program),
                    s.Token
                ) { ScopeId = s.ScopeId };

            case CircleStmt s:
                return new CircleStmt(
                    ResolveExpression(s.X, program),
                    ResolveExpression(s.Y, program),
                    ResolveExpression(s.Radius, program),
                    s.Color != null ? ResolveExpression(s.Color, program) : null,
                    s.Start != null ? ResolveExpression(s.Start, program) : null,
                    s.End != null ? ResolveExpression(s.End, program) : null,
                    s.Aspect != null ? ResolveExpression(s.Aspect, program) : null,
                    s.Token
                ) { ScopeId = s.ScopeId };

            case DrawStmt s:
                return new DrawStmt(ResolveExpression(s.CommandString, program), s.Token) { ScopeId = s.ScopeId };

            case FontStmt s:
                return new FontStmt(
                    ResolveExpression(s.FontPath, program),
                    ResolveExpression(s.Size, program),
                    s.Token
                ) { ScopeId = s.ScopeId };
            case LineStmt s:
                return new LineStmt(
                    s.X1 != null ? ResolveExpression(s.X1, program) : null,
                    s.Y1 != null ? ResolveExpression(s.Y1, program) : null,
                    ResolveExpression(s.X2, program),
                    ResolveExpression(s.Y2, program),
                    s.Color != null ? ResolveExpression(s.Color, program) : null,
                    s.Style != null ? ResolveExpression(s.Style, program) : null,
                    s.Token
                ) { ScopeId = s.ScopeId };

            case PaintStmt s:
                return new PaintStmt(
                    ResolveExpression(s.X, program),
                    ResolveExpression(s.Y, program),
                    s.FillColor != null ? ResolveExpression(s.FillColor, program) : null,
                    s.BorderColor != null ? ResolveExpression(s.BorderColor, program) : null,
                    s.Token
                ) { ScopeId = s.ScopeId };

            case PCopyStmt s:
                return new PCopyStmt(
                    ResolveExpression(s.SourcePage, program),
                    ResolveExpression(s.DestinationPage, program),
                    s.Token
                ) { ScopeId = s.ScopeId };

            case PresetStmt s:
                return new PresetStmt(
                    ResolveExpression(s.X, program),
                    ResolveExpression(s.Y, program),
                    s.Token
                ) { ScopeId = s.ScopeId };

            case PSetStmt s:
                return new PSetStmt(
                    ResolveExpression(s.X, program),
                    ResolveExpression(s.Y, program),
                    s.Color != null ? ResolveExpression(s.Color, program) : null,
                    s.Token
                ) { ScopeId = s.ScopeId };

            case ScreenStmt s:
                return new ScreenStmt(
                    ResolveExpression(s.Mode, program),
                    s.ActivePage != null ? ResolveExpression(s.ActivePage, program) : null,
                    s.VisiblePage != null ? ResolveExpression(s.VisiblePage, program) : null,
                    s.CloseRepl != null ? ResolveExpression(s.CloseRepl, program) : null,
                    s.Token, null, s.CoordMode
                ) { ScopeId = s.ScopeId };

            case TextModeStmt tms:
                return new TextModeStmt(ResolveExpression(tms.Mode, program), tms.Token) { ScopeId = tms.ScopeId };

            case ScreenSetStmt s:
                return new ScreenSetStmt(
                    s.VisiblePage != null ? ResolveExpression(s.VisiblePage, program) : null,
                    s.ActivePage != null ? ResolveExpression(s.ActivePage, program) : null,
                    s.Token
                ) { ScopeId = s.ScopeId };

            case PixelBufferStmt s:
                return new PixelBufferStmt(
                    ResolveExpression(s.Width, program),
                    ResolveExpression(s.Height, program),
                    ResolveExpression(s.Buffer, program),
                    s.Token
                ) { ScopeId = s.ScopeId };

            case ImageGetStmt s:
                return new ImageGetStmt(
                    ResolveExpression(s.X1, program),
                    ResolveExpression(s.Y1, program),
                    ResolveExpression(s.X2, program),
                    ResolveExpression(s.Y2, program),
                    ResolveTarget(s.Array, program),
                    s.Token
                ) { ScopeId = s.ScopeId };

            case ImagePutStmt s:
                return new ImagePutStmt(
                    ResolveExpression(s.X, program),
                    ResolveExpression(s.Y, program),
                    ResolveTarget(s.Array, program),
                    s.Mode != null ? ResolveExpression(s.Mode, program) : null,
                    s.Token
                ) { ScopeId = s.ScopeId };

            case SaveScreenStmt s:
                return new SaveScreenStmt(ResolveExpression(s.Name, program), s.Token) { ScopeId = s.ScopeId };

            case RestoreScreenStmt s:
                return new RestoreScreenStmt(ResolveExpression(s.Name, program), s.Token) { ScopeId = s.ScopeId };

            case PointStmt s:
                return new PointStmt(
                    ResolveExpression(s.X, program),
                    ResolveExpression(s.Y, program),
                    s.Color != null ? ResolveExpression(s.Color, program) : null,
                    s.Token
                ) { ScopeId = s.ScopeId };

            case BSaveStmt s:
                return new BSaveStmt(
                    ResolveExpression(s.X1, program),
                    ResolveExpression(s.Y1, program),
                    ResolveExpression(s.X2, program),
                    ResolveExpression(s.Y2, program),
                    ResolveTarget(s.Buffer, program),
                    s.Token
                ) { ScopeId = s.ScopeId };

            case BLoadStmt s:
                return new BLoadStmt(
                    ResolveExpression(s.X, program),
                    ResolveExpression(s.Y, program),
                    ResolveExpression(s.Buffer, program),
                    s.Token
                ) { ScopeId = s.ScopeId };

            case PaletteStmt s:
                return new PaletteStmt(
                    ResolveExpression(s.Index, program),
                    ResolveExpression(s.R, program),
                    ResolveExpression(s.G, program),
                    ResolveExpression(s.B, program),
                    s.Token
                ) { ScopeId = s.ScopeId };

            case PaletteUseStmt s:
                return new PaletteUseStmt(
                    ResolveExpression(s.Name, program),
                    s.Token
                ) { ScopeId = s.ScopeId };

            case PaletteDefineStmt s:
                return new PaletteDefineStmt(
                    ResolveExpression(s.Name, program),
                    ResolveExpression(s.Colors, program),
                    s.Token
                ) { ScopeId = s.ScopeId };

            case PaletteReplaceStmt s:
                return new PaletteReplaceStmt(
                    ResolveExpression(s.Name, program),
                    ResolveExpression(s.Colors, program),
                    s.Token
                ) { ScopeId = s.ScopeId };

            case PaletteSaveStmt s:
                return new PaletteSaveStmt(
                    ResolveExpression(s.FileName, program),
                    s.Token
                ) { ScopeId = s.ScopeId };

            case PaletteLoadStmt s:
                return new PaletteLoadStmt(
                    ResolveExpression(s.FileName, program),
                    s.Token
                ) { ScopeId = s.ScopeId };

            case ImagePutsStmt s:
                return new ImagePutsStmt(
                    ResolveTarget(s.SpriteArray, program, s.ScopeId),
                    ResolveTarget(s.PositionsArray, program, s.ScopeId),
                    s.Mode != null ? ResolveExpression(s.Mode, program) : null,
                    s.Token
                ) { ScopeId = s.ScopeId };

            case LoadTextureStmt s:
                return new LoadTextureStmt(
                    ResolveTarget(s.Destination, program, s.ScopeId),
                    ResolveExpression(s.FileName, program),
                    s.CellWidth != null ? ResolveExpression(s.CellWidth, program) : null,
                    s.CellHeight != null ? ResolveExpression(s.CellHeight, program) : null,
                    s.Token
                ) { ScopeId = s.ScopeId };
            
            case PolygonStmt s:
                return new PolygonStmt(
                    ResolveTarget(s.PointsArray, program, s.ScopeId),
                    ResolveExpression(s.Color, program),
                    s.Token
                ) { ScopeId = s.ScopeId };

            case DrawTextureStmt s:
                return new DrawTextureStmt(
                    ResolveExpression(s.TextureId, program),
                    ResolveExpression(s.X, program),
                    ResolveExpression(s.Y, program),
                    s.Rotation != null ? ResolveExpression(s.Rotation, program) : null,
                    s.ScaleX != null ? ResolveExpression(s.ScaleX, program) : null,
                    s.ScaleY != null ? ResolveExpression(s.ScaleY, program) : null,
                    s.Token
                ) { ScopeId = s.ScopeId };

            case DrawTexturesStmt s:
            {
                // Modalità Oggetto Sprite (nuova)
                if (s.SpritesArray != null)
                {
                    var rewritten = s with
                    {
                        SpritesArray = ResolveTarget(s.SpritesArray, program, s.ScopeId),
                        TextureId = null, 
                        XArray = null,
                        YArray = null,
                        RotArray = null,
                        ScaleArray = null
                    };
                    return rewritten with { ScopeId = s.ScopeId };
                }
                
                // Modalità Array Paralleli (legacy)
                if (s.TextureId != null && s.XArray != null && s.YArray != null && s.RotArray != null && s.ScaleArray != null)
                {
                    var rewritten = s with
                    {
                        SpritesArray = null,
                        TextureId = ResolveExpression(s.TextureId, program),
                        XArray = ResolveTarget(s.XArray, program, s.ScopeId),
                        YArray = ResolveTarget(s.YArray, program, s.ScopeId),
                        RotArray = ResolveTarget(s.RotArray, program, s.ScopeId),
                        ScaleArray = ResolveTarget(s.ScaleArray, program, s.ScopeId)
                    };
                    return rewritten with { ScopeId = s.ScopeId };
                }
                
                // Se il nodo è malformato, restituiscilo così com'è.
                return s;
            }
            
            case SpriteCommandStmt s:
                Log(
                    $"[RESOLVER-TRACE] SpriteCommand '{s.Command.Lexeme}' in Scope {s.ScopeId}. Risoluzione argomenti...",
                    ResolverLog);
                var resolvedArgs = s.Arguments.Select(a =>
                {
                    var res = ResolveExpression(a, program);
                    if (a is ArrayAccessExpr aae)
                        Log(
                            $"[RESOLVER-TRACE]   Arg ArrayAccess '{aae.NameSuffixed.Lexeme}': ID {aae.SymbolId} -> " +
                            $"{((ArrayAccessExpr)res).SymbolId}", ResolverLog);
                    return res;
                }).ToImmutableArray();
                return new SpriteCommandStmt(s.Command, resolvedArgs, s.Token) { ScopeId = s.ScopeId };

            case SyncStmt: return stmt;
            case TeamCreateStmt: return stmt;
            case TeamAddAgentStmt: return stmt;
            case TeamRemoveAgentStmt: return stmt;
            case TeamMessageStmt s:
                return new TeamMessageStmt(s.TeamName, ResolveExpression(s.Message, program), s.Token) { ScopeId = s.ScopeId };
            case TeamRunStmt s:
                return new TeamRunStmt(s.TeamName, ResolveExpression(s.Task, program), s.Token) { ScopeId = s.ScopeId };
            case TeamBroadcastStmt s:
                return new TeamBroadcastStmt(s.TeamName, ResolveExpression(s.Message, program), s.Token) { ScopeId = s.ScopeId };
            case TeamRedefineStmt s:
                return new TeamRedefineStmt(s.TeamName, ResolveExpression(s.Definition, program), s.Token) { ScopeId = s.ScopeId };
            case TeamSetRoleStmt: return stmt;
            case TeamSetStrategyStmt: return stmt;
            case TeamSetCoordinatorStmt: return stmt;
            case TeamSetRecursionStmt s:
                return new TeamSetRecursionStmt(s.TeamName, ResolveExpression(s.Rounds, program), s.Token) { ScopeId = s.ScopeId };
            case TeamSetLatentStmt: return stmt;
            case TeamSetLinkStmt s2:
                return new TeamSetLinkStmt(s2.TeamName, s2.Which, ResolveExpression(s2.Config, program), s2.Token) { ScopeId = s2.ScopeId };
            case TeamSetThoughtsStmt s3:
                return new TeamSetThoughtsStmt(s3.TeamName, ResolveExpression(s3.Count, program), s3.Token) { ScopeId = s3.ScopeId };
            case TeamTrainInnerStmt s4:
                return new TeamTrainInnerStmt(s4.TeamName, ResolveExpression(s4.Agent, program), ResolveExpression(s4.Dataset, program), s4.Token) { ScopeId = s4.ScopeId };
            case TeamTrainOuterStmt s5:
                return new TeamTrainOuterStmt(s5.TeamName, ResolveExpression(s5.Dataset, program), s5.Token) { ScopeId = s5.ScopeId };
            case TeamSetDagStmt sd:
                return new TeamSetDagStmt(sd.TeamName, ResolveExpression(sd.DMax, program), ResolveExpression(sd.LMax, program), ResolveExpression(sd.NMax, program), sd.Token) { ScopeId = sd.ScopeId };
            case TeamSetComplexityStmt scx:
                return new TeamSetComplexityStmt(scx.TeamName, ResolveExpression(scx.Threshold, program), scx.Token) { ScopeId = scx.ScopeId };
            case TeamSetReflectionStmt: return stmt;
            case TeamSetCriticStmt scr:
                return new TeamSetCriticStmt(scr.TeamName, scr.Personas, ResolveExpression(scr.Judge, program), scr.Token) { ScopeId = scr.ScopeId };
            case TeamSetPruneStmt spr:
                return new TeamSetPruneStmt(spr.TeamName, ResolveExpression(spr.TopK, program), spr.Token) { ScopeId = spr.ScopeId };
            case TeamTrainThinkingStmt sth:
                return new TeamTrainThinkingStmt(sth.TeamName, ResolveExpression(sth.Dataset, program), sth.Token) { ScopeId = sth.ScopeId };
            case TeamTrainActorCriticStmt sac:
                return new TeamTrainActorCriticStmt(sac.TeamName, ResolveExpression(sac.Dataset, program), sac.Token) { ScopeId = sac.ScopeId };
            case TeamDistillThinkingStmt sdt:
                return new TeamDistillThinkingStmt(sdt.TeamName, ResolveExpression(sdt.Dataset, program), sdt.Token) { ScopeId = sdt.ScopeId };
            case TeamSaveStmt: return stmt;
            case TeamLoadStmt: return stmt;
            case WorkflowDefStmt: return stmt;
            case WorkflowStepStmt s:
                return new WorkflowStepStmt(s.StepName, ResolveExpression(s.Action, program),
                    s.RetryCount is not null ? ResolveExpression(s.RetryCount, program) : null,
                    s.TimeoutMs is not null ? ResolveExpression(s.TimeoutMs, program) : null, s.Token) { ScopeId = s.ScopeId };
            case WorkflowEndStmt: return stmt;
            case WorkflowExecuteStmt: return stmt;
            case TickStmt: return stmt;
            case TickEndStmt: return stmt;
            case TilemapCommandStmt s:
                Log($"[RESOLVER-TRACE] TilemapCommand '{s.Command.Lexeme}' in Scope {s.ScopeId}. Risoluzione argomenti...", ResolverLog);
                var tileResolvedArgs = s.Arguments.Select(a => ResolveExpression(a, program)).ToImmutableArray();
                return new TilemapCommandStmt(s.Command, tileResolvedArgs, s.Token) { ScopeId = s.ScopeId };
            case SpritedefStmt s:
                Log($"[RESOLVER-TRACE] SpritedefStmt in Scope {s.ScopeId}. Risoluzione espressione sprite ID...", ResolverLog);
                var resolvedSpriteId = ResolveExpression(s.SpriteId, program);
                return new SpritedefStmt(resolvedSpriteId, s.Rows, s.Token) { ScopeId = s.ScopeId };

            case ViewStmt s:
                return new ViewStmt(
                    s.Screen,
                    ResolveExpression(s.X1, program),
                    ResolveExpression(s.Y1, program),
                    ResolveExpression(s.X2, program),
                    ResolveExpression(s.Y2, program),
                    s.Color != null ? ResolveExpression(s.Color, program) : null,
                    s.Border != null ? ResolveExpression(s.Border, program) : null,
                    s.Token
                ) { ScopeId = s.ScopeId };

            case ViewPortStmt s:
                return new ViewPortStmt(
                    s.Screen,
                    ResolveExpression(s.X1, program),
                    ResolveExpression(s.Y1, program),
                    ResolveExpression(s.X2, program),
                    ResolveExpression(s.Y2, program),
                    s.Token
                ) { ScopeId = s.ScopeId };
            
            case VectorCommandStmt s:
                return new VectorCommandStmt(
                    s.Command,
                    [..s.Arguments.Select(a => ResolveExpression(a, program))]
                ) { ScopeId = s.ScopeId };

            case MatrixCommandStmt s:
                return new MatrixCommandStmt(
                    s.Command,
                    [..s.Arguments.Select(a => ResolveExpression(a, program))],
                    s.Token
                ) { ScopeId = s.ScopeId };

            case WorldCommandStmt s:
                return new WorldCommandStmt(
                    s.Command,
                    [..s.Arguments.Select(a => ResolveExpression(a, program))],
                    s.Token
                ) { ScopeId = s.ScopeId, CommandSymbolId = s.CommandSymbolId };

            case GraphicsPrintExprStmt s:
                return new GraphicsPrintExprStmt(ResolveExpression(s.Expression, program)) { ScopeId = s.ScopeId };

            case GraphicsTabStmt s:
                return new GraphicsTabStmt(ResolveExpression(s.ColumnExpression, program)) { ScopeId = s.ScopeId };

            case GraphicsTabZoneStmt:
            case GraphicsNewlineStmt:
                return stmt;

            // ================================================================================
            // --- NETWORKING ---
            // ================================================================================
            case TcpOpenStmt s:
                return new TcpOpenStmt(
                    ResolveExpression(s.Port, program),
                    ResolveTarget(s.Handle, program),
                    s.Engine != null ? ResolveExpression(s.Engine, program) : null,
                    s.Token
                ) { ScopeId = s.ScopeId };

            case TcpConnectStmt s:
                return new TcpConnectStmt(
                    ResolveExpression(s.Host, program),
                    ResolveExpression(s.Port, program),
                    ResolveTarget(s.Handle, program),
                    s.Engine != null ? ResolveExpression(s.Engine, program) : null,
                    s.Timeout != null ? ResolveExpression(s.Timeout, program) : null,
                    s.Token
                ) { ScopeId = s.ScopeId };

            case TcpAcceptStmt s:
                return new TcpAcceptStmt(
                    ResolveExpression(s.ServerHandle, program),
                    ResolveTarget(s.ClientHandle, program),
                    s.Timeout != null ? ResolveExpression(s.Timeout, program) : null,
                    s.Token
                ) { ScopeId = s.ScopeId };

            case TcpRecvStmt s:
                return new TcpRecvStmt(
                    ResolveExpression(s.ClientHandle, program),
                    ResolveTarget(s.Buffer, program),
                    ResolveTarget(s.BytesReceived, program),
                    s.Timeout != null ? ResolveExpression(s.Timeout, program) : null,
                    s.Token
                ) { ScopeId = s.ScopeId };

            case TcpSendStmt s:
                return new TcpSendStmt(
                    ResolveExpression(s.ClientHandle, program),
                    ResolveExpression(s.Data, program),
                    s.Token
                ) { ScopeId = s.ScopeId };

            case TcpCloseStmt s:
                return new TcpCloseStmt(ResolveExpression(s.Handle, program), s.Token) { ScopeId = s.ScopeId };

            case SslOpenStmt s:
                return new SslOpenStmt(
                    ResolveExpression(s.Port, program),
                    ResolveTarget(s.Handle, program),
                    ResolveExpression(s.CertificatePath, program),
                    s.CertificatePassword != null ? ResolveExpression(s.CertificatePassword, program) : null,
                    s.Token
                ) { ScopeId = s.ScopeId };

            case SslConnectStmt s:
                return new SslConnectStmt(
                    ResolveExpression(s.Host, program),
                    s.Port != null ? ResolveExpression(s.Port, program) : null,
                    ResolveTarget(s.Handle, program),
                    s.Timeout != null ? ResolveExpression(s.Timeout, program) : null,
                    s.Token
                ) { ScopeId = s.ScopeId };

            case UdpOpenStmt s:
                return new UdpOpenStmt(
                    s.Port != null ? ResolveExpression(s.Port, program) : null,
                    ResolveTarget(s.Handle, program),
                    s.Engine != null ? ResolveExpression(s.Engine, program) : null,
                    s.Token
                ) { ScopeId = s.ScopeId };

            case UdpSendStmt s:
                return new UdpSendStmt(
                    ResolveExpression(s.Handle, program),
                    ResolveExpression(s.Host, program),
                    ResolveExpression(s.Port, program),
                    ResolveExpression(s.Data, program),
                    s.Token
                ) { ScopeId = s.ScopeId };

            case UdpRecvStmt s:
                return new UdpRecvStmt(
                    ResolveExpression(s.Handle, program),
                    ResolveTarget(s.Buffer, program),
                    ResolveTarget(s.RemoteIp, program),
                    ResolveTarget(s.RemotePort, program),
                    s.Timeout != null ? ResolveExpression(s.Timeout, program) : null,
                    s.Token
                ) { ScopeId = s.ScopeId };

            case UdpCloseStmt s:
                return new UdpCloseStmt(ResolveExpression(s.Handle, program), s.Token) { ScopeId = s.ScopeId };

            case WebSocketConnectStmt s:
                return new WebSocketConnectStmt(
                    ResolveExpression(s.Host, program),
                    ResolveExpression(s.Port, program),
                    ResolveTarget(s.Handle, program),
                    s.Path != null ? ResolveExpression(s.Path, program) : null,
                    s.Timeout != null ? ResolveExpression(s.Timeout, program) : null,
                    s.Token) { ScopeId = s.ScopeId };

            case HttpFetchStmt s:
                return new HttpFetchStmt(
                    ResolveExpression(s.Url, program),
                    s.Options != null ? ResolveExpression(s.Options, program) : null,
                    s.CallbackName,
                    s.Token
                ) { ScopeId = s.ScopeId };

            case ShutdownStmt s:
                return new ShutdownStmt(
                    ResolveExpression(s.Handle, program),
                    ResolveExpression(s.Mode, program),
                    s.Token
                ) { ScopeId = s.ScopeId };

            case SetOptionStmt s:
                return new SetOptionStmt(
                    ResolveExpression(s.Handle, program),
                    ResolveExpression(s.Option, program),
                    ResolveExpression(s.Value, program),
                    s.Token
                ) { ScopeId = s.ScopeId };

            case HttpServerStmt s:
                return new HttpServerStmt(
                    s.Port != null ? ResolveExpression(s.Port, program) : null,
                    s.Callback,
                    s.CertificatePath != null ? ResolveExpression(s.CertificatePath, program) : null,
                    s.CertificatePassword != null ? ResolveExpression(s.CertificatePassword, program) : null,
                    s.IsStopCommand
                ) { ScopeId = s.ScopeId };

            case HttpRouteStmt s:
                return new HttpRouteStmt(
                    ResolveExpression(s.Method, program),
                    ResolveExpression(s.Path, program),
                    s.Callback
                ) { ScopeId = s.ScopeId };

            case HttpUseStmt s:
                return new HttpUseStmt(
                    s.Path != null ? ResolveExpression(s.Path, program) : null,
                    s.Callback
                ) { ScopeId = s.ScopeId };

            case HttpUseNextStmt s:
                return new HttpUseNextStmt(ResolveExpression(s.NextHandle, program)) { ScopeId = s.ScopeId };

            case HttpListenerLoopStmt:
            case HttpConnectionHandlerStmt:
                return stmt;

            case HttpConfigStmt s:
                return new HttpConfigStmt(ResolveExpression(s.ConfigObject, program), s.Token) { ScopeId = s.ScopeId };

            case HttpStaticStmt s:
                return new HttpStaticStmt(
                    ResolveExpression(s.UrlPath, program),
                    ResolveExpression(s.PhysicalPath, program),
                    s.Token) { ScopeId = s.ScopeId };

            // ================================================================================
            // --- GOROUTINES ---
            // ================================================================================
            case GoProcedureStmt s:
            {
                Log($"[RESOLVER-DEBUG] Entrato in GoProcedureStmt.", ResolverLog);
                Log($"[RESOLVER-DEBUG]   -> Tipo del nodo Call: {s.Call.GetType().Name}", ResolverLog);
                Log($"[RESOLVER-DEBUG]   -> Nodo Call Iniziale: {s.Call}", ResolverLog);

                var callToResolve = s.Call;

                // CASO PRINCIPALE: GO SUB viene parsato come FunctionExpr con ID=-1
                if (s.Call is FunctionExpr { SymbolId: < 0 } fe)
                {
                    Log(
                        $"[RESOLVER-DEBUG]   -> Rilevato FunctionExpr non risolto. ScopeId: {fe.ScopeId}, Nome: '{fe.OriginalToken.Lexeme}'",
                        ResolverLog);

                    var candidateId =
                        FindSymbolIdInScopeChain(fe.OriginalToken.Lexeme, fe.ScopeId, fe.Position, program);
                    Log($"[RESOLVER-DEBUG]   -> Risultato di FindSymbolIdInScopeChain: {candidateId}", ResolverLog);

                    if (candidateId != -1)
                    {
                        var symbol = program.Symbols.GetSymbol(candidateId);
                        if (symbol.DeclarationNode is ProcedureDefinition proc &&
                            SymbolTable.IsProcedureSignatureCompatible(proc, fe.Args.Length))
                        {
                            Log(
                                $"[RESOLVER-DEBUG]   -> Trovata procedura compatibile ID {proc.SymbolId}. Aggiorno FunctionExpr.",
                                ResolverLog);
                            // Aggiorna il nodo che verrà processato successivamente
                            callToResolve = fe with { SymbolId = proc.SymbolId };
                        }
                    }
                }
                // Fallback per il caso in cui GO SUB fosse parsato come ArrayAccessExpr
                else if (s.Call is ArrayAccessExpr { SymbolId: < 0 } aae)
                {
                    var candidateId =
                        FindSymbolIdInScopeChain(aae.NameCleaned.Lexeme, aae.ScopeId, aae.Position, program);
                    if (candidateId != -1)
                    {
                        var symbol = program.Symbols.GetSymbol(candidateId);
                        if (symbol.DeclarationNode is ProcedureDefinition proc &&
                            SymbolTable.IsProcedureSignatureCompatible(proc, aae.Indices.Length))
                        {
                            callToResolve = new FunctionExpr(
                                proc.SymbolId,
                                aae.NameCleaned,
                                aae.Indices,
                                proc is FunctionDefinition fd ? fd.ReturnType : new VoidType()
                            ) { ScopeId = aae.ScopeId };
                        }
                    }
                }

                // Ora, risolvi l'espressione di chiamata (con l'ID potenzialmente aggiornato) e i suoi argomenti.
                var resolvedCall = ResolveExpression(callToResolve, program);

                Log($"[RESOLVER-DEBUG]   -> CALL finale dopo ResolveExpression: {resolvedCall}", ResolverLog);

                return new GoProcedureStmt(
                    resolvedCall,
                    s.WaitGroup != null ? ResolveExpression(s.WaitGroup, program) : null
                ) { ScopeId = s.ScopeId };
            }

            case WaitStmt s:
                return new WaitStmt(ResolveExpression(s.WaitGroup, program), s.Token) { ScopeId = s.ScopeId };

            case SendStmt s:
                return new SendStmt(
                    ResolveExpression(s.Value, program),
                    ResolveExpression(s.Channel, program),
                    s.Token
                ) { ScopeId = s.ScopeId };

            case ReceiveStmt s:
                return new ReceiveStmt(
                    ResolveTarget(s.Destination, program),
                    ResolveExpression(s.Channel, program),
                    s.Token,
                    s.OkFlag is null ? null : ResolveTarget(s.OkFlag, program)
                ) { ScopeId = s.ScopeId };

            case CloseChannelStmt s:
                return new CloseChannelStmt(ResolveExpression(s.Channel, program), s.Token) { ScopeId = s.ScopeId };

            case TimeoutStmt s:
                return new TimeoutStmt(ResolveExpression(s.Duration, program), s.Token) { ScopeId = s.ScopeId };

            case DoneStmt s:
                return new DoneStmt(ResolveExpression(s.WaitGroup, program), s.Token) { ScopeId = s.ScopeId };

            case GoExitStmt: return stmt;

            case GoSelectStmt s:
                return new GoSelectStmt(
                    s.SelectToken,
                    [
                        ..s.Cases.Select(c =>
                            new GoSelectCase(
                                CommunicationStmt: ResolveStatement(c.CommunicationStmt, program),
                                Body: ResolveStatementList(c.Body, program)
                            )
                        )
                    ],
                    s.DefaultCase != null ? ResolveStatementList(s.DefaultCase.Value, program) : null
                ) { ScopeId = s.ScopeId };

            case SleepStmt s:
                return new SleepStmt(ResolveExpression(s.Duration, program), s.Token) { ScopeId = s.ScopeId };

            case MutexLockStmt s:
                return new MutexLockStmt(ResolveExpression(s.Mutex, program), s.Token) { ScopeId = s.ScopeId };

            case MutexUnlockStmt s:
                return new MutexUnlockStmt(ResolveExpression(s.Mutex, program), s.Token) { ScopeId = s.ScopeId };

            case MutexRLockStmt s:
                return new MutexRLockStmt(ResolveExpression(s.Mutex, program), s.Token) { ScopeId = s.ScopeId };

            case MutexRUnlockStmt s:
                return new MutexRUnlockStmt(ResolveExpression(s.Mutex, program), s.Token) { ScopeId = s.ScopeId };

            case OnceStmt s:
                return new OnceStmt(ResolveExpression(s.Once, program), s.ProcedureName, s.Token) { ScopeId = s.ScopeId };

            case CondWaitStmt s:
                return new CondWaitStmt(ResolveExpression(s.Cond, program), ResolveExpression(s.Mutex, program), s.Token) { ScopeId = s.ScopeId };

            case CondSignalStmt s:
                return new CondSignalStmt(ResolveExpression(s.Cond, program), s.Token) { ScopeId = s.ScopeId };

            case CondBroadcastStmt s:
                return new CondBroadcastStmt(ResolveExpression(s.Cond, program), s.Token) { ScopeId = s.ScopeId };

            case DeferStmt s:
                return new DeferStmt(ResolveStatement(s.Body, program), s.Token) { ScopeId = s.ScopeId };

            // ================================================================================
            // --- ARRAYS & COLLECTIONS ---
            // ================================================================================
            case AAddStmt s:
                return new AAddStmt(
                    ResolveTarget(s.Array, program),
                    ResolveExpression(s.Value, program),
                    s.Token
                ) { ScopeId = s.ScopeId };

            case AInsStmt s:
                return new AInsStmt(
                    ResolveTarget(s.Array, program),
                    ResolveExpression(s.PositionExpr, program),
                    s.Token
                ) { ScopeId = s.ScopeId };

            case ADelStmt s:
                return new ADelStmt(
                    ResolveTarget(s.Array, program),
                    ResolveExpression(s.PositionExpr, program),
                    s.Token
                ) { ScopeId = s.ScopeId };

            case ASortStmt s:
                return new ASortStmt(
                    ResolveTarget(s.Array, program),
                    s.CompareFunction != null ? ResolveExpression(s.CompareFunction, program) : null,
                    s.Order != null ? ResolveExpression(s.Order, program) : null,
                    s.Token
                ) { ScopeId = s.ScopeId };

            case AEvalStmt s:
                return new AEvalStmt(
                    ResolveTarget(s.Array, program),
                    ResolveExpression(s.ActionFunction, program),
                    s.Token
                ) { ScopeId = s.ScopeId };

            case AShuffleStmt s:
                return new AShuffleStmt(
                    ResolveTarget(s.Array, program),
                    s.Token
                ) { ScopeId = s.ScopeId };

            case AFillStmt s:
                return new AFillStmt(
                    ResolveTarget(s.Array, program),
                    ResolveExpression(s.Value, program),
                    s.Start != null ? ResolveExpression(s.Start, program) : null,
                    s.Count != null ? ResolveExpression(s.Count, program) : null,
                    s.Token
                ) { ScopeId = s.ScopeId };

            case ACopyStmt s:
                return new ACopyStmt(
                    ResolveTarget(s.Source, program),
                    ResolveTarget(s.Destination, program),
                    s.SourceStart != null ? ResolveExpression(s.SourceStart, program) : null,
                    s.Count != null ? ResolveExpression(s.Count, program) : null,
                    s.DestStart != null ? ResolveExpression(s.DestStart, program) : null,
                    s.Token
                ) { ScopeId = s.ScopeId };

            // ================================================================================
            // --- JSON & CRYPTO & MISC ---
            // ================================================================================
            case JsonSaveStmt s:
                return new JsonSaveStmt(
                    ResolveExpression(s.Source, program),
                    ResolveExpression(s.FilePath, program),
                    s.Token
                ) { ScopeId = s.ScopeId };

            case JsonWriteValueStmt s:
                return new JsonWriteValueStmt(
                    ResolveExpression(s.Key, program),
                    ResolveExpression(s.Value, program),
                    s.Section != null ? ResolveExpression(s.Section, program) : null,
                    s.Token
                ) { ScopeId = s.ScopeId };

            case JsonWriteSectionStmt s:
                return new JsonWriteSectionStmt(
                    ResolveExpression(s.Source, program),
                    s.Section != null ? ResolveExpression(s.Section, program) : null,
                    s.Token
                ) { ScopeId = s.ScopeId };

            // ================================================================================
            // --- DBF STATEMENTS ---
            // ================================================================================
            case BeginTransactionStmt:
            case CommitStmt:
            case RollbackStmt:
                return stmt;

            case ExportToFileStmt s:
                return new ExportToFileStmt(
                    s.EngineType,
                    ResolveExpression(s.Area, program),
                    ResolveExpression(s.FilePath, program),
                    s.Format,
                    s.Token
                ) { ScopeId = s.ScopeId };

            case ImportFromFileStmt s:
                return new ImportFromFileStmt(
                    s.EngineType,
                    ResolveExpression(s.Area, program),
                    ResolveExpression(s.FilePath, program),
                    s.Format,
                    s.Token
                ) { ScopeId = s.ScopeId };

            case ConvertDbfToIsamStmt s:
                return new ConvertDbfToIsamStmt(
                    ResolveExpression(s.SourceFile, program),
                    ResolveExpression(s.DestinationFile, program),
                    s.TypeSymbolId,
                    s.OriginalTypeName,
                    s.PkFieldSymbolId,
                    s.OriginalPkFieldName,
                    ResolveExpression(s.KeyLength, program),
                    s.IsUnsafe,
                    s.Token
                ) { ScopeId = s.ScopeId };

            case DbfUseStmt s:
                // La logica complessa è in un helper
                return ResolveDbfUseStatement(s, program);

            case DbfSelectWorkAreaStmt s:
                return new DbfSelectWorkAreaStmt(ResolveExpression(s.WorkArea, program), s.Token)
                    { ScopeId = s.ScopeId };

            case DbfGetRecordStmt s:
            {
                // `DBF GET <var>` late-binding: il target è una variabile NON dichiarata.
                // Prima si ritornava lo stmt invariato (SymbolId -1) e il backend IL finiva
                // per store-are il record in un local fittizio: `var.Campo` leggeva sempre
                // 0/"" (ClipperDbfLateBinding §3). Qui la variabile viene creata/risolta,
                // come fa già RewriteDbfGetStatement (che però non è raggiunto da nessun
                // dispatcher).
                if (s.Destination is VariableTarget { SymbolId: < 0 } vtGet)
                {
                    var existing = program.Symbols.FindSymbolInScope(vtGet.NameCleaned.Lexeme);
                    var id = existing is { Count: > 0 }
                        ? existing[0]
                        : program.Symbols.RegisterSymbol(vtGet.NameCleaned, SymbolFlags.IsLocal,
                            new AnyType(vtGet.NameCleaned), vtGet);
                    var resolvedTarget = vtGet with { SymbolId = id };
                    if (existing is null || existing.Count == 0)
                        program.Symbols.UpdateSymbolDeclaration(id, resolvedTarget);
                    return new DbfGetRecordStmt(resolvedTarget, s.Token) { ScopeId = s.ScopeId };
                }

                return new DbfGetRecordStmt(s.Destination != null ? ResolveTarget(s.Destination, program) : null,
                    s.Token) { ScopeId = s.ScopeId };
            }

            case DbfReplaceStmt s:
                return new DbfReplaceStmt(
                    [
                        ..s.Replacements.Select(r => new Assignment(
                            ResolveTarget(r.Target, program),
                            ResolveExpression(r.Value, program, true)
                        ))
                    ],
                    ResolveDbfScope(s.Scope, program, true),
                    s.Token
                ) { ScopeId = s.ScopeId };

            case DbfCreateStmt s:
                return new DbfCreateStmt(
                    ResolveExpression(s.FileName, program, false),
                    s.TypeNameToken,
                    s.Token
                ) { ScopeId = s.ScopeId };

            case DbfDeleteStmt s:
                return s;

            case DbfRecallStmt s:
                return s;

            case DbfSortStmt s:
                return new DbfSortStmt(
                    ResolveExpression(s.FileName, program, false),
                    s.SortFields,
                    (DbfForClause?)ResolveDbfScope(s.Scope, program, true),
                    s.Token
                ) { ScopeId = s.ScopeId };

            case DbfUpdateStmt s:
                return s;

            case DbfJoinWithStmt s:
                return new DbfJoinWithStmt(
                    s.AliasSymbolId,
                    s.OriginalAliasToken,
                    ResolveExpression(s.FileName, program, false),
                    s.Condition,
                    s.Fields,
                    s.Token
                ) { ScopeId = s.ScopeId };

            case DbfCreateViewStmt s:
                return new DbfCreateViewStmt(
                    s.ViewSymbolId,
                    s.ViewNameToken,
                    s.SelectQuery,
                    s.QueryExpression != null ? ResolveExpression(s.QueryExpression, program) : null,
                    s.Token
                ) { ScopeId = s.ScopeId };

            case DbfTotalOnStmt s:
                return new DbfTotalOnStmt(
                    s.KeyExpression,
                    ResolveExpression(s.FileName, program, false),
                    s.Fields,
                    s.Scope,
                    s.Token
                ) { ScopeId = s.ScopeId };

            case DbfReportStmt s:
                return new DbfReportStmt(
                    ResolveExpression(s.FormFile, program),
                    ResolveDbfScope(s.Scope, program, true),
                    s.Destination != null ? ResolveExpression(s.Destination, program) : null,
                    s.ToPrint,
                    s.Token
                ) { ScopeId = s.ScopeId };

            case DbfPackStmt:
            case DbfZapStmt:
                return stmt;

            case DbfGoStmt s:
                return new DbfGoStmt(ResolveExpression(s.Destination, program), s.Token) { ScopeId = s.ScopeId };

            case DbfSkipStmt s:
                return new DbfSkipStmt(
                    s.RecordCount != null ? ResolveExpression(s.RecordCount, program) : null,
                    s.Token
                ) { ScopeId = s.ScopeId };

            case DbfSeekStmt s:
                return new DbfSeekStmt(ResolveExpression(s.Value, program), s.Token) { ScopeId = s.ScopeId };

            case DbfCloseStmt s:
                return new DbfCloseStmt(
                    [.. s.WorkAreas.Select(wa => ResolveExpression(wa, program))],
                    s.Token,
                    s.All
                ) { ScopeId = s.ScopeId };

            case DbfSetDeletedStmt:
                return stmt;

            case DbfSetFilterStmt s:
                return new DbfSetFilterStmt(
                    s.Condition != null ? ResolveExpression(s.Condition, program) : null,
                    s.Token
                ) { ScopeId = s.ScopeId };

            case DbfCopyToStmt s:
                return new DbfCopyToStmt(
                    ResolveExpression(s.FileName, program, false),
                    (DbfForClause?)ResolveDbfScope(s.Scope, program, true),
                    s.Token
                ) { ScopeId = s.ScopeId };

            case DbfReindexStmt:
                return stmt;

            case DbfAppendRecordStmt s:
                return new DbfAppendRecordStmt(ResolveExpression(s.Source, program, false), s.Token)
                    { ScopeId = s.ScopeId };

            case DbfAppendFromStmt s:
                return new DbfAppendFromStmt(
                    ResolveExpression(s.FileName, program, false),
                    (DbfForClause?)ResolveDbfScope(s.Scope, program, true),
                    s.Token
                ) { ScopeId = s.ScopeId };

            case DbfAggregateStmt s:
                return new DbfAggregateStmt(
                    ResolveExpression(s.KeyExpression, program, true), // inDatabaseContext
                    ResolveExpression(s.FileName, program, false),
                    ResolveExpression(s.AggregationsExpr, program, true), // inDatabaseContext
                    (DbfForClause?)ResolveDbfScope(s.Scope, program, true), // inDatabaseContext
                    s.Token
                ) { ScopeId = s.ScopeId };

            case DbfCopyStructureStmt s:
                return new DbfCopyStructureStmt(ResolveExpression(s.FileName, program), s.Token)
                    { ScopeId = s.ScopeId };

            case DbfSetOrderStmt s:
                return new DbfSetOrderStmt(ResolveExpression(s.Value, program), s.Token) { ScopeId = s.ScopeId };

            case DbfSetRelationStmt s:
                return new DbfSetRelationStmt(
                    s.KeyExpression != null ? ResolveExpression(s.KeyExpression, program) : null,
                    s.ChildAlias,
                    s.Token
                ) { ScopeId = s.ScopeId };

            case DbfIndexOnStmt s:
                return new DbfIndexOnStmt(
                    ResolveExpression(s.KeyExpression, program, true),
                    ResolveExpression(s.FileName, program, false),
                    s.Token,
                    s.TagName != null ? ResolveExpression(s.TagName, program, false) : null
                ) { ScopeId = s.ScopeId };

            case DbfSetExactStmt:
                return stmt;

            case DbfSetDefaultStmt dbfSetDef:
                return new DbfSetDefaultStmt(ResolveExpression(dbfSetDef.Path, program), dbfSetDef.Token)
                    { ScopeId = dbfSetDef.ScopeId };

            case RddSetDefaultStmt rddSetDef:
                return new RddSetDefaultStmt(ResolveExpression(rddSetDef.DriverName, program), rddSetDef.Token)
                    { ScopeId = rddSetDef.ScopeId };

            case RddUseStmt rddUse:
                return new RddUseStmt(ResolveExpression(rddUse.FileName, program),
                    ResolveExpression(rddUse.Driver, program), rddUse.AliasToken, rddUse.Token)
                    { ScopeId = rddUse.ScopeId };

            case RddLockStmt rddLock:
                return rddLock with { ScopeId = rddLock.ScopeId };

            case RddSetDeletedStmt rddDel:
                return rddDel with { ScopeId = rddDel.ScopeId };

            case RddSetFieldStmt rddSetField:
                return new RddSetFieldStmt(ResolveExpression(rddSetField.Area, program),
                    ResolveExpression(rddSetField.Field, program), ResolveExpression(rddSetField.Value, program),
                    rddSetField.Token) { ScopeId = rddSetField.ScopeId };

            case RddSetReprocessStmt rddSetRep:
                return new RddSetReprocessStmt(ResolveExpression(rddSetRep.Ms, program), rddSetRep.Token)
                    { ScopeId = rddSetRep.ScopeId };

            case RddSetOrderStmt rddSetOrd:
                return new RddSetOrderStmt(ResolveExpression(rddSetOrd.Area, program),
                    ResolveExpression(rddSetOrd.Order, program), rddSetOrd.Token) { ScopeId = rddSetOrd.ScopeId };

            case RddDeleteStmt rddDelStmt:
                return new RddDeleteStmt(ResolveDbfScope(rddDelStmt.Scope, program, true), rddDelStmt.Token)
                    { ScopeId = rddDelStmt.ScopeId };

            case RddRecallStmt rddRecStmt:
                return new RddRecallStmt(ResolveDbfScope(rddRecStmt.Scope, program, true), rddRecStmt.Token)
                    { ScopeId = rddRecStmt.ScopeId };

            case RddReplaceStmt rddRepStmt:
                return new RddReplaceStmt(
                    [
                        ..rddRepStmt.Replacements.Select(r => new Assignment(
                            ResolveTarget(r.Target, program),
                            ResolveExpression(r.Value, program, true)
                        ))
                    ],
                    ResolveDbfScope(rddRepStmt.Scope, program, true),
                    rddRepStmt.Token
                ) { ScopeId = rddRepStmt.ScopeId };

            case RddSetLockTimeoutStmt rddSetLt:
                return new RddSetLockTimeoutStmt(ResolveExpression(rddSetLt.Ms, program), rddSetLt.Token)
                    { ScopeId = rddSetLt.ScopeId };

            case RddOnEventStmt rddOnEv:
                return new RddOnEventStmt(ResolveExpression(rddOnEv.EventName, program),
                    rddOnEv.Callback != null ? ResolveExpression(rddOnEv.Callback, program) : null,
                    rddOnEv.Token) { ScopeId = rddOnEv.ScopeId };

            case RddOffEventStmt rddOffEv:
                return new RddOffEventStmt(ResolveExpression(rddOffEv.EventName, program), rddOffEv.Token)
                    { ScopeId = rddOffEv.ScopeId };

            case MessageConnectStmt mConn:
                return new MessageConnectStmt(ResolveExpression(mConn.Uri, program), mConn.Handle != null ? ResolveTarget(mConn.Handle, program) : null, mConn.Token) { ScopeId = mConn.ScopeId };
            case MessageSubscribeStmt mSub:
                return new MessageSubscribeStmt(ResolveExpression(mSub.Target, program),
                    mSub.Topic != null ? ResolveExpression(mSub.Topic, program) : null,
                    mSub.Queue != null ? ResolveExpression(mSub.Queue, program) : null,
                    mSub.Alias != null ? ResolveExpression(mSub.Alias, program) : null, mSub.Token) { ScopeId = mSub.ScopeId };
            case MessageUnsubscribeStmt mUnsub:
                return new MessageUnsubscribeStmt(mUnsub.Sub != null ? ResolveExpression(mUnsub.Sub, program) : null, mUnsub.Token) { ScopeId = mUnsub.ScopeId };
            case MessagePublishStmt mPub:
                return new MessagePublishStmt(ResolveExpression(mPub.Target, program),
                    mPub.Topic != null ? ResolveExpression(mPub.Topic, program) : null,
                    ResolveExpression(mPub.Data, program), mPub.Token) { ScopeId = mPub.ScopeId };
            case MessagePingStmt mPing:
                return new MessagePingStmt(mPing.Target != null ? ResolveExpression(mPing.Target, program) : null, mPing.Token) { ScopeId = mPing.ScopeId };
            case MessageCloseStmt mClose:
                return new MessageCloseStmt(mClose.Target != null ? ResolveExpression(mClose.Target, program) : null, mClose.Token) { ScopeId = mClose.ScopeId };
            case OnMessageStmt mOn:
                return new OnMessageStmt(mOn.Sub != null ? ResolveExpression(mOn.Sub, program) : null, ResolveMessageAction(mOn.Action, program), mOn.Token) { ScopeId = mOn.ScopeId };

            case SignalSubscribeStmt sigSub:
                return new SignalSubscribeStmt(ResolveExpression(sigSub.Id, program),
                    sigSub.Types != null ? ResolveExpression(sigSub.Types, program) : null,
                    sigSub.Asset != null ? ResolveExpression(sigSub.Asset, program) : null,
                    sigSub.Market != null ? ResolveExpression(sigSub.Market, program) : null,
                    sigSub.Symbols != null ? ResolveExpression(sigSub.Symbols, program) : null,
                    sigSub.Timeframe != null ? ResolveExpression(sigSub.Timeframe, program) : null,
                    sigSub.Token) { ScopeId = sigSub.ScopeId };
            case SignalEmitStmt sigEmit:
                return new SignalEmitStmt(ResolveExpression(sigEmit.Type, program),
                    ResolveExpression(sigEmit.Symbol, program), ResolveExpression(sigEmit.Value, program),
                    sigEmit.Asset != null ? ResolveExpression(sigEmit.Asset, program) : null,
                    sigEmit.Market != null ? ResolveExpression(sigEmit.Market, program) : null,
                    sigEmit.Timeframe != null ? ResolveExpression(sigEmit.Timeframe, program) : null,
                    sigEmit.Token) { ScopeId = sigEmit.ScopeId };
            case SignalUnsubscribeStmt sigUnsub:
                return new SignalUnsubscribeStmt(sigUnsub.Id != null ? ResolveExpression(sigUnsub.Id, program) : null, sigUnsub.Token) { ScopeId = sigUnsub.ScopeId };
            case SignalPingStmt sigPing:
                return new SignalPingStmt(sigPing.Token) { ScopeId = sigPing.ScopeId };
            case OnSignalStmt sigOn:
                return new OnSignalStmt(sigOn.Sub != null ? ResolveExpression(sigOn.Sub, program) : null, ResolveMessageAction(sigOn.Action, program), sigOn.Token) { ScopeId = sigOn.ScopeId };

            case RddCreateIndexStmt rddCrtIdx:
                return new RddCreateIndexStmt(ResolveExpression(rddCrtIdx.Area, program),
                    ResolveExpression(rddCrtIdx.Name, program), ResolveExpression(rddCrtIdx.Key, program),
                    rddCrtIdx.Token) { ScopeId = rddCrtIdx.ScopeId };

            case RddCloseIndexStmt rddClsIdx:
                return new RddCloseIndexStmt(ResolveExpression(rddClsIdx.Area, program),
                    ResolveExpression(rddClsIdx.Name, program), rddClsIdx.Token) { ScopeId = rddClsIdx.ScopeId };

            case RddCopyToStmt rddCpy:
                return new RddCopyToStmt(ResolveExpression(rddCpy.Area, program),
                    ResolveExpression(rddCpy.FileName, program),
                    rddCpy.OutDriver != null ? ResolveExpression(rddCpy.OutDriver, program) : null,
                    rddCpy.Token) { ScopeId = rddCpy.ScopeId };

            case RddSortToStmt rddSrt:
                return new RddSortToStmt(ResolveExpression(rddSrt.Area, program),
                    ResolveExpression(rddSrt.FileName, program),
                    rddSrt.Fields != null ? ResolveExpression(rddSrt.Fields, program) : null,
                    rddSrt.Token) { ScopeId = rddSrt.ScopeId };

            case RddTotalOnStmt rddTot:
                return new RddTotalOnStmt(ResolveExpression(rddTot.Area, program),
                    ResolveExpression(rddTot.Key, program), rddTot.Token) { ScopeId = rddTot.ScopeId };

            case DbfSetPathStmt dbfSetPath:
                return new DbfSetPathStmt(ResolveExpression(dbfSetPath.PathList, program), dbfSetPath.Token)
                    { ScopeId = dbfSetPath.ScopeId };

            case DbfSetExclusiveStmt:
            case DbfSetSchemaStmt:
            case DbfAppendBlankStmt:
                return stmt;

            case DbfValidateStmt dbfVal:
                return new DbfValidateStmt(
                    dbfVal.Area != null ? ResolveExpression(dbfVal.Area, program) : null,
                    dbfVal.Token
                ) { ScopeId = dbfVal.ScopeId };

            case DbfListStmt s:
                return new DbfListStmt(
                    ResolveDbfScope(s.Scope, program, true),
                    s.Fields != null
                        ? new DbfFieldsClause([
                            ..s.Fields.FieldExpressions.Select(f => ResolveExpression(f, program, true))
                        ])
                        : null,
                    s.Destination != null ? ResolveExpression(s.Destination, program, false) : null,
                    s.ToPrint,
                    s.Token
                ) { ScopeId = s.ScopeId };

            // ================================================================================
            // --- ISAM STATEMENTS ---
            // ================================================================================
            case IsamOpenStmt s:
                // La logica complessa è in un helper
                return ResolveIsamOpenStatement(s, program);

            case IsamUseStmt s:
            {
                var modulePath = program.FileIdToPathMap.GetValueOrDefault(s.Token.Position.FileId, string.Empty);
                var resolvedType =
                    ResolveDataType(new UnresolvedQualifiedType(new QualifiedNameExpr([s.OriginalTypeName])), program,
                        modulePath);

                var typeSymbolId = resolvedType switch
                {
                    UserDefinedType udt => udt.SymbolId,
                    _ => -1
                };

                if (typeSymbolId == -1)
                    throw new BasicException(s.OriginalTypeName,
                        $"Il tipo '{s.OriginalTypeName.Lexeme}' specificato per ISAM USE non è un UDT valido.",
                        ErrorCode.TypeMismatch);
                if (resolvedType is UserDefinedType uIsam && !uIsam.IsFixedSize)
                    throw new BasicException(s.OriginalTypeName,
                        $"Il tipo '{uIsam.Name.Lexeme}' ha campi a lunghezza variabile (STRING/LIST/DICT/ANY) e non può essere usato come record a lunghezza fissa (ISAM USE).",
                        ErrorCode.TypeMismatch);

                return new IsamUseStmt(
                    ResolveExpression(s.FileName, program, false),
                    typeSymbolId,
                    s.OriginalTypeName,
                    s.Locking,
                    s.IsUnsafe,
                    ResolveIsamIndexedBy(s.IndexedBy, program),
                    s.AliasSymbolId,
                    s.OriginalAliasToken,
                    s.Token
                ) { ScopeId = s.ScopeId };
            }

            case IsamSelectWorkAreaStmt s:
                return new IsamSelectWorkAreaStmt(ResolveExpression(s.WorkArea, program), s.Token)
                    { ScopeId = s.ScopeId };

            case IsamRecordStmt s:
                return new IsamRecordStmt(ResolveExpression(s.FileNumber, program), s.Location, s.Token)
                    { ScopeId = s.ScopeId };

            case IsamSkipStmt s:
                return new IsamSkipStmt(
                    ResolveExpression(s.FileNumber, program),
                    s.RecordCount != null ? ResolveExpression(s.RecordCount, program) : null,
                    s.Token
                ) { ScopeId = s.ScopeId };

            case IsamSeekStmt s:
                return new IsamSeekStmt(
                    ResolveExpression(s.FileNumber, program, false),
                    ResolveExpression(s.PositionExpr, program, true), // Contesto DB
                    s.IndexName != null ? ResolveExpression(s.IndexName, program, false) : null,
                    s.Token
                ) { ScopeId = s.ScopeId };

            case IsamDeleteStmt s:
                return new IsamDeleteStmt(
                    ResolveExpression(s.FileNumber, program, false),
                    ResolveDbfScope(s.Scope, program, true), // Contesto DB
                    s.Token
                ) { ScopeId = s.ScopeId };

            case IsamRecallStmt s:
                return new IsamRecallStmt(
                    ResolveExpression(s.FileNumber, program, false),
                    ResolveDbfScope(s.Scope, program, true), // Contesto DB
                    s.Token
                ) { ScopeId = s.ScopeId };

            case IsamPackStmt s:
                return new IsamPackStmt(ResolveExpression(s.FileNumber, program), s.Token) { ScopeId = s.ScopeId };

            case IsamReindexStmt s:
                return new IsamReindexStmt(ResolveExpression(s.FileNumber, program), s.Token) { ScopeId = s.ScopeId };
            case IsamRepairStmt s:
                return new IsamRepairStmt(ResolveExpression(s.FileNumber, program), s.Token) { ScopeId = s.ScopeId };

            case IsamBackupStmt s:
                return new IsamBackupStmt(
                    ResolveExpression(s.FileNumber, program),
                    ResolveExpression(s.DestinationFile, program),
                    s.Token
                ) { ScopeId = s.ScopeId };

            case IsamAppendStmt s:
                return new IsamAppendStmt(
                    ResolveExpression(s.FileNumber, program),
                    ResolveExpression(s.Source, program),
                    s.Token
                ) { ScopeId = s.ScopeId };

            case IsamPutStmt s:
                return new IsamPutStmt(
                    ResolveExpression(s.Alias, program),
                    s.Source is not null ? ResolveExpression(s.Source, program) : null,
                    s.Token
                ) { ScopeId = s.ScopeId };

            case IsamLockStmt s:
                return new IsamLockStmt(ResolveExpression(s.FileNumber, program), s.IsRecordLock, s.Token)
                    { ScopeId = s.ScopeId };

            case IsamUnlockStmt s:
                return new IsamUnlockStmt(ResolveExpression(s.FileNumber, program), s.IsRecordLock, s.Token)
                    { ScopeId = s.ScopeId };

            case IsamSetOrderStmt s:
                return new IsamSetOrderStmt(
                    ResolveExpression(s.FileNumber, program),
                    ResolveExpression(s.IndexName, program),
                    s.Token
                ) { ScopeId = s.ScopeId };

            case IsamSetRelationStmt s:
                return new IsamSetRelationStmt(
                    ResolveExpression(s.FileNumber, program, false),
                    s.KeyExpression != null ? ResolveExpression(s.KeyExpression, program, true) : null, // Contesto DB
                    s.ChildAlias,
                    s.Token
                ) { ScopeId = s.ScopeId };

            case IsamCreateIndexStmt s:
                return new IsamCreateIndexStmt(
                    ResolveExpression(s.FileNumber, program, false),
                    ResolveExpression(s.IndexName, program, false),
                    ResolveExpression(s.KeyExpression, program, true), // Contesto DB
                    ResolveExpression(s.KeyLength, program, false),
                    s.Token
                ) { ScopeId = s.ScopeId };

            case IsamDropIndexStmt s:
                return new IsamDropIndexStmt(
                    ResolveExpression(s.FileNumber, program),
                    ResolveExpression(s.IndexName, program),
                    s.Token
                ) { ScopeId = s.ScopeId };

            case IsamValidateStmt s:
                return new IsamValidateStmt(ResolveExpression(s.FileNumber, program), s.Token) { ScopeId = s.ScopeId };

            case IsamCopyToStmt s:
                return new IsamCopyToStmt(
                    ResolveExpression(s.FileNumber, program, false),
                    ResolveExpression(s.DestinationFile, program, false),
                    (DbfForClause?)ResolveDbfScope(s.Scope, program, true), // Contesto DB
                    s.Token
                ) { ScopeId = s.ScopeId };

            case IsamJoinWithStmt s:
                return new IsamJoinWithStmt(
                    ResolveExpression(s.FileNumber, program, false),
                    s.SecondaryAliasSymbolId,
                    s.SecondaryAliasToken,
                    ResolveExpression(s.DestinationFile, program, false),
                    (DbfForClause)ResolveDbfScope(s.Condition, program, true)!,
                    s.Token
                ) { ScopeId = s.ScopeId };

            case IsamSortToStmt s:
                return new IsamSortToStmt(
                    ResolveExpression(s.FileNumber, program, false),
                    ResolveExpression(s.DestinationFile, program, false),
                    [
                        ..s.SortFields.Select(f =>
                            f with { KeyExpression = ResolveExpression(f.KeyExpression, program, true) })
                    ], // Contesto DB
                    (DbfForClause?)ResolveDbfScope(s.Scope, program, true), // Contesto DB
                    s.Token
                ) { ScopeId = s.ScopeId };

            case IsamSetFilterStmt s:
                return new IsamSetFilterStmt(
                    ResolveExpression(s.FileNumber, program, false),
                    s.Condition != null ? ResolveExpression(s.Condition, program, true) : null, // Contesto DB
                    s.Token
                ) { ScopeId = s.ScopeId };

            // ================================================================================
            // --- SPEED STATEMENTS ---
            // ================================================================================
            case SpeedOpenStmt s:
                return new SpeedOpenStmt(
                    ResolveExpression(s.DatabasePath, program),
                    s.OriginalAliasToken,
                    s.Token
                ) { ScopeId = s.ScopeId };

            case SpeedCloseStmt sc:
                // Nessuna espressione da risolvere (solo un eventuale alias-token).
                return new SpeedCloseStmt(sc.AllDatabases, sc.AliasToken, sc.Token) { ScopeId = sc.ScopeId };

            case SpeedUseStmt s:
                return ResolveSpeedUseStatement(s, program);

            case SpeedCreateIndexStmt s:
                return ResolveSpeedCreateIndexStatement(s, program);

            case SpeedDropIndexStmt s:
                return new SpeedDropIndexStmt(
                    ResolveExpression(s.IndexName, program),
                    ResolveExpression(s.Alias, program),
                    s.Token
                ) { ScopeId = s.ScopeId };

            case SpeedGetStmt s:
                return new SpeedGetStmt(
                    ResolveExpression(s.Alias, program),
                    ResolveTarget(s.Destination, program),
                    s.Token
                ) { ScopeId = s.ScopeId };

            case SpeedPutStmt s:
                return new SpeedPutStmt(
                    ResolveExpression(s.Alias, program),
                    s.Source != null ? ResolveExpression(s.Source, program) : null,
                    s.Token
                ) { ScopeId = s.ScopeId };

            case SpeedDeleteStmt s:
                return new SpeedDeleteStmt(
                    ResolveExpression(s.Alias, program),
                    s.Source != null ? ResolveExpression(s.Source, program) : null,
                    s.Token
                ) { ScopeId = s.ScopeId };

            case SpeedSeekStmt s:
                return new SpeedSeekStmt(
                    ResolveExpression(s.Alias, program),
                    ResolveExpression(s.KeyValue, program),
                    s.IndexName != null ? ResolveExpression(s.IndexName, program) : null,
                    s.Token
                ) { ScopeId = s.ScopeId };

            case SpeedGoStmt s:
                return new SpeedGoStmt(ResolveExpression(s.Alias, program), s.Location, s.Token)
                    { ScopeId = s.ScopeId };

            case SpeedSkipStmt s:
                return new SpeedSkipStmt(
                    ResolveExpression(s.Alias, program),
                    s.Count != null ? ResolveExpression(s.Count, program) : null,
                    s.Token
                ) { ScopeId = s.ScopeId };

            case SpeedSetOrderStmt s:
                return new SpeedSetOrderStmt(
                    ResolveExpression(s.Alias, program),
                    ResolveExpression(s.IndexName, program), // Aggiunto IndexName
                    s.Token
                ) { ScopeId = s.ScopeId };

            case SpeedSetFilterStmt s:
                return ResolveSpeedSetFilterStatement(s, program);

            case SpeedSetRelationStmt s:
                return ResolveSpeedSetRelationStatement(s, program);

            case SpeedSelectStmt s:
                return ResolveSpeedSelect(s, program);

            case SpeedCreateViewStmt s:
                return new SpeedCreateViewStmt(
                    s.ViewSymbolId,
                    s.OriginalViewName,
                    s.OfTypeSymbolId,
                    s.OriginalTypeName,
                    s.SelectStatement != null ? (SpeedSelectStmt)ResolveStatement(s.SelectStatement, program) : null,
                    s.QueryExpression != null ? ResolveExpression(s.QueryExpression, program) : null,
                    s.Token
                ) { ScopeId = s.ScopeId };

            case SpeedRefreshViewStmt:
                return stmt; // Solo token

            case SpeedDropViewStmt:
                return stmt; // Solo token

            // ================================================================================
            // --- PLOTTING & CHARTS ---
            // ================================================================================
            case ChartPlotStmt s:
                return new ChartPlotStmt(
                    [..s.Args.Select(a => ResolveExpression(a, program))],
                    s.Token
                ) { ScopeId = s.ScopeId };

            case ChartFinancialStmt s:
                return new ChartFinancialStmt(
                    [..s.Args.Select(a => ResolveExpression(a, program))],
                    s.Token
                ) { ScopeId = s.ScopeId };

            case ChartConfigStmt s:
                return new ChartConfigStmt(
                    ResolveExpression(s.Handle, program), s.Sub, ResolveExpression(s.Prop, program),
                    s.Num != null ? ResolveExpression(s.Num, program) : null,
                    s.Str != null ? ResolveExpression(s.Str, program) : null,
                    s.Into, s.Token, s.SubKind
                ) { ScopeId = s.ScopeId };

            case ChartDrawStmt s:
                return new ChartDrawStmt(
                    ResolveExpression(s.Handle, program), ResolveExpression(s.Op, program),
                    [..s.Args.Select(a => ResolveExpression(a, program))],
                    s.Into, s.Token
                ) { ScopeId = s.ScopeId };

            case CanvasStmt s:
                return new CanvasStmt(s.Sub,
                    [..s.Args.Select(a => ResolveExpression(a, program))],
                    s.Into != null ? ResolveTarget(s.Into, program) : null,
                    s.Token) { ScopeId = s.ScopeId };

            case ChartClearStmt s:
                return new ChartClearStmt(ResolveExpression(s.Handle, program), s.Token) { ScopeId = s.ScopeId };

            case ChartCloseStmt s:
                return new ChartCloseStmt(ResolveExpression(s.Handle, program), s.Token) { ScopeId = s.ScopeId };

            case ChartRenderStmt s:
                return new ChartRenderStmt(
                    s.IsSave,
                    [..s.Args.Select(a => ResolveExpression(a, program))],
                    s.Token
                ) { ScopeId = s.ScopeId };

            // ================================================================================
            // --- LEDGER STATEMENTS ---
            // ================================================================================
            case LedgerOpenStmt s:
                return new LedgerOpenStmt(
                    ResolveExpression(s.FilePath, program),
                    ResolveTarget(s.Handle, program)
                ) { ScopeId = s.ScopeId };

            case LedgerAddStmt s:
                return new LedgerAddStmt(
                    ResolveExpression(s.Handle, program),
                    ResolveExpression(s.Date, program),
                    ResolveExpression(s.Description, program),
                    [
                        ..s.Postings.Select(p => new LedgerPosting(
                            Account: ResolveExpression(p.Account, program),
                            Amount: p.Amount != null ? ResolveExpression(p.Amount, program) : null
                        ))
                    ],
                    ResolveTarget(s.NewIdVariable, program)
                ) { ScopeId = s.ScopeId };

            case LedgerRegisterStmt s:
                return new LedgerRegisterStmt(
                    ResolveExpression(s.Handle, program),
                    ResolveTarget(s.DestinationArray, program),
                    s.Query != null ? ResolveExpression(s.Query, program) : null
                ) { ScopeId = s.ScopeId };

            case LedgerBalanceStmt s:
                return new LedgerBalanceStmt(
                    ResolveExpression(s.Handle, program),
                    ResolveTarget(s.DestinationArray, program),
                    s.Query != null ? ResolveExpression(s.Query, program) : null
                ) { ScopeId = s.ScopeId };

            case LedgerEditStmt s:
                return new LedgerEditStmt(
                    ResolveExpression(s.Handle, program),
                    ResolveExpression(s.TransactionId, program),
                    ResolveExpression(s.Date, program),
                    ResolveExpression(s.Description, program),
                    [
                        ..s.Postings.Select(p => new LedgerPosting(
                            Account: ResolveExpression(p.Account, program),
                            Amount: p.Amount != null ? ResolveExpression(p.Amount, program) : null
                        ))
                    ],
                    ResolveTarget(s.NewIdVariable, program)
                ) { ScopeId = s.ScopeId };

            case LedgerDeleteStmt s:
                return new LedgerDeleteStmt(
                    ResolveExpression(s.Handle, program),
                    ResolveExpression(s.TransactionId, program)
                ) { ScopeId = s.ScopeId };

            case LedgerSaveStmt s:
                return new LedgerSaveStmt(ResolveExpression(s.Handle, program)) { ScopeId = s.ScopeId };

            case LedgerCloseStmt s:
                return new LedgerCloseStmt(ResolveExpression(s.Handle, program)) { ScopeId = s.ScopeId };

            case LedgerPackStmt s:
                return new LedgerPackStmt(ResolveExpression(s.Handle, program)) { ScopeId = s.ScopeId };

            case LedgerAccountsStmt s:
                return new LedgerAccountsStmt(
                    ResolveExpression(s.Handle, program),
                    s.RootAccount != null ? ResolveExpression(s.RootAccount, program) : null,
                    ResolveTarget(s.DestinationArray, program)
                ) { ScopeId = s.ScopeId };

            case LedgerValidateStmt s:
                return new LedgerValidateStmt(
                    ResolveExpression(s.Handle, program),
                    ResolveTarget(s.DestinationVariable, program)
                ) { ScopeId = s.ScopeId };

            case LedgerGetStmt s:
                return new LedgerGetStmt(
                    ResolveExpression(s.Handle, program),
                    s.TransactionId is null ? null : ResolveExpression(s.TransactionId, program),
                    ResolveTarget(s.DestinationArray, program)
                ) { ScopeId = s.ScopeId };

            case LedgerGoStmt s:
                return new LedgerGoStmt(
                    ResolveExpression(s.Handle, program),
                    ResolveExpression(s.Destination, program)
                ) { ScopeId = s.ScopeId };

            case LedgerSkipStmt s:
                return new LedgerSkipStmt(
                    ResolveExpression(s.Handle, program),
                    s.Count is null ? null : ResolveExpression(s.Count, program)
                ) { ScopeId = s.ScopeId };

            case LedgerBudgetStmt s:
                return new LedgerBudgetStmt(
                    ResolveExpression(s.Handle, program),
                    ResolveExpression(s.Account, program),
                    ResolveExpression(s.Amount, program),
                    ResolveExpression(s.Period, program)
                ) { ScopeId = s.ScopeId };

            case LedgerVarianceStmt s:
                return new LedgerVarianceStmt(
                    ResolveExpression(s.Handle, program),
                    ResolveExpression(s.Account, program),
                    ResolveExpression(s.Period, program),
                    ResolveTarget(s.DestinationVariable, program)
                ) { ScopeId = s.ScopeId };

            case LedgerPlotStmt s:
                return new LedgerPlotStmt(
                    ResolveExpression(s.Handle, program),
                    ResolveExpression(s.Query, program),
                    ResolveExpression(s.ChartType, program),
                    ResolveTarget(s.DestinationVariable, program)
                ) { ScopeId = s.ScopeId };

            case LedgerImportStmt s:
                return new LedgerImportStmt(
                    ResolveExpression(s.Handle, program),
                    ResolveExpression(s.CsvPath, program),
                    ResolveExpression(s.MappingRules, program),
                    s.DestinationVariable is null ? null : ResolveTarget(s.DestinationVariable, program)
                ) { ScopeId = s.ScopeId };

            // ================================================================================
            // --- SQL STATEMENTS ---
            // ================================================================================
            case SqlOpenStmt s:
                return new SqlOpenStmt(
                    ResolveExpression(s.FilePath, program),
                    s.OriginalAliasToken,
                    s.Token) { ScopeId = s.ScopeId };
            case SqlCloseStmt s:
                return new SqlCloseStmt(ResolveExpression(s.DbAlias, program), s.Token) { ScopeId = s.ScopeId };
            case SqlCloseResultSetStmt s:
                return new SqlCloseResultSetStmt(ResolveExpression(s.ResultSet, program), s.Token)
                    { ScopeId = s.ScopeId };
            case SqlExecStmt s:
                return new SqlExecStmt(
                    ResolveExpression(s.DbAlias, program),
                    ResolveExpression(s.Sql, program),
                    s.Parameters.Select(p => ResolveExpression(p, program)).ToImmutableArray(),
                    s.Token) { ScopeId = s.ScopeId };
            case SqlQueryStmt s:
                return new SqlQueryStmt(
                    ResolveExpression(s.DbAlias, program),
                    ResolveTarget(s.ResultSet, program),
                    ResolveExpression(s.Sql, program),
                    s.Parameters.Select(p => ResolveExpression(p, program)).ToImmutableArray(),
                    s.Token) { ScopeId = s.ScopeId };
            case SqlBeginTransactionStmt s:
                return new SqlBeginTransactionStmt(ResolveExpression(s.DbAlias, program), s.Token)
                    { ScopeId = s.ScopeId };
            case SqlCommitStmt s:
                return new SqlCommitStmt(ResolveExpression(s.DbAlias, program), s.Token) { ScopeId = s.ScopeId };
            case SqlRollbackStmt s:
                return new SqlRollbackStmt(ResolveExpression(s.DbAlias, program),
                    s.Token, s.SavepointName is null ? null : ResolveExpression(s.SavepointName, program))
                { ScopeId = s.ScopeId };
            case SqlSavepointStmt s:
                return new SqlSavepointStmt(ResolveExpression(s.DbAlias, program),
                    ResolveExpression(s.Name, program), s.Token) { ScopeId = s.ScopeId };
            case SqlReleaseStmt s:
                return new SqlReleaseStmt(ResolveExpression(s.DbAlias, program),
                    ResolveExpression(s.Name, program), s.Token) { ScopeId = s.ScopeId };
            case SqlAttachStmt s:
                return new SqlAttachStmt(
                    ResolveExpression(s.DbAlias, program),
                    ResolveExpression(s.AttachPath, program),
                    s.SchemaToken,
                    s.Token) { ScopeId = s.ScopeId };
            case SqlDetachStmt s:
                return new SqlDetachStmt(
                    ResolveExpression(s.DbAlias, program),
                    ResolveExpression(s.SchemaName, program),
                    s.Token) { ScopeId = s.ScopeId };
            case SqlBackupStmt s:
                return new SqlBackupStmt(
                    ResolveExpression(s.DbAlias, program),
                    ResolveExpression(s.DestinationPath, program),
                    s.Token) { ScopeId = s.ScopeId };

            case VectorDbEnsureCollectionStmt s:
                return new VectorDbEnsureCollectionStmt(
                    ResolveExpression(s.DbPath, program),
                    ResolveExpression(s.CollectionName, program), s.Token) { ScopeId = s.ScopeId };
            case VectorDbIndexTextStmt s:
                return new VectorDbIndexTextStmt(
                    ResolveExpression(s.DbPath, program),
                    ResolveExpression(s.CollectionName, program),
                    ResolveExpression(s.DocId, program),
                    ResolveExpression(s.Text, program), s.Token) { ScopeId = s.ScopeId };
            case VectorDbIndexMetaStmt s:
                return new VectorDbIndexMetaStmt(
                    ResolveExpression(s.DbPath, program),
                    ResolveExpression(s.CollectionName, program),
                    ResolveExpression(s.DocId, program),
                    ResolveExpression(s.Text, program),
                    ResolveExpression(s.MetaJson, program), s.Token) { ScopeId = s.ScopeId };
            case VectorDbSearchStmt s:
                return new VectorDbSearchStmt(
                    ResolveExpression(s.DbPath, program),
                    ResolveExpression(s.CollectionName, program),
                    ResolveExpression(s.Query, program),
                    ResolveExpression(s.TopK, program),
                    ResolveTarget(s.Result, program), s.Token) { ScopeId = s.ScopeId };
            case VectorDbCountStmt s:
                return new VectorDbCountStmt(
                    ResolveExpression(s.DbPath, program),
                    ResolveExpression(s.CollectionName, program),
                    ResolveTarget(s.Result, program), s.Token) { ScopeId = s.ScopeId };
            case VectorDbListCollectionsStmt s:
                return new VectorDbListCollectionsStmt(
                    ResolveExpression(s.DbPath, program),
                    ResolveTarget(s.Result, program), s.Token) { ScopeId = s.ScopeId };
            case VectorDbRemoveSourceStmt s:
                return new VectorDbRemoveSourceStmt(
                    ResolveExpression(s.DbPath, program),
                    ResolveExpression(s.CollectionName, program),
                    ResolveExpression(s.SourceId, program), s.Token) { ScopeId = s.ScopeId };
            case VectorDbLoadDocumentsStmt s:
                return new VectorDbLoadDocumentsStmt(
                    ResolveExpression(s.DbPath, program),
                    ResolveExpression(s.CollectionName, program),
                    ResolveExpression(s.DirectoryPath, program),
                    ResolveTarget(s.Result, program), s.Token) { ScopeId = s.ScopeId };
            case VectorDbPurgeDeprecatedStmt s:
                return new VectorDbPurgeDeprecatedStmt(
                    ResolveExpression(s.DbPath, program),
                    ResolveExpression(s.CollectionName, program), s.Token) { ScopeId = s.ScopeId };

            case TorchSetDeviceStmt s:
                return new TorchSetDeviceStmt(
                    ResolveExpression(s.Device, program), s.Token) { ScopeId = s.ScopeId };
            case TorchSetLibPathStmt s:
                return new TorchSetLibPathStmt(
                    ResolveExpression(s.LibPath, program), s.Token) { ScopeId = s.ScopeId };
            case TorchLoadLibStmt:
            case TorchUnloadLibStmt:
            case TorchInfoStmt:
                return stmt;

            case CreateAgentStmt:
            case DeleteAgentStmt:
            case SaveAgentStmt:
            case LoadAgentStmt:
                return stmt;

            case MatchStmt m:
                return m with
                {
                    Expression = ResolveExpression(m.Expression, program),
                    Cases = m.Cases.Select(c => c with
                    {
                        NullPattern = c.NullPattern != null ? ResolveExpression(c.NullPattern, program) : null,
                        TypePattern = c.TypePattern != null ? ResolveTypePattern(c.TypePattern, program) : null,
                        ValuePattern = c.ValuePattern != null ? ResolveCaseValuePattern(c.ValuePattern, program) : null,
                        ValueTest = c.ValueTest != null ? new CaseValue(ResolveExpression(c.ValueTest.Value, program)) : null,
                        RangeTest = c.RangeTest != null ? new CaseRange(
                            ResolveExpression(c.RangeTest.Start, program),
                            ResolveExpression(c.RangeTest.End, program)) : null,
                        ComparisonTest = c.ComparisonTest != null ? new CaseComparison(
                            c.ComparisonTest.Operator,
                            ResolveExpression(c.ComparisonTest.Value, program)) : null,
                        Statements = ResolveStatementList(c.Statements, program)
                    }).ToImmutableArray()
                };

            case TupleAssignmentStmt ta:
                return ta with
                {
                    TupleExpression = ResolveExpression(ta.TupleExpression, program)
                };

            case TupleDestructuringStmt td:
                return td with
                {
                    TupleExpression = ResolveExpression(td.TupleExpression, program)
                };

            case UsingStmt us:
                return us with
                {
                    ResourceTarget = ResolveTarget(us.ResourceTarget, program),
                    ResourceInit = ResolveExpression(us.ResourceInit, program),
                    Body = [..ResolveStatementList([..us.Body], program)]
                };

            case ViewPrintStmt s:
                return new ViewPrintStmt(
                    s.TopLine != null ? ResolveExpression(s.TopLine, program) : null,
                    s.BottomLine != null ? ResolveExpression(s.BottomLine, program) : null,
                    s.Token
                ) { ScopeId = s.ScopeId, SourceLineIndex = s.SourceLineIndex };

            // --- Clipper @...SAY: risolve le espressioni interne (Row/Col/Expression/Picture).
            // Prima era pass-through → TIME$()/DATE$() inline (builtin con nome Identifier-kind)
            // restavano ArrayAccessExpr non risolti → OP_PUSH_NIL a runtime ("Ora: " vuoto).
            // Allineato GMBasic (stesso gap, stessa fix).
            case AtSayStmt s:
                return new AtSayStmt(
                    ResolveExpression(s.Row, program),
                    ResolveExpression(s.Col, program),
                    ResolveExpression(s.Expression, program),
                    s.Picture != null ? ResolveExpression(s.Picture, program) : null,
                    s.Token)
                { ScopeId = s.ScopeId, SourceLineIndex = s.SourceLineIndex };

            // --- Clipper @...GET: risolve il target della variabile ---
            case AtGetStmt atGet:
                Expr? RA(Expr? e) => e is null ? null : ResolveExpression(e, program);
                return atGet with
                {
                    Variable = ResolveTarget(atGet.Variable, program),
                    Picture = RA(atGet.Picture), Valid = RA(atGet.Valid),
                    RangeLo = RA(atGet.RangeLo), RangeHi = RA(atGet.RangeHi),
                    Color = RA(atGet.Color), Items = RA(atGet.Items),
                    ControlWidth = RA(atGet.ControlWidth), ControlHeight = RA(atGet.ControlHeight),
                    Caption = RA(atGet.Caption), StepValue = RA(atGet.StepValue),
                    MinValue = RA(atGet.MinValue), MaxValue = RA(atGet.MaxValue),
                    When = RA(atGet.When), Message = RA(atGet.Message), Error = RA(atGet.Error),
                    ScopeId = atGet.ScopeId
                };

            case WindowSetTitleStmt wst:
                return new WindowSetTitleStmt(ResolveExpression(wst.Title, program), wst.Token)
                    { ScopeId = wst.ScopeId };

            case WindowAppendLineStmt wall:
                return new WindowAppendLineStmt(wall.Name, ResolveExpression(wall.Text, program), wall.Token)
                    { ScopeId = wall.ScopeId };
            case WindowSetContentStmt wsc:
                return new WindowSetContentStmt(wsc.Name, ResolveExpression(wsc.Lines, program), wsc.Token)
                    { ScopeId = wsc.ScopeId };
            case WindowScrollStmt wscr:
                return new WindowScrollStmt(wscr.Name, ResolveExpression(wscr.Delta, program), wscr.Token)
                    { ScopeId = wscr.ScopeId };
            case WindowSelectStmt wsel:
                return new WindowSelectStmt(ResolveExpression(wsel.Target, program), wsel.Token)
                    { ScopeId = wsel.ScopeId };
            case WindowThemeStmt wth:
                return new WindowThemeStmt(ResolveExpression(wth.ThemeName, program), wth.Token)
                    { ScopeId = wth.ScopeId };
            case WindowDesktopStmt wds:
                return new WindowDesktopStmt(ResolveExpression(wds.Desktop, program), wds.Token)
                    { ScopeId = wds.ScopeId };
            case WindowMoveDesktopStmt wmd:
                return new WindowMoveDesktopStmt(wmd.Name, ResolveExpression(wmd.Desktop, program), wmd.Token)
                    { ScopeId = wmd.ScopeId };
            case WindowFrameStmt:
            case WindowShadowStmt:

            // --- Clipper READ (pass-through) ---
            case ClipperReadStmt:

            case DefineWindowStmt:
            case WindowVisibilityStmt:
            case WindowRequestCloseStmt:   /* `WINDOW CLOSE`: nessuna espressione da risolvere */
            case WindowActionStmt:
            case WindowClearStmt:
            case WindowEditStmt:
            case ActivateWindowStmt:
            case DeactivateWindowStmt:
            case ReleaseWindowStmt:
                return stmt;
            case BoxStmt boxS:
                return new BoxStmt(
                    ResolveExpression(boxS.Top, program), ResolveExpression(boxS.Left, program),
                    ResolveExpression(boxS.Bottom, program), ResolveExpression(boxS.Right, program),
                    boxS.BorderStyle, boxS.Token);
            case ScrollStmt scr:
                return new ScrollStmt(
                    ResolveExpression(scr.Top, program), ResolveExpression(scr.Left, program),
                    ResolveExpression(scr.Bottom, program), ResolveExpression(scr.Right, program),
                    ResolveExpression(scr.Count, program), scr.Token);
                        case CheckBoxStmt xs:
                return xs with { Variable = ResolveTarget(xs.Variable, program), Row = ResolveExpression(xs.Row, program), Col = ResolveExpression(xs.Col, program), Caption = ResolveExpression(xs.Caption, program), Id = xs.Id != null ? ResolveTarget(xs.Id, program) : null };
                        case ListBoxStmt xs:
                return xs with { Variable = ResolveTarget(xs.Variable, program), Row = ResolveExpression(xs.Row, program), Col = ResolveExpression(xs.Col, program), Width = ResolveExpression(xs.Width, program), Height = ResolveExpression(xs.Height, program), Items = ResolveExpression(xs.Items, program), Id = xs.Id != null ? ResolveTarget(xs.Id, program) : null };
                        case ComboBoxStmt xs:
                return xs with { Variable = ResolveTarget(xs.Variable, program), Row = ResolveExpression(xs.Row, program), Col = ResolveExpression(xs.Col, program), Width = ResolveExpression(xs.Width, program), Items = ResolveExpression(xs.Items, program), Id = xs.Id != null ? ResolveTarget(xs.Id, program) : null };
            case MenuStmt xs:
                return xs with { Variable = xs.Variable != null ? ResolveTarget(xs.Variable, program) : null, Id = xs.Id != null ? ResolveTarget(xs.Id, program) : null };
            case StatusBarStmt xs:
                return xs with { Id = xs.Id != null ? ResolveTarget(xs.Id, program) : null };
            case FileDialogStmt xs:
                return xs with { Id = xs.Id != null ? ResolveTarget(xs.Id, program) : null };
            case PrinterDialogStmt xs:
                return xs with { Id = xs.Id != null ? ResolveTarget(xs.Id, program) : null };
                        case ToggleStmt xs:
                return xs with { Variable = ResolveTarget(xs.Variable, program), Row = ResolveExpression(xs.Row, program), Col = ResolveExpression(xs.Col, program), Caption = ResolveExpression(xs.Caption, program), Id = xs.Id != null ? ResolveTarget(xs.Id, program) : null };
                        case RadioGroupStmt xs:
                return xs with { Variable = ResolveTarget(xs.Variable, program), Row = ResolveExpression(xs.Row, program), Col = ResolveExpression(xs.Col, program), Items = ResolveExpression(xs.Items, program), Id = xs.Id != null ? ResolveTarget(xs.Id, program) : null };
                        case SliderStmt xs:
                return xs with { Variable = ResolveTarget(xs.Variable, program), Row = ResolveExpression(xs.Row, program), Col = ResolveExpression(xs.Col, program), Length = ResolveExpression(xs.Length, program), Min = ResolveExpression(xs.Min, program), Max = ResolveExpression(xs.Max, program), Step = ResolveExpression(xs.Step, program), Id = xs.Id != null ? ResolveTarget(xs.Id, program) : null };
            case NumericUpDownStmt xs:
                return xs with { Variable = ResolveTarget(xs.Variable, program), Id = xs.Id != null ? ResolveTarget(xs.Id, program) : null };
            case StarRatingStmt xs:
                return xs with { Variable = xs.Variable != null ? ResolveTarget(xs.Variable, program) : null, Id = xs.Id != null ? ResolveTarget(xs.Id, program) : null };
            case ScrollBarStmt xs:
                return xs with { Variable = ResolveTarget(xs.Variable, program), Id = xs.Id != null ? ResolveTarget(xs.Id, program) : null };
            case ScrollViewStmt xs:
                return xs with { Variable = ResolveTarget(xs.Variable, program), Id = xs.Id != null ? ResolveTarget(xs.Id, program) : null };
            case LabelControlStmt xs:
                return xs with { Variable = ResolveTarget(xs.Variable, program), Id = xs.Id != null ? ResolveTarget(xs.Id, program) : null };
            case DataListStmt xs:
                return xs with { Variable = ResolveTarget(xs.Variable, program), Id = xs.Id != null ? ResolveTarget(xs.Id, program) : null };
            case PropertyGridStmt xs:
                return xs with { Variable = ResolveTarget(xs.Variable, program), Id = xs.Id != null ? ResolveTarget(xs.Id, program) : null };
            case SplitterStmt xs:
                return xs with { Variable = ResolveTarget(xs.Variable, program), Id = xs.Id != null ? ResolveTarget(xs.Id, program) : null };
            case ContextMenuStmt xs:
                return xs with { Variable = ResolveTarget(xs.Variable, program), Id = xs.Id != null ? ResolveTarget(xs.Id, program) : null };
            case LogViewStmt xs:
                return xs with { Variable = xs.Variable != null ? ResolveTarget(xs.Variable, program) : null, Id = xs.Id != null ? ResolveTarget(xs.Id, program) : null };
            case HexViewStmt xs:
                return xs with { Variable = xs.Variable != null ? ResolveTarget(xs.Variable, program) : null, Id = xs.Id != null ? ResolveTarget(xs.Id, program) : null };
            case BigTextStmt xs:
                return xs with { Variable = ResolveTarget(xs.Variable, program), Id = xs.Id != null ? ResolveTarget(xs.Id, program) : null };
            case CodeAreaStmt xs:
                return xs with { Variable = ResolveTarget(xs.Variable, program), Id = xs.Id != null ? ResolveTarget(xs.Id, program) : null };
            case ReorderListBoxStmt xs:
                return xs with { Variable = ResolveTarget(xs.Variable, program), Id = xs.Id != null ? ResolveTarget(xs.Id, program) : null };
            case AlertBoxStmt xs:
                return xs with { Variable = ResolveTarget(xs.Variable, program), Id = xs.Id != null ? ResolveTarget(xs.Id, program) : null };
            case ConfirmationBoxStmt xs:
                return xs with { Variable = ResolveTarget(xs.Variable, program), Id = xs.Id != null ? ResolveTarget(xs.Id, program) : null };
            case FileExplorerStmt xs:
                return xs with { Variable = ResolveTarget(xs.Variable, program), Id = xs.Id != null ? ResolveTarget(xs.Id, program) : null };
            case DashboardStmt xs:
                return xs with { Variable = ResolveTarget(xs.Variable, program), Id = xs.Id != null ? ResolveTarget(xs.Id, program) : null };
            case TabStmt xs:
                return xs with { Variable = ResolveTarget(xs.Variable, program), Id = xs.Id != null ? ResolveTarget(xs.Id, program) : null };
            case GridStmt xs:
                return xs with { Variable = ResolveTarget(xs.Variable, program), Id = xs.Id != null ? ResolveTarget(xs.Id, program) : null };
            case DatePickerStmt xs:
                return xs with { Variable = ResolveTarget(xs.Variable, program), Id = xs.Id != null ? ResolveTarget(xs.Id, program) : null };
            case ColorPickerStmt xs:
                return xs with { Variable = ResolveTarget(xs.Variable, program), Id = xs.Id != null ? ResolveTarget(xs.Id, program) : null };
            case TreeViewStmt xs:
                return xs with { Variable = ResolveTarget(xs.Variable, program), Id = xs.Id != null ? ResolveTarget(xs.Id, program) : null };
            case SpinnerStmt xs:
                return xs with { Variable = ResolveTarget(xs.Variable, program), Id = xs.Id != null ? ResolveTarget(xs.Id, program) : null };
            case TBrowseStmt xs:
                Expr? RB(Expr? e) => e is null ? null : ResolveExpression(e, program);
                return xs with
                {
                    Row = RB(xs.Row), Col = RB(xs.Col), Width = RB(xs.Width), Height = RB(xs.Height),
                    Columns = RB(xs.Columns), Data = RB(xs.Data), Options = RB(xs.Options),
                    Variable = ResolveTarget(xs.Variable, program),
                    Id = xs.Id != null ? ResolveTarget(xs.Id, program) : null
                };

            default:
                // Se arriviamo qui, abbiamo dimenticato un tipo di statement.
                // È meglio lanciare un'eccezione che ignorare silenziosamente.
                throw new NotImplementedException(
                    $"ResolveStatement: Tipo di istruzione non gestito: {stmt.GetType().Name}");
        }
    }

    // ================================================================================
    // --- METODI HELPER PER STRUTTURE COMPLESSE ---
    // ================================================================================

    private static CaseTypePattern ResolveTypePattern(CaseTypePattern tp, BasicProgram program)
    {
        return tp with
        {
            TargetType = (DataType)tp.TargetType.DeepClone(),
            VariableName = tp.VariableName,
            WhenCondition = tp.WhenCondition != null ? ResolveExpression(tp.WhenCondition, program) : null
        };
    }

    private static CaseValuePattern ResolveCaseValuePattern(CaseValuePattern vp, BasicProgram program)
    {
        return vp with
        {
            PatternValue = ResolveExpression(vp.PatternValue, program),
            VariableName = vp.VariableName,
            WhenCondition = vp.WhenCondition != null ? ResolveExpression(vp.WhenCondition, program) : null
        };
    }






    /// <summary>
    /// Risolve le espressioni all'interno di una condizione di un blocco `CASE` (in uno statement `SELECT`).
    /// Un `CaseTest` può assumere diverse forme (valore singolo, intervallo, confronto, pattern matching) e ognuna
    /// contiene una o più espressioni che devono essere risolte.
    ///
    /// RESPONSABILITÀ:
    /// 1.  Dispatching Polimorfico: Identifica il tipo specifico di test.
    /// 2.  Risoluzione Espressioni:
    ///     - Per <see cref="CaseValue"/> (es. `CASE 1`): Risolve l'espressione del valore (`Value`).
    ///     - Per <see cref="CaseRange"/> (es. `CASE 1 TO 10`): Risolve le espressioni di inizio (`Start`) e fine (`End`) dell'intervallo.
    ///     - Per <see cref="CaseComparison"/> (es. `CASE IS > 5`): Risolve l'espressione di confronto (`Value`).
    ///     - Per <see cref="CaseTypePattern"/> (es. `CASE IS STRING s`): Risolve l'espressione opzionale `WHEN` (`WhenCondition`).
    ///     - Per <see cref="CaseValuePattern"/> (es. `CASE 10 AS x`): Risolve l'espressione del valore (`PatternValue`) e la condizione opzionale `WHEN` (`WhenCondition`).
    /// 3.  Ricostruzione: Restituisce una nuova istanza del test con le espressioni risolte.
    ///
    /// Questo assicura che variabili o funzioni usate nelle condizioni del `CASE` siano correttamente collegate.
    /// </summary>
    /// <param name="test">Il test del caso da risolvere.</param>
    /// <param name="program">L'istanza del programma.</param>
    /// <returns>Una nuova istanza di <see cref="CaseTest"/> con le espressioni risolte.</returns>
    private static CaseTest ResolveCaseTest(CaseTest test, BasicProgram program)
    {
        return test switch
        {
            CaseValue cv => cv with { Value = ResolveExpression(cv.Value, program) },
            CaseRange cr => cr with
            {
                Start = ResolveExpression(cr.Start, program), 
                End = ResolveExpression(cr.End, program)
            },
            CaseComparison cc => cc with { Value = ResolveExpression(cc.Value, program) },
            CaseTypePattern ctp => ctp with
            {
                // Risolve la condizione WHEN se presente
                WhenCondition = ctp.WhenCondition != null ? ResolveExpression(ctp.WhenCondition, program) : null
            },
            CaseValuePattern cvp => cvp with
            {
                // Risolve il valore del pattern e la condizione WHEN
                PatternValue = ResolveExpression(cvp.PatternValue, program),
                WhenCondition = cvp.WhenCondition != null ? ResolveExpression(cvp.WhenCondition, program) : null
            },
            _ => test
        };
    }

    /// <summary>
    /// Risolve i riferimenti ai simboli all'interno di un nodo `Target` (L-value, destinazione di assegnazione).
    /// Un `Target` può essere una semplice variabile, un elemento di array, o un campo di un oggetto.
    ///
    /// LOGICA DI RISOLUZIONE:
    /// 1.  `VariableTarget`: Usa `ScopeId` del nodo per cercare il simbolo. Se trovato, aggiorna `SymbolId`.
    /// 2.  `ArrayElementTarget`: Risolve il nome dell'array e poi ricorsivamente gli indici.
    /// 3.  `FieldTarget`: Risolve la struttura base. Tenta di risolvere il campo solo se la base è un UDT o una Classe.
    ///     IMPORTANTE: Se la base è un tipo built-in (es. HttpOptions), il campo viene lasciato irrisolto (-1)
    ///     in questa fase, delegando la validazione e il binding finale alla Fase 3 (Rewriter).
    /// </summary>
    /// <param name="target">Il nodo Target da risolvere.</param>
    /// <param name="program">L'istanza del programma.</param>
    /// <returns>Il nodo Target aggiornato con SymbolId risolti dove possibile.</returns>
    private static Target ResolveTarget(Target target, BasicProgram program, int fallbackScopeId = -1)
    {
        switch (target)
        {
            case VariableTarget vt:
                // Se l'ID è già valido o è una keyword speciale, ritorna subito.
                if (vt.SymbolId >= 0 ||
                    vt.NameCleaned.Kind is TokenKind.Self or TokenKind.Base ||
                    vt.NameCleaned.Lexeme.Equals("SELF", StringComparison.OrdinalIgnoreCase) ||
                    vt.NameCleaned.Lexeme.Equals("BASE", StringComparison.OrdinalIgnoreCase))
                {
                    return vt;
                }

                // Usa SEMPRE NameCleaned per la ricerca. Lo scope di ricerca è quello del
                // target se valido, altrimenti il fallback (scope del nodo che contiene il
                // target — es. DrawTexturesStmt, dove il parser crea VariableTarget senza
                // ScopeId per la sintassi `array()`).
                var effectiveScope = vt.ScopeId >= 0 ? vt.ScopeId : fallbackScopeId;
                var varId = effectiveScope >= 0
                    ? FindSymbolIdInScopeChain(vt.NameCleaned.Lexeme, effectiveScope, vt.Position, program)
                    : -1;
                if (varId < 0)
                {
                    var match = program.Symbols.FindSymbolsByName(vt.NameCleaned.Lexeme)
                        .FirstOrDefault(s => s.DeclarationNode is VariableDeclaration &&
                                             s.Flags.HasFlag(SymbolFlags.IsGlobal));
                    if (match != null)
                    {
                        varId = match.Id;
                    }
                    else
                    {
                        // Broad match: anche variabili LOCALI (es. array impliciti `DIM x$() = {...}`
                        // usati da comandi statement ASHUFFLE/AADD in una procedura) — il filtro
                        // IsGlobal sopra li escludeva, lasciando il target a SymbolId -1.
                        match = program.Symbols.FindSymbolsByName(vt.NameCleaned.Lexeme)
                            .FirstOrDefault(s => s.DeclarationNode is VariableDeclaration);
                        if (match != null) varId = match.Id;
                    }
                }

                // Se viene trovato un simbolo, aggiorna l'ID.
                if (varId != -1)
                {
                    // 'with' propaga correttamente lo ScopeId, ma per essere ultra-sicuri:
                    return vt with { SymbolId = varId, ScopeId = vt.ScopeId };
                }

                // Se non trovato, ritorna l'originale (possibile late-binding DBF).
                return vt;

            case ArrayElementTarget aet:
            {
                // Resolve via FindSymbolsByName when scope-chain lookup fails
                // (handles locally-declared arrays whose ScopeId is -1 at parse time).
                int arrayId;
                if (aet.SymbolId >= 0)
                {
                    arrayId = aet.SymbolId;
                }
                else
                {
                    var chainResult = aet.ScopeId >= 0
                        ? FindSymbolIdInScopeChain(aet.NameCleaned.Lexeme, aet.ScopeId, aet.Position, program)
                        : -1;
                    if (chainResult >= 0)
                    {
                        arrayId = chainResult;
                    }
                    else
                    {
                        // Broad match — any symbol with this name is a candidate.
                        // Narrowing by DataType risks missing UDTs whose DeclarationNode shape
                        // differs from VariableDeclaration (e.g. forwarded type symbols).
                        var match = program.Symbols.FindSymbolsByName(aet.NameCleaned.Lexeme)
                            .FirstOrDefault();
                        arrayId = match?.Id ?? -1;
                    }
                }

                var resolvedIndices = aet.Indices.Select(i => ResolveExpression(i, program)).ToImmutableArray();
                return new ArrayElementTarget(arrayId, aet.NameSuffixed, aet.NameCleaned, resolvedIndices)
                    { ScopeId = aet.ScopeId };
            }

            case FieldTarget ft when ft.Structure is null:
                return ft; // WITH block implicit member — resolved at runtime via WithTarget
            case FieldTarget ft:
            {
                var resolvedStructure = ResolveTarget(ft.Structure!, program);

                var structureSymbolId = resolvedStructure switch
                {
                    VariableTarget svt => svt.SymbolId,
                    ArrayElementTarget saet => saet.SymbolId,
                    _ => -1
                };

                if (structureSymbolId < 0)
                    return ft with { Structure = resolvedStructure };

                var structureSymbol = program.Symbols.GetSymbol(structureSymbolId);
                var modulePath =
                    program.FileIdToPathMap.GetValueOrDefault(ft.OriginalToken.Position.FileId, string.Empty);
                var rawStructureType = ResolveDataType(structureSymbol.DataType, program, modulePath);

                // Determina il tipo base su cui cercare i campi.
                var baseTypeForFieldLookup = rawStructureType;
                if (resolvedStructure is ArrayElementTarget && rawStructureType is ArrayType at)
                {
                    baseTypeForFieldLookup = at.ElementType;
                }

                var isStaticAccess = false;
                if (resolvedStructure is VariableTarget baseVar)
                {
                    var baseSymbol = program.Symbols.GetSymbol(baseVar.SymbolId);
                    if (baseSymbol.DeclarationNode is ClassDefinition &&
                        baseVar.NameCleaned.Kind is not (TokenKind.Self or TokenKind.Base))
                    {
                        isStaticAccess = true;
                    }
                }

                var fieldSymbolId = -1;

                // CORREZIONE: Normalizza il nome del campo (es. "Age%" -> "Age") per il matching.
                var (fieldName, _) = SplitNameAndSuffix(ft.OriginalToken.Lexeme);

                // Tenta la risoluzione SOLO se è un tipo strutturato utente.
                switch (baseTypeForFieldLookup)
                {
                    case InterfaceType it:
                    {
                        Log($"[RESOLVER-TRACE] FieldTarget: La struttura base '{fieldName}' è un'interfaccia '{it.Definition.Name.Lexeme}'.", ResolverLog);
                        // Cerca se il membro esiste nell'interfaccia.
                        var member = it.Definition.Members.FirstOrDefault(m =>
                            m.Name.Lexeme.Equals(fieldName, StringComparison.OrdinalIgnoreCase));

                        if (member is PropertyMember)
                        {
                            // È una proprietà di un'interfaccia. È un target valido.
                            // Non c'è un FieldSymbolId da risolvere, ma il nodo è sintatticamente corretto
                            // per essere passato al Rewriter, che lo trasformerà in PropertyTarget/AccessExpr.
                            // Nessuna modifica al nodo qui, la validazione è sufficiente.
                            Log($"[RESOLVER-TRACE]   -> Trovata proprietà '{fieldName}' nell'interfaccia. Lascio FieldSymbolId a -1 per il Rewriter.", ResolverLog);
                        }
                        else if (member == null)
                        {
                            throw CreateError(ft.OriginalToken,
                                $"Nessun membro con nome '{fieldName}' trovato nell'interfaccia '{it.Definition.Name.Lexeme}'.");
                        }
                        else
                        {
                            throw CreateError(ft.OriginalToken,
                                $"Il membro '{fieldName}' dell'interfaccia '{it.Definition.Name.Lexeme}' non è una proprietà assegnabile.");
                        }
                        break;
                    }
                    case ClassType ct:
                    {
                        var classDefSymbol = program.Symbols.FindSymbolsByName(ct.Definition.OriginalName.Lexeme)
                            .FirstOrDefault(s => s.DeclarationNode is ClassDefinition);

                        if (classDefSymbol?.DeclarationNode is not ClassDefinition authoritativeDef)
                        {
                            break;
                        }

                        // Cerca il membro (campo/proprietà) risalendo la GERARCHIA di ereditarietà:
                        // prima nella classe diretta, poi nelle basi finché non si trova. Senza
                        // questa risalita un campo ereditato dalla base NON era accessibile da
                        // un'istanza esterna (LIMITE-9 del bug report Classi).
                        ClassMember? member = null;
                        ClassDefinition? current = authoritativeDef;
                        while (current != null && member == null)
                        {
                            member = current.Members.FirstOrDefault(m =>
                            {
                                // Usa fieldName normalizzato
                                if (!m.Name.Lexeme.Equals(fieldName, StringComparison.OrdinalIgnoreCase)) return false;

                                var isStaticMember = m switch
                                {
                                    FieldMember fm => fm.Declaration.Flags.HasFlag(SymbolFlags.IsStatic),
                                    PropertyMember pm => pm.Property.Flags.HasFlag(SymbolFlags.IsStatic),
                                    _ => false
                                };
                                return isStaticAccess == isStaticMember;
                            });
                            if (member != null) break;

                            // Risali alla base: ResolvedBaseClass se già risolta (fase 3.3),
                            // altrimenti via BaseClassSymbolId, altrimenti per NOME
                            // (OriginalBaseClassName) quando la base non è ancora linkata.
                            if (current.ResolvedBaseClass != null)
                            {
                                current = current.ResolvedBaseClass;
                            }
                            else if (current.BaseClassSymbolId is int baseSymbolId)
                            {
                                current = program.Symbols.GetSymbol(baseSymbolId)?.DeclarationNode as ClassDefinition;
                            }
                            else if (current.OriginalBaseClassName != null)
                            {
                                var baseName = current.OriginalBaseClassName.Lexeme;
                                ClassDefinition? FindBase(string n) => program.Symbols.FindSymbolsByName(n)
                                    .FirstOrDefault(s => s.DeclarationNode is ClassDefinition)?.DeclarationNode as ClassDefinition;
                                current = FindBase(baseName);
                                // Base QUALIFICATA (`INHERITS MVC.HttpController`): il nome e' il
                                // segmento finale. Senza, la risalita si fermava e un campo EREDITATO
                                // accesso dall'esterno (`istanza.CampoBase`) dava "Campo o proprieta'
                                // di istanza non trovato nella classe" (divergenza vs C99Native, che
                                // risolve il nome nudo — bug trovato dal MiniTest mvc/inherits_qualified).
                                if (current is null && baseName.Contains('.'))
                                    current = FindBase(baseName[(baseName.LastIndexOf('.') + 1)..]);
                            }
                            else
                            {
                                current = null;
                            }
                        }

                        fieldSymbolId = member switch
                        {
                            FieldMember fm => fm.Declaration.SymbolId,
                            PropertyMember pm => pm.Property.SymbolId,
                            _ => -1
                        };

                        if (fieldSymbolId == -1)
                        {
                            // §11.1: `istanza.Prop` di una classe INHERITS Control → proprietà del
                            // controllo (risolta a Rewrite in CONTROL GET/SET). Non è un errore.
                            if (IsControlBaseName(ct.Definition) && ControlPropRuntime(fieldName) != null)
                                break;
                            var accessType = isStaticAccess ? "statico" : "di istanza";
                            throw new BasicException(ft.OriginalToken,
                                $"Campo o proprietà {accessType} '{fieldName}' non trovato nella classe '{ct.Definition.Name.Lexeme}'.",
                                ErrorCode.MemberNotFound);
                        }

                        break;
                    }
                    case UserDefinedType udt:
                    {
                        if (isStaticAccess) break;

                        // The AST node's SymbolId is -1 (unresolved type ref);
                        // look up the registered UDT symbol by name instead.
                        var udtSymbol = program.Symbols.FindSymbolsByName(udt.Name.Lexeme)
                            .FirstOrDefault(s => s.DeclarationNode is UserDefinedType);
                        if (udtSymbol?.DeclarationNode is not UserDefinedType authoritativeUdt)
                        {
                            break;
                        }

                        // Usa fieldName normalizzato
                        var field = authoritativeUdt.Fields.FirstOrDefault(f =>
                            f.Name.Lexeme.Equals(fieldName, StringComparison.OrdinalIgnoreCase));
                        if (field != null)
                        {
                            fieldSymbolId = field.SymbolId;
                        }
                        else
                        {
                            var prop = authoritativeUdt.Properties.FirstOrDefault(p =>
                                p.Name.Lexeme.Equals(fieldName, StringComparison.OrdinalIgnoreCase));
                            if (prop != null)
                            {
                                fieldSymbolId = prop.SymbolId;
                            }
                        }

                        if (fieldSymbolId == -1)
                        {
                            throw new BasicException(ft.OriginalToken,
                                $"Campo o proprietà '{fieldName}' non trovato nel tipo '{udt.Name.Lexeme}'.",
                                ErrorCode.MemberNotFound);
                        }

                        break;
                    }
                }

                return ft with { Structure = resolvedStructure, FieldSymbolId = fieldSymbolId };
            }

            case IndexedTarget it:
            {
                var resolvedBase = ResolveExpression(it.Base, program);
                var resolvedIndices = it.Indices.Select(i => ResolveExpression(i, program))
                    .ToImmutableArray();
                var newIndexedTarget = new IndexedTarget(resolvedBase, resolvedIndices)
                {
                    ScopeId = it.ScopeId,
                    ResolvedType = it.ResolvedType
                };
                return newIndexedTarget;
            }

            case MacroTarget mt:
                return mt with { Expression = (MacroExpr)ResolveExpression(mt.Expression, program) };

            default:
                return target;
        }
    }

    /// <summary>
    /// Risolve i riferimenti ai simboli all'interno di un nodo `Expr` (espressione).
    /// Questo è il cuore della Fase 2.5.
    ///
    /// LOGICA DI RISOLUZIONE:
    /// 1.  `VariableExpr`: Cerca il nome nello scope indicato da `ve.ScopeId`. Se trovato, aggiorna `SymbolId`.
    ///     Se non trovato, lancia errore.
    /// 2.  `ArrayAccessExpr`: Questo nodo è ambiguo (può essere array o chiamata a funzione).
    ///     Il Resolver cerca il nome.
    ///     - Se trova un simbolo (variabile/array), aggiorna `SymbolId` e risolve gli indici.
    ///     - Se NON trova un simbolo variabile, potrebbe essere una funzione.
    ///     - Tuttavia, il Resolver NON deve trasformare l'AST (es. cambiare ArrayAccess in FunctionCall).
    ///     - Deve limitarsi a cercare un simbolo *variabile*. Se non lo trova, lascia `SymbolId = -1`.
    ///     - La trasformazione in `FunctionExpr` avverrà in Fase 3 (Rewriter), che ha la logica
    ///       per cercare le procedure se non è una variabile.
    ///     - *Eccezione*: Se trova una procedura con quel nome nello scope? In teoria ArrayAccessExpr
    ///       dovrebbe puntare solo a variabili. Se punta a una funzione, è una chiamata.
    ///       Il Resolver può "preparare" il terreno trovando l'ID, ma il tipo di nodo rimarrebbe errato.
    ///       PER SEMPLICITÀ E CORRETTEZZA: Il Resolver risolve SOLO variabili per ArrayAccessExpr.
    ///       Se non trova variabile, lascia -1. Il Rewriter gestirà la risoluzione di funzione.
    ///
    /// 3.  Ricorsione: Per tutti gli altri nodi (Binary, Unary, etc.), scende ricorsivamente.
    /// </summary>
    /// <param name="expr">L'espressione da risolvere.</param>
    /// <param name="program">L'istanza del programma.</param>
    /// <param name="inDatabaseContext">True se l'espressione è all'interno di un comando DBF/ISAM/SPEED.</param>
    /// <returns>Un nuovo nodo Expr con tutti i SymbolId interni risolti.</returns>
    /* §11.1: true se la classe (o una sua base per NOME) è `Control` — durante il Binding la catena
     * ResolvedBaseClass può non essere ancora risolta, quindi si usa OriginalBaseClassName. */
    private static bool IsControlBaseName(ClassDefinition? c)
    {
        for (int guard = 0; c != null && guard < 16; guard++)
        {
            if (c.OriginalName.Lexeme.Equals("Control", StringComparison.OrdinalIgnoreCase)) return true;
            if (c.ResolvedBaseClass != null) { c = c.ResolvedBaseClass; continue; }
            if (c.OriginalBaseClassName is { } b)
                return b.Lexeme.Equals("Control", StringComparison.OrdinalIgnoreCase)
                    || b.Lexeme.EndsWith(".Control", StringComparison.OrdinalIgnoreCase);
            return false;
        }
        return false;
    }

    private static Expr ResolveExpression(Expr expr, BasicProgram program, bool inDatabaseContext = false)
    {
        switch (expr)
        {
            case VariableExpr ve:
            {
                if (ve.SymbolId >= 0 ||
                    ve.NameCleaned.Kind is TokenKind.Self or TokenKind.Base ||
                    ve.NameCleaned.Lexeme.Equals("SELF", StringComparison.OrdinalIgnoreCase) ||
                    ve.NameCleaned.Lexeme.Equals("BASE", StringComparison.OrdinalIgnoreCase)) return ve;

                var varId = ve.ScopeId >= 0
                    ? FindSymbolIdInScopeChain(ve.NameCleaned.Lexeme, ve.ScopeId, ve.Position, program)
                    : -1;
                if (varId < 0)
                {
                    // Prova anche dallo scope corrente (es. scope della procedura
                    // se il resolver e' entrato con EnterScope prima di processare il corpo).
                    var currentScopeId = program.Symbols.GetCurrentScope().Id;
                    if (currentScopeId != ve.ScopeId)
                        varId = FindSymbolIdInScopeChain(ve.NameCleaned.Lexeme, currentScopeId, ve.Position, program);
                }
                if (varId < 0)
                {
                    var match = program.Symbols.FindSymbolsByName(ve.NameCleaned.Lexeme)
                        .FirstOrDefault(s => s.DeclarationNode is VariableDeclaration &&
                                             s.Flags.HasFlag(SymbolFlags.IsGlobal));
                    if (match != null)
                    {
                        varId = match.Id;
                    }
                }
                if (varId < 0)
                {
                    // COSTANTI di namespace (es. FILE_CREATED, JOYUP, KEY.ESC): erano trovate
                    // solo dentro le procedure (dove lo scope le include) ma NON a livello
                    // TOP-LEVEL — il nome restava SymbolId=-1 → variabile implicita (vuota).
                    var constMatch = program.Symbols.FindSymbolsByName(ve.NameCleaned.Lexeme)
                        .FirstOrDefault(s => s.DeclarationNode is ConstDeclaration &&
                                             s.Flags.HasFlag(SymbolFlags.IsConst));
                    if (constMatch != null)
                    {
                        varId = constMatch.Id;
                    }
                }

                if (varId != -1)
                {
                    return ve with { SymbolId = varId };
                }

                // Se non abbiamo trovato una variabile dichiarata, NON lanciamo subito un errore.
                // Restituiamo il VariableExpr originale con SymbolId = -1 per il late-binding.
                return ve;
            }

            case ArrayAccessExpr aae:
            {
                var resolvedIndices = aae.Indices.Select(i => ResolveExpression(i, program, inDatabaseContext))
                    .ToImmutableArray();
                var baseId = aae.SymbolId < 0
                    ? FindSymbolIdInScopeChain(aae.NameCleaned.Lexeme, aae.ScopeId, aae.Position, program)
                    : aae.SymbolId;
                // ====================================================================
                // ATTENZIONE — REGRESSIONE POSSIBILE (sprite/naming in backend IL)
                // ====================================================================
                // Fix per GRAFX-01/SpriteCommandStmt: gli ArrayAccessExpr passati come
                // ARGOMENTI di un comando sprite (SPRITE LOAD "f.png", SprID(I) /
                // SPRITE SET POSITION SprID(I), ...) arrivano qui con ScopeId = 0 (il
                // parser non lo imposta sugli argomenti di SpriteCommandStmt), quindi
                // FindSymbolIdInScopeChain fallisce e il SymbolId resta -1.
                //   - Il PCodeVM di gmo NON lo nota: risolve i nomi a RUNTIME (opcode).
                //   - Il backend IL di gm LO nota: EmitSpriteCommandStatement richiede il
                //     SymbolId per trovare l'array in _arrays → "array non DIMensionato".
                // Qui sotto si replica il fallback di VariableExpr (GetCurrentScope):
                // lo scope CORRENTE del resolver (procedura/block in EnterScope) ha la
                // catena che contiene la variabile locale.
                // ====================================================================
                // SE SI DOVESSE FARE ROLLBACK (regressione altrove): usare invece la
                // soluzione 2 — NON toccare il linker, risolvere il NOME del simbolo nel
                // backend IL (EmitSpriteCommandStatement) dal contesto quando SymbolId è
                // -1. Il linker è condiviso con gmo; un fallback qui tocca TUTTI i
                // backend. Valutare bene prima di rimuovere questo blocco.
                // ====================================================================
                if (baseId < 0 && aae.SymbolId < 0)
                {
                    var currentScopeId = program.Symbols.GetCurrentScope().Id;
                    if (currentScopeId != aae.ScopeId)
                        baseId = FindSymbolIdInScopeChain(aae.NameCleaned.Lexeme, currentScopeId, aae.Position, program);
                    if (baseId < 0)
                    {
                        var match = program.Symbols.FindSymbolsByName(aae.NameCleaned.Lexeme)
                            .FirstOrDefault(s => s.DeclarationNode is VariableDeclaration &&
                                                 s.Flags.HasFlag(SymbolFlags.IsGlobal));
                        if (match != null)
                            baseId = match.Id;
                    }
                }
                return new ArrayAccessExpr(baseId, aae.NameSuffixed, aae.NameCleaned, resolvedIndices, aae.IsSafe)
                    { ScopeId = aae.ScopeId };
            }

            case BuiltinFunctionCallExpr bce:
            {
                var resolvedArgs = bce.Args.Select(arg => ResolveExpression(arg, program, inDatabaseContext))
                    .ToImmutableArray();
                /* CSNG è un'identità numerica (parità con le gambe C, che rappresentano tutti i
                 * numerici come double): normalizzato a CDbl così IL/PCode non arrotondano a
                 * float32 (1.23456788) mentre VM/C/C99 danno 1.23456789. Le variabili SINGLE
                 * restano float32: la conversione esplicita CSNG è double. */
                var nameToken = bce.Name.Kind == TokenKind.CSng
                    ? bce.Name with { Kind = TokenKind.CDbl, Lexeme = "CDbl" }
                    : bce.Name;
                var retType = bce.Name.Kind == TokenKind.CSng ? new DoubleType() : bce.ReturnType;
                return new BuiltinFunctionCallExpr(bce.Namespace, nameToken, resolvedArgs, retType)
                {
                    OriginalToken = bce.OriginalToken,
                    ScopeId = bce.ScopeId
                };
            }

            case BinaryExpr be:
            {
                var resolvedLeft = ResolveExpression(be.Left, program, inDatabaseContext);
                var resolvedRight = ResolveExpression(be.Right, program, inDatabaseContext);
                
                // Constant folding: valuta espressioni costanti a compile-time
                var folded = TryFoldConstant(resolvedLeft, be.Op, resolvedRight);
                if (folded != null) return folded;
                
                return new BinaryExpr(resolvedLeft, be.Op, resolvedRight) { ScopeId = be.ScopeId };
            }

            case UnaryExpr ue:
            {
                var resolvedRight = ResolveExpression(ue.Right, program, inDatabaseContext);
                return new UnaryExpr(ue.Op, resolvedRight) { ScopeId = ue.ScopeId };
            }

            case GroupingExpr ge:
            {
                var resolvedExpression = ResolveExpression(ge.Expression, program, inDatabaseContext);
                return new GroupingExpr(resolvedExpression) { ScopeId = ge.ScopeId };
            }

            case FieldAccessExpr fae:
            {
                var resolvedStructure = ResolveExpression(fae.Structure, program, inDatabaseContext);
                return new FieldAccessExpr(resolvedStructure, fae.Field)
                {
                    FieldSymbolId = fae.FieldSymbolId,
                    ResolvedType = fae.ResolvedType,
                    ScopeId = fae.ScopeId
                };
            }

            case MethodCallExpr mce:
            {
                var resolvedInstance = ResolveExpression(mce.Instance, program, inDatabaseContext);
                var resolvedArgs = mce.Args.Select(a => ResolveExpression(a, program, inDatabaseContext))
                    .ToImmutableArray();
                return new MethodCallExpr(resolvedInstance, mce.MethodName, resolvedArgs)
                {
                    ResolvedDefinition = mce.ResolvedDefinition,
                    ScopeId = mce.ScopeId
                };
            }

            case NewExpr ne:
            {
                var resolvedArgs = ne.ConstructorArgs?.Select(a => ResolveExpression(a, program, inDatabaseContext))
                    .ToImmutableArray();
                var resolvedInits = ne.InitialValues?.Select(v => ResolveExpression(v, program, inDatabaseContext))
                    .ToImmutableArray();
                return new NewExpr(ne.ParsedType, resolvedArgs, resolvedInits, ne.NewToken) { ScopeId = ne.ScopeId };
            }

            case DirectCastExpr dce:
            {
                var resolvedExpression = ResolveExpression(dce.Expression, program, inDatabaseContext);
                return new DirectCastExpr(resolvedExpression, dce.TargetType, dce.CastToken) { ScopeId = dce.ScopeId };
            }
            case SafeCastExpr sce:
            {
                var resolvedExpression = ResolveExpression(sce.Expression, program, inDatabaseContext);
                return new SafeCastExpr(resolvedExpression, sce.TargetType, sce.AsToken) { ScopeId = sce.ScopeId };
            }
            case NullableHasValueExpr nhv:
            {
                var resolvedExpression = ResolveExpression(nhv.Expression, program, inDatabaseContext);
                return new NullableHasValueExpr(resolvedExpression, nhv.HasToken) { ScopeId = nhv.ScopeId };
            }
            case NullableValueExpr nv:
            {
                var resolvedExpression = ResolveExpression(nv.Expression, program, inDatabaseContext);
                return new NullableValueExpr(resolvedExpression, nv.ValueToken) { ScopeId = nv.ScopeId };
            }
            case ListLiteralExpr lle:
            {
                var resolvedElements = lle.Elements.Select(e => ResolveExpression(e, program, inDatabaseContext))
                    .ToImmutableArray();
                return new ListLiteralExpr(resolvedElements, lle.OpenBracket) { ScopeId = lle.ScopeId };
            }

            case ArrayLiteralExpr ale:
            {
                var resolvedElements = ale.Elements.Select(e => ResolveExpression(e, program, inDatabaseContext))
                    .ToImmutableArray();
                return new ArrayLiteralExpr(resolvedElements, ale.OpenBrace) { ScopeId = ale.ScopeId };
            }

            case DictionaryLiteralExpr dle:
            {
                var resolvedEntries = dle.Entries.Select(e => new DictionaryEntry(
                    ResolveExpression(e.Key, program, inDatabaseContext),
                    ResolveExpression(e.Value, program, inDatabaseContext)
                )).ToImmutableArray();
                return new DictionaryLiteralExpr(resolvedEntries, dle.OpenBrace) { ScopeId = dle.ScopeId };
            }

            case IndexedAccessExpr iae:
            {
                var resolvedBase = ResolveExpression(iae.Base, program, inDatabaseContext);
                var resolvedIndices = iae.Indices.Select(i => ResolveExpression(i, program, inDatabaseContext))
                    .ToImmutableArray();
                return new IndexedAccessExpr(resolvedBase, resolvedIndices, iae.IsSafe) { ScopeId = iae.ScopeId };
            }

            case LambdaExpr le:
            {
                // Il Binder ha già creato lo scope e registrato i parametri.
                // Dobbiamo solo assicurarci di risolvere il corpo.
                // Poiché il corpo è stato timbrato con lo ScopeId corretto dal Binder,
                // le chiamate ricorsive a ResolveExpression/ResolveStatementList funzioneranno
                // automaticamente.

                var resolvedExprBody = le.ExpressionBody != null
                    ? ResolveExpression(le.ExpressionBody, program, inDatabaseContext)
                    : null;

                var resolvedBlockBody = le.BlockBody.HasValue
                    ? ResolveStatementList(le.BlockBody.Value, program)
                    : (ImmutableArray<Stmt>?)null;

                // Restituiamo la lambda con i corpi risolti. I parametri sono già stati
                // aggiornati nella SymbolTable dal Binder, non c'è bisogno di toccarli qui.
                return le with
                {
                    ExpressionBody = resolvedExprBody,
                    BlockBody = resolvedBlockBody
                };
            }

            case ListComprehensionExpr lce:
                return ResolveListComprehension(lce, program, inDatabaseContext);

            case DictComprehensionExpr dce:
                return ResolveDictComprehension(dce, program, inDatabaseContext);

            case SetComprehensionExpr sce:
                return ResolveSetComprehension(sce, program, inDatabaseContext);

            case GeneratorExpr ge:
                return ResolveGeneratorExpr(ge, program, inDatabaseContext);

            case FunctionExpr fe:
            {
                var resolvedArgs = fe.Args.Select(a => ResolveExpression(a, program, inDatabaseContext))
                    .ToImmutableArray();
                // ID funzione può restare -1, verrà risolto dal Rewriter o qui se vogliamo.
                // L'importante è che gli argomenti siano risolti.
                return new FunctionExpr(fe.SymbolId, fe.OriginalToken, resolvedArgs, fe.ReturnType)
                    { ScopeId = fe.ScopeId, ResolvedDefinition = fe.ResolvedDefinition };
            }

            case AssignmentExpr ae:
            {
                Log($"[RESOLVER-DEBUG] Entrato nel case AssignmentExpr per: '{ae}'", ResolverLog);

                // Scende ricorsivamente per risolvere il Target (L-value) e il Value (R-value).
                var resolvedTarget = ResolveTarget(ae.Target, program);
                var resolvedValue = ResolveExpression(ae.Value, program, inDatabaseContext);

                Log($"[RESOLVER-DEBUG]   -> Target risolto: '{resolvedTarget}'", ResolverLog);
                Log($"[RESOLVER-DEBUG]   -> Valore risolto: '{resolvedValue}'", ResolverLog);

                var rewrittenNode = new AssignmentExpr(resolvedTarget, resolvedValue, ae.Operator)
                {
                    ScopeId = ae.ScopeId
                };

                Log($"[RESOLVER-DEBUG]   -> Nodo AssignmentExpr ricostruito: '{rewrittenNode}'", ResolverLog);
                return rewrittenNode;
            }

            case UnresolvedCallExpr uce:
            {
                // Risolviamo ricorsivamente gli argomenti.
                var uceArgs = uce.Arguments.Select(a => ResolveExpression(a, program, inDatabaseContext))
                    .ToImmutableArray();

                // Creiamo una nuova istanza con gli argomenti risolti, mantenendo gli altri campi inalterati.
                return new UnresolvedCallExpr(uce.Name, uceArgs, uce.ReturnType, uce.Reason, uce.Candidates, uce.QualifiedNamespace)
                {
                    ScopeId = uce.ScopeId
                };
            }

            case GoFunctionExpr gfe:
            {
                Log($"[RESOLVER-DEBUG] GoFunctionExpr trovato in Scope {gfe.ScopeId}. Args count: {gfe.Args.Length}",
                    ResolverLog);
                var resolvedArgs = gfe.Args.Select(a =>
                {
                    Log($"[RESOLVER-DEBUG]   Risolvo argomento: {a}", ResolverLog);
                    return ResolveExpression(a, program, inDatabaseContext);
                }).ToImmutableArray();
                var resolvedWaitGroup = gfe.WaitGroup != null
                    ? ResolveExpression(gfe.WaitGroup, program, inDatabaseContext)
                    : null;

                return new GoFunctionExpr(gfe.SymbolId, gfe.OriginalToken, resolvedArgs, resolvedWaitGroup,
                    gfe.ReturnType)
                {
                    ScopeId = gfe.ScopeId
                };
            }

            case MacroExpr me:
            {
                var resolvedInnerExpression = ResolveExpression(me.Expression, program, inDatabaseContext);
                return new MacroExpr(resolvedInnerExpression, me.Token) { ScopeId = me.ScopeId };
            }

            case EvalExpr ee:
            {
                var resolvedEvalTarget = ResolveExpression(ee.Target, program, inDatabaseContext);
                var resolvedEvalArgs = ee.Args
                    .Select(a => ResolveExpression(a, program, inDatabaseContext))
                    .ToImmutableArray();
                return new EvalExpr(resolvedEvalTarget, resolvedEvalArgs, ee.Token) { ScopeId = ee.ScopeId };
            }

            case ForeignAmountExpr fx:
            {
                var resolvedAmount = ResolveExpression(fx.Amount, program, inDatabaseContext);
                var resolvedRate = fx.ExchangeRate != null
                    ? ResolveExpression(fx.ExchangeRate, program, inDatabaseContext)
                    : null;
                return new ForeignAmountExpr(resolvedAmount, fx.CurrencyToken, resolvedRate)
                    { ScopeId = fx.ScopeId };
            }

            case NullConditionalAccessExpr nca:
            {
                var resolvedExpr = ResolveExpression(nca.Expression, program, inDatabaseContext);
                var resolvedArgs = nca.Arguments?
                    .Select(a => ResolveExpression(a, program, inDatabaseContext))
                    .ToImmutableArray();
                return new NullConditionalAccessExpr(resolvedExpr, nca.Kind, nca.MemberToken, resolvedArgs)
                    { ScopeId = nca.ScopeId };
            }

            case InterpolatedStringExpr ise:
            {
                var resolvedSegments = ise.Segments.Select(seg => seg switch
                {
                    InterpolatedStringExprSegment es => (InterpolatedStringSegment)(es with
                    {
                        Expression = ResolveExpression(es.Expression, program, inDatabaseContext)
                    }),
                    _ => seg
                }).ToImmutableArray();
                return ise with { Segments = resolvedSegments };
            }

            case TupleLiteralExpr tle:
            {
                var resolvedElements = tle.Elements
                    .Select(e => new NamedTupleElement(ResolveExpression(e.Value, program, inDatabaseContext), e.Name))
                    .ToImmutableArray();
                return new TupleLiteralExpr(resolvedElements, tle.OpenParen, tle.CloseParen)
                    { ScopeId = tle.ScopeId };
            }

            default:
                return expr;
        }
    }

    /// <summary>
    /// Risolve ricorsivamente i SymbolId all'interno di una comprensione di lista.
    /// </summary>
    private static ListComprehensionExpr ResolveListComprehension(ListComprehensionExpr lce, BasicProgram program, bool inDatabaseContext)
    {
        var resolvedClauses = lce.Clauses.Select(c => new ComprehensionClause(
            c.IteratorSymbolId,
            c.IteratorVariable,
            ResolveExpression(c.SourceExpression, program, inDatabaseContext),
            c.ConditionExpression != null ? ResolveExpression(c.ConditionExpression, program, inDatabaseContext) : null
        )).ToImmutableArray();

        var resolvedOutput = ResolveExpression(lce.OutputExpression, program, inDatabaseContext);

        return new ListComprehensionExpr(resolvedOutput, resolvedClauses, lce.OpenBracket, lce.OrderByClauses) { ScopeId = lce.ScopeId };
    }

    /// <summary>
    /// Risolve ricorsivamente i SymbolId all'interno di una comprensione di dizionario.
    /// </summary>
    private static DictComprehensionExpr ResolveDictComprehension(DictComprehensionExpr dce, BasicProgram program, bool inDatabaseContext)
    {
        var resolvedClauses = dce.Clauses.Select(c => new ComprehensionClause(
            c.IteratorSymbolId,
            c.IteratorVariable,
            ResolveExpression(c.SourceExpression, program, inDatabaseContext),
            c.ConditionExpression != null ? ResolveExpression(c.ConditionExpression, program, inDatabaseContext) : null
        )).ToImmutableArray();

        var resolvedKey = ResolveExpression(dce.KeyExpression, program, inDatabaseContext);
        var resolvedValue = ResolveExpression(dce.ValueExpression, program, inDatabaseContext);

        return new DictComprehensionExpr(resolvedKey, resolvedValue, resolvedClauses, dce.OpenBrace) { ScopeId = dce.ScopeId };
    }

    /// <summary>
    /// Risolve ricorsivamente i SymbolId all'interno di una comprensione di set.
    /// </summary>
    private static SetComprehensionExpr ResolveSetComprehension(SetComprehensionExpr sce, BasicProgram program, bool inDatabaseContext)
    {
        var resolvedClauses = sce.Clauses.Select(c => new ComprehensionClause(
            c.IteratorSymbolId,
            c.IteratorVariable,
            ResolveExpression(c.SourceExpression, program, inDatabaseContext),
            c.ConditionExpression != null ? ResolveExpression(c.ConditionExpression, program, inDatabaseContext) : null
        )).ToImmutableArray();

        var resolvedElement = ResolveExpression(sce.ElementExpression, program, inDatabaseContext);

        return new SetComprehensionExpr(resolvedElement, resolvedClauses, sce.OpenBrace) { ScopeId = sce.ScopeId };
    }

    private static GeneratorExpr ResolveGeneratorExpr(GeneratorExpr ge, BasicProgram program, bool inDatabaseContext)
    {
        // Per ogni clause, raccogliamo il mapping nome → scope per gli iterator.
        // L'output expression deve essere risolta con questi scope per trovare
        // le iterator variables (che non sono visibili dallo scope esterno).
        var iteratorScopes = new Dictionary<string, int>(StringComparer.OrdinalIgnoreCase);
        var resolvedClauses = ImmutableArray.CreateBuilder<ComprehensionClause>();

        foreach (var c in ge.Clauses)
        {
            // La SourceExpression è risolta nel contesto esterno (non nel gen scope)
            var resolvedSource = ResolveExpression(c.SourceExpression, program, inDatabaseContext);

            var resolvedCondition = c.ConditionExpression != null
                ? ResolveExpression(c.ConditionExpression, program, inDatabaseContext)
                : null;

            // Registra il mapping per l'output expression
            if (c.IteratorSymbolId >= 0 && c.IteratorVariable != null)
            {
                var sym = program.Symbols.GetSymbol(c.IteratorSymbolId);
                if (sym != null)
                    iteratorScopes[c.IteratorVariable.Lexeme] = sym.ScopeId;
            }

            resolvedClauses.Add(new ComprehensionClause(
                c.IteratorSymbolId,
                c.IteratorVariable!,
                resolvedSource,
                resolvedCondition
            ));
        }

        // Applica gli scope degli iterator alle VariableExpr nell'output expression
        var scopedOutput = iteratorScopes.Count > 0
            ? ApplyIteratorScopeIds(ge.OutputExpression, iteratorScopes)
            : ge.OutputExpression;

        var resolvedOutput = ResolveExpression(scopedOutput, program, inDatabaseContext);

        return new GeneratorExpr(resolvedOutput, resolvedClauses.ToImmutable(), ge.OpenParen) { ScopeId = ge.ScopeId };
    }

    /// <summary>
    /// Ricorsivamente imposta lo ScopeId sulle VariableExpr nell'output expression
    /// di un generatore, in modo che il Resolver possa trovare le iterator variables
    /// nei loro scope annidati.
    /// </summary>
    private static Expr ApplyIteratorScopeIds(Expr expr, Dictionary<string, int> iteratorScopes)
    {
        return expr switch
        {
            VariableExpr ve when iteratorScopes.TryGetValue(ve.NameCleaned.Lexeme, out var scopeId)
                => ve with { ScopeId = scopeId },
            BinaryExpr be => be with
            {
                Left = ApplyIteratorScopeIds(be.Left, iteratorScopes),
                Right = ApplyIteratorScopeIds(be.Right, iteratorScopes)
            },
            UnaryExpr ue => ue with { Right = ApplyIteratorScopeIds(ue.Right, iteratorScopes) },
            ArrayAccessExpr aae => aae with
            {
                Indices = aae.Indices.Select(i => ApplyIteratorScopeIds(i, iteratorScopes)).ToImmutableArray()
            },
            FunctionExpr fe => fe with
            {
                Args = fe.Args.Select(a => ApplyIteratorScopeIds(a, iteratorScopes)).ToImmutableArray()
            },
            _ => expr
        };
    }

    /// <summary>
    /// Metodo helper fondamentale per la risoluzione dei nomi.
    /// Cerca un simbolo risalendo la catena degli scope a partire da uno scope specifico.
    /// VERSIONE CORRETTA: Gestisce lo shadowing rigoroso (arresta la ricerca se il nome esiste nello scope)
    /// e supporta le forward references nelle comprensioni (rilassa il vincolo della colonna).
    /// </summary>
    /// <param name="name">Il nome del simbolo da cercare.</param>
    /// <param name="startScopeId">L'ID dello scope da cui iniziare la ricerca.</param>
    /// <param name="usagePosition">La posizione nel codice sorgente da cui parte la richiesta di risoluzione.</param>
    /// <param name="program">L'istanza del programma.</param>
    /// <returns>Il SymbolId trovato, oppure -1 se non trovato.</returns>
    /// <summary>True se il nodo è una dichiarazione DI MODULO (TYPE/CLASS/PROCEDURE/INTERFACE/
    /// ENUM) — mai un valore leggibile/scrivibile. Usata da <see cref="FindSymbolIdInScopeChain"/>
    /// per preferire, tra candidati omonimi nello stesso scope, quello che rappresenta un
    /// riferimento-VALORE quando il nome collide case-insensitive con una dichiarazione di
    /// modulo (es. `TYPE P` + `DIM p AS P` nello stesso scope — vedi Bugs/(open) - TYPE-
    /// TopLevel-NonRegistrato-Come-Tipo-Modulo.md).</summary>
    private static bool IsModuleTypeDeclarationNode(AstNode? node) =>
        node is UserDefinedType or ClassDefinition or ProcedureDefinition or InterfaceDefinition or EnumDefinition;

    private static int FindSymbolIdInScopeChain(string name, int startScopeId, SourcePosition usagePosition,
        BasicProgram program)
    {
        const bool log = false; // Log esaustivi abilitati per il debug

        if (startScopeId == -1)
        {
            Log($"[RESOLVER] Ricerca di '{name}' fallita: StartScopeId è -1.", log);
            return -1;
        }

        Log($"[RESOLVER] Inizio ricerca per '{name}' da Scope ID {startScopeId} (Pos: {usagePosition})", log);

        var currentId = startScopeId;
        var isFirstScope = true;

        // --- FASE 1: Ricerca nella catena di scope lessicale (dal corrente verso il globale) ---
        while (currentId != -1)
        {
            var currentScope = program.Symbols.GetScope(currentId);
            Log($"[RESOLVER]   -> Controllo Scope ID {currentId} ('{currentScope.Name}')...", log);

            if (currentScope.SymbolsByName.TryGetValue(name, out var ids) && ids.Count > 0)
            {
                Log($"[RESOLVER]     -> Trovati {ids.Count} candidati con nome '{name}' in questo scope.", log);

                // 2026-09-24 (overload): dentro il corpo di un omonimo, il bare `Nome`
                // è il return-slot della procedura contenitrice — prima dei controlli
                // posizionali (la dichiarazione che contiene l'uso è sempre visibile).
                if (ids.Count > 1)
                {
                    var ownProcId = program.Symbols.PreferEnclosingProcedureSymbolFrom(name, ids, startScopeId);
                    if (ownProcId >= 0)
                    {
                        Log($"[RESOLVER]         -> Return-slot della procedura contenitrice! Restituisco ID {ownProcId}.", log);
                        return ownProcId;
                    }
                }
                
                // Se siamo nello scope di partenza (quello più interno), applichiamo le regole di visibilità posizionale.
                if (isFirstScope)
                {
                    // Riconosciamo se lo scope appartiene a una comprensione (List/Set/Dict, prefisso
                    // "CompClause" — BindDictComprehension e affini) o a un GENERATOR EXPRESSION
                    // (prefisso "GenClause" — BindGeneratorExpr, Linker.Binding.cs) per permettere
                    // all'espressione di output di vedere l'iteratore definito dopo. Il prefisso
                    // "GenClause" mancava qui: per `(x FOR x IN nums IF cond)` l'output "x" precede
                    // testualmente "FOR x" sulla stessa riga, quindi falliva la visibilità
                    // posizionale (regola "dichiarato prima" pensata per variabili normali) e la
                    // ricerca si fermava per shadowing (return -1) invece di risolvere l'iteratore
                    // di QUESTA clause — il fallback successivo (ricerca globale per nome) trovava
                    // poi un "x" qualunque di un'ALTRA generator expression nello stesso file/scope
                    // (bug: `(x FOR x IN nums IF x MOD 2 = 0)` restituiva ripetutamente il valore
                    // dell'iteratore di UN GENERATOR EXPRESSION PRECEDENTE invece dei propri).
                    bool isComprehension = currentScope.Name.StartsWith("CompClause", StringComparison.OrdinalIgnoreCase)
                                           || currentScope.Name.StartsWith("GenClause", StringComparison.OrdinalIgnoreCase);

                    // Preferenza (2026-10-01, fix collisione nome TYPE/variabile a livello di
                    // modulo — Bugs/(open) - TYPE-TopLevel-NonRegistrato-Come-Tipo-
                    // Modulo.md): questo metodo risolve SOLO riferimenti-VALORE (VariableExpr/
                    // ArrayAccessExpr/FieldAccessExpr/Target — mai un nome-di-tipo dopo AS, che
                    // si risolve altrove). Se un nome come "p" collide case-insensitive con un
                    // TYPE "P" dichiarato nello stesso scope (es. script senza SUB Main, dove
                    // TYPE e DIM condividono lo scope di modulo — dentro una procedura restano
                    // scope separati, nessuna collisione), tra i candidati VISIBILI preferiamo
                    // quello che NON è una dichiarazione di modulo (TYPE/CLASS/PROCEDURE/
                    // INTERFACE/ENUM): un riferimento a "p" in un'espressione è sempre un uso-
                    // valore, mai un uso-tipo. A parità (nessun candidato non-modulo), il
                    // comportamento resta quello originale (primo visibile).
                    int? preferredVisibleId = null;
                    int? firstVisibleId = null;
                    foreach (var id in ids)
                    {
                        var symbol = program.Symbols.GetSymbol(id);
                        var declPos = symbol.DeclarationNode.Position;
                        Log(
                            $"[RESOLVER]       - Candidato ID {id}. Uso a [{usagePosition.Line},{usagePosition.Column}], Dichiarazione a [{declPos.Line},{declPos.Column}]",
                            log);

                        // Regola di visibilità:
                        // 1. La dichiarazione è in una riga precedente.
                        // 2. La dichiarazione è nella stessa riga ma in una colonna precedente.
                        // 3. ECCEZIONE: Nelle comprensioni, accettiamo l'iteratore sulla stessa riga anche se la colonna è successiva.
                        bool isVisible = declPos.Line < usagePosition.Line ||
                                         (declPos.Line == usagePosition.Line && (declPos.Column < usagePosition.Column || isComprehension));

                        if (!isVisible) continue;
                        firstVisibleId ??= id;
                        if (!IsModuleTypeDeclarationNode(symbol.DeclarationNode))
                        {
                            preferredVisibleId = id;
                            break;
                        }
                    }
                    if (preferredVisibleId.HasValue)
                    {
                        Log($"[RESOLVER]         -> TROVATO (Position-Aware, preferenza valore su tipo)! Restituisco ID {preferredVisibleId.Value}.", log);
                        return preferredVisibleId.Value;
                    }
                    if (firstVisibleId.HasValue)
                    {
                        Log($"[RESOLVER]         -> TROVATO (Position-Aware)! Restituisco ID {firstVisibleId.Value}.", log);
                        return firstVisibleId.Value;
                    }

                    // --- LOGICA DI SHADOWING ---
                    // Se il nome esiste in questo scope ma nessun candidato è visibile (es. variabile locale
                    // dichiarata dopo l'uso fuori da una comprensione), dobbiamo ARRESTARE la ricerca.
                    // Questo impedisce al Resolver di "saltare" la variabile locale e trovare erroneamente
                    // una variabile globale o di un ciclo precedente con lo stesso nome (Leak del simbolo).
                    Log($"[RESOLVER]       - Candidato trovato in questo scope ma non ancora visibile. Arresto ricerca per Shadowing.", log);
                    return -1;
                }
                else
                {
                    // Negli scope genitori: stessa preferenza valore-su-tipo di sopra (nessun
                    // controllo di visibilità per colonna qui, come da comportamento originale).
                    var preferredParentId = ids.FirstOrDefault(id =>
                        !IsModuleTypeDeclarationNode(program.Symbols.GetSymbol(id).DeclarationNode), -1);
                    var chosenParentId = preferredParentId >= 0 ? preferredParentId : ids[0];
                    Log($"[RESOLVER]     -> TROVATO (Scope Genitore)! Restituisco ID {chosenParentId}.", log);
                    return chosenParentId;
                }
            }

            currentId = currentScope.ParentId;
            isFirstScope = false;
        }

        Log($"[RESOLVER]   -> Ricerca locale/scope-up per '{name}' fallita. Avvio fallback su IMPORT.", log);

        // --- FASE 2: Fallback - Ricerca nei membri importati selettivamente nel modulo corrente ---
        if (usagePosition.FileId != -1 &&
            program.FileIdToPathMap.TryGetValue(usagePosition.FileId, out var modulePath) &&
            program.LoadedModules.TryGetValue(modulePath, out var currentModule))
        {
            Log($"[RESOLVER]   -> Controllo IMPORT selettivi nel modulo '{Path.GetFileName(modulePath)}'.", log);
            foreach (var import in currentModule.Imports.Where(i => i.Mode == ImportMode.SpecificMembers))
            {
                var memberMatch = import.SpecificMembers.FirstOrDefault(m =>
                    (m.Alias?.Lexeme ?? m.OriginalName.Lexeme).Equals(name, StringComparison.OrdinalIgnoreCase));

                if (memberMatch != null)
                {
                    var originalNameToFind = memberMatch.OriginalName.Lexeme;
                    Log(
                        $"[RESOLVER]     -> Trovata corrispondenza per '{name}' nell'IMPORT di '{import.NamespaceName.Lexeme}'. Cerco il simbolo originale '{originalNameToFind}'.",
                        log);

                    var nsSymbolIds = program.Symbols.GetSymbolIdsByNamespace(import.NamespaceName.Lexeme);
                    if (nsSymbolIds != null)
                    {
                        foreach (var id in nsSymbolIds)
                        {
                            var symbol = program.Symbols.GetSymbol(id);
                            if (symbol.OriginalName.Equals(originalNameToFind, StringComparison.OrdinalIgnoreCase))
                            {
                                if (symbol.IsPublic)
                                {
                                    Log($"[RESOLVER]         -> TROVATO (Import Selettivo)! Restituisco ID {id}.", log);
                                    return id;
                                }
                            }
                        }
                    }
                }
            }
        }

        Log($"[RESOLVER] Ricerca per '{name}' fallita in tutti gli scope e gli import. Restituisco -1.", log);
        return -1;
    }


    /// <summary>Constant folding: valuta espressioni costanti a link-time.</summary>
    private static Expr? TryFoldConstant(Expr left, Token op, Expr right)
    {
        if (left is NumberExpr nl && right is NumberExpr nr)
        {
            var result = op.Kind switch {
                TokenKind.Plus => nl.Value + nr.Value,
                TokenKind.Minus => nl.Value - nr.Value,
                TokenKind.Multiply => nl.Value * nr.Value,
                TokenKind.Divide => nr.Value != 0 ? nl.Value / nr.Value : double.NaN,
                TokenKind.IntegerDivide => nr.Value != 0 ? Math.Truncate(nl.Value / nr.Value) : double.NaN,
                TokenKind.Mod => nr.Value != 0 ? nl.Value % nr.Value : double.NaN,
                _ => double.NaN };
            if (!double.IsNaN(result)) return new NumberExpr(result, nl.Token);
        }
        if (left is StringExpr sl && right is StringExpr sr && op.Kind == TokenKind.Plus)
            return new StringExpr(sl.Value + sr.Value, sl.Token);
        if (left is BooleanExpr bl && right is BooleanExpr br)
        {
            var result = op.Kind switch { TokenKind.And => bl.Value && br.Value, TokenKind.Or => bl.Value || br.Value, _ => (bool?)null };
            if (result.HasValue) return new BooleanExpr(result.Value, bl.Token);
        }
        /* Le identità algebriche/booleane (`x + 0` → x, `x * 1` → x, `x AND TRUE` → x,
         * `s + ""` → s, …) NON vengono applicate: cambierebbero la FORMA dell'espressione, e il
         * canone di SIZEOF è basato sulla forma (una variabile ha un tipo statico misurabile, una
         * espressione vale -1). Con il fold, `SIZEOF(i + 0)` dava 2 su IL/PCode/VM-C e -1 su
         * C99Native (divergenza misurata 2026-11-01). Restano solo i fold
         * letterale-con-letterale, che non hanno effetti collaterali. */
        return null;
    }

    private static Expr? Opt(Expr? e, BasicProgram program) => e != null ? ResolveExpression(e, program) : null;

    private static MessageAction ResolveMessageAction(MessageAction action, BasicProgram program)
    {
        return action switch
        {
            MessageCallAction ca => ca,
            _ => action
        };
    }
}