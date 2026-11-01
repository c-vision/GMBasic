// FILE: Interpreter.Linker.Binder.cs
// ReSharper disable UnusedParameter.Local

using System.Collections.Immutable;

// ReSharper disable RedundantArgumentDefaultValue

namespace Basic;

// Lo stesso vale per RegisterGlobalVariables, RegisterLocalConstants, RegisterGlobalConstants, RegisterFieldMember.

/// <summary>
/// Contiene la logica di linking per il compilatore.
/// Questa classe è statica e stateless, e opera sull'oggetto BasicProgram
/// dopo che la fase di parsing è stata completata.
/// </summary>
public static partial class Linker
{
    // ================================================================================================================
    //                                         FASE 2: BINDER
    // ================================================================================================================
    //
    // OBIETTIVO E RESPONSABILITÀ:
    // Questa fase del compilatore ha un compito unico e cruciale: costruire la SymbolTable completa e "timbrare"
    // l'Abstract Syntax Tree (AST) con le informazioni di scope. È l'unica fase che gestisce attivamente la
    // navigazione degli scope (`EnterScope`/`ExitScope`) per replicare la struttura lessicale del codice sorgente.
    //
    // COSA FA IL BINDER (FASE 2):
    // 1.  ATTRAVERSAMENTO STATEFUL DELL'AST: Percorre l'AST "ingenuo" prodotto dalla FASE 1.
    // 2.  NAVIGAZIONE DEGLI SCOPE: Chiama `program.Symbols.EnterScope()` quando entra in un namespace, procedura
    //     o blocco (`FOR`, `IF`, `TRY`, etc.) e `program.Symbols.ExitScope()` quando esce. Questo mantiene
    //     uno stato interno (`_currentScopeId`) nella SymbolTable che riflette sempre il contesto lessicale corrente.
    // 3.  REGISTRAZIONE DI TUTTI I SIMBOLI: Per ogni dichiarazione (esplicita o implicita) che incontra, invoca
    //     `program.Symbols.RegisterSymbol()`. Questo include:
    //     - Procedure, Classi, Tipi (UDT).
    //     - Parametri di procedure e membri di classi/UDT.
    //     - Dichiarazioni esplicite con `DIM`, `LOCAL`, `STATIC`, `GLOBAL`, `CONST`.
    //     - Variabili di ciclo `FOR`.
    //     - Variabili di eccezione `CATCH ex`.
    //     - Variabili create implicitamente da assegnazioni `LET` a un nome non esistente.
    //     - Variabili create implicitamente dal passaggio a un parametro `BYREF` di una procedura.
    // 4.  ARRICCHIMENTO PARZIALE DELL'AST: Quando un simbolo viene registrato, il Binder aggiorna il NODO AST
    //     corrispondente (es. `VariableDeclaration`, `ProcedureDefinition`) con il `SymbolId` appena creato.
    // 5.  TIMBRATURA DEGLI SCOPE ID: Per OGNI nodo `AstNode` (`Stmt`, `Expr`, `Decl`) che attraversa o crea,
    //     imposta la sua proprietà `ScopeId` con il valore dello scope correntemente attivo nella SymbolTable.
    //
    // COSA NON FA IL BINDER (DELEGATO ALLA FASE 2.5 - RESOLVER):
    // 1.  NON RISOLVE GLI USI DEI SIMBOLI: Se incontra un'espressione come `x + y`, il Binder NON tenta di
    //     cercare i `SymbolId` per `x` e `y`. Lascia i `SymbolId` dei `VariableExpr` a `-1`. La sua unica
    //     preoccupazione è registrare le DICHIARAZIONI.
    // 2.  NON RISOLVE AMBIGUITÀ: Non si occupa di risolvere chiamate a funzioni sovraccaricate o di distinguere
    //     una chiamata a funzione da un accesso ad array (es. `Nome(...)`).
    // 3.  NON ESEGUE INFERENZA DI TIPO COMPLESSA: Sebbene risolva i tipi espliciti (`AS Cerchio`), non inferisce
    //     il tipo di una variabile da un'assegnazione (es. `LET x = "ciao"`). Registra la variabile con `InferredType`.
    //
    // OUTPUT:
    // - Una `SymbolTable` completa con tutti i simboli del programma.
    // - Un AST "semi-arricchito" dove:
    //   a) Tutte le dichiarazioni hanno `SymbolId` e `ScopeId` corretti.
    //   b) Tutti gli altri nodi (`Stmt`, `Expr`) hanno il loro `ScopeId` corretto.
    //   c) I riferimenti/usi dei simboli nelle espressioni hanno ancora `SymbolId = -1`.
    //
    // ================================================================================================================

    private const bool BindingLog = false;

    /// <summary>
    /// [FASE 2] Punto di ingresso principale per il processo di binding e popolamento della SymbolTable.
    /// Questo metodo agisce come orchestratore per la seconda fase del ciclo di vita della compilazione.
    ///
    /// Responsabilità Architetturali:
    /// 1.  Gestione del Ciclo di Vita dei Moduli: Itera attraverso tutti i moduli caricati (`program.LoadedModules`)
    ///     in ordine deterministico (alfabetico per path) per garantire che la registrazione dei simboli sia
    ///     riproducibile.
    /// 2.  Isolamento delle Modifiche: Crea un nuovo dizionario di moduli aggiornati, evitando modifiche concorrenti
    ///     alla collezione originale durante l'iterazione.
    /// 3.  Delega del Lavoro: Invoca `BindModule` per ogni singolo file sorgente. `BindModule` è il vero motore
    ///     che scende nel dettaglio dell'AST.
    /// 4.  Persistenza dello Stato: Una volta che tutti i moduli sono stati processati e i loro AST sono stati
    ///     "arricchiti" con i `SymbolId` (per le dichiarazioni) e gli `ScopeId` (per tutti i nodi), aggiorna
    ///     l'oggetto `BasicProgram` sostituendo i vecchi moduli "ingenui" (uscita Fase 1) con i nuovi moduli
    ///     "legati" (uscita Fase 2).
    ///
    /// Nota sulla Concorrenza:
    /// Sebbene questo metodo operi sequenzialmente per garantire il determinismo degli ID dei simboli, la
    /// struttura è preparata per eventuali future parallelizzazioni, dato che ogni modulo viene processato
    /// in modo relativamente isolato, con la SymbolTable che agisce come risorsa condivisa thread-safe (se necessario).
    ///
    /// Stato Iniziale Atteso:
    /// - `program.LoadedModules` contiene AST sintatticamente corretti ma privi di significato semantico
    ///   (tutti i `SymbolId` sono -1, gli `ScopeId` non sono impostati).
    /// - `program.Symbols` contiene solo i simboli built-in e i namespace di base.
    ///
    /// Stato Finale Garantito:
    /// - `program.Symbols` è completamente popolata con tutte le definizioni del codice utente.
    /// - `program.LoadedModules` contiene AST dove ogni nodo di dichiarazione punta al suo record nella SymbolTable
    ///   e ogni nodo è consapevole del suo scope lessicale.
    /// </summary>
    /// <param name="program">L'istanza di `BasicProgram` che contiene lo stato della compilazione.
    /// Verrà modificata in-place aggiornando la lista dei moduli e la SymbolTable.</param>
    /// <summary>
    /// Ordina i moduli caricati per il binding (FASE 2) rispettando il grafo delle dipendenze
    /// IMPORT, invece del solo ordine alfabetico del percorso file.
    /// </summary>
    /// <remarks>
    /// BUG (Classi-Distruttore-Ereditarieta-CrossFile, "BUG-1/2/3/5"): il codice precedente
    /// ordinava i moduli con `OrderBy(kvp => kvp.Key)` (puro ordine alfabetico del percorso).
    /// Se il file che IMPORTA un namespace (es. `Auto INHERITS Mezzi.Veicolo` in `X.bas`) ordina
    /// alfabeticamente PRIMA del file che lo DICHIARA (`Mezzi.inc`), il binder registra la classe
    /// derivata PRIMA della sua base — la classe base non è ancora nella SymbolTable quando la
    /// derivata viene collegata. Sintomo osservato: `SELF.Marca` letto da un metodo EREDITATO
    /// (`Veicolo.GetMarca`, mai ridefinito da `Auto`) legge un offset di campo DIVERSO (e quindi
    /// un valore vuoto/sbagliato) a seconda che il file main si chiami "T.bas" (funziona, > "Mezzi"
    /// alfabeticamente) o "ClassesInheritsCrossFile.bas" (rotto, < "Mezzi" alfabeticamente) — la
    /// causa non è il NOME del file, è l'ORDINE DI BINDING che quel nome produce.
    /// Fix: topological sort (Kahn) sul grafo "namespace dichiarato in file B" -> "file A che lo
    /// importa" (B deve essere bindato prima di A); tie-break alfabetico tra moduli indipendenti
    /// per preservare il determinismo del comportamento precedente. Import circolari o file non
    /// raggiunti dal grafo (non dovrebbero verificarsi) ricadono in coda in ordine alfabetico,
    /// come comportamento precedente, invece di essere persi silenziosamente.
    /// </remarks>
    private static List<KeyValuePair<string, BasicModule>> OrderModulesForBinding(BasicProgram program)
    {
        // Namespace -> file che lo dichiarano (scansione dei Declarations di ogni modulo,
        // stessa estrazione di namespace usata da BindModule sotto).
        var namespaceToFiles = new Dictionary<string, List<string>>(StringComparer.OrdinalIgnoreCase);
        foreach (var (path, module) in program.LoadedModules)
        {
            var namespacesInModule = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
            foreach (var declNode in module.Declarations)
            {
                var ns = declNode switch
                {
                    Decl d => d.DeclaredNamespace,
                    UserDefinedType udt => udt.DeclaredNamespace,
                    ClassDefinition cd => cd.DeclaredNamespace,
                    InterfaceDefinition idef => idef.DeclaredNamespace,
                    _ => null
                };
                if (ns != null) namespacesInModule.Add(ns);
            }
            foreach (var ns in namespacesInModule)
            {
                if (!namespaceToFiles.TryGetValue(ns, out var list))
                    namespaceToFiles[ns] = list = new List<string>();
                list.Add(path);
            }
        }

        // Grafo delle dipendenze: providerFile -> [file che lo importano] (providerFile deve
        // essere bindato prima). inDegree = quante dipendenze NON ancora bindate ha ogni file.
        var dependents = new Dictionary<string, List<string>>(StringComparer.OrdinalIgnoreCase);
        var inDegree = new Dictionary<string, int>(StringComparer.OrdinalIgnoreCase);
        foreach (var path in program.LoadedModules.Keys)
            inDegree[path] = 0;

        foreach (var (path, imports) in program.ImportsByFile)
        {
            if (!program.LoadedModules.ContainsKey(path)) continue;
            foreach (var import in imports)
            {
                if (!namespaceToFiles.TryGetValue(import.NamespaceName.Lexeme, out var providerFiles)) continue;
                foreach (var providerFile in providerFiles)
                {
                    if (string.Equals(providerFile, path, StringComparison.OrdinalIgnoreCase)) continue;
                    if (!dependents.TryGetValue(providerFile, out var list))
                        dependents[providerFile] = list = new List<string>();
                    if (list.Contains(path, StringComparer.OrdinalIgnoreCase)) continue;
                    list.Add(path);
                    inDegree[path]++;
                }
            }
        }

        // Kahn's algorithm; SortedSet dà il tie-break alfabetico tra moduli senza dipendenze
        // reciproche, preservando il determinismo del vecchio ordinamento puramente alfabetico.
        var ready = new SortedSet<string>(
            inDegree.Where(kv => kv.Value == 0).Select(kv => kv.Key), StringComparer.OrdinalIgnoreCase);
        var result = new List<KeyValuePair<string, BasicModule>>(program.LoadedModules.Count);
        while (ready.Count > 0)
        {
            var current = ready.Min!;
            ready.Remove(current);
            result.Add(new KeyValuePair<string, BasicModule>(current, program.LoadedModules[current]));
            if (!dependents.TryGetValue(current, out var deps)) continue;
            foreach (var dep in deps)
            {
                if (--inDegree[dep] == 0) ready.Add(dep);
            }
        }

        // Import circolari (o file non raggiunti): non dovrebbero verificarsi, ma un ciclo
        // lascerebbe simboli non processati — accoda quanto resta in ordine alfabetico invece
        // di perderlo silenziosamente (stesso comportamento del vecchio ordinamento).
        if (result.Count != program.LoadedModules.Count)
        {
            var processed = new HashSet<string>(result.Select(kv => kv.Key), StringComparer.OrdinalIgnoreCase);
            foreach (var (path, module) in program.LoadedModules.OrderBy(kv => kv.Key, StringComparer.OrdinalIgnoreCase))
                if (processed.Add(path))
                    result.Add(new KeyValuePair<string, BasicModule>(path, module));
        }

        return result;
    }

    public static void PopulateSymbolTable(BasicProgram program)
    {
        Log("[BINDER] === INIZIO FASE 2: Registrazione Simboli e Timbratura ScopeId ===", BindingLog);

        var orderedModules = OrderModulesForBinding(program);

        // --- FASE 2.A: HOISTING DEI GLOBAL ---
        // Prima di qualsiasi binding, eseguiamo una passata su tutti i moduli per registrare
        // solo le dichiarazioni `GLOBAL`, ovunque si trovino.
        Log("[BINDER]   -> Inizio Sotto-Fase 2.A: Hoisting delle dichiarazioni GLOBAL...", BindingLog);
        var globalVisitor = new GlobalDeclarationVisitor(program);
        foreach (var (_, module) in orderedModules)
        {
            globalVisitor.VisitModule(module);
        }

        Log("[BINDER]   -> Fine Sotto-Fase 2.A.", BindingLog);


        // --- FASE 2.B: BINDING COMPLETO ---
        // Ora eseguiamo il binding completo, modulo per modulo. La SymbolTable
        // contiene già tutti i simboli globali, prevenendo la creazione di duplicati impliciti.
        Log("[BINDER]   -> Inizio Sotto-Fase 2.B: Binding completo dei moduli...", BindingLog);
        var updatedModules = new Dictionary<string, BasicModule>(StringComparer.OrdinalIgnoreCase);
        foreach (var (path, module) in orderedModules)
        {
            Log($"[BINDER]     - Processo modulo: {Path.GetFileName(path)}", BindingLog);
            updatedModules[path] = BindModule(module, program);
        }

        // Persisti l'AST aggiornato in BasicProgram
        foreach (var (path, module) in updatedModules)
        {
            program.LoadedModules[path] = module;
        }

        Log("[BINDER] === FINE FASE 2 ===", BindingLog);
    }

    /// <summary>
    /// Esegue il processo di binding su un singolo `BasicModule`.
    /// Questo metodo è responsabile di impostare lo scope di namespace iniziale per il modulo e di orchestrare
    /// il binding delle dichiarazioni e degli statement.
    ///
    /// Responsabilità Architetturali:
    /// 1.  Reset dello Scope: Chiama `program.Symbols.ResetScope()` per garantire che l'analisi inizi sempre
    ///     dallo scope globale (`_global`), evitando contaminazioni da moduli precedenti.
    /// 2.  Gestione dei Namespace: Determina il namespace di appartenenza del modulo (tramite `program.GetNamespaceForModule`)
    ///     e chiama `program.Symbols.EnterScope` per posizionarsi nel contesto corretto prima di registrare qualsiasi simbolo.
    ///     Questo è fondamentale perché in GM-BASIC i file fisici sono mappati su namespace logici.
    /// 3.  Binding Dichiarazioni Top-Level: Itera su `module.Declarations` (la lista di nodi come FUNCTION, CLASS, TYPE
    ///     trovati dal parser) e invoca `BindDeclaration`. Questo registra i simboli "globali" o "di modulo" nella SymbolTable.
    /// 4.  Binding Statement Body: Itera su `module.Statements` (il codice eseguibile numerato o main-less) e invoca
    ///     `BindStatementList`. Questo gestisce variabili locali implicite o esplicite nel corpo del modulo.
    /// 5.  Gestione Uscita Scope: Assicura che `ExitScope` venga chiamato se è stato aperto un namespace, mantenendo
    ///     l'integrità dello stack degli scope.
    /// 6.  Immutabilità dell'AST: Non modifica il modulo originale in-place, ma restituisce una nuova istanza di `BasicModule`
    ///     contenente le collezioni di nodi aggiornate (i nodi originali vengono sostituiti dalle loro versioni "timbrate").
    ///
    /// Regola Aurea della FASE 2:
    /// In questo metodo NON viene eseguita alcuna risoluzione di espressioni. I nodi vengono solo attraversati per
    /// trovare definizioni e assegnare ScopeId. Le espressioni rimangono "opache".
    /// </summary>
    /// <param name="module">Il `BasicModule` originale con l'AST grezzo dalla FASE 1.</param>
    /// <param name="program">L'istanza del programma che contiene la `SymbolTable`.</param>
    /// <returns>Un nuovo `BasicModule` con l'AST completamente timbrato con gli ID di scope e i SymbolId delle dichiarazioni.</returns>
    private static BasicModule BindModule(BasicModule module, BasicProgram program)
    {
        module = AttachModuleOperators(module);
        program.Symbols.ResetScope();
        string? currentActiveNamespace = null;

        // Passa il flag IsStrict ai metodi di binding
        var isStrict = module.IsStrict;

        // 1. Binding delle Dichiarazioni con gestione dinamica del Namespace
        var boundDeclarationsBuilder = ImmutableArray.CreateBuilder<AstNode>();
        foreach (var declNode in module.Declarations)
        {
            // --- LOG AGGIUNTO ---
            var nsInfo = declNode switch
            {
                Decl d => d.DeclaredNamespace,
                DeclarationStmt ds => ds.Declarations.FirstOrDefault()?.DeclaredNamespace,
                ConstStmt cs => cs.Declarations.FirstOrDefault()?.DeclaredNamespace,
                _ => "N/A"
            };
            Log(
                $"[BINDER-NS-TRACE] Processo nodo: {declNode.GetType().Name} ('{declNode}'). Namespace annotato: '{nsInfo ?? "null"}'");
            // --- FINE LOG ---

            // Estrae il namespace in modo robusto da nodi contenitore ---
            string? nodeNamespace = null;
            switch (declNode)
            {
                case Decl d: // Function, Class, UDT, Enum, Delegate, etc.
                    nodeNamespace = d.DeclaredNamespace;
                    break;
                case UserDefinedType udt:
                    nodeNamespace = udt.DeclaredNamespace;
                    break;
                case ClassDefinition cd:
                    nodeNamespace = cd.DeclaredNamespace;
                    break;
                case InterfaceDefinition idef:
                    // Z2: le INTERFACE non sono un `Decl` (estendono DataType) — senza questo
                    // case il binder NON entrava nello scope del NAMESPACE per un'interfaccia
                    // PUBLIC, quindi `_globalSymbolIdsByNamespace` (popolato solo quando lo
                    // scope corrente è il namespace, SymbolTable.RegisterSymbol) non la
                    // includeva e `N.IFoo` falliva ("tipo non trovato") mentre `N.Bar` (case
                    // ClassDefinition) funzionava.
                    nodeNamespace = idef.DeclaredNamespace;
                    break;
                case DeclarationStmt ds: // DIM, GLOBAL
                    nodeNamespace = ds.Declarations.FirstOrDefault()?.DeclaredNamespace;
                    break;
                case ConstStmt cs: // CONST
                    nodeNamespace = cs.Declarations.FirstOrDefault()?.DeclaredNamespace;
                    break;
            }

            // Se il nodo dichiara un namespace diverso da quello attivo, cambia scope.
            if (nodeNamespace != currentActiveNamespace)
            {
                // Se eravamo già in un namespace, usciamo.
                if (currentActiveNamespace != null)
                {
                    program.Symbols.ExitScope();
                }

                // Se il nuovo nodo ha un namespace, entriamo.
                if (nodeNamespace != null)
                {
                    program.Symbols.EnterScope(nodeNamespace, SymbolTable.ScopeType.Namespace);
                }

                currentActiveNamespace = nodeNamespace;
            }

            // Esegue il binding della dichiarazione nel contesto corrente.
            boundDeclarationsBuilder.Add(BindDeclaration(declNode, program, isStrict));
        }

        // Chiude l'ultimo namespace rimasto aperto, se presente.
        if (currentActiveNamespace != null)
        {
            program.Symbols.ExitScope();
        }

        // 2. Binding degli Statement (Codice Eseguibile)
        // Nota: Il codice eseguibile top-level in BASIC tipicamente risiede nello scope globale
        // o nel namespace di default del file. Qui assumiamo che segua l'ultimo namespace attivo
        // o, più comunemente per i moduli script, che non sia dentro un namespace esplicito.
        // Per sicurezza e retrocompatibilità, resettiamo a scope globale o usiamo il namespace dominante del file.

        program.Symbols.ResetScope();
        var dominantNamespace = program.GetNamespaceForModule(module.FilePath);
        if (dominantNamespace != SymbolTable.GlobalScopeName)
        {
            program.Symbols.EnterScope(dominantNamespace, SymbolTable.ScopeType.Namespace);
        }

        var boundStatements = module.Statements.ToDictionary(
            kvp => kvp.Key,
            kvp => BindListOfStatements([..kvp.Value], program, isStrict).ToList()
        );

        if (dominantNamespace != SymbolTable.GlobalScopeName)
        {
            program.Symbols.ExitScope();
        }

        var boundModule = new BasicModule(module.FilePath, module.IsStrict, module.SourceMap, boundStatements, module.Imports,
            boundDeclarationsBuilder.ToImmutable(), module.LibraryName);
        boundModule.ExecutionOrder = module.ExecutionOrder;
        return boundModule;
    }

    /// <summary>
    /// Associa gli operatori dichiarati a livello modulo al tipo indicato dal primo parametro.
    /// </summary>
    /// <param name="module">Modulo contenente tipi e operatori ancora non legati.</param>
    /// <returns>Modulo con gli operatori incorporati nelle rispettive dichiarazioni di tipo.</returns>
    private static BasicModule AttachModuleOperators(BasicModule module)
    {
        var moduleOperators = module.Declarations.OfType<OperatorDefinition>().ToArray();
        if (moduleOperators.Length == 0) return module;

        var operatorsByOwner = moduleOperators
            .GroupBy(GetModuleOperatorOwnerName, StringComparer.OrdinalIgnoreCase)
            .ToDictionary(group => group.Key, group => group.ToImmutableArray(), StringComparer.OrdinalIgnoreCase);

        var declarations = ImmutableArray.CreateBuilder<AstNode>(module.Declarations.Length - moduleOperators.Length);
        foreach (var declaration in module.Declarations)
        {
            switch (declaration)
            {
                case OperatorDefinition:
                    continue;
                case UserDefinedType udt when operatorsByOwner.Remove(udt.OriginalName.Lexeme, out var udtOperators):
                    declarations.Add(udt with { Operators = [..udt.Operators, ..udtOperators] });
                    break;
                case ClassDefinition classDefinition when
                    operatorsByOwner.Remove(classDefinition.OriginalName.Lexeme, out var classOperators):
                    var members = classOperators.Select(op => new OperatorMember(
                        op with { Flags = op.Flags | SymbolFlags.IsStatic },
                        op.Flags.HasFlag(SymbolFlags.IsPublic) ? AccessModifier.Public : AccessModifier.Private));
                    declarations.Add(classDefinition with { Members = [..classDefinition.Members, ..members] });
                    break;
                default:
                    declarations.Add(declaration);
                    break;
            }
        }

        if (operatorsByOwner.Count > 0)
        {
            var unresolved = operatorsByOwner.First();
            throw CreateError(unresolved.Value[0].OperatorToken,
                $"Impossibile associare OPERATOR al tipo sorgente '{unresolved.Key}'.");
        }

        var normalized = new BasicModule(module.FilePath, module.IsStrict, module.SourceMap, module.Statements,
            module.Imports, declarations.ToImmutable(), module.LibraryName);
        normalized.ExecutionOrder = module.ExecutionOrder;
        return normalized;
    }

    /// <summary>
    /// Ricava il nome del tipo proprietario di un operatore modulo dal primo parametro.
    /// </summary>
    /// <param name="operatorDefinition">Operatore modulo da associare.</param>
    /// <returns>Nome non qualificato del tipo sorgente.</returns>
    private static string GetModuleOperatorOwnerName(OperatorDefinition operatorDefinition)
    {
        if (operatorDefinition.Parameters.IsEmpty)
            throw CreateError(operatorDefinition.OperatorToken,
                "Un OPERATOR dichiarato a livello modulo richiede almeno un parametro sorgente.");

        var sourceTypeName = operatorDefinition.Parameters[0].DataType switch
        {
            UnresolvedQualifiedType unresolved => unresolved.QualifiedName.FullName,
            UserDefinedType udt => udt.OriginalName.Lexeme,
            ClassType classType => classType.Definition.OriginalName.Lexeme,
            var dataType => dataType.ToString() ?? string.Empty
        };
        var separator = sourceTypeName.LastIndexOf('.');
        return separator >= 0 ? sourceTypeName[(separator + 1)..] : sourceTypeName;
    }

    /// <summary>
    /// Funge da dispatcher per il binding delle dichiarazioni di alto livello (quelle che si trovano
    /// direttamente all'interno di un namespace o nello scope globale del modulo).
    ///
    /// Responsabilità Architetturali:
    /// 1.  Identificazione del Tipo: Ispeziona il tipo runtime del nodo AST (`UserDefinedType`, `ClassDefinition`, etc.).
    /// 2.  Routing: Invia il nodo al metodo di registrazione specifico (`RegisterUdt`, `RegisterClass`, `RegisterProcedure`).
    ///     Questi metodi specifici contengono la logica per registrare il simbolo principale e scendere ricorsivamente
    ///     nei membri (es. campi di un tipo, corpo di una funzione).
    /// 3.  Gestione Variabili Globali: Intercetta `DeclarationStmt`, `ConstStmt` e `LetStmt` a livello di modulo
    ///     per registrare variabili e costanti globali (o di namespace).
    /// 4.  Fallback e Timbratura: Per qualsiasi nodo non specificamente gestito (es. `ImportStmt`), si limita a
    ///     crearne una copia "timbrata" con lo `ScopeId` corrente. Questo garantisce che *ogni* nodo nell'AST finale
    ///     abbia uno ScopeId valido, requisito fondamentale per il Resolver (Fase 2.5).
    ///
    /// Nota sulle Espressioni:
    /// Anche se questo metodo gestisce `ConstStmt` che ha un valore iniziale, NON risolve l'espressione del valore.
    /// L'espressione viene lasciata intatta (con SymbolId -1) per essere processata nelle fasi successive.
    /// </summary>
    /// <param name="node">Il nodo di dichiarazione grezzo proveniente dalla FASE 1.</param>
    /// <param name="program">L'istanza del programma che contiene la `SymbolTable`.</param>
    /// <param name="isStrict">OPTION STRICT enabled or disabled flag</param>
    /// <returns>Il nodo `AstNode` "arricchito" dopo la registrazione del suo simbolo e la timbratura dello scope.</returns>
    private static AstNode BindDeclaration(AstNode node, BasicProgram program, bool isStrict)
    {
        var currentScopeId = program.Symbols.GetCurrentScope().Id;
        return node switch
        {
            UserDefinedType udt => RegisterUdt(program, udt, isStrict),
            ClassDefinition classDef => RegisterClass(program, classDef, isStrict),
            InterfaceDefinition interfaceDef => RegisterInterface(program, interfaceDef, isStrict),
            EnumDefinition enumDef => RegisterEnum(program, enumDef),
            DelegateDefinition delegateDef => RegisterDelegate(program, delegateDef),
            ProcedureDefinition proc => RegisterProcedure(program, proc, isStrict),
            DeclarationStmt declStmt => RegisterGlobalVariables(program, declStmt, bindExpressions: false, isStrict),
            ConstStmt constStmt => RegisterGlobalConstants(program, constStmt, bindExpressions: false, isStrict),
            LetStmt letStmt => RegisterLetVariables(program, letStmt),
            _ => node with { ScopeId = currentScopeId }
        };
    }

    /// <summary>
    /// Registra una definizione di tipo delegato (DELEGATE) nella SymbolTable.
    /// Un delegato viene registrato come un alias di tipo (`IsTypeAlias`) il cui
    /// tipo sottostante è un `FunctionPointerType` che corrisponde alla sua firma.
    /// </summary>
    /// <param name="program">L'istanza del programma.</param>
    /// <param name="delegateDef">Il nodo DelegateDefinition da registrare.</param>
    /// <returns>Il nodo `DelegateDefinition` aggiornato con il suo SymbolId.</returns>
    private static DelegateDefinition RegisterDelegate(BasicProgram program, DelegateDefinition delegateDef)
    {
        var parentScopeId = program.Symbols.GetCurrentScope().Id;

        // Costruisce il tipo di dato che questo delegato rappresenta: un puntatore a funzione.
        var functionPointerType = new FunctionPointerType(
            [..delegateDef.Parameters.Select(p => p.DataType)],
            delegateDef.ReturnType
        );

        // Aggiunge il flag IsTypeAlias per marcare questo simbolo come un nome alternativo per un tipo.
        var finalFlags = delegateDef.Flags | SymbolFlags.IsTypeAlias;

        // Registra il simbolo.
        var symbolId = program.Symbols.RegisterSymbol(
            delegateDef.OriginalName,
            finalFlags,
            functionPointerType, // Il tipo "reale" è il puntatore a funzione.
            delegateDef
        );

        Log(
            $"[BINDER]  -> Registrato DELEGATE '{delegateDef.OriginalName.Lexeme}' come Alias di Tipo per '{functionPointerType}' (ID: {symbolId}).",
            BindingLog);

        // I parametri di un delegato non creano un nuovo scope né sono simboli in sé,
        // sono solo parte della definizione del tipo. Non c'è bisogno di registrarli.

        var finalDelegateDef = delegateDef with
        {
            SymbolId = symbolId,
            ScopeId = parentScopeId
        };

        program.Symbols.UpdateSymbolDeclaration(symbolId, finalDelegateDef);
        return finalDelegateDef;
    }

    /// <summary>
    /// Registra una definizione di enumerazione (ENUM) e tutti i suoi membri nella SymbolTable,
    /// seguendo l'architettura semplificata.
    /// L'enum viene registrato come un alias di tipo e i suoi membri come costanti globali qualificate
    /// nello scope del genitore (namespace o globale).
    /// </summary>
    /// <param name="program">L'istanza del programma.</param>
    /// <param name="enumDef">Il nodo EnumDefinition da registrare.</param>
    /// <returns>Il nodo `EnumDefinition` aggiornato con i SymbolId corretti.</returns>
    private static EnumDefinition RegisterEnum(BasicProgram program, EnumDefinition enumDef)
    {
        var parentScopeId = program.Symbols.GetCurrentScope().Id;

        // FASE 1: Registra il nome dell'ENUM stesso come un ALIAS DI TIPO per il suo tipo sottostante.
        var enumAliasFlags = enumDef.Flags | SymbolFlags.IsTypeAlias;
        var enumType = new EnumType(enumDef.Name, enumDef.UnderlyingType);
        var enumSymbolId = program.Symbols.RegisterSymbol(enumDef.OriginalName, enumAliasFlags, enumType, enumDef);

        Log(
            $"[BINDER]  -> Registrato ENUM '{enumDef.OriginalName.Lexeme}' come Alias di Tipo per '{enumDef.UnderlyingType}' (ID: {enumSymbolId}).",
            BindingLog);

        // FASE 2: Itera sui membri e li registra come COSTANTI nello scope corrente (namespace).
        var updatedMembers = ImmutableArray.CreateBuilder<EnumMember>();
        foreach (var member in enumDef.Members)
        {
            var qualifiedName = $"{enumDef.OriginalName.Lexeme}.{member.Name.Lexeme}";
            var qualifiedToken = new Token(TokenKind.Identifier, qualifiedName, null, member.Name.Position);

            const SymbolFlags memberFlags = SymbolFlags.IsEnumMember
                                                        | SymbolFlags.IsConst
                                                        | SymbolFlags.IsPublic
                                                        | SymbolFlags.IsGlobal;

            var memberId = program.Symbols.RegisterSymbol(qualifiedToken, memberFlags, enumDef.UnderlyingType, member);

            var finalMember = member with { SymbolId = memberId, ScopeId = parentScopeId };
            program.Symbols.UpdateSymbolDeclaration(memberId, finalMember);
            updatedMembers.Add(finalMember);

            Log(
                $"[BINDER]    -> Membro ENUM '{qualifiedName}' registrato come Costante (ID: {memberId}) in Scope {parentScopeId}.",
                BindingLog);
        }

        // FASE 3: Restituisci la definizione aggiornata con gli ID.
        var finalEnumDef = enumDef with
        {
            SymbolId = enumSymbolId,
            ScopeId = parentScopeId,
            Members = updatedMembers.ToImmutable()
        };

        program.Symbols.UpdateSymbolDeclaration(enumSymbolId, finalEnumDef);
        return finalEnumDef;
    }

    /// <summary>
    /// Registra una definizione di procedura (FUNCTION, SUB, GENERATOR, DECLARE) nella SymbolTable
    /// e attraversa il suo corpo per registrare parametri e variabili locali.
    ///
    /// Responsabilità Architetturali:
    /// 1.  Registrazione Simbolo Principale: Crea un `Symbol` per la procedura stessa nello scope corrente (modulo/namespace)
    ///     utilizzando `program.Symbols.RegisterSymbol`. Assegna un ID univoco.
    /// 2.  Creazione Scope Procedurale: Invoca `program.Symbols.EnterScope` con tipo `ScopeType.Procedure`.
    ///     Questo crea un nuovo contesto isolato per i parametri e le variabili locali.
    /// 3.  Registrazione Parametri: Itera sui parametri della procedura. Per ognuno, crea un `Symbol` nello scope
    ///     della procedura. È qui che i nomi dei parametri vengono legati a ID univoci.
    /// 4.  Binding del Corpo: Chiama `BindStatementList` sul corpo della procedura (`proc.Body`). Questo avvia
    ///     la discesa ricorsiva all'interno delle istruzioni della funzione, gestendo blocchi annidati (`IF`, `FOR`)
    ///     e registrando variabili locali (`DIM`).
    /// 5.  Aggiornamento Dichiarazione: Una volta completato il binding interno, aggiorna il nodo `ProcedureDefinition`
    ///     con i nuovi parametri (dotati di ID) e il nuovo corpo (dotato di ScopeId) e lo salva nella SymbolTable
    ///     tramite `UpdateSymbolDeclaration`.
    /// 6.  Uscita Scope: Chiude lo scope della procedura con `ExitScope`.
    ///
    /// Note Importanti:
    /// - Non risolve i tipi di ritorno o dei parametri se non sono primitivi.
    /// - Non risolve le espressioni di default dei parametri.
    /// </summary>
    /// <param name="program">L'istanza del programma.</param>
    /// <param name="proc">Il nodo di definizione della procedura grezzo.</param>
    /// <param name="isStrict">OPTION STRICT enabled or disabled flag</param>
    /// <returns>Il nodo `ProcedureDefinition` arricchito.</returns>
    private static ProcedureDefinition RegisterProcedure(BasicProgram program, ProcedureDefinition proc, bool isStrict)
    {
        // Nota: DataType qui è puramente indicativo o primitivo. La risoluzione complessa avviene in Fase 3.
        var returnType = proc switch
        {
            FunctionDefinition fd => fd.ReturnType,
            GeneratorDefinition gd => new GeneratorType(gd.YieldType),
            ExternalProcedureDefinition epd => epd.ReturnType ?? new VoidType(),
            _ => new VoidType()
        };

        var procScopeId = program.Symbols.GetCurrentScope().Id;
        var procFlags = proc.Flags;
        if (!proc.Attributes.IsDefaultOrEmpty && proc.Attributes.Any(a => a.Lexeme.Equals("Pure", StringComparison.OrdinalIgnoreCase)))
        {
            procFlags |= SymbolFlags.IsPure;
        }

        var symbolId = program.Symbols.RegisterSymbol(proc.OriginalName, procFlags, returnType, proc);
        var procWithIds = proc with { SymbolId = symbolId, ScopeId = procScopeId, Flags = procFlags };

        Log(
            $"[BINDER]    -> Registrata Procedura '{proc.OriginalName.Lexeme}' con ID {symbolId} in Scope {procScopeId}.",
            BindingLog);

        program.Symbols.EnterScope(proc.OriginalName.Lexeme, SymbolTable.ScopeType.Procedure, symbolId);
        try
        {
            // Se la procedura è un metodo di una classe, registra 'SELF' e 'ME'.
            if (proc.Parent is ClassDefinition ownerClass)
            {
                var selfToken = new Token(TokenKind.Self, "SELF", null, proc.Position);
                program.Symbols.RegisterSymbol(
                    selfToken,
                    SymbolFlags.IsLocal | SymbolFlags.IsSynthetic,
                    new ClassType(ownerClass),
                    selfToken
                );
                var meToken = new Token(TokenKind.Identifier, "ME", null, proc.Position);
                program.Symbols.RegisterSymbol(
                    meToken,
                    SymbolFlags.IsLocal | SymbolFlags.IsSynthetic,
                    new ClassType(ownerClass),
                    meToken
                );
            }
            // UDT method: registra SELF con UserDefinedType.
            if (proc.Parent is UserDefinedType udtOwner)
            {
                var udtToken = new Token(TokenKind.Self, "SELF", null, proc.Position);
                program.Symbols.RegisterSymbol(
                    udtToken,
                    SymbolFlags.IsLocal | SymbolFlags.IsSynthetic,
                    udtOwner,
                    udtToken
                );
            }
            var procedureBodyScopeId = program.Symbols.GetCurrentScope().Id;
            var updatedParameters = proc.Parameters.Select(param =>
            {
                var paramFlags = param.Flags | SymbolFlags.IsParameter;
                var paramSymbolId = program.Symbols.RegisterSymbol(param.Name, paramFlags, param.DataType, param);
                var finalParam = param with
                {
                    SymbolId = paramSymbolId,
                    Flags = paramFlags,
                    ScopeId = procedureBodyScopeId
                };
                program.Symbols.UpdateSymbolDeclaration(paramSymbolId, finalParam);
                Log(
                    $"[BINDER]      -> Parametro '{param.OriginalName.Lexeme}' registrato con ID {paramSymbolId} in Scope {procedureBodyScopeId}.",
                    BindingLog);
                return finalParam;
            }).ToImmutableArray();

            var updatedBody = BindListOfStatements(proc.Body, program, isStrict);

            ProcedureDefinition finalProc = procWithIds switch
            {
                FunctionDefinition fd => fd with { Parameters = updatedParameters, Body = updatedBody },
                GeneratorDefinition gd => gd with { Parameters = updatedParameters, Body = updatedBody },
                ExternalProcedureDefinition epd => epd with { Parameters = updatedParameters },
                _ => throw new InvalidOperationException("Tipo di procedura non supportato durante la registrazione.")
            };

            program.Symbols.UpdateSymbolDeclaration(symbolId, finalProc);
            return finalProc;
        }
        finally
        {
            program.Symbols.ExitScope();
        }
    }

    /// <summary>
    /// Attraversa una lista di statement in modo stateful, gestendo la navigazione degli scope
    /// e delegando il binding di ogni singolo statement a `BindStatement`.
    /// Questa funzione è il motore ricorsivo per l'analisi dei corpi di procedure e blocchi.
    ///
    /// Responsabilità Architetturali:
    /// 1.  Iterazione Lineare: Scorre la lista di istruzioni fornita (che rappresenta un blocco di codice).
    /// 2.  Delega: Per ogni istruzione, chiama `BindStatement`.
    /// 3.  Collezione Risultati: Raccoglie le versioni "arricchite" restituite da `BindStatement`.
    /// 4.  Immutabilità: Restituisce una nuova `ImmutableArray` contenente la sequenza trasformata.
    ///
    /// Questo metodo è fondamentale per garantire che l'ordine di dichiarazione venga rispettato
    /// (le variabili dichiarate all'inizio del blocco sono visibili alle istruzioni successive)
    /// e per propagare il contesto di scope correttamente attraverso i blocchi annidati.
    /// </summary>
    /// <param name="statements">La lista di statement grezzi da analizzare.</param>
    /// <param name="program">L'istanza del programma.</param>
    /// <param name="isStrict">OPTION STRICT enabled or disabled flag</param>
    /// <returns>Una nuova lista immutabile di statement con ScopeId assegnati e dichiarazioni registrate.</returns>
    private static ImmutableArray<Stmt> BindListOfStatements(ImmutableArray<Stmt> statements, BasicProgram program,
        bool isStrict)
    {
        var boundStatements = ImmutableArray.CreateBuilder<Stmt>();
        foreach (var stmt in statements)
        {
            boundStatements.Add(BindStatement(stmt, program, isStrict));
        }

        return boundStatements.ToImmutable();
    }

    /// <summary>
    /// Verifica se un target è una variabile non definita e, in tal caso, la crea implicitamente
    /// nello scope corrente. Restituisce il target aggiornato con il nuovo SymbolId (se creato) 
    /// o il target originale. Da usare prima di BindTarget per comandi di I/O.
    /// </summary>
    /// <summary>
    /// "Casa" di una variabile IMPLICITA (nome mai dichiarato, creato da una prima assegnazione/uso): lo scope della PROCEDURA
    /// (o del modulo/namespace), mai un blocco IF/FOR/WHILE/SELECT/TRY. In BASIC una variabile implicita vive per tutta la
    /// procedura: `IF c THEN r = 2` poi `PRINT r` deve vedere r. Registrandola nello scope del blocco spariva all'uscita dal blocco
    /// (PCode/VM-C leggevano 0, IL e C99Native 2: divergenza fra backend).
    /// </summary>
    private static (int ScopeId, SymbolFlags Storage) ImplicitVariableHome(BasicProgram program)
    {
        var scope = program.Symbols.GetCurrentScope();
        while (scope.Type == SymbolTable.ScopeType.Block && scope.ParentId >= 0)
            scope = program.Symbols.GetScope(scope.ParentId);
        var storage = scope.Type == SymbolTable.ScopeType.Procedure ? SymbolFlags.IsLocal : SymbolFlags.IsGlobal;
        return (scope.Id, storage);
    }

    private static Target EnsureImplicitTarget(BasicProgram program, Target target)
    {
        if (target is VariableTarget { SymbolId: < 0 } vt)
        {
            // Verifica se il simbolo esiste già (scope lookup)
            var existingSymbolIds = program.Symbols.FindSymbolInScope(vt.NameCleaned.Lexeme);

            if (existingSymbolIds == null || existingSymbolIds.Count == 0)
            {
                // Inferisci il tipo dal suffisso ($=string, %=integer, &=long, !=single, #=double)
                var lastChar = vt.NameSuffixed.Lexeme.Length > 0 ? vt.NameSuffixed.Lexeme[^1] : '\0';
                var inferredType = (DataType)(lastChar switch
                {
                    '$' => new StringType(),
                    '%' => new IntegerType(),
                    '&' => new LongType(),
                    '!' => new SingleType(),
                    '#' => new DoubleType(),
                    _ => null!
                }) ?? new InferredType();

                // Non esiste: Creazione Implicita Locale — nello scope della procedura, non del blocco corrente.
                var symbolId = program.Symbols.RegisterSymbol(
                    vt.NameCleaned,
                    SymbolFlags.IsLocal,
                    inferredType,
                    vt,
                    ImplicitVariableHome(program).ScopeId
                );

                Log(
                    $"[BINDER]        -> Variabile Implicita (IO) '{vt.NameCleaned.Lexeme}' registrata con ID {symbolId}.",
                    BindingLog);

                var boundTarget = vt with { SymbolId = symbolId };
                program.Symbols.UpdateSymbolDeclaration(symbolId, boundTarget);
                return boundTarget;
            }
        }

        return target;
    }

    /// <summary>
    /// Verifica se un'espressione è una variabile non definita e, in tal caso, la crea implicitamente.
    /// Usato specificamente per comandi come SPRITE LOAD che accettano variabili in posizione di espressione.
    /// </summary>
    private static Expr EnsureImplicitVariableExpr(BasicProgram program, Expr expr)
    {
        if (expr is VariableExpr { SymbolId: < 0 } ve)
        {
            var existingSymbolIds = program.Symbols.FindSymbolInScope(ve.NameCleaned.Lexeme);

            if (existingSymbolIds == null || existingSymbolIds.Count == 0)
            {
                var symbolId = program.Symbols.RegisterSymbol(
                    ve.NameCleaned,
                    SymbolFlags.IsLocal,
                    new InferredType(),
                    ve.NameCleaned,
                    ImplicitVariableHome(program).ScopeId
                );

                // Creiamo una dichiarazione fittizia per la symbol table
                var decl = new VariableDeclaration(
                    -1,
                    ve.NameSuffixed,
                    ve.NameCleaned,
                    ImmutableArray<Expr>.Empty,
                    new InferredType(),
                    null,
                    false,
                    true,
                    SymbolFlags.IsLocal);
                program.Symbols.UpdateSymbolDeclaration(symbolId, decl);

                Log(
                    $"[BINDER]        -> Variabile Implicita (Expr-IO) '{ve.NameCleaned.Lexeme}' registrata con ID {symbolId}.",
                    BindingLog);

                return ve with { SymbolId = symbolId };
            }
        }

        return expr;
    }

    /// <summary>
    /// Esegue il binding su un singolo statement. Registra i simboli dichiarati, gestisce l'entrata/uscita
    /// dagli scope di blocco e restituisce il nodo Stmt aggiornato con i nuovi SymbolId e ScopeId.
    ///
    /// Responsabilità Architetturali:
    /// 1.  Timbratura Scope: Assegna a `stmt.ScopeId` l'ID dello scope corrente della SymbolTable.
    /// 2.  Gestione Blocchi (`IF`, `SELECT`, `TRY`):
    ///     - Riconosce istruzioni che introducono nuovi scope.
    ///     - Chiama `EnterScope` prima di processare il corpo del blocco.
    ///     - Chiama ricorsivamente `BindStatementList` per il corpo.
    ///     - Chiama `ExitScope` al termine.
    /// 3.  Gestione Loop (`FOR`, `DO`, `WHILE`):
    ///     - Gestisce `EnterLoopStmt` e `ExitLoopStmt` per creare/distruggere scope di blocco per i cicli.
    ///     - Registra la variabile di controllo del ciclo `FOR` come simbolo locale allo scope del ciclo.
    /// 4.  Registrazione Variabili Locali (`DIM`, `CONST`, `LET`):
    ///     - Intercetta `DeclarationStmt` e registra i simboli nello scope corrente.
    ///     - Intercetta `LetStmt` per registrare variabili implicite (se non esistono già).
    /// 5.  Pass-through: Per istruzioni semplici (`PRINT`, `GOTO`), si limita a timbrare lo scope
    ///     senza analizzare le espressioni (vietato in Fase 2).
    ///
    /// Nota sui Tipi:
    /// Nessuna inferenza o risoluzione di tipi complessi avviene qui. I tipi vengono registrati come
    /// trovati dal parser o come `InferredType`.
    /// </summary>
    /// <param name="stmt">Lo statement da processare.</param>
    /// <param name="program">L'istanza del programma.</param>
    /// <param name="isStrict">OPTION STRICT enabled or disabled flag</param>
    /// <returns>Lo statement aggiornato con le informazioni di scope/simbolo.</returns>
    private static Stmt BindStatement(Stmt stmt, BasicProgram program, bool isStrict)
    {
        var currentScopeId = program.Symbols.GetCurrentScope().Id;

        // Prima timbriamo lo statement stesso con lo scope corrente
        var scopedStmt = stmt with { ScopeId = currentScopeId };

        switch (scopedStmt)
        {
            // DECLARATIONS
            case DeclarationStmt declStmt:
            {
                var currentScope = program.Symbols.GetCurrentScope();
                var isExplicitGlobal = declStmt.Keyword.Kind == TokenKind.Global;
                var isInLocalScope =
                    currentScope.Type is SymbolTable.ScopeType.Procedure or SymbolTable.ScopeType.Block;

                if (isExplicitGlobal && isInLocalScope)
                {
                    // Caso speciale: GLOBAL dentro una procedura.
                    // Dobbiamo registrare il simbolo nello scope del Namespace/Global, non in quello locale.
                    // "Saltiamo" fuori dagli scope locali temporaneamente.
                    var scopeStack = new Stack<SymbolTable.Scope>();
                    var tempScope = currentScope;

                    // Risale la gerarchia fino a trovare uno scope non locale (Namespace o Global)
                    while (tempScope.Type is SymbolTable.ScopeType.Procedure or SymbolTable.ScopeType.Block &&
                           tempScope.ParentId != -1)
                    {
                        scopeStack.Push(tempScope);
                        tempScope = program.Symbols.GetScope(tempScope.ParentId);
                    }

                    // Esegue l'uscita fisica dagli scope per allineare lo stato della SymbolTable
                    for (var i = 0; i < scopeStack.Count; i++) program.Symbols.ExitScope();

                    // Registra le variabili nello scope target (Global/Namespace)
                    var finalStmt = RegisterGlobalVariables(program, declStmt, bindExpressions: true, isStrict);

                    // Ripristina lo stato originale degli scope rientrando in ordine inverso
                    foreach (var scopeToRestore in scopeStack)
                    {
                        program.Symbols.EnterScope(scopeToRestore.Name, scopeToRestore.Type,
                            scopeToRestore.OwnerSymbolId);
                    }

                    return finalStmt;
                }

                // Logica standard di dispatching basata sul tipo di scope.
                return isInLocalScope
                    ? RegisterLocalVariables(program, declStmt, isStrict)
                    : RegisterGlobalVariables(program, declStmt, bindExpressions: true, isStrict);
            }

            case ConstStmt constStmt:
            {
                // Verifica lo scope per decidere se la costante è locale o globale/namespace.
                // Questo allinea il comportamento a quello di DeclarationStmt.
                var currentScopeType = program.Symbols.GetCurrentScope().Type;
                return currentScopeType is SymbolTable.ScopeType.Procedure or SymbolTable.ScopeType.Block
                    ? RegisterLocalConstants(program, constStmt)
                    : RegisterGlobalConstants(program, constStmt, bindExpressions: true, isStrict);
            }

            case LetStmt letStmt:
            {
                // 1. Gestione variabili implicite.
                var regLet = RegisterImplicitLocalVariables(program, letStmt, isStrict);

                // 2. Binding ricorsivo di TUTTI i figli.
                var boundAssignments = regLet.Assignments.Select(a => new Assignment(
                    BindTarget(a.Target, program, isStrict),
                    BindExpression(a.Value, program, isStrict)
                )).ToImmutableArray();

                // 3. Ricostruisci il nodo LetStmt finale, assicurandoti che abbia lo ScopeId corretto.
                // NON USIAMO 'with' per evitare di propagare lo ScopeId vecchio (-1).
                var finalLetStmt = new LetStmt(boundAssignments, regLet.LetToken)
                {
                    ScopeId = currentScopeId
                };
                Log($"[BINDER-DEBUG] Uscendo dal case LetStmt. ScopeId del nodo finale: {finalLetStmt.ScopeId}",
                    BindingLog);
                return finalLetStmt;
            }

            // LOOPS
            case EnterLoopStmt enterLoop:
                program.Symbols.EnterScope($"Loop_{enterLoop.StartToken.Line}", SymbolTable.ScopeType.Block);
                // EnterLoopStmt appartiene allo scope ESTERNO (già timbrato sopra).
                return scopedStmt;

            case ExitLoopStmt exitLoop:
                program.Symbols.ExitScope();
                // ExitLoopStmt segna l'uscita, quindi appartiene al nuovo scope (il genitore).
                return exitLoop with { ScopeId = program.Symbols.GetCurrentScope().Id };

            case ForStmt forStmt:
            {
                // 1. Registra la variabile del ciclo nello scope corrente
                var registeredForStmt = RegisterForLoopVariable(program, forStmt);

                // 2. Timbra ricorsivamente le espressioni dei limiti
                return registeredForStmt with
                {
                    Start = BindExpression(registeredForStmt.Start, program, isStrict),
                    End = BindExpression(registeredForStmt.End, program, isStrict),
                    Step = registeredForStmt.Step != null
                        ? BindExpression(registeredForStmt.Step, program, isStrict)
                        : null
                };
            }

            case ForEachInitStmt s:
            {
                // Lo scope del ciclo è già stato aperto dal precedente EnterLoopStmt.
                var loopScopeId = program.Symbols.GetCurrentScope().Id;

                // Registra le variabili iteratore in questo scope.
                foreach (var varToken in s.Variables)
                {
                    // Normalizza il nome (es. k$ -> k) per garantire la risoluzione corretta.
                    var (baseName, _) = SplitNameAndSuffix(varToken.Lexeme);
                    var cleanToken = varToken with { Lexeme = baseName };

                    program.Symbols.RegisterSymbol(
                        cleanToken,
                        SymbolFlags.IsLocal,
                        s.DeclaredType ?? new InferredType(),
                        cleanToken
                    );
                }

                // Lega l'espressione della collezione.
                return s with
                {
                    CollectionExpr = BindExpression(s.CollectionExpr, program, isStrict),
                    ScopeId = loopScopeId // Lo statement stesso vive nello scope del loop
                };
            }

            // BLOCKS
            case TryCatchStmt tryStmt:
                return BindTryCatchStatement(program, tryStmt, isStrict);

            case IfStmt ifStmt:
                return BindIfStatement(program, ifStmt, isStrict);

            case SelectStmt selectStmt:
                return BindSelectStatement(program, selectStmt, isStrict);

            case WithStmt withStmt:
                return BindWithStatement(program, withStmt, isStrict);

            case UsingStmt usingStmt:
                return BindUsingStatement(program, usingStmt, isStrict);

            // Le tuple erano escluse dal binder e i VariableExpr del RHS restavano
            // con SymbolId=-1. Il backend IL (e l'Executor) non possono distinguere
            // tali riferimenti da variabili inesistenti; il binding ricorsivo qui
            // preserva lo stesso scope delle normali LET/DIM.
            case TupleAssignmentStmt tupleAssignment:
                return tupleAssignment with
                {
                    TupleExpression = BindExpression(tupleAssignment.TupleExpression, program, isStrict)
                };

            case TupleDestructuringStmt tupleDestructuring:
                return RegisterTupleDestructuringVariables(program, tupleDestructuring, isStrict);

            // --- ISTRUZIONI CON ESPRESSIONI DA LEGARE ---

            case ExpressionStmt s:
                return s with { Expression = BindExpression(s.Expression, program, isStrict) };

            case ReturnStmt s:
                return s with { Value = s.Value != null ? BindExpression(s.Value, program, isStrict) : null };

            case ConditionalJumpStmt s:
                Log(
                    $"[BINDER-TRACE] Trovato ConditionalJumpStmt alla riga {s.Position.Line}. Eseguo BindExpression sulla condizione.",
                    BindingLog);
                var boundCondition = BindExpression(s.Condition, program, isStrict);
                return new ConditionalJumpStmt(boundCondition, s.InitialTargetLabel, s.JumpWhenTrue)
                {
                    TargetLabel = s.TargetLabel,
                    ScopeId = s.ScopeId // Assicuriamoci che lo ScopeId dello statement stesso sia preservato
                };

            case OnJumpStmt s:
                return s with { IndexExpression = BindExpression(s.IndexExpression, program, isStrict) };

            case ErrorStmt s:
                return s with
                {
                    ErrorCode = BindExpression(s.ErrorCode, program, isStrict),
                    Description = s.Description != null ? BindExpression(s.Description, program, isStrict) : null
                };

            case ReadStmt s:
                return s with
                {
                    // Applica EnsureImplicitTarget prima di BindTarget
                    Variables =
                    [
                        ..s.Variables.Select(v => BindTarget(EnsureImplicitTarget(program, v), program, isStrict))
                    ]
                };

            case DataStmt s:
                return s with { Values = [..s.Values.Select(v => BindExpression(v, program, isStrict))] };

            case RedimStmt s:
                return s with
                {
                    ArrayTarget = BindTarget(s.ArrayTarget, program, isStrict),
                    Dimensions = [..s.Dimensions.Select(d => BindExpression(d, program, isStrict))]
                };

            case SwapStmt s:
                return s with
                {
                    Left = BindTarget(s.Left, program, isStrict), Right = BindTarget(s.Right, program, isStrict)
                };

            case RandomizeStmt s:
                return s with { Seed = s.Seed != null ? BindExpression(s.Seed, program, isStrict) : null };

            case RunStmt s:
                return s with { Command = BindExpression(s.Command, program, isStrict) };

            case KeyGenStmt s:
                return s with
                {
                    Algorithm = BindExpression(s.Algorithm, program, isStrict),
                    PrivateKeyHandle = BindTarget(s.PrivateKeyHandle, program, isStrict),
                    PublicKeyHandle = BindTarget(s.PublicKeyHandle, program, isStrict),
                    KeySize = s.KeySize != null ? BindExpression(s.KeySize, program, isStrict) : null
                };

            case KeyCloseStmt s:
                return s with { KeyHandle = BindExpression(s.KeyHandle, program, isStrict) };

            case OnKeyStmt s:
                return s with
                {
                    KeyExpression = s.KeyExpression != null ? BindExpression(s.KeyExpression, program, isStrict) : null
                    // Action (Call, Goto) contiene Expr o Token, ma non c'è un BindAction generico.
                    // Se Action è CallAction, dovremmo legare la chiamata.
                    // Tuttavia, OnKeyAction è un record separato, non Stmt.
                    // Lasciamo così, il Resolver gestirà l'azione.
                };

            case MacroStmt s:
                return s with { Expression = BindExpression(s.Expression, program, isStrict) };

            case YieldStmt s:
                return s with { ValueToYield = BindExpression(s.ValueToYield, program, isStrict) };

            case LocalProcedureStmt s:
                // La procedura locale deve essere registrata come simbolo.
                // Usiamo RegisterProcedure che gestisce anche il binding del corpo.
                var boundProc = RegisterProcedure(program, s.Procedure, isStrict);
                return s with { Procedure = boundProc };

            case LocalTypeStmt s:
                // Se incontriamo una dichiarazione di tipo locale, dobbiamo registrarla.
                // RegisterUdt gestirà la creazione del simbolo del tipo nello scope corrente (quello della procedura).
                var registeredUdt = RegisterUdt(program, s.Definition, isStrict);
                // Restituiamo un nuovo LocalTypeStmt che contiene la definizione del tipo aggiornata con il suo SymbolId.
                return s with { Definition = registeredUdt };

            case BaseConstructorCallStmt s:
                return s with { Args = [..s.Args.Select(a => BindExpression(a, program, isStrict))] };

            // CONSOLE
            case PrintStmt s:
                return s with
                {
                    Arguments =
                    [
                        ..s.Arguments.Select(a => a.Expression != null
                            ? a with { Expression = BindExpression(a.Expression, program, isStrict) }
                            : a)
                    ]
                };

            case InputStmt s:
                return s with
                {
                    // Applica EnsureImplicitTarget prima di BindTarget
                    Variables =
                    [
                        ..s.Variables.Select(v => BindTarget(EnsureImplicitTarget(program, v), program, isStrict))
                    ],
                    Prompt = s.Prompt != null ? BindExpression(s.Prompt, program, isStrict) : null,
                    MaskExpr = s.MaskExpr != null ? BindExpression(s.MaskExpr, program, isStrict) : null
                };

            case LineInputStmt s:
                return s with
                {
                    // Applica EnsureImplicitTarget prima di BindTarget
                    Variable = BindTarget(EnsureImplicitTarget(program, s.Variable), program, isStrict),
                    Prompt = s.Prompt != null ? BindExpression(s.Prompt, program, isStrict) : null,
                    MaskExpr = s.MaskExpr != null ? BindExpression(s.MaskExpr, program, isStrict) : null
                };

            case GetKeyboardStmt s:
                return s with { Variable = BindTarget(EnsureImplicitTarget(program, s.Variable), program, isStrict) };

            case InkeyStmt s:
                return s with
                {
                    Variable = BindTarget(EnsureImplicitTarget(program, s.Variable), program, isStrict),
                    TimeoutExpr = s.TimeoutExpr != null ? BindExpression(s.TimeoutExpr, program, isStrict) : null
                };

            // --- AUDIO & GRAPHICS ---

            case SoundStmt s:
                return s with
                {
                    Frequency = BindExpression(s.Frequency, program, isStrict),
                    Duration = BindExpression(s.Duration, program, isStrict),
                    Volume = s.Volume != null ? BindExpression(s.Volume, program, isStrict) : null
                };

            case PlayStmt:
                // PlayStmt ha un'Action che contiene espressioni.
                // Per completezza dovremmo scendere nell'Action.
                // Ma Action non è Stmt. Per ora timbriamo lo statement,
                // il Resolver scenderà nell'Action.
                return scopedStmt;

            case AudioControlStmt s:
                return s with { Argument = s.Argument != null ? BindExpression(s.Argument, program, isStrict) : null };

            case SetFilterStmt s:
                return s with
                {
                    Voice = BindExpression(s.Voice, program, isStrict),
                    Type = BindExpression(s.Type, program, isStrict),
                    Cutoff = BindExpression(s.Cutoff, program, isStrict),
                    Resonance = BindExpression(s.Resonance, program, isStrict),
                };

            // --- GRAFICA & CONSOLE ---
            case LocateStmt s:
                return s with
                {
                    LineExpr = BindExpression(s.LineExpr, program, isStrict),
                    ColumnExpr = BindExpression(s.ColumnExpr, program, isStrict)
                };

            case ScreenStmt s:
                return s with
                {
                    Mode = BindExpression(s.Mode, program, isStrict),
                    ActivePage = s.ActivePage != null ? BindExpression(s.ActivePage, program, isStrict) : null,
                    VisiblePage = s.VisiblePage != null ? BindExpression(s.VisiblePage, program, isStrict) : null,
                    CloseRepl = s.CloseRepl != null ? BindExpression(s.CloseRepl, program, isStrict) : null
                };

            case TextModeStmt tms:
                return tms with { Mode = BindExpression(tms.Mode, program, isStrict) };

            case VectorCommandStmt s:
                var vNs = "VECTOR";
                var vSyms = program.Symbols.GetSymbolIdsByNamespace(vNs);
                var vId = vSyms?.Select(id => program.Symbols.GetSymbol(id))
                    .FirstOrDefault(sym =>
                        sym.OriginalName.Equals(s.Command.Lexeme, StringComparison.OrdinalIgnoreCase))?.Id ?? -1;
                return s with
                {
                    Arguments = [..s.Arguments.Select(a => BindExpression(a, program, isStrict))],
                    CommandSymbolId = vId,
                    ScopeId = currentScopeId
                };

            case MatrixCommandStmt s:
                var mNs = "MATRIX";
                var mSyms = program.Symbols.GetSymbolIdsByNamespace(mNs);
                var mId = mSyms?.Select(id => program.Symbols.GetSymbol(id))
                    .FirstOrDefault(sym =>
                        sym.OriginalName.Equals(s.Command.Lexeme, StringComparison.OrdinalIgnoreCase))?.Id ?? -1;
                return s with
                {
                    Arguments = [..s.Arguments.Select(a => BindExpression(a, program, isStrict))],
                    CommandSymbolId = mId,
                    ScopeId = currentScopeId
                };

            case SaveScreenStmt s:
                return s with { Name = BindExpression(s.Name, program, isStrict) };

            case RestoreScreenStmt s:
                return s with { Name = BindExpression(s.Name, program, isStrict) };

            case PaletteStmt s:
                return s with
                {
                    Index = BindExpression(s.Index, program, isStrict), R = BindExpression(s.R, program, isStrict),
                    G = BindExpression(s.G, program, isStrict), B = BindExpression(s.B, program, isStrict)
                };

            case PaletteUseStmt s:
                return s with { Name = BindExpression(s.Name, program, isStrict) };

            case PaletteDefineStmt s:
                return s with
                {
                    Name = BindExpression(s.Name, program, isStrict),
                    Colors = BindExpression(s.Colors, program, isStrict)
                };

            case PaletteReplaceStmt s:
                return s with
                {
                    Name = BindExpression(s.Name, program, isStrict),
                    Colors = BindExpression(s.Colors, program, isStrict)
                };

            case PaletteSaveStmt s:
                return s with { FileName = BindExpression(s.FileName, program, isStrict) };

            case PaletteLoadStmt s:
                return s with { FileName = BindExpression(s.FileName, program, isStrict) };

            case ClearRectStmt s:
                return s with
                {
                    X1 = BindExpression(s.X1, program, isStrict), Y1 = BindExpression(s.Y1, program, isStrict),
                    X2 = BindExpression(s.X2, program, isStrict), Y2 = BindExpression(s.Y2, program, isStrict)
                };

            case CursorShapeStmt s:
                return s with { Shape = BindExpression(s.Shape, program, isStrict) };

            // --- FILE I/O & DB ---

            case OpenStmt s:
            {
                // Questa è la versione semplificata per I/O standard.
                // Non deve gestire tipi UDT.
                var boundFileName = BindExpression(s.FileName, program, isStrict);
                var boundFileNumber = BindExpression(s.FileNumber, program, isStrict);
                var boundRecordLength =
                    s.RecordLength != null ? BindExpression(s.RecordLength, program, isStrict) : null;
                var boundEncoding = s.EncodingExpr != null ? BindExpression(s.EncodingExpr, program, isStrict) : null;

                return s with
                {
                    FileName = boundFileName,
                    FileNumber = boundFileNumber,
                    RecordLength = boundRecordLength,
                    EncodingExpr = boundEncoding
                };
            }

            case CloseStmt s:
                return s with
                {
                    FileNumbers = [..s.FileNumbers.Select(f => BindExpression(f, program, isStrict))]
                };

            case PrintFileStmt s:
                return s with
                {
                    FileNumber = BindExpression(s.FileNumber, program, isStrict),
                    Arguments =
                    [
                        ..s.Arguments.Select(a => a.Expression != null
                            ? a with { Expression = BindExpression(a.Expression, program, isStrict) }
                            : a)
                    ]
                };

            case WriteFileStmt s:
                return s with
                {
                    FileNumber = BindExpression(s.FileNumber, program, isStrict),
                    Arguments = [..s.Arguments.Select(a => BindExpression(a, program, isStrict))]
                };

            case InputFileStmt s:
                return s with
                {
                    FileNumber = BindExpression(s.FileNumber, program, isStrict),
                    // Aggiunto EnsureImplicitTarget
                    Variables =
                    [
                        ..s.Variables.Select(v => BindTarget(EnsureImplicitTarget(program, v), program, isStrict))
                    ]
                };

            case LineInputFileStmt s:
                return s with
                {
                    FileNumber = BindExpression(s.FileNumber, program, isStrict),
                    // Aggiunto EnsureImplicitTarget
                    Variable = BindTarget(EnsureImplicitTarget(program, s.Variable), program, isStrict)
                };

            case GetFileStmt s:
                return s with
                {
                    FileNumber = BindExpression(s.FileNumber, program, isStrict),
                    RecordNumber = s.RecordNumber != null ? BindExpression(s.RecordNumber, program, isStrict) : null,
                    Destination = BindTarget(s.Destination, program, isStrict)
                };

            case PutFileStmt s:
                return s with
                {
                    FileNumber = BindExpression(s.FileNumber, program, isStrict),
                    RecordNumber = s.RecordNumber != null ? BindExpression(s.RecordNumber, program, isStrict) : null,
                    Source = BindExpression(s.Source, program, isStrict)
                };

            case SeekStmt s:
                return s with
                {
                    FileNumber = BindExpression(s.FileNumber, program, isStrict),
                    PositionExpr = BindExpression(s.PositionExpr, program, isStrict)
                };

            case KillStmt s: return s with { FileName = BindExpression(s.FileName, program, isStrict) };
            case RenameStmt s:
                return s with
                {
                    OldFileName = BindExpression(s.OldFileName, program, isStrict),
                    NewFileName = BindExpression(s.NewFileName, program, isStrict)
                };
            case MkDirStmt s: return s with { DirectoryPath = BindExpression(s.DirectoryPath, program, isStrict) };
            case RmDirStmt s:
                return s with
                {
                    DirectoryPath = BindExpression(s.DirectoryPath, program, isStrict),
                    RecursiveExpr = s.RecursiveExpr != null ? BindExpression(s.RecursiveExpr, program, isStrict) : null
                };
            case ChDirStmt s: return s with { DirectoryPath = BindExpression(s.DirectoryPath, program, isStrict) };
            case CopyStmt s:
                return s with
                {
                    SourceFile = BindExpression(s.SourceFile, program, isStrict),
                    DestinationFile = BindExpression(s.DestinationFile, program, isStrict)
                };
            case MoveStmt s:
                return s with
                {
                    SourceFile = BindExpression(s.SourceFile, program, isStrict),
                    DestinationFile = BindExpression(s.DestinationFile, program, isStrict)
                };
            case XCopyStmt s:
                return s with
                {
                    SourceDirectory = BindExpression(s.SourceDirectory, program, isStrict),
                    DestinationDirectory = BindExpression(s.DestinationDirectory, program, isStrict),
                    RecursiveExpr = s.RecursiveExpr != null ? BindExpression(s.RecursiveExpr, program, isStrict) : null,
                    OverwriteExpr = s.OverwriteExpr != null ? BindExpression(s.OverwriteExpr, program, isStrict) : null
                };

            // --- MEMORY MAP ---

            case MemoryMapStmt s:
                return s with
                {
                    FilePath = BindExpression(s.FilePath, program, isStrict),
                    Handle = BindTarget(s.Handle, program, isStrict)
                };

            case MemoryMapPokeStmt s:
                return s with
                {
                    Handle = BindExpression(s.Handle, program, isStrict),
                    Offset = BindExpression(s.Offset, program, isStrict),
                    Value = BindExpression(s.Value, program, isStrict)
                };

            case MemoryMapWriteBytesStmt s:
                return s with
                {
                    Handle = BindExpression(s.Handle, program, isStrict),
                    Offset = BindExpression(s.Offset, program, isStrict),
                    SourceArray = BindExpression(s.SourceArray, program, isStrict),
                    Count = s.Count != null ? BindExpression(s.Count, program, isStrict) : null
                };

            // --- GOROUTINES ---

            case GoProcedureStmt s:
                return s with
                {
                    Call = BindExpression(s.Call, program, isStrict),
                    WaitGroup = s.WaitGroup != null ? BindExpression(s.WaitGroup, program, isStrict) : null
                };
            case WaitStmt s: return s with { WaitGroup = BindExpression(s.WaitGroup, program, isStrict) };
            case SendStmt s:
                return s with
                {
                    Value = BindExpression(s.Value, program, isStrict),
                    Channel = BindExpression(s.Channel, program, isStrict)
                };
            case ReceiveStmt s:
                return s with
                {
                    Destination = BindTarget(s.Destination, program, isStrict),
                    Channel = BindExpression(s.Channel, program, isStrict),
                    OkFlag = s.OkFlag is null ? null : BindTarget(s.OkFlag, program, isStrict)
                };
            case CloseChannelStmt s:
                return s with { Channel = BindExpression(s.Channel, program, isStrict) };
            case TimeoutStmt s:
                return s with { Duration = BindExpression(s.Duration, program, isStrict) };
            case DoneStmt s: return s with { WaitGroup = BindExpression(s.WaitGroup, program, isStrict) };
            case GoSelectStmt s:
                // I case contengono Stmt, quindi ricorsione necessaria
                return s with
                {
                    Cases =
                    [
                        ..s.Cases.Select(c =>
                            new GoSelectCase(CommunicationStmt: BindStatement(c.CommunicationStmt, program, isStrict),
                                Body: BindListOfStatements(c.Body, program, isStrict)))
                    ],
                    DefaultCase = s.DefaultCase != null
                        ? BindListOfStatements(s.DefaultCase.Value, program, isStrict)
                        : null
                };
            case SleepStmt s: return s with { Duration = BindExpression(s.Duration, program, isStrict) };
            case MutexLockStmt s: return s with { Mutex = BindExpression(s.Mutex, program, isStrict) };
            case MutexUnlockStmt s: return s with { Mutex = BindExpression(s.Mutex, program, isStrict) };
            case DeferStmt s: return s with { Body = BindStatement(s.Body, program, isStrict) };

            // --- JSON ---
            case JsonSaveStmt s:
                return s with
                {
                    Source = BindExpression(s.Source, program, isStrict),
                    FilePath = BindExpression(s.FilePath, program, isStrict)
                };
            case JsonWriteValueStmt s:
                return s with
                {
                    Key = BindExpression(s.Key, program, isStrict), Value = BindExpression(s.Value, program, isStrict),
                    Section = s.Section != null ? BindExpression(s.Section, program, isStrict) : null
                };
            case JsonWriteSectionStmt s:
                return s with
                {
                    Source = BindExpression(s.Source, program, isStrict),
                    Section = s.Section != null ? BindExpression(s.Section, program, isStrict) : null
                };

            // --- ARRAYS ---
            case AAddStmt s:
                return s with
                {
                    Array = BindTarget(s.Array, program, isStrict),
                    Value = BindExpression(s.Value, program, isStrict)
                };
            case AInsStmt s:
                return s with
                {
                    Array = BindTarget(s.Array, program, isStrict),
                    PositionExpr = BindExpression(s.PositionExpr, program, isStrict)
                };
            case ADelStmt s:
                return s with
                {
                    Array = BindTarget(s.Array, program, isStrict),
                    PositionExpr = BindExpression(s.PositionExpr, program, isStrict)
                };
            case ASortStmt s:
                return s with
                {
                    Array = BindTarget(s.Array, program, isStrict),
                    CompareFunction = s.CompareFunction != null
                        ? BindExpression(s.CompareFunction, program, isStrict)
                        : null,
                    Order = s.Order != null ? BindExpression(s.Order, program, isStrict) : null
                };
            case AEvalStmt s:
                return s with
                {
                    Array = BindTarget(s.Array, program, isStrict),
                    ActionFunction = BindExpression(s.ActionFunction, program, isStrict)
                };
            case AFillStmt s:
                return s with
                {
                    Array = BindTarget(s.Array, program, isStrict), Value = BindExpression(s.Value, program, isStrict),
                    Start = s.Start != null ? BindExpression(s.Start, program, isStrict) : null,
                    Count = s.Count != null ? BindExpression(s.Count, program, isStrict) : null
                };
            case ACopyStmt s:
                return s with
                {
                    Source = BindTarget(s.Source, program, isStrict),
                    Destination = BindTarget(s.Destination, program, isStrict),
                    SourceStart = s.SourceStart != null ? BindExpression(s.SourceStart, program, isStrict) : null,
                    Count = s.Count != null ? BindExpression(s.Count, program, isStrict) : null,
                    DestStart = s.DestStart != null ? BindExpression(s.DestStart, program, isStrict) : null
                };

            // ================================================================================
            // --- SQL STATEMENTS ---
            // ================================================================================
            case SqlOpenStmt s:
                program.Symbols.RegisterSymbol(
                    s.OriginalAliasToken,
                    SymbolFlags.IsDynamicScope,
                    new InferredType(),
                    s.OriginalAliasToken
                );
                return s with { FilePath = BindExpression(s.FilePath, program, isStrict) };

            // --- ISAM OPEN: registra alias come simbolo dinamico ---
            case IsamOpenStmt s:
            {
                var existing = program.Symbols.FindSymbolsByName(s.OriginalAliasToken.Lexeme);
                if (existing.Count == 0)
                    program.Symbols.RegisterSymbol(
                        s.OriginalAliasToken,
                        SymbolFlags.IsDynamicScope,
                        new InferredType(),
                        s.OriginalAliasToken);
                var result = s with
                {
                    FileName = BindExpression(s.FileName, program, isStrict),
                    IndexedBy = s.IndexedBy != null
                        ? new IsamIndexedByClause([..s.IndexedBy.KeyFields.Select(f =>
                            f with { KeyExpression = BindExpression(f.KeyExpression, program, isStrict) })])
                        : null
                };
                return result;
            }

            // --- DBF USE: registra alias come simbolo dinamico ---
            case DbfUseStmt s when s.OriginalAliasToken != null:
            {
                var existing = program.Symbols.FindSymbolsByName(s.OriginalAliasToken.Lexeme);
                if (existing.Count == 0)
                    program.Symbols.RegisterSymbol(
                        s.OriginalAliasToken,
                        SymbolFlags.IsDynamicScope,
                        new InferredType(),
                        s.OriginalAliasToken);
                return s with { FileName = BindExpression(s.FileName, program, isStrict) };
            }

            // --- RDD USE: registra alias come simbolo dinamico ---
            case RddUseStmt s when s.AliasToken != null:
            {
                var rddExisting = program.Symbols.FindSymbolsByName(s.AliasToken.Lexeme);
                if (rddExisting.Count == 0)
                    program.Symbols.RegisterSymbol(
                        s.AliasToken,
                        SymbolFlags.IsDynamicScope,
                        new InferredType(),
                        s.AliasToken);
                return s with
                {
                    FileName = BindExpression(s.FileName, program, isStrict),
                    Driver = BindExpression(s.Driver, program, isStrict)
                };
            }

            case SqlCloseStmt s:
                return s with { DbAlias = BindExpression(s.DbAlias, program, isStrict) };
            case SqlExecStmt s:
                return s with
                {
                    DbAlias = BindExpression(s.DbAlias, program, isStrict),
                    Sql = BindExpression(s.Sql, program, isStrict),
                    Parameters = [..s.Parameters.Select(p => BindExpression(p, program, isStrict))]
                };
            case SqlQueryStmt s:
                return s with
                {
                    DbAlias = BindExpression(s.DbAlias, program, isStrict),
                    ResultSet = BindTarget(s.ResultSet, program, isStrict),
                    Sql = BindExpression(s.Sql, program, isStrict),
                    Parameters = [..s.Parameters.Select(p => BindExpression(p, program, isStrict))]
                };
            case SqlBeginTransactionStmt s:
                return s with { DbAlias = BindExpression(s.DbAlias, program, isStrict) };
            case SqlCommitStmt s:
                return s with { DbAlias = BindExpression(s.DbAlias, program, isStrict) };
            case SqlRollbackStmt s:
                return s with { DbAlias = BindExpression(s.DbAlias, program, isStrict) };
            case SqlAttachStmt s:
                return s with
                {
                    DbAlias = BindExpression(s.DbAlias, program, isStrict),
                    AttachPath = BindExpression(s.AttachPath, program, isStrict)
                };
            case SqlDetachStmt s:
                return s with
                {
                    DbAlias = BindExpression(s.DbAlias, program, isStrict),
                    SchemaName = BindExpression(s.SchemaName, program, isStrict)
                };
            case SqlBackupStmt s:
                return s with
                {
                    DbAlias = BindExpression(s.DbAlias, program, isStrict),
                    DestinationPath = BindExpression(s.DestinationPath, program, isStrict)
                };

            // ================================================================================
            // --- VECTORDB (database vettoriale autonoma) ---
            // ================================================================================
            case VectorDbEnsureCollectionStmt s:
                return s with
                {
                    DbPath = BindExpression(s.DbPath, program, isStrict),
                    CollectionName = BindExpression(s.CollectionName, program, isStrict)
                };
            case VectorDbIndexTextStmt s:
                return s with
                {
                    DbPath = BindExpression(s.DbPath, program, isStrict),
                    CollectionName = BindExpression(s.CollectionName, program, isStrict),
                    DocId = BindExpression(s.DocId, program, isStrict),
                    Text = BindExpression(s.Text, program, isStrict)
                };
            case VectorDbIndexMetaStmt s:
                return s with
                {
                    DbPath = BindExpression(s.DbPath, program, isStrict),
                    CollectionName = BindExpression(s.CollectionName, program, isStrict),
                    DocId = BindExpression(s.DocId, program, isStrict),
                    Text = BindExpression(s.Text, program, isStrict),
                    MetaJson = BindExpression(s.MetaJson, program, isStrict)
                };
            case VectorDbSearchStmt s:
                return s with
                {
                    DbPath = BindExpression(s.DbPath, program, isStrict),
                    CollectionName = BindExpression(s.CollectionName, program, isStrict),
                    Query = BindExpression(s.Query, program, isStrict),
                    TopK = BindExpression(s.TopK, program, isStrict),
                    Result = BindTarget(EnsureImplicitTarget(program, s.Result), program, isStrict)
                };
            case VectorDbCountStmt s:
                return s with
                {
                    DbPath = BindExpression(s.DbPath, program, isStrict),
                    CollectionName = BindExpression(s.CollectionName, program, isStrict),
                    Result = BindTarget(EnsureImplicitTarget(program, s.Result), program, isStrict)
                };
            case VectorDbListCollectionsStmt s:
                return s with
                {
                    DbPath = BindExpression(s.DbPath, program, isStrict),
                    Result = BindTarget(EnsureImplicitTarget(program, s.Result), program, isStrict)
                };
            case VectorDbRemoveSourceStmt s:
                return s with
                {
                    DbPath = BindExpression(s.DbPath, program, isStrict),
                    CollectionName = BindExpression(s.CollectionName, program, isStrict),
                    SourceId = BindExpression(s.SourceId, program, isStrict)
                };
            case VectorDbLoadDocumentsStmt s:
                return s with
                {
                    DbPath = BindExpression(s.DbPath, program, isStrict),
                    CollectionName = BindExpression(s.CollectionName, program, isStrict),
                    DirectoryPath = BindExpression(s.DirectoryPath, program, isStrict),
                    Result = BindTarget(EnsureImplicitTarget(program, s.Result), program, isStrict)
                };
            case VectorDbPurgeDeprecatedStmt s:
                return s with
                {
                    DbPath = BindExpression(s.DbPath, program, isStrict),
                    CollectionName = BindExpression(s.CollectionName, program, isStrict)
                };

            // --- TORCH (comandi legacy, nessun valore di ritorno) ---
            case TorchSetDeviceStmt s:
                return s with { Device = BindExpression(s.Device, program, isStrict) };
            case TorchSetLibPathStmt s:
                return s with { LibPath = BindExpression(s.LibPath, program, isStrict) };
            case TorchLoadLibStmt sl: return sl;
            case TorchUnloadLibStmt su: return su;
            case TorchInfoStmt si: return si;

            // ================================================================================
            // --- LEDGER: bind dei target INTO (crea le variabili implicite, es.
            //     `LEDGER VALIDATE h INTO ok` con ok non DIM → SymbolId reale, altrimenti
            //     il backend IL emetteva store su simbolo -1 e il valore andava perso). ---
            // ================================================================================
            case LedgerValidateStmt s:
                return s with
                {
                    Handle = BindExpression(s.Handle, program, isStrict),
                    DestinationVariable = BindTarget(EnsureImplicitTarget(program, s.DestinationVariable), program, isStrict)
                };
            case LedgerVarianceStmt s:
                return s with
                {
                    Handle = BindExpression(s.Handle, program, isStrict),
                    Account = BindExpression(s.Account, program, isStrict),
                    Period = BindExpression(s.Period, program, isStrict),
                    DestinationVariable = BindTarget(EnsureImplicitTarget(program, s.DestinationVariable), program, isStrict)
                };
            case LedgerPlotStmt s:
                return s with
                {
                    Handle = BindExpression(s.Handle, program, isStrict),
                    Query = BindExpression(s.Query, program, isStrict),
                    ChartType = BindExpression(s.ChartType, program, isStrict),
                    DestinationVariable = BindTarget(EnsureImplicitTarget(program, s.DestinationVariable), program, isStrict)
                };
            case LedgerImportStmt s:
                return s with
                {
                    Handle = BindExpression(s.Handle, program, isStrict),
                    CsvPath = BindExpression(s.CsvPath, program, isStrict),
                    MappingRules = BindExpression(s.MappingRules, program, isStrict),
                    DestinationVariable = s.DestinationVariable is null
                        ? null
                        : BindTarget(EnsureImplicitTarget(program, s.DestinationVariable), program, isStrict)
                };
            case LedgerGetStmt s:
                return s with
                {
                    Handle = BindExpression(s.Handle, program, isStrict),
                    TransactionId = s.TransactionId is null ? null : BindExpression(s.TransactionId, program, isStrict),
                    DestinationArray = BindTarget(EnsureImplicitTarget(program, s.DestinationArray), program, isStrict)
                };
            case LedgerRegisterStmt s:
                return s with
                {
                    Handle = BindExpression(s.Handle, program, isStrict),
                    Query = s.Query is null ? null : BindExpression(s.Query, program, isStrict),
                    DestinationArray = BindTarget(EnsureImplicitTarget(program, s.DestinationArray), program, isStrict)
                };
            case LedgerBalanceStmt s:
                return s with
                {
                    Handle = BindExpression(s.Handle, program, isStrict),
                    Query = s.Query is null ? null : BindExpression(s.Query, program, isStrict),
                    DestinationArray = BindTarget(EnsureImplicitTarget(program, s.DestinationArray), program, isStrict)
                };
            case LedgerAccountsStmt s:
                return s with
                {
                    Handle = BindExpression(s.Handle, program, isStrict),
                    RootAccount = s.RootAccount is null ? null : BindExpression(s.RootAccount, program, isStrict),
                    DestinationArray = BindTarget(EnsureImplicitTarget(program, s.DestinationArray), program, isStrict)
                };
            case LedgerAddStmt s:
                return s with
                {
                    Handle = BindExpression(s.Handle, program, isStrict),
                    Date = BindExpression(s.Date, program, isStrict),
                    Description = BindExpression(s.Description, program, isStrict),
                    NewIdVariable = BindTarget(EnsureImplicitTarget(program, s.NewIdVariable), program, isStrict)
                };
            case LedgerEditStmt s:
                return s with
                {
                    Handle = BindExpression(s.Handle, program, isStrict),
                    TransactionId = BindExpression(s.TransactionId, program, isStrict),
                    Date = BindExpression(s.Date, program, isStrict),
                    Description = BindExpression(s.Description, program, isStrict),
                    NewIdVariable = BindTarget(EnsureImplicitTarget(program, s.NewIdVariable), program, isStrict)
                };

            // ================================================================================
            // --- PLOTTING ---
            // ================================================================================
            case ChartPlotStmt s:
                return s with { Args = [..s.Args.Select(a => BindExpression(a, program, isStrict))] };
            case ChartFinancialStmt s:
                return s with { Args = [..s.Args.Select(a => BindExpression(a, program, isStrict))] };
            case ChartConfigStmt s:
                return s with {
                    Handle = BindExpression(s.Handle, program, isStrict),
                    Prop = BindExpression(s.Prop, program, isStrict),
                    Num = s.Num != null ? BindExpression(s.Num, program, isStrict) : null,
                    Str = s.Str != null ? BindExpression(s.Str, program, isStrict) : null };
            case ChartDrawStmt s:
                return s with {
                    Handle = BindExpression(s.Handle, program, isStrict),
                    Op = BindExpression(s.Op, program, isStrict),
                    Args = [..s.Args.Select(a => BindExpression(a, program, isStrict))] };
            case ChartClearStmt s: return s with { Handle = BindExpression(s.Handle, program, isStrict) };
            case ChartCloseStmt s: return s with { Handle = BindExpression(s.Handle, program, isStrict) };
            case ChartRenderStmt s:
                return s with { Args = [..s.Args.Select(a => BindExpression(a, program, isStrict))] };

            // --- CASI BASE ---
            case StopStmt:
            case EndStmt:
            case RemStmt:
            case JumpStmt:
            case GosubStmt:
            case OnErrorResumeStmt:
            case ResumeStmt:
            case LabelStmt:
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
            case HttpListenerLoopStmt:
            case HttpConnectionHandlerStmt:
            case BeginTransactionStmt:
            case CommitStmt:
            case RollbackStmt:
                return scopedStmt;

            // --- ERRORI  ---
            case OnErrorStmt s:
                // Se è un handler procedurale, leghiamo l'espressione di chiamata.
                return s with 
                { 
                    ProcedureCall = s.ProcedureCall != null ? BindExpression(s.ProcedureCall, program, isStrict) : null 
                };

            case RethrowStmt:
                // Il nodo non ha figli espressione, restituiamo la versione timbrata dallo switch superiore.
                return scopedStmt;

            case ThrowStmt s:
                return s with { Value = BindExpression(s.Value, program, isStrict) };
            
            // --- NETWORKING & HTTP  ---

            case TcpOpenStmt s:
                return s with
                {
                    Port = BindExpression(s.Port, program, isStrict), Handle = BindTarget(s.Handle, program, isStrict),
                    Engine = s.Engine != null ? BindExpression(s.Engine, program, isStrict) : null
                };
            case TcpConnectStmt s:
                return s with
                {
                    Host = BindExpression(s.Host, program, isStrict), Port = BindExpression(s.Port, program, isStrict),
                    Handle = BindTarget(s.Handle, program, isStrict),
                    Engine = s.Engine != null ? BindExpression(s.Engine, program, isStrict) : null,
                    Timeout = s.Timeout != null ? BindExpression(s.Timeout, program, isStrict) : null
                };
            case TcpAcceptStmt s:
                return s with
                {
                    ServerHandle = BindExpression(s.ServerHandle, program, isStrict),
                    ClientHandle = BindTarget(s.ClientHandle, program, isStrict),
                    Timeout = s.Timeout != null ? BindExpression(s.Timeout, program, isStrict) : null
                };
            case TcpRecvStmt s:
                return s with
                {
                    ClientHandle = BindExpression(s.ClientHandle, program, isStrict),
                    Buffer = BindTarget(s.Buffer, program, isStrict),
                    BytesReceived = BindTarget(s.BytesReceived, program, isStrict),
                    Timeout = s.Timeout != null ? BindExpression(s.Timeout, program, isStrict) : null
                };
            case TcpSendStmt s:
                return s with
                {
                    ClientHandle = BindExpression(s.ClientHandle, program, isStrict),
                    Data = BindExpression(s.Data, program, isStrict)
                };
            case WebSocketConnectStmt s:
                // Mancava del tutto un case per questo statement: senza BindTarget(s.Handle, ...)
                // il Target della variabile handle non riceve mai un ScopeId valido, e
                // ResolveTarget (Linker.Resolver.cs) non riesce a risolverne il SymbolId per una
                // variabile locale (il suo fallback funziona solo per variabili GLOBAL) — il
                // Target resta con SymbolId=-1 per l'intera compilazione, e l'assegnazione del
                // risultato di WEBSOCKET CONNECT finisce scritta altrove rispetto alla variabile
                // dichiarata dall'utente.
                return s with
                {
                    Host = BindExpression(s.Host, program, isStrict), Port = BindExpression(s.Port, program, isStrict),
                    Handle = BindTarget(s.Handle, program, isStrict),
                    Path = s.Path != null ? BindExpression(s.Path, program, isStrict) : null,
                    Timeout = s.Timeout != null ? BindExpression(s.Timeout, program, isStrict) : null
                };
            case TcpCloseStmt s:
                return s with { Handle = BindExpression(s.Handle, program, isStrict) };

            case SslOpenStmt s:
                return s with
                {
                    Port = BindExpression(s.Port, program, isStrict),
                    Handle = BindTarget(s.Handle, program, isStrict),
                    CertificatePath = BindExpression(s.CertificatePath, program, isStrict),
                    CertificatePassword = s.CertificatePassword != null
                        ? BindExpression(s.CertificatePassword, program, isStrict)
                        : null
                };
            case SslConnectStmt s:
                return s with
                {
                    Host = BindExpression(s.Host, program, isStrict),
                    Port = s.Port != null ? BindExpression(s.Port, program, isStrict) : null,
                    Handle = BindTarget(s.Handle, program, isStrict),
                    Timeout = s.Timeout != null ? BindExpression(s.Timeout, program, isStrict) : null
                };
            case UdpOpenStmt s:
                return s with
                {
                    Port = s.Port != null ? BindExpression(s.Port, program, isStrict) : null,
                    Handle = BindTarget(s.Handle, program, isStrict),
                    Engine = s.Engine != null ? BindExpression(s.Engine, program, isStrict) : null
                };
            case UdpSendStmt s:
                return s with
                {
                    Handle = BindExpression(s.Handle, program, isStrict),
                    Host = BindExpression(s.Host, program, isStrict),
                    Port = BindExpression(s.Port, program, isStrict),
                    Data = BindExpression(s.Data, program, isStrict)
                };
            case UdpRecvStmt s:
                return s with
                {
                    Handle = BindExpression(s.Handle, program, isStrict),
                    Buffer = BindTarget(s.Buffer, program, isStrict),
                    RemoteIp = BindTarget(s.RemoteIp, program, isStrict),
                    RemotePort = BindTarget(s.RemotePort, program, isStrict),
                    Timeout = s.Timeout != null ? BindExpression(s.Timeout, program, isStrict) : null
                };
            case UdpCloseStmt s:
                return s with { Handle = BindExpression(s.Handle, program, isStrict) };
            case ShutdownStmt s:
                return s with
                {
                    Handle = BindExpression(s.Handle, program, isStrict),
                    Mode = BindExpression(s.Mode, program, isStrict)
                };
            case SetOptionStmt s:
                return s with
                {
                    Handle = BindExpression(s.Handle, program, isStrict),
                    Option = BindExpression(s.Option, program, isStrict),
                    Value = BindExpression(s.Value, program, isStrict)
                };
            case HttpFetchStmt s:
                return s with
                {
                    Url = BindExpression(s.Url, program, isStrict),
                    Options = s.Options != null ? BindExpression(s.Options, program, isStrict) : null
                };
            case HttpServerStmt s:
                return s with
                {
                    Port = s.Port != null ? BindExpression(s.Port, program, isStrict) : null,
                    CertificatePath = s.CertificatePath != null
                        ? BindExpression(s.CertificatePath, program, isStrict)
                        : null,
                    CertificatePassword = s.CertificatePassword != null
                        ? BindExpression(s.CertificatePassword, program, isStrict)
                        : null
                };
            case HttpRouteStmt s:
                return s with
                {
                    Method = BindExpression(s.Method, program, isStrict),
                    Path = BindExpression(s.Path, program, isStrict)
                };
            case HttpUseStmt s:
                return s with { Path = s.Path != null ? BindExpression(s.Path, program, isStrict) : null };
            case HttpUseNextStmt s:
                return s with { NextHandle = BindExpression(s.NextHandle, program, isStrict) };
            case HttpConfigStmt s:
                return s with { ConfigObject = BindExpression(s.ConfigObject, program, isStrict) };

            case ViewPrintStmt s:
                return s with
                {
                    TopLine = s.TopLine != null ? BindExpression(s.TopLine, program, isStrict) : null,
                    BottomLine = s.BottomLine != null ? BindExpression(s.BottomLine, program, isStrict) : null
                };

            default:
                // Fallback sicuro: se abbiamo dimenticato qualcosa, almeno ha lo ScopeId corretto.
                // In più: binding RIFLESSIVO delle proprietà Expr/ImmutableArray<Expr> per gli
                // statement non elencati sopra (es. tutti i comandi grafici/audio/clipboard):
                // senza, un ArrayAccessExpr dentro un argomento di statement non gestito
                // restava con SymbolId=-1 → path dinamico con default 0 → IL invalido
                // (InvalidProgramException su DRAW TEXT STR(arr(i)), 2026-10-03).
                return TryBindExprProperties(scopedStmt, program, isStrict) ?? scopedStmt;
        }
    }

    /// <summary>
    /// Binding riflessivo delle proprietà espressione di uno statement non gestito da casi
    /// espliciti: clona il record (`&lt;Clone&gt;$`) e rilega ogni proprietà di tipo Expr o
    /// ImmutableArray&lt;Expr&gt;. Null se lo statement non ha proprietà espressione.
    /// </summary>
    private static Stmt? TryBindExprProperties(Stmt stmt, BasicProgram program, bool isStrict)
    {
        var type = stmt.GetType();
        var cloneMethod = type.GetMethod("<Clone>$", System.Reflection.BindingFlags.Public | System.Reflection.BindingFlags.Instance);
        if (cloneMethod == null) return null;
        var props = type.GetProperties(System.Reflection.BindingFlags.Public | System.Reflection.BindingFlags.Instance)
            .Where(p => typeof(Expr).IsAssignableFrom(p.PropertyType)
                     || p.PropertyType == typeof(ImmutableArray<Expr>))
            .ToArray();
        if (props.Length == 0) return null;
        object? clone;
        try { clone = cloneMethod.Invoke(stmt, null); }
        catch { return null; }
        if (clone == null) return null;
        foreach (var p in props)
        {
            try
            {
                var val = p.GetValue(stmt);
                if (val is Expr e)
                    p.SetValue(clone, BindExpression(e, program, isStrict));
                else if (val is ImmutableArray<Expr> arr && !arr.IsDefault)
                    p.SetValue(clone, arr.Select(x => BindExpression(x, program, isStrict)).ToImmutableArray());
            }
            catch { /* proprietà non settabile: lascia il valore clonato */ }
        }
        return (Stmt)clone;
    }

    /// <summary>
    /// Gestisce il binding di un blocco `TRY...CATCH...FINALLY`, creando scope isolati per le clausole di gestione errori.
    /// Assicura che le variabili di eccezione siano visibili solo all'interno del loro blocco `CATCH` specifico.
    ///
    /// Responsabilità Architetturali:
    /// 1.  Scope Principale (TRY): Elabora il corpo del `TRY` nello scope corrente. In GM-BASIC, il blocco `TRY`
    ///     non crea necessariamente un nuovo scope, condividendo quello della procedura, ma le sue istruzioni vengono comunque "legate".
    /// 2.  Scope Clausole CATCH: Per ogni clausola `CATCH`, crea un nuovo scope di blocco (`EnterScope`).
    ///     Questo è fondamentale per registrare la variabile dell'eccezione (es. `CATCH ex AS ERR`) in modo che sia
    ///     accessibile solo all'interno di quel blocco `CATCH` e non interferisca con altri blocchi o con la procedura.
    /// 3.  Registrazione Variabile Errore: Se presente, registra la variabile dell'eccezione nel nuovo scope del `CATCH`
    ///     come variabile locale, assegnandole un `SymbolId` univoco.
    /// 4.  Scope FINALLY: Se presente, crea un nuovo scope di blocco per il `FINALLY`, garantendo l'isolamento
    ///     delle variabili dichiarate al suo interno.
    /// 5.  Chiusura Scope: Assicura che `ExitScope` venga chiamato alla fine di ogni blocco `CATCH` e `FINALLY`,
    ///     ripristinando correttamente lo stack degli scope.
    /// </summary>
    /// <param name="program">L'istanza del programma.</param>
    /// <param name="tryStmt">Lo statement TRY da processare.</param>
    /// <param name="isStrict">OPTION STRICT enabled or disabled flag</param>
    /// <returns>Il nodo `TryCatchStmt` aggiornato con tutti i sottoblocchi legati ai rispettivi scope.</returns>
    private static TryCatchStmt BindTryCatchStatement(BasicProgram program, TryCatchStmt tryStmt, bool isStrict)
    {
        var currentScopeId = program.Symbols.GetCurrentScope().Id;
        var boundTryBody = BindListOfStatements(tryStmt.TryBody, program, isStrict);

        var boundCatchClauses = tryStmt.CatchClauses.Select(catchClause =>
        {
            program.Symbols.EnterScope($"Catch_{catchClause.Token.Line}_{catchClause.Token.Column}",
                SymbolTable.ScopeType.Block);
            try
            {
                var catchScopeId = program.Symbols.GetCurrentScope().Id;
                VariableDeclaration? boundVar = null;

                if (catchClause.Variable != null)
                {
                    var decl = catchClause.Variable;
                    var finalFlags = decl.Flags | SymbolFlags.IsLocal;
                    var symbolId = program.Symbols.RegisterSymbol(decl.NameCleaned, finalFlags,
                        decl.DataType ?? new ErrType(), decl);

                    boundVar = decl with { SymbolId = symbolId, Flags = finalFlags, ScopeId = catchScopeId };
                    program.Symbols.UpdateSymbolDeclaration(symbolId, boundVar);
                }

                // Timbra la condizione WHEN (se presente), valutata nel contesto del CATCH
                var boundWhen = catchClause.WhenCondition != null
                    ? BindExpression(catchClause.WhenCondition, program, isStrict)
                    : null;

                var boundCatchBody = BindListOfStatements(catchClause.Body, program, isStrict);
                return catchClause with
                {
                    Variable = boundVar,
                    WhenCondition = boundWhen,
                    Body = boundCatchBody,
                    ScopeId = catchScopeId
                };
            }
            finally
            {
                program.Symbols.ExitScope();
            }
        }).ToImmutableArray();

        FinallyClause? boundFinally = null;
        if (tryStmt.Finally != null)
        {
            program.Symbols.EnterScope($"Finally_{tryStmt.Finally.Token.Line}_{tryStmt.Finally.Token.Column}",
                SymbolTable.ScopeType.Block);
            try
            {
                var finallyScopeId = program.Symbols.GetCurrentScope().Id;
                var boundFinallyBody = BindListOfStatements(tryStmt.Finally.Body, program, isStrict);
                boundFinally = tryStmt.Finally with { Body = boundFinallyBody, ScopeId = finallyScopeId };
            }
            finally
            {
                program.Symbols.ExitScope();
            }
        }

        return tryStmt with
        {
            ScopeId = currentScopeId,
            TryBody = boundTryBody,
            CatchClauses = boundCatchClauses,
            Finally = boundFinally
        };
    }

    /// <summary>
    /// Gestisce il binding di un blocco `IF...ELSEIF...ELSE`, creando scope isolati per ogni ramo condizionale.
    /// Questo garantisce che le variabili dichiarate in un ramo (es. `THEN`) non siano visibili negli altri (es. `ELSE`).
    ///
    /// Responsabilità Architetturali:
    /// 1.  Scope Clausole IF/ELSEIF: Itera su tutte le clausole condizionali. Per ciascuna, crea un nuovo
    ///     scope di blocco (`EnterScope`). Questo isola lessicalmente le variabili dichiarate all'interno del ramo.
    /// 2.  Binding Corpi: Chiama ricorsivamente `BindStatementList` per elaborare le istruzioni all'interno
    ///     di ogni blocco `IF` o `ELSEIF`, registrando eventuali simboli nel nuovo scope.
    /// 3.  Scope Clausola ELSE: Se presente, crea un nuovo scope di blocco separato per il ramo `ELSE`
    ///     e ne elabora il corpo.
    /// 4.  Gestione Condizioni: Nota importante: le espressioni di condizione (`IF condizione`) vengono valutate
    ///     nello scope *esterno* (quello che contiene l'istruzione IF), non nello scope del blocco. Tuttavia,
    ///     poiché il Binder non tocca le espressioni, qui ci limitiamo a passare i nodi condizione invariati.
    /// 5.  Chiusura Scope: Assicura che `ExitScope` venga chiamato al termine dell'elaborazione di ogni singolo
    ///     ramo, mantenendo la SymbolTable coerente.
    /// </summary>
    /// <param name="program">L'istanza del programma.</param>
    /// <param name="ifStmt">Lo statement IF da processare.</param>
    /// <param name="isStrict">OPTION STRICT enabled or disabled flag</param>
    /// <returns>Il nodo `IfStmt` aggiornato con i rami legati ai rispettivi scope.</returns>
    private static IfStmt BindIfStatement(BasicProgram program, IfStmt ifStmt, bool isStrict)
    {
        var currentScopeId = program.Symbols.GetCurrentScope().Id;

        var boundIfClauses = ifStmt.IfClauses.Select(clause =>
        {
            // 1. Timbra l'espressione della condizione (va valutata nello scope corrente/esterno)
            var boundCondition = BindExpression(clause.Condition, program, isStrict);

            program.Symbols.EnterScope($"If_{clause.Token.Line}_{clause.Token.Column}", SymbolTable.ScopeType.Block);
            try
            {
                var boundStatements = BindListOfStatements(clause.Statements, program, isStrict);
                return clause with
                {
                    Condition = boundCondition,
                    Statements = boundStatements,
                    ScopeId = program.Symbols.GetCurrentScope().Id
                };
            }
            finally
            {
                program.Symbols.ExitScope();
            }
        }).ToImmutableArray();

        ElseClause? boundElseClause = null;
        if (ifStmt.ElseClause != null)
        {
            program.Symbols.EnterScope($"Else_{ifStmt.ElseClause.Token.Line}_{ifStmt.ElseClause.Token.Column}",
                SymbolTable.ScopeType.Block);
            try
            {
                var boundElseStatements = BindListOfStatements(ifStmt.ElseClause.Statements, program, isStrict);
                boundElseClause = ifStmt.ElseClause with
                {
                    Statements = boundElseStatements,
                    ScopeId = program.Symbols.GetCurrentScope().Id
                };
            }
            finally
            {
                program.Symbols.ExitScope();
            }
        }

        return ifStmt with
        {
            ScopeId = currentScopeId,
            IfClauses = boundIfClauses,
            ElseClause = boundElseClause
        };
    }

    /// <summary>
    /// Gestisce il binding di un blocco `SELECT CASE`, creando scope isolati per ogni caso.
    /// Simile all'`IF`, garantisce l'isolamento lessicale delle variabili dichiarate nei diversi `CASE`.
    ///
    /// Responsabilità Architetturali:
    /// 1.  Scope Clausole CASE: Itera su tutte le clausole `CASE`. Per ciascuna, apre un nuovo scope di blocco
    ///     (`EnterScope`) con un nome descrittivo (es. "Case_123").
    /// 2.  Binding Corpi: Invoca `BindStatementList` per elaborare le istruzioni all'interno di ogni `CASE`.
    ///     Qualsiasi `DIM` all'interno di un `CASE` sarà registrata in questo scope locale e cesserà di esistere
    ///     al `NEXT CASE` o `END SELECT`.
    /// 3.  Scope Clausola CASE ELSE: Se presente, crea un ulteriore scope di blocco separato per il ramo
    ///     `CASE ELSE` e ne processa le istruzioni.
    /// 4.  Chiusura Scope: Garantisce rigorosamente che per ogni `EnterScope` eseguito per un `CASE`,
    ///     venga chiamato il corrispondente `ExitScope` prima di passare al successivo o terminare lo statement.
    /// </summary>
    /// <param name="program">L'istanza del programma.</param>
    /// <param name="selectStmt">Lo statement SELECT da processare.</param>
    /// <param name="isStrict">OPTION STRICT enabled or disabled flag</param>
    /// <returns>Il nodo `SelectStmt` aggiornato con i blocchi CASE legati ai rispettivi scope.</returns>
    private static SelectStmt BindSelectStatement(BasicProgram program, SelectStmt selectStmt, bool isStrict)
    {
        var currentScopeId = program.Symbols.GetCurrentScope().Id;

        // 1. Timbra l'espressione principale del SELECT
        var boundExpression = BindExpression(selectStmt.Expression, program, isStrict);

        var boundCaseClauses = selectStmt.CaseClauses.Select(clause =>
        {
            // MODIFICA: Entriamo nello scope PRIMA di bindare i test.
            // Questo permette a BindCaseTest di registrare le variabili (AS var) 
            // direttamente nello scope del blocco CASE, rendendole visibili al WHEN e al Body.
            program.Symbols.EnterScope($"Case_{clause.Token.Line}_{clause.Token.Column}", SymbolTable.ScopeType.Block);
            try
            {
                // 2. Timbra le espressioni di test (CaseValue, CaseRange, CaseTypePattern, etc.)
                //    Ora BindCaseTest ha accesso allo scope attivo per registrare simboli.
                var boundTests = clause.Tests.Select(t => BindCaseTest(t, program, isStrict)).ToImmutableArray();

                var boundStatements = BindListOfStatements(clause.Statements, program, isStrict);
                return clause with
                {
                    Tests = boundTests,
                    Statements = boundStatements,
                    ScopeId = program.Symbols.GetCurrentScope().Id
                };
            }
            finally
            {
                program.Symbols.ExitScope();
            }
        }).ToImmutableArray();

        CaseElseClause? boundElseClause = null;
        if (selectStmt.ElseClause != null)
        {
            program.Symbols.EnterScope(
                $"CaseElse_{selectStmt.ElseClause.Token.Line}_{selectStmt.ElseClause.Token.Column}",
                SymbolTable.ScopeType.Block);
            try
            {
                var boundElseStatements = BindListOfStatements(selectStmt.ElseClause.Statements, program, isStrict);
                boundElseClause = selectStmt.ElseClause with
                {
                    Statements = boundElseStatements,
                    ScopeId = program.Symbols.GetCurrentScope().Id
                };
            }
            finally
            {
                program.Symbols.ExitScope();
            }
        }

        return selectStmt with
        {
            Expression = boundExpression,
            ScopeId = currentScopeId,
            CaseClauses = boundCaseClauses,
            ElseClause = boundElseClause
        };
    }

    private static WithStmt BindWithStatement(BasicProgram program, WithStmt withStmt, bool isStrict)
    {
        var boundExpression = BindExpression(withStmt.Expression, program, isStrict);
        var boundBody = BindListOfStatements(withStmt.Body, program, isStrict);
        return withStmt with { Expression = boundExpression, Body = boundBody };
    }

    /// <summary>
    /// Lega uno statement USING. Prima di questo fix, UsingStmt cadeva nel caso
    /// 'default' del binder: ResourceInit e Body non venivano mai legati, e la
    /// variabile risorsa (es. "F" in USING F = OPEN(...)) non veniva mai
    /// registrata come simbolo. A runtime, sia F sia qualunque variabile DIM
    /// dichiarata dentro il corpo del blocco restavano con SymbolId = -1 —
    /// una chiave NON UNIVOCA nel dizionario Variables dello scope, condivisa
    /// da tutte le variabili "non risolte". Risultato: un DIM nel corpo poteva
    /// sovrascrivere silenziosamente il valore della variabile del USING
    /// (es. il numero di file). Porting da GMBasic (stesso gap, stesso codice
    /// pre-fork).
    /// </summary>
    private static UsingStmt BindUsingStatement(BasicProgram program, UsingStmt usingStmt, bool isStrict)
    {
        // L'espressione di init (es. OPEN(file, OUTPUT)) va legata prima di
        // registrare la variabile risorsa, cosi' non puo' vedere se stessa.
        var boundInit = BindExpression(usingStmt.ResourceInit, program, isStrict);

        // La variabile del USING e' una dichiarazione implicita (mai vista da
        // DIM): va registrata come nuovo simbolo locale, analogamente alla
        // variabile iteratore di un FOR EACH. Se lo stesso nome e' gia' stato
        // usato da un blocco USING precedente nello stesso scope (es. due
        // "USING F = OPEN(...)" sequenziali per scrittura e poi lettura),
        // RegisterSymbol lancerebbe un errore di doppia dichiarazione: in
        // quel caso il simbolo esistente va riusato, non ridichiarato,
        // proprio come una normale variabile implicita riassegnata due volte.
        var boundTarget = usingStmt.ResourceTarget is VariableTarget { SymbolId: < 0 } vt
            ? vt with { SymbolId = RegisterOrReuseLocalSymbol(program, vt.NameCleaned) }
            : usingStmt.ResourceTarget;

        var boundBody = BindListOfStatements(usingStmt.Body, program, isStrict);

        return usingStmt with
        {
            ResourceInit = boundInit,
            ResourceTarget = boundTarget,
            Body = boundBody
        };
    }

    /// <summary>
    /// Registra un simbolo locale nello scope corrente, riusando l'ID esistente
    /// se un simbolo con lo stesso nome e' gia' presente in quello scope
    /// (es. un secondo blocco USING che riusa lo stesso nome di variabile).
    /// </summary>
    private static int RegisterOrReuseLocalSymbol(BasicProgram program, Token nameToken)
    {
        var currentScope = program.Symbols.GetCurrentScope();
        if (currentScope.SymbolsByName.TryGetValue(nameToken.Lexeme, out var existingIds) && existingIds.Count > 0)
            return existingIds[0];

        return program.Symbols.RegisterSymbol(nameToken, SymbolFlags.IsLocal, new InferredType(), nameToken);
    }

    /// <summary>
    /// Helper per legare le espressioni dentro un CaseTest.
    /// </summary>
    private static CaseTest BindCaseTest(CaseTest test, BasicProgram program, bool isStrict)
    {
        // Non c'è ScopeId su CaseTest, ma dobbiamo scendere nelle Expr
        switch (test)
        {
            case CaseValue cv:
                return cv with { Value = BindExpression(cv.Value, program, isStrict) };
            
            case CaseRange cr:
                return cr with
                {
                    Start = BindExpression(cr.Start, program, isStrict), 
                    End = BindExpression(cr.End, program, isStrict)
                };
            
            case CaseComparison cc:
                return cc with { Value = BindExpression(cc.Value, program, isStrict) };

            case CaseTypePattern ctp:
                // Se c'è una variabile di cattura (CASE IS TYPE var), la registriamo nello scope corrente (il blocco CASE).
                if (ctp.VariableName != null)
                {
                    program.Symbols.RegisterSymbol(
                        ctp.VariableName,
                        SymbolFlags.IsLocal,
                        ctp.TargetType, // Il tipo è esplicito nel pattern
                        ctp.VariableName // Usiamo il token come placeholder di dichiarazione
                    );
                }
                // Il WHEN deve essere bindato DOPO la registrazione per vedere la variabile.
                return ctp with { WhenCondition = ctp.WhenCondition != null ? BindExpression(ctp.WhenCondition, program, isStrict) : null };

            case CaseValuePattern cvp:
                // Bindiamo il valore del pattern.
                var boundPatternValue = BindExpression(cvp.PatternValue, program, isStrict);

                // Registriamo la variabile di cattura (CASE val AS var).
                if (cvp.VariableName != null)
                {
                    program.Symbols.RegisterSymbol(
                        cvp.VariableName,
                        SymbolFlags.IsLocal,
                        new InferredType(), // Il tipo sarà inferito dal valore del pattern o dall'espressione del SELECT
                        cvp.VariableName
                    );
                }
                
                // Il WHEN deve vedere la variabile appena registrata.
                var boundWhen = cvp.WhenCondition != null ? BindExpression(cvp.WhenCondition, program, isStrict) : null;
                
                return cvp with { PatternValue = boundPatternValue, WhenCondition = boundWhen };

            default:
                return test;
        }
    }

    /// <summary>
    /// Registra la variabile di controllo di un ciclo `FOR` nello scope corrente.
    /// Si assume che lo scope corrente sia già quello del blocco del ciclo, creato da un `EnterLoopStmt` precedente.
    ///
    /// Responsabilità Architetturali:
    /// 1.  Registrazione Variabile: Prende il token della variabile di controllo (es. `I` in `FOR I = 1 TO 10`)
    ///     e lo registra come simbolo locale (`IsLocal`) nello scope attuale usando `program.Symbols.RegisterSymbol`.
    ///     Il tipo viene impostato inizialmente come `InferredType`, in attesa della risoluzione nella Fase 3.
    /// 2.  Aggiornamento Nodo: Aggiorna il nodo `VariableTarget` all'interno del `ForStmt` con il nuovo `SymbolId`
    ///     e lo `ScopeId` corrente.
    /// 3.  Aggiornamento SymbolTable: Chiama `UpdateSymbolDeclaration` per assicurarsi che il simbolo nella
    ///     tabella punti al nodo `VariableTarget` aggiornato e completo.
    /// 4.  Ignora Espressioni: Non tenta di legare o risolvere le espressioni di `Start`, `End` o `Step`.
    ///     Queste rimangono come nodi `Expr` non risolti, da gestire nel Resolver o Rewriter.
    /// </summary>
    /// <param name="program">L'istanza del programma.</param>
    /// <param name="forStmt">Lo statement FOR da processare.</param>
    /// <returns>Il nodo `ForStmt` aggiornato con il SymbolId della variabile di ciclo.</returns>
    private static ForStmt RegisterForLoopVariable(BasicProgram program, ForStmt forStmt)
    {
        var currentScopeId = program.Symbols.GetCurrentScope().Id;
        var vt = forStmt.Variable;

        // Se 'i' e' già una variabile dichiarata esplicitamente (DIM i AS ...) in uno scope
        // accessibile, il FOR NON riusa quel simbolo: crea un'ombra block-scoped che eredita
        // il TIPO dichiarato esterno (canone shadowing 2026-09-30: dopo il ciclo la DIM esterna
        // conserva il suo valore — `samples/Loops/LoopsForNextOptimized.bas` TEST 4).
        // Il riuso diretto (pre-2026-09-30) condivideva lo slot e il contatore scriveva sopra
        // la variabile utente su IL (in SUB), PCode e VM/C. Ereditare il tipo (invece di un
        // InferredType vergine) conserva il fix anti-narrowing originale: un'ombra InferredType
        // era vittima legittima del TARGET TYPING ("DIM i AS INTEGER: FOR i=0 TO 31: key(i)=i"
        // con key AS BYTE retipizzava l'ombra a BYTE e il locale CLR del contatore IL si
        // rompeva). Il simbolo esterno resta intatto (mai UpdateSymbolDeclaration su di lui).
        var existingSymbolIds = program.Symbols.FindSymbolInScope(vt.NameCleaned.Lexeme);
        if (existingSymbolIds is { Count: 1 } &&
            program.Symbols.GetSymbol(existingSymbolIds[0]).DeclarationNode is VariableDeclaration outerDecl)
        {
            var shadowType = outerDecl.DataType ?? new InferredType();
            var shadowSymbolId = program.Symbols.RegisterSymbol(
                vt.NameCleaned,
                SymbolFlags.IsLocal,
                shadowType,
                vt);

            Log(
                $"[BINDER]        -> Variabile di Ciclo FOR '{vt.NameSuffixed.Lexeme}' ombra block-scoped ID {shadowSymbolId} (tipo ereditato {shadowType}) invece del riuso del DIM ID {existingSymbolIds[0]}.",
                BindingLog);

            var shadowVariableTarget = vt with { SymbolId = shadowSymbolId, ScopeId = currentScopeId };
            program.Symbols.UpdateSymbolDeclaration(shadowSymbolId, shadowVariableTarget);

            return forStmt with { Variable = shadowVariableTarget, ScopeId = currentScopeId };
        }

        // Registra la variabile nello scope corrente (locale al loop).
        // Usiamo InferredType perché il tipo sarà determinato dall'espressione Start/End in Fase 3.
        var symbolId = program.Symbols.RegisterSymbol(
            vt.NameCleaned,
            SymbolFlags.IsLocal,
            new InferredType(),
            vt
        );

        Log(
            $"[BINDER]        -> Variabile di Ciclo FOR '{vt.NameSuffixed.Lexeme}' registrata con ID {symbolId} in Scope {currentScopeId}.",
            BindingLog);

        var updatedVariableTarget = vt with { SymbolId = symbolId, ScopeId = currentScopeId };
        program.Symbols.UpdateSymbolDeclaration(symbolId, updatedVariableTarget);

        // Nota: NON tocchiamo Start, End, Step. Rimangono con i loro nodi Expr originali non risolti.
        return forStmt with { Variable = updatedVariableTarget, ScopeId = currentScopeId };
    }

    /// <summary>
    /// Registra le variabili dichiarate esplicitamente (`DIM`, `LOCAL`, `STATIC`) all'interno di una procedura.
    /// Assegna ID univoci e timbra i nodi con lo scope corrente.
    ///
    /// Responsabilità Architetturali:
    /// 1.  Iterazione Dichiarazioni: Scorre la lista di singole dichiarazioni contenute nell'istruzione.
    /// 2.  Applicazione Flag: Combina i flag specifici della dichiarazione (es. `IsArray`) con il flag `IsLocal`,
    ///     che identifica queste variabili come appartenenti allo scope corrente (procedura o blocco).
    /// 3.  Registrazione Simbolo: Chiama `RegisterSymbol` per ogni variabile. Se la variabile ha un tipo
    ///     esplicito, lo usa; altrimenti usa `InferredType`. Non tenta di risolvere i tipi complessi (UDT) in questa fase.
    /// 4.  Aggiornamento Nodo: Crea una nuova istanza di `VariableDeclaration` con il `SymbolId` assegnato e lo `ScopeId` corrente.
    ///     Il valore iniziale (`InitialValue`) viene preservato ma non risolto (rimane un'espressione opaca).
    /// 5.  Persistenza: Aggiorna la definizione del simbolo nella SymbolTable con il nodo arricchito.
    /// 
    /// Utilizza il nome normalizzato dal Parser per la registrazione del simbolo,
    /// garantendo che dichiarazioni duplicate (es. 'var$' e 'var AS STRING') vengano rilevate.
    /// </summary>
    /// <param name="program">L'istanza del programma.</param>
    /// <param name="declStmt">Lo statement di dichiarazione.</param>
    /// <param name="isStrict">OPTION STRICT enabled or disabled flag</param>
    /// <returns>Il nodo `DeclarationStmt` aggiornato con le dichiarazioni legate.</returns>
    private static DeclarationStmt RegisterLocalVariables(BasicProgram program, DeclarationStmt declStmt, bool isStrict)
    {
        const bool log = false;
        var currentScopeId = program.Symbols.GetCurrentScope().Id;
        Log($"[BINDER-DEBUG] Entrando in RegisterLocalVariables per Scope ID: {currentScopeId}", log);

        var updatedDeclarations = declStmt.Declarations.Select(decl =>
        {
            // Determina i flag corretti in base alla keyword di dichiarazione.
            var finalFlags = decl.Flags;
            if (declStmt.Keyword.Kind == TokenKind.Dynamic)
            {
                // Le variabili PRIVATE hanno solo scope dinamico. Rimuoviamo altri flag di storage.
                finalFlags = (finalFlags & ~SymbolFlags.IsGlobal & ~SymbolFlags.IsLocal &
                              ~SymbolFlags.IsStaticStorage)
                             | SymbolFlags.IsDynamicScope;
            }
            else
            {
                // Per le altre dichiarazioni locali (DIM, LOCAL), assicuriamo IsLocal.
                finalFlags = (finalFlags & ~SymbolFlags.IsGlobal) | SymbolFlags.IsLocal;
                // STATIC: mantiene il valore tra chiamate — flag IsStaticStorage.
                if (declStmt.Keyword.Kind == TokenKind.Static)
                    finalFlags |= SymbolFlags.IsStaticStorage;
            }

            Log($"[BINDER-DEBUG]   -> Tentativo di registrazione simbolo con nome normalizzato: " +
                $"'{decl.NameCleaned.Lexeme}' (da token originale: '{decl.NameSuffixed.Lexeme}')", log);

            var newSymbolId = program.Symbols.RegisterSymbol(decl.NameCleaned, finalFlags,
                decl.DataType ?? new InferredType(), decl);

            Log($"[BINDER-DEBUG]   -> Simbolo registrato con ID: {newSymbolId}", log);

            // Timbra e lega l'inizializzatore
            var boundInitialValue = decl.InitialValue != null
                ? BindExpression(decl.InitialValue, program, isStrict)
                : null;

            // Timbra e lega le dimensioni (e lower bounds)
            var boundDimensions = decl.Dimensions
                .Select(d => BindExpression(d, program, isStrict))
                .ToImmutableArray();

            var boundLowerBounds = decl.LowerBounds
                .Select(d => BindExpression(d, program, isStrict))
                .ToImmutableArray();

            var finalDecl = decl with
            {
                SymbolId = newSymbolId,
                Flags = finalFlags,
                ScopeId = currentScopeId,
                InitialValue = boundInitialValue,
                Dimensions = boundDimensions,
                LowerBounds = boundLowerBounds
            };

            program.Symbols.UpdateSymbolDeclaration(newSymbolId, finalDecl);

            return finalDecl;
        }).ToImmutableArray();

        return declStmt with { Declarations = updatedDeclarations, ScopeId = currentScopeId };
    }

    /// <summary>
    /// Registra le variabili introdotte da una distrutturazione di tupla (`DIM (a, b) = expr`)
    /// nello scope corrente del Linker, che è quello corretto (procedure/block/global).
    /// La registrazione NON avviene più nel parser, dove lo scope corrente è sempre quello di
    /// modulo (0): lì due `DIM (a, b)` in procedure diverse collidevano come duplicati e i
    /// backend che risolvono per nome+scope (IL) non trovavano i simboli.
    /// </summary>
    private static TupleDestructuringStmt RegisterTupleDestructuringVariables(
        BasicProgram program, TupleDestructuringStmt stmt, bool isStrict)
    {
        var currentScope = program.Symbols.GetCurrentScope();
        var currentScopeId = currentScope.Id;
        var isInLocalScope =
            currentScope.Type is SymbolTable.ScopeType.Procedure or SymbolTable.ScopeType.Block;
        var flags = isInLocalScope ? SymbolFlags.IsLocal : SymbolFlags.IsGlobal;

        foreach (var nameToken in stmt.VariableNames)
        {
            var decl = new VariableDeclaration(-1, nameToken, nameToken,
                ImmutableArray<Expr>.Empty, new InferredType(), null, false, false,
                flags, ImmutableArray<Token>.Empty);
            var symId = program.Symbols.RegisterSymbol(nameToken, flags, new InferredType(), decl);
            program.Symbols.UpdateSymbolDeclaration(symId, decl with { SymbolId = symId, ScopeId = currentScopeId });
        }

        return stmt with
        {
            TupleExpression = BindExpression(stmt.TupleExpression, program, isStrict),
            ScopeId = currentScopeId
        };
    }

    /// <summary>
    /// Registra le costanti locali (`CONST`) all'interno di una procedura.
    /// Simile alla registrazione delle variabili, ma applica il flag `IsConst`.
    ///
    /// Responsabilità Architetturali:
    /// 1.  Flag Costante: Assicura che ogni simbolo registrato abbia il flag `IsConst` oltre a `IsLocal`.
    ///     Questo permetterà al Linker (Fase 3) di trattarle come valori immutabili e di eseguire il constant folding.
    /// 2.  Registrazione: Registra il simbolo nella SymbolTable, ottenendo un ID.
    /// 3.  Timbratura: Aggiorna il nodo `ConstDeclaration` con `SymbolId` e `ScopeId`.
    /// 4.  Nessuna Valutazione: Non valuta l'espressione del valore della costante. La valutazione avviene
    ///     rigorosamente nella Fase 3 (Rewriter/Optimization). Qui ci limitiamo a prenotare il nome.
    /// </summary>
    /// <param name="program">L'istanza del programma.</param>
    /// <param name="constStmt">Lo statement di dichiarazione costante.</param>
    /// <returns>Il nodo `ConstStmt` aggiornato.</returns>
    private static ConstStmt RegisterLocalConstants(BasicProgram program, ConstStmt constStmt)
    {
        var currentScopeId = program.Symbols.GetCurrentScope().Id;

        var updatedDeclarations = constStmt.Declarations.Select(decl =>
        {
            var finalFlags = decl.Flags | SymbolFlags.IsLocal | SymbolFlags.IsConst;
            var symbolId = program.Symbols.RegisterSymbol(decl.OriginalToken, finalFlags,
                decl.DataType ?? new InferredType(), decl);

            var finalDecl = decl with
            {
                SymbolId = symbolId,
                Flags = finalFlags,
                ScopeId = currentScopeId
            };

            program.Symbols.UpdateSymbolDeclaration(symbolId, finalDecl);

            Log(
                $"[BINDER]        -> Costante Locale '{decl.OriginalToken.Lexeme}' registrata con ID {symbolId} in Scope {currentScopeId}.",
                BindingLog);
            return finalDecl;
        }).ToImmutableArray();

        return constStmt with { Declarations = updatedDeclarations, ScopeId = currentScopeId };
    }

    /// <summary>
    /// Gestisce la registrazione implicita di variabili tramite assegnazione (`LET`).
    /// Implementa la logica "crea se non esiste" tipica del BASIC per le variabili locali.
    /// 
    /// Responsabilità Architetturali:
    /// 1.  Analisi Target: Esamina il lato sinistro dell'assegnazione. Se è una `VariableTarget` semplice
    ///     che non ha ancora un `SymbolId` risolto (minore di 0), procede.
    /// 2.  Lookup Esistenza: Interroga `program.Symbols.FindSymbolInScope` per vedere se il nome esiste già
    ///     nello scope corrente o in uno dei genitori.
    /// 3.  Creazione Implicita: Se il simbolo NON esiste, lo registra immediatamente come nuova variabile locale
    ///     (`IsLocal`) con tipo `InferredType`. Questo avviene nello scope corrente.
    /// 4.  Aggiornamento Target: Se è stata creata una nuova variabile, aggiorna il nodo `VariableTarget`
    ///     con il nuovo `SymbolId` e `ScopeId`.
    /// 5.  Ignora Altro: Se il simbolo esiste già, o se il target è complesso (array, campo), non fa nulla.
    ///     Il collegamento al simbolo esistente avverrà nella Fase 2.5 (Resolver).
    /// </summary>
    /// <param name="program">L'istanza del programma.</param>
    /// <param name="letStmt">Lo statement LET.</param>
    /// <param name="isStrict">OPTION STRICT enabled or disabled flag</param>
    /// <returns>Il nodo `LetStmt` aggiornato (eventualmente con nuovi simboli registrati).</returns>
    private static LetStmt RegisterImplicitLocalVariables(BasicProgram program, LetStmt letStmt, bool isStrict)
    {
        var currentScope = program.Symbols.GetCurrentScope();
        var currentScopeId = currentScope.Id;

        var updatedAssignments = letStmt.Assignments.Select(assignment =>
        {
            var target = assignment.Target;

            // Determina il flag di storage corretto (Local vs Global) in base al contesto.
            var storageFlag =
                (currentScope.Type is SymbolTable.ScopeType.Procedure or SymbolTable.ScopeType.Block)
                    ? SymbolFlags.IsLocal
                    : SymbolFlags.IsGlobal;

            var isLetKeyword = letStmt.LetToken != null;

            // Assegnazione implicita SENZA `LET` dentro un blocco: la variabile appartiene alla procedura (o al modulo), non al blocco.
            // `LET x = ...` mantiene lo scope corrente (e' il modo esplicito di creare/ombreggiare nel blocco).
            var homeScopeId = currentScopeId;
            if (!isLetKeyword && currentScope.Type == SymbolTable.ScopeType.Block)
            {
                var home = ImplicitVariableHome(program);
                homeScopeId = home.ScopeId;
                storageFlag = home.Storage;
            }

            // CASO 1: Variabile SCALARE implicita (logica esistente)
            if (target is VariableTarget { SymbolId: < 0 } vt)
            {
                // Se in strict mode non permettiamo creazione implicita.
                if (isStrict)
                {
                    throw new BasicException(vt.NameSuffixed,
                        $"In OPTION STRICT mode, la variabile '{vt.NameSuffixed.Lexeme}' deve essere dichiarata esplicitamente.",
                        ErrorCode.UndefinedVariable);
                }

                // LET: crea un nuovo simbolo nello scope corrente solo se il nome NON e' gia'
                // dichiarato in tale scope (permette lo shadowing di variabili di scope superiori);
                // se esiste gia' nello scope corrente, e' una semplice riassegnazione allo stesso simbolo.
                // Assegnazione implicita: cerca verso l'alto (scope corrente + genitori) e modifica se trovato.
                var existingSymbolIds = isLetKeyword
                    ? currentScope.SymbolsByName.TryGetValue(vt.NameCleaned.Lexeme, out var idsInCurrentScope)
                        ? idsInCurrentScope
                        : null
                    : program.Symbols.FindSymbolInScope(vt.NameCleaned.Lexeme);

                if (existingSymbolIds == null || existingSymbolIds.Count == 0)
                {
                    var declNode = new VariableDeclaration(
                        -1, vt.NameSuffixed, vt.NameCleaned, ImmutableArray<Expr>.Empty, new InferredType(),
                        null, false, false, storageFlag);

                    var symbolId =
                        program.Symbols.RegisterSymbol(vt.NameCleaned, storageFlag, new InferredType(), declNode, homeScopeId);

                    Log(
                        $"[BINDER]        -> Variabile Scalare Implicita ({(isLetKeyword ? "LET" : "Implicit")}) '{vt.NameSuffixed.Lexeme}' registrata come {storageFlag} con ID {symbolId} in Scope {currentScopeId}.",
                        BindingLog);

                    var boundTarget = vt with { SymbolId = symbolId, ScopeId = currentScopeId };
                    var updatedDeclNode = declNode with { SymbolId = symbolId, ScopeId = homeScopeId };
                    program.Symbols.UpdateSymbolDeclaration(symbolId, updatedDeclNode);

                    return assignment with { Target = boundTarget };
                }

                // 2026-09-24 (overload): con omonimi, preferire il return-slot della
                // procedura contenitrice (`Nome = ...` dentro il proprio corpo) — mai il
                // primo overload per posizione di registrazione.
                var ownProcId = program.Symbols.PreferEnclosingProcedureSymbol(
                    vt.NameCleaned.Lexeme, existingSymbolIds);
                return assignment with
                {
                    Target = vt with { SymbolId = ownProcId >= 0 ? ownProcId : existingSymbolIds[0], ScopeId = currentScopeId }
                };
            }
            // CASO 2: Variabile ARRAY implicita (nuova logica)
            // ArrayElementTarget con 0 indici è come il parser vede 'Nome()'
            else if (target is ArrayElementTarget { SymbolId: < 0, Indices.IsEmpty: true } aet)
            {
                // LET: crea nuovo simbolo solo se non gia' dichiarato nello scope corrente (shadowing
                // di scope superiori); se esiste gia' nello scope corrente, e' una riassegnazione.
                // Implicita: cerca verso l'alto (scope corrente + genitori).
                var existingSymbolIds = isLetKeyword
                    ? currentScope.SymbolsByName.TryGetValue(aet.NameCleaned.Lexeme, out var idsInCurrentScope)
                        ? idsInCurrentScope
                        : null
                    : program.Symbols.FindSymbolInScope(aet.NameCleaned.Lexeme);

                if (existingSymbolIds == null || existingSymbolIds.Count == 0)
                {
                    var arrayFlags = storageFlag | SymbolFlags.IsArray;
                    var arrayType = new ArrayType(new InferredType());

                    var declNode = new VariableDeclaration(
                        -1, aet.NameSuffixed, aet.NameCleaned, ImmutableArray<Expr>.Empty, arrayType,
                        null, true, false, arrayFlags);

                    var symbolId = program.Symbols.RegisterSymbol(aet.NameCleaned, arrayFlags, arrayType, declNode, homeScopeId);

                    Log(
                        $"[BINDER]        -> Variabile Array Implicita ({(isLetKeyword ? "LET" : "Implicit")}) '{aet.NameSuffixed.Lexeme}' registrata come {arrayFlags} con ID {symbolId} in Scope {currentScopeId}.",
                        BindingLog);

                    var boundTarget = aet with { SymbolId = symbolId, ScopeId = currentScopeId };
                    var updatedDeclNode = declNode with { SymbolId = symbolId, ScopeId = homeScopeId };
                    program.Symbols.UpdateSymbolDeclaration(symbolId, updatedDeclNode);

                    return assignment with { Target = boundTarget };
                }
            }

            return assignment;
        }).ToImmutableArray();

        return letStmt with { Assignments = updatedAssignments, ScopeId = currentScopeId };
    }

    /// <summary>
    /// Registra un TIPO definito dall'utente (UDT) e tutti i suoi membri nella SymbolTable.
    /// Crea uno scope dedicato di tipo `ScopeType.Type` per isolare i membri del tipo.
    ///
    /// Responsabilità Architetturali:
    /// 1.  Registrazione Tipo: Registra il simbolo del tipo stesso nello scope genitore (modulo/namespace).
    /// 2.  Scope Tipo: Apre un nuovo scope per l'UDT.
    /// 3.  Membri: Itera su campi, metodi, proprietà, operatori e costruttori, registrando ciascuno come simbolo
    ///     all'interno dello scope del tipo. I metodi vengono registrati chiamando ricorsivamente `RegisterProcedure`.
    /// 4.  Timbratura: Aggiorna il nodo `UserDefinedType` con il suo `SymbolId` e la lista aggiornata dei membri.
    /// 5.  Chiusura: Chiude lo scope del tipo.
    /// </summary>
    /// <param name="program">L'istanza del programma.</param>
    /// <param name="udt">Il nodo UDT da registrare.</param>
    /// <param name="isStrict">OPTION STRICT enabled or disabled flag</param>
    /// <returns>Il nodo `UserDefinedType` aggiornato.</returns>
    private static UserDefinedType RegisterUdt(BasicProgram program, UserDefinedType udt, bool isStrict)
    {
        var parentScopeId = program.Symbols.GetCurrentScope().Id;
        var typeSymbolId = program.Symbols.RegisterSymbol(udt.OriginalName, udt.Flags, udt, udt);
        Log($"[BINDER]  -> Registrato UDT '{udt.OriginalName.Lexeme}' con ID {typeSymbolId} in Scope {parentScopeId}.",
            BindingLog);

        program.Symbols.EnterScope(udt.Name.Lexeme, SymbolTable.ScopeType.Type, typeSymbolId);
        try
        {
            var udtScopeId = program.Symbols.GetCurrentScope().Id;

            // Registra i type parameters (T1, T2, ...) nello scope del tipo
            if (!udt.TypeParameters.IsDefaultOrEmpty)
            {
                foreach (var tp in udt.TypeParameters)
                {
                    program.Symbols.RegisterSymbol(tp.Name, SymbolFlags.IsTypeParam, tp, tp);
                }
            }

            // Registra i campi
            var updatedFields = udt.Fields.Select(field =>
            {
                const SymbolFlags fieldFlags =
                    SymbolFlags.IsField | SymbolFlags.IsPublic;
                var fieldId = program.Symbols.RegisterSymbol(field.Name, fieldFlags, field.DataType, field);
                var finalField = field with { SymbolId = fieldId, ScopeId = udtScopeId };
                program.Symbols.UpdateSymbolDeclaration(fieldId, finalField);
                return finalField;
            }).ToImmutableArray();

            // Registra i metodi
            var updatedMethods = udt.Methods.Select(m => { m.Parent = udt; return RegisterProcedure(program, m, isStrict); }).ToImmutableArray();

            // Registra proprietà
            var updatedProperties = udt.Properties
                .Select(p =>
                    RegisterPropertyMember(program, new PropertyMember(p, AccessModifier.Public), isStrict).Property)
                .ToImmutableArray();

            // Registra operatori e costruttori
            var updatedOperators = RegisterUdtOperators(program, udt.Operators, isStrict);
            var updatedConstructors = RegisterUdtConstructors(program, udt.Constructors, isStrict, udt);
            // Destruttori non implementati completamente in questa fase, ma la logica sarebbe analoga

            var finalUdt = udt with
            {
                SymbolId = typeSymbolId,
                ScopeId = parentScopeId, // Il nodo UDT stesso vive nello scope genitore
                Fields = updatedFields,
                Methods = updatedMethods,
                Properties = updatedProperties,
                Operators = updatedOperators,
                Constructors = updatedConstructors
            };

            program.Symbols.UpdateSymbolDeclaration(typeSymbolId, finalUdt);
            return finalUdt;
        }
        finally
        {
            program.Symbols.ExitScope();
        }
    }

    /// <summary>
    /// Itera e registra la lista di operatori sovraccaricati definiti in un UDT.
    /// Simile ai costruttori, delega la logica all'helper <see cref="RegisterOperatorMember"/>.
    ///
    /// RESPONSABILITÀ:
    /// 1.  Determinazione Accesso: Calcola l'AccessModifier (Public/Private) basandosi sui flag
    ///     della definizione dell'operatore.
    /// 2.  Creazione Wrapper: Crea un `OperatorMember` temporaneo per adattare la definizione dell'UDT
    ///     alla firma dell'helper di registrazione.
    /// 3.  Delega: Invoca `RegisterOperatorMember` per gestire la creazione dello scope, la registrazione
    ///     dei parametri e il binding del corpo.
    /// 4.  Estrazione e Collezione: Recupera la definizione arricchita e la aggiunge alla lista finale.
    ///
    /// Permette agli UDT di supportare l'overloading degli operatori con la stessa semantica delle classi.
    /// </summary>
    /// <param name="program">L'istanza del programma.</param>
    /// <param name="operators">La lista di definizioni di operatori dell'UDT.</param>
    /// <param name="isStrict">OPTION STRICT enabled or disabled flag</param>
    /// <returns>Una nuova lista immutabile di operatori registrati e legati.</returns>
    private static ImmutableArray<OperatorDefinition> RegisterUdtOperators(BasicProgram program,
        ImmutableArray<OperatorDefinition> operators, bool isStrict)
    {
        var updatedOperators = ImmutableArray.CreateBuilder<OperatorDefinition>();
        foreach (var op in operators)
        {
            // Un operatore è essenzialmente una procedura. Usiamo un helper simile a RegisterOperatorMember ma adattato.
            var access = op.Flags.HasFlag(SymbolFlags.IsPublic)
                ? AccessModifier.Public
                : AccessModifier.Private;
            var registeredMember = RegisterOperatorMember(program, new OperatorMember(op, access), isStrict);
            updatedOperators.Add(registeredMember.Operator);
        }

        return updatedOperators.ToImmutable();
    }

    /// <summary>
    /// Itera e registra la lista di costruttori definiti in un UDT (User-Defined Type).
    /// Delega la registrazione di ogni singolo costruttore all'helper <see cref="RegisterConstructorMember"/>
    /// (adattato per gestire definizioni pure invece di membri di classe).
    ///
    /// RESPONSABILITÀ:
    /// 1.  Creazione Wrapper Temporaneo: Poiché `RegisterConstructorMember` è progettato per lavorare con
    ///     `ConstructorMember` (che appartengono alle classi), questo metodo crea un wrapper `ConstructorMember`
    ///     al volo per ogni `ConstructorDefinition` dell'UDT.
    /// 2.  Delega: Chiama `RegisterConstructorMember` per gestire lo scope e i parametri.
    /// 3.  Estrazione: Recupera la `ConstructorDefinition` arricchita dal wrapper restituito.
    /// 4.  Collezione: Raccoglie i costruttori registrati in una `ImmutableArray`.
    ///
    /// Questo metodo colma il divario strutturale tra UDT e Classi per quanto riguarda i costruttori,
    /// permettendo di riutilizzare la logica di binding.
    /// </summary>
    /// <param name="program">L'istanza del programma.</param>
    /// <param name="constructors">La lista di definizioni di costruttori dell'UDT.</param>
    /// <param name="isStrict">OPTION STRICT enabled or disabled flag</param>
    /// <returns>Una nuova lista immutabile di costruttori registrati e legati.</returns>
    private static ImmutableArray<ConstructorDefinition> RegisterUdtConstructors(BasicProgram program,
        ImmutableArray<ConstructorDefinition> constructors, bool isStrict, UserDefinedType? parentUdt = null)
    {
        var updatedConstructors = ImmutableArray.CreateBuilder<ConstructorDefinition>();
        foreach (var ctor in constructors)
        {
            if (parentUdt != null) ctor.Parent = parentUdt;
            var registeredMember = RegisterConstructorMember(program, new ConstructorMember(ctor), isStrict);
            updatedConstructors.Add(registeredMember.Constructor);
        }

        return updatedConstructors.ToImmutable();
    }

    /// <summary>
    /// [FASE 2] Registra una definizione di interfaccia e tutti i suoi membri nella SymbolTable,
    /// creando uno scope dedicato.
    /// </summary>
    private static InterfaceDefinition RegisterInterface(BasicProgram program, InterfaceDefinition interfaceDef, bool isStrict)
    {
        var parentScopeId = program.Symbols.GetCurrentScope().Id;

        // Registra l'interfaccia stessa come simbolo di tipo
        var interfaceSymbolId = program.Symbols.RegisterSymbol(interfaceDef, interfaceDef.Flags);
        Log($"[BINDER]  -> Registrata Interfaccia '{interfaceDef.OriginalName.Lexeme}' con ID {interfaceSymbolId} in Scope {parentScopeId}.", BindingLog);

        program.Symbols.EnterScope(interfaceDef.Name.Lexeme, SymbolTable.ScopeType.Class, interfaceSymbolId);
        try
        {
            var updatedMembers = interfaceDef.Members.Select(member =>
            {
                // Qui, a differenza del parser di classe, non abbiamo bisogno di un dispatcher complesso
                // perché un'interfaccia può contenere solo firme di metodi, proprietà ed eventi.
                return (ClassMember)(member switch
                {
                    MethodMember mm => RegisterMethodMember(program, mm, isStrict),
                    PropertyMember pm => RegisterPropertyMember(program, pm, isStrict),
                    EventMember em => RegisterEventMember(program, em),
                    _ => throw new InvalidOperationException($"Tipo di membro '{member.GetType().Name}' non valido per un'interf'accia.")
                });
            }).ToImmutableArray();

            var finalInterfaceDef = interfaceDef with
            {
                SymbolId = interfaceSymbolId,
                ScopeId = parentScopeId,
                Members = updatedMembers
            };
            
            program.Symbols.UpdateSymbolDeclaration(interfaceSymbolId, finalInterfaceDef);
            return finalInterfaceDef;
        }
        finally
        {
            program.Symbols.ExitScope();
        }
    }
    
    /// <summary>
    /// Registra una CLASSE e i suoi membri. Crea uno scope dedicato di tipo `ScopeType.Class`.
    ///
    /// Responsabilità Architetturali:
    /// 1.  Registrazione Classe: Registra il simbolo della classe nello scope genitore.
    /// 2.  Scope Classe: Apre un nuovo scope per la classe.
    /// 3.  Membri: Delega a `RegisterClassMembers` la registrazione di tutti i membri (campi, metodi, etc.),
    ///     che verranno inseriti nello scope della classe.
    /// 4.  Timbratura: Restituisce il nodo `ClassDefinition` aggiornato.
    /// 5.  Chiusura: Chiude lo scope della classe.
    /// </summary>
    /// <param name="program">L'istanza del programma.</param>
    /// <param name="classDef">Il nodo ClassDefinition da registrare.</param>
    /// <param name="isStrict">OPTION STRICT enabled or disabled flag</param>
    /// <returns>Il nodo `ClassDefinition` aggiornato.</returns>
    private static ClassDefinition RegisterClass(BasicProgram program, ClassDefinition classDef, bool isStrict)
    {
        var parentScopeId = program.Symbols.GetCurrentScope().Id;
        // Il tipo di una classe è ClassType(classDef)
        var classSymbolId =
            program.Symbols.RegisterSymbol(classDef.OriginalName, classDef.Flags, new ClassType(classDef), classDef);
        Log(
            $"[BINDER]  -> Registrata Classe '{classDef.OriginalName.Lexeme}' con ID {classSymbolId} in Scope {parentScopeId}.",
            BindingLog);

        program.Symbols.EnterScope(classDef.Name.Lexeme, SymbolTable.ScopeType.Class, classSymbolId);
        try
        {
            var updatedMembers = RegisterClassMembers(program, classDef.Members, isStrict);

            var finalClass = classDef with
            {
                SymbolId = classSymbolId,
                ScopeId = parentScopeId,
                Members = updatedMembers
            };

            program.Symbols.UpdateSymbolDeclaration(classSymbolId, finalClass);
            return finalClass;
        }
        finally
        {
            program.Symbols.ExitScope();
        }
    }

    /// <summary>
    /// Itera su una collezione di membri di una classe (campi, metodi, proprietà, etc.) e delega
    /// la registrazione di ciascuno al metodo specifico corrispondente.
    ///
    /// RESPONSABILITÀ:
    /// 1.  Dispatching Polimorfico: Usa il pattern matching switch per identificare il tipo concreto
    ///     di ogni `ClassMember` (es. `FieldMember`, `MethodMember`).
    /// 2.  Delega: Invoca l'helper specializzato (es. `RegisterFieldMember`, `RegisterMethodMember`)
    ///     che contiene la logica di binding specifica per quel tipo di membro.
    /// 3.  Pass-through: Per i tipi di membro non gestiti esplicitamente, restituisce il membro originale.
    /// 4.  Aggregazione: Raccoglie i membri "arricchiti" (con SymbolId e ScopeId risolti) in una
    ///     nuova `ImmutableArray`, pronta per essere assegnata alla definizione della classe.
    ///
    /// Questo metodo agisce come un "hub" per processare in massa tutti i costituenti di una classe.
    /// </summary>
    /// <param name="program">L'istanza del programma.</param>
    /// <param name="members">La lista dei membri grezzi della classe da registrare.</param>
    /// <param name="isStrict">OPTION STRICT enabled or disabled flag</param>
    /// <returns>Una nuova lista immutabile contenente i membri registrati e legati.</returns>
    private static ImmutableArray<ClassMember> RegisterClassMembers(BasicProgram program,
        ImmutableArray<ClassMember> members, bool isStrict)
    {
        return
        [
            ..members.Select(member => member switch
            {
                FieldMember fm => RegisterFieldMember(program, fm, isStrict),
                MethodMember mm => RegisterMethodMember(program, mm, isStrict),
                PropertyMember pm => RegisterPropertyMember(program, pm, isStrict),
                OperatorMember om => RegisterOperatorMember(program, om, isStrict),
                ConstructorMember cm => RegisterConstructorMember(program, cm, isStrict),
                DestructorMember dm => RegisterDestructorMember(program, dm, isStrict),
                EventMember em => RegisterEventMember(program, em),
                _ => member
            })
        ];
    }

    /// <summary>
    /// Registra un membro di tipo campo (`FieldMember`) all'interno di una classe.
    /// Esegue la registrazione del simbolo nella SymbolTable e la timbratura dello scope.
    ///
    /// RESPONSABILITÀ:
    /// 1.  Calcolo dei Flag: Combina i flag definiti nella dichiarazione (`DIM`, `STATIC`, visibilità)
    ///     con il flag obbligatorio `SymbolFlags.IsField`.
    /// 2.  Registrazione Simbolo: Registra il campo nella SymbolTable come un nuovo simbolo nello scope
    ///     della classe corrente. Usa `InferredType` come placeholder se il tipo non è esplicito,
    ///     lasciando la risoluzione completa alla Fase 3.
    /// 3.  Timbratura Scope: Assegna lo `ScopeId` corrente (che è lo scope della classe) al nodo
    ///     di dichiarazione del campo.
    /// 4.  Aggiornamento Simbolo: Aggiorna il record del simbolo nella SymbolTable con il nodo AST
    ///     arricchito (contenente il `SymbolId` e lo `ScopeId`).
    /// 5.  Ricostruzione Membro: Restituisce una nuova istanza di `FieldMember` che contiene la
    ///     dichiarazione aggiornata e legata.
    ///
    /// Nota: L'espressione di inizializzazione (`= value`) NON viene risolta qui. Viene lasciata
    /// intatta per essere processata dal Resolver (Fase 2.5) e dal Linker (Fase 3).
    /// </summary>
    /// <param name="program">L'istanza del programma.</param>
    /// <param name="fieldMember">Il membro campo da registrare.</param>
    /// <param name="isStrict">OPTION STRICT enabled or disabled flag</param>
    /// <returns>Una nuova istanza di `FieldMember` con la dichiarazione legata.</returns>
    private static FieldMember RegisterFieldMember(BasicProgram program, FieldMember fieldMember, bool isStrict)
    {
        var currentScopeId = program.Symbols.GetCurrentScope().Id;
        var decl = fieldMember.Declaration;

        var finalFlags = decl.Flags | SymbolFlags.IsField;

        var symbolId =
            program.Symbols.RegisterSymbol(decl.NameCleaned, finalFlags, decl.DataType ?? new InferredType(), decl);

        var boundInitialValue = decl.InitialValue != null
            ? BindExpression(decl.InitialValue, program, isStrict)
            : null;

        // Timbra le dimensioni
        var boundDimensions = decl.Dimensions
            .Select(d => BindExpression(d, program, isStrict))
            .ToImmutableArray();

        var boundLowerBounds = decl.LowerBounds
            .Select(d => BindExpression(d, program, isStrict))
            .ToImmutableArray();

        var finalDecl = decl with
        {
            SymbolId = symbolId,
            Flags = finalFlags,
            ScopeId = currentScopeId,
            InitialValue = boundInitialValue,
            Dimensions = boundDimensions,
            LowerBounds = boundLowerBounds
        };

        program.Symbols.UpdateSymbolDeclaration(symbolId, finalDecl);
        Log(
            $"[BINDER]    -> Registrato Campo '{finalDecl.NameSuffixed.Lexeme}' con ID {symbolId} (Flags: {finalFlags}) in Scope {currentScopeId}.",
            BindingLog);

        return fieldMember with { Declaration = finalDecl };
    }

    /// <summary>
    /// Registra un membro di tipo metodo (`MethodMember`) all'interno di una classe.
    ///
    /// RESPONSABILITÀ:
    /// Poiché un metodo è essenzialmente una procedura (FUNCTION o SUB) definita in uno scope di classe,
    /// questo helper delega l'intera logica di registrazione (creazione simbolo, scope procedura,
    /// binding parametri e corpo) al metodo generale <see cref="RegisterProcedure"/>.
    ///
    /// FLUSSO:
    /// 1. Estrae la `ProcedureDefinition` dal membro.
    /// 2. Chiama `RegisterProcedure`, che restituisce una versione "arricchita" della procedura
    ///    (con SymbolId, parametri legati e corpo processato).
    /// 3. Restituisce un nuovo `MethodMember` che incapsula la procedura registrata.
    /// </summary>
    /// <param name="program">L'istanza del programma.</param>
    /// <param name="methodMember">Il membro metodo da registrare.</param>
    /// <param name="isStrict">OPTION STRICT enabled or disabled flag</param>
    /// <returns>Una nuova istanza di `MethodMember` contenente la procedura registrata.</returns>
    private static MethodMember RegisterMethodMember(BasicProgram program, MethodMember methodMember, bool isStrict)
    {
        var registeredProcedure = RegisterProcedure(program, methodMember.Procedure, isStrict);
        return methodMember with { Procedure = registeredProcedure };
    }

    /// <summary>
    /// Registra un membro di tipo proprietà (`PropertyMember`) all'interno di una classe.
    /// Le proprietà sono complesse perché definiscono un simbolo (il nome della proprietà) ma contengono
    /// anche blocchi di codice (gli accessori GET e SET) che richiedono i propri scope.
    ///
    /// RESPONSABILITÀ:
    /// 1.  Registrazione Simbolo: Registra la proprietà stessa nella SymbolTable nello scope della classe.
    /// 2.  Scope di Blocco: Crea un nuovo scope di tipo `Block` (es. `PROP_NomeProprieta`) per isolare
    ///     gli accessori dal resto della classe.
    /// 3.  Binding Getter: Se presente, processa il corpo del `GET` invocando `BindStatementList`.
    /// 4.  Binding Setter: Se presente, crea un ulteriore sotto-scope procedurale (`SET_...`), registra
    ///     il parametro implicito `value` e poi processa il corpo del `SET`.
    /// 5.  Aggiornamento: Aggiorna la definizione della proprietà nella SymbolTable con gli accessori legati.
    /// 6.  Ricostruzione: Restituisce un nuovo `PropertyMember` con la definizione completa e legata.
    ///
    /// Questo metodo garantisce che le variabili dichiarate dentro un GET non siano visibili nel SET
    /// e che il parametro `value` sia correttamente tipizzato e accessibile nel SET.
    /// </summary>
    /// <param name="program">L'istanza del programma.</param>
    /// <param name="propertyMember">Il membro proprietà da registrare.</param>
    /// <param name="isStrict">OPTION STRICT enabled or disabled flag</param>
    /// <returns>Una nuova istanza di `PropertyMember` con la proprietà e gli accessori legati.</returns>
    private static PropertyMember RegisterPropertyMember(BasicProgram program, PropertyMember propertyMember,
        bool isStrict)
    {
        var classScopeId = program.Symbols.GetCurrentScope().Id;
        var prop = propertyMember.Property;
        // Le proprietà sono simboli nello scope della classe
        var propId = program.Symbols.RegisterSymbol(prop.Name, prop.Flags, prop.Type, prop);

        // Le proprietà hanno un loro scope interno per i getter/setter
        program.Symbols.EnterScope($"PROP_{prop.Name.Lexeme}", SymbolTable.ScopeType.Block, propId);
        try
        {
            // Se la proprietà appartiene a una classe, registra 'SELF' nel suo scope.
            if (propertyMember.Parent is { } ownerClass)
            {
                var selfToken = new Token(TokenKind.Self, "SELF", null, prop.Position);
                program.Symbols.RegisterSymbol(
                    selfToken,
                    SymbolFlags.IsLocal | SymbolFlags.IsSynthetic,
                    new ClassType(ownerClass),
                    selfToken
                );
            }
            PropertyGetter? updatedGetter = null;
            if (prop.Getter != null)
            {
                // Il Getter è un blocco di codice, non una vera funzione con nome per l'utente, 
                // ma internamente ha uno scope. BindStatementList gestirà il contenuto.
                var boundGetterBody = BindListOfStatements(prop.Getter.Body, program, isStrict);
                updatedGetter = new PropertyGetter(boundGetterBody);
            }

            PropertySetter? updatedSetter = null;
            if (prop.Setter != null)
            {
                // Il Setter ha un parametro 'value'. Dobbiamo registrarlo.
                program.Symbols.EnterScope($"SET_{prop.Name.Lexeme}", SymbolTable.ScopeType.Procedure);
                var setterScopeId = program.Symbols.GetCurrentScope().Id;
                var param = prop.Setter.ValueParameter;
                var paramFlags = param.Flags | SymbolFlags.IsParameter;
                var paramId = program.Symbols.RegisterSymbol(param.OriginalName, paramFlags, param.DataType, param);
                var finalParam = param with { SymbolId = paramId, ScopeId = setterScopeId };
                program.Symbols.UpdateSymbolDeclaration(paramId, finalParam);

                var boundSetterBody = BindListOfStatements(prop.Setter.Body, program, isStrict);
                program.Symbols.ExitScope(); // Chiude scope SET

                updatedSetter = new PropertySetter(finalParam, boundSetterBody);
            }

            var updatedProp = prop with
            {
                SymbolId = propId, Getter = updatedGetter, Setter = updatedSetter, ScopeId = classScopeId
            };
            program.Symbols.UpdateSymbolDeclaration(propId, updatedProp);

            return propertyMember with { Property = updatedProp };
        }
        finally
        {
            program.Symbols.ExitScope(); // Chiude scope PROP
        }
    }

    /// <summary>
    /// Registra un membro di tipo operatore sovraccaricato (`OperatorMember`) all'interno di una classe.
    /// Un operatore è semanticamente analogo a una procedura statica con un nome speciale.
    ///
    /// RESPONSABILITÀ:
    /// 1.  Naming Convenzionale: Genera un nome interno univoco per l'operatore (es. `op_+`) basato
    ///     sul token dell'operatore, per evitare conflitti nella SymbolTable.
    /// 2.  Registrazione Simbolo: Registra l'operatore come simbolo nello scope della classe.
    /// 3.  Scope Procedurale: Crea un nuovo scope di tipo `Procedure` per il corpo dell'operatore.
    /// 4.  Binding Parametri: Registra i parametri dell'operatore (es. `a`, `b` per un binario) nel nuovo scope.
    /// 5.  Binding Corpo: Processa il corpo dell'operatore invocando `BindStatementList`.
    /// 6.  Aggiornamento: Aggiorna la definizione dell'operatore nella SymbolTable con i parametri e il corpo legati.
    /// 7.  Ricostruzione: Restituisce un nuovo `OperatorMember` contenente la definizione aggiornata.
    /// </summary>
    /// <param name="program">L'istanza del programma.</param>
    /// <param name="operatorMember">Il membro operatore da registrare.</param>
    /// <param name="isStrict">OPTION STRICT enabled or disabled flag</param>
    /// <returns>Una nuova istanza di `OperatorMember` con la definizione legata.</returns>
    private static OperatorMember RegisterOperatorMember(BasicProgram program, OperatorMember operatorMember,
        bool isStrict)
    {
        var op = operatorMember.Operator;
        var classScopeId = program.Symbols.GetCurrentScope().Id;
        // L'arità (numero di parametri) va nel nome interno: senza di essa un operatore
        // che ammette sia forma binaria SIA forma unaria con lo stesso token (es. Minus:
        // sottrazione "a - b" vs negazione "-a", pattern comune per un tipo Vettore)
        // collideva sullo stesso simbolo "op_-", fallendo con "simbolo già dichiarato in
        // questo scope" — scoperto testando OPERATOR unario nel backend IL.
        var opName = op.OperatorToken.Kind == TokenKind.CType
            ? "op_" + op.OperatorToken.Lexeme + "_" + op.ReturnType.ToString()
            : "op_" + op.OperatorToken.Lexeme + "_" + op.Parameters.Length;
        var opToken = new Token(TokenKind.Identifier, opName, null, op.Position);

        var symbolId = program.Symbols.RegisterSymbol(opToken, op.Flags, op.ReturnType, op);

        program.Symbols.EnterScope(opName, SymbolTable.ScopeType.Procedure, symbolId);
        try
        {
            var operatorScopeId = program.Symbols.GetCurrentScope().Id;

            var updatedParameters = op.Parameters.Select(param =>
            {
                var paramFlags = param.Flags | SymbolFlags.IsParameter;
                var paramId = program.Symbols.RegisterSymbol(param.OriginalName, paramFlags, param.DataType, param);
                var finalParam = param with { SymbolId = paramId, ScopeId = operatorScopeId };
                program.Symbols.UpdateSymbolDeclaration(paramId, finalParam);
                return finalParam;
            }).ToImmutableArray();

            var updatedBody = BindListOfStatements(op.Body, program, isStrict);

            var finalOp = op with { Parameters = updatedParameters, Body = updatedBody, ScopeId = classScopeId };

            program.Symbols.UpdateSymbolDeclaration(symbolId, finalOp);
            return operatorMember with { Operator = finalOp };
        }
        finally
        {
            program.Symbols.ExitScope();
        }
    }

    /// <summary>
    /// Registra un membro costruttore (`ConstructorMember`) all'interno di una classe.
    /// I costruttori sono metodi speciali (`SUB NEW`) che inizializzano le istanze.
    ///
    /// RESPONSABILITÀ:
    /// 1.  Scope Procedurale: Crea un nuovo scope di tipo `Procedure` con nome "NEW". Poiché in BASIC
    ///     il costruttore si chiama sempre `NEW`, questo scope isola le variabili locali e i parametri
    ///     del costruttore.
    /// 2.  Binding Parametri: Itera sui parametri del costruttore, registrandoli come simboli locali
    ///     nel nuovo scope e timbrandoli con lo `ScopeId` e il `SymbolId`.
    /// 3.  Binding Corpo: Processa il corpo del costruttore invocando `BindStatementList`. Questo
    ///     assicura che le istruzioni (inclusi eventuali `BASE.NEW(...)`) siano analizzate nel contesto corretto.
    /// 4.  Ricostruzione: Restituisce un nuovo `ConstructorMember` con la definizione del costruttore
    ///     aggiornata (parametri e corpo legati).
    ///
    /// Nota: Non registra un simbolo per il costruttore stesso nella SymbolTable della classe in questo momento,
    /// in quanto i costruttori non sono membri "chiamabili" standard, ma punti di ingresso per l'istanziazione.
    /// </summary>
    /// <param name="program">L'istanza del programma.</param>
    /// <param name="constructorMember">Il membro costruttore da registrare.</param>
    /// <param name="isStrict">OPTION STRICT enabled or disabled flag</param>
    /// <returns>Una nuova istanza di `ConstructorMember` con la definizione legata.</returns>
    private static ConstructorMember RegisterConstructorMember(BasicProgram program,
        ConstructorMember constructorMember, bool isStrict)
    {
        var ctor = constructorMember.Constructor;

        // Crea uno scope procedurale univoco per ogni costruttore.
        var pos = ctor.Parameters.FirstOrDefault()?.Position ?? constructorMember.Name.Position;
        program.Symbols.EnterScope($"NEW_{pos.Line}_{pos.Column}", SymbolTable.ScopeType.Procedure);
        try
        {
            // Se il costruttore appartiene a una classe, registra 'SELF' e 'ME'.
            if (constructorMember.Parent is { } ownerClass)
            {
                var classType = new ClassType(ownerClass);
                var selfToken = new Token(TokenKind.Self, "SELF", null, constructorMember.Name.Position);
                program.Symbols.RegisterSymbol(selfToken, SymbolFlags.IsLocal | SymbolFlags.IsSynthetic, classType, selfToken);
                var meToken = new Token(TokenKind.Identifier, "ME", null, constructorMember.Name.Position);
                program.Symbols.RegisterSymbol(meToken, SymbolFlags.IsLocal | SymbolFlags.IsSynthetic, classType, meToken);
            }
            // UDT constructor: registra SELF con UserDefinedType.
            if (constructorMember.Constructor.Parent is UserDefinedType udtOwner)
            {
                var udtToken = new Token(TokenKind.Self, "SELF", null, constructorMember.Name.Position);
                program.Symbols.RegisterSymbol(udtToken, SymbolFlags.IsLocal | SymbolFlags.IsSynthetic, udtOwner, udtToken);
            }
            var constructorScopeId = program.Symbols.GetCurrentScope().Id;
            var updatedParameters = ctor.Parameters.Select(param =>
            {
                var paramFlags = param.Flags | SymbolFlags.IsParameter;
                var paramName = param.OriginalName.Lexeme;
                // Skip if the symbol is already declared (e.g., inherited or duplicate)
                var scope2 = program.Symbols.GetCurrentScope();
                int paramId;
                if (scope2.SymbolsByName.ContainsKey(paramName))
                {
                    // Find existing symbol
                    var existingIds = scope2.SymbolsByName[paramName];
                    paramId = existingIds[0];
                }
                else
                {
                    paramId = program.Symbols.RegisterSymbol(param.OriginalName, paramFlags, param.DataType, param);
                }
                var finalParam = param with { SymbolId = paramId, ScopeId = constructorScopeId };
                program.Symbols.UpdateSymbolDeclaration(paramId, finalParam);
                return finalParam;
            }).ToImmutableArray();

            var updatedBody = BindListOfStatements(ctor.Body, program, isStrict);
            var updatedCtor = ctor with { Parameters = updatedParameters, Body = updatedBody };

            return constructorMember with { Constructor = updatedCtor };
        }
        finally
        {
            program.Symbols.ExitScope();
        }
    }

    /// <summary>
    /// Registra un membro distruttore (`DestructorMember`) all'interno di una classe.
    /// I distruttori sono metodi speciali (`SUB DESTRUCTOR`) invocati alla distruzione dell'istanza.
    ///
    /// RESPONSABILITÀ:
    /// 1.  Scope Procedurale: Crea un nuovo scope di tipo `Procedure` con nome "DELETE" per isolare
    ///     le variabili locali del distruttore.
    /// 2.  Binding Corpo: Processa il corpo del distruttore invocando `BindStatementList`.
    /// 3.  Ricostruzione: Restituisce un nuovo `DestructorMember` con il corpo legato.
    ///
    /// Similmente al costruttore, il distruttore non ha parametri espliciti e il suo scopo è puramente
    /// esecutivo (pulizia risorse), quindi la logica principale è la gestione dello scope del corpo.
    /// </summary>
    /// <param name="program">L'istanza del programma.</param>
    /// <param name="destructorMember">Il membro distruttore da registrare.</param>
    /// <param name="isStrict">OPTION STRICT enabled or disabled flag</param>
    /// <returns>Una nuova istanza di `DestructorMember` con il corpo legato.</returns>
    private static DestructorMember RegisterDestructorMember(BasicProgram program, DestructorMember destructorMember,
        bool isStrict)
    {
        var dtor = destructorMember.Destructor;
        program.Symbols.EnterScope("DELETE", SymbolTable.ScopeType.Procedure);
        try
        {
            var updatedBody = BindListOfStatements(dtor.Body, program, isStrict);
            var updatedDtor = dtor with { Body = updatedBody };
            return destructorMember with { Destructor = updatedDtor };
        }
        finally
        {
            program.Symbols.ExitScope();
        }
    }

    /// <summary>
    /// Registra un membro di tipo evento (`EventMember`) all'interno di una classe.
    /// </summary>
    private static EventMember RegisterEventMember(BasicProgram program, EventMember eventMember)
    {
        var currentScopeId = program.Symbols.GetCurrentScope().Id;
        var evtDef = eventMember.Event;

        // Combina i flag (IsEvent è già presente, aggiungi IsPublic/Private/etc. dal membro)
        var finalFlags = evtDef.Flags;

        // I parametri dell'evento definiscono la firma del delegato implicito.
        // Costruiamo un FunctionPointerType per rappresentare il tipo dell'evento nella SymbolTable.
        var paramTypes = evtDef.Parameters.Select(p => p.DataType).ToImmutableArray();
        var eventType = new FunctionPointerType(paramTypes, new VoidType());

        var symbolId = program.Symbols.RegisterSymbol(evtDef.Name, finalFlags, eventType, evtDef);

        // Timbra i parametri con lo scope corrente (Classe) o crea uno scope fittizio?
        // Gli eventi non hanno un corpo, ma i parametri devono essere risolti/registrati se hanno tipi complessi.
        // Per ora registriamo l'evento come simbolo.

        var finalDef = evtDef with { SymbolId = symbolId, ScopeId = currentScopeId };
        program.Symbols.UpdateSymbolDeclaration(symbolId, finalDef);

        Log(
            $"[BINDER]    -> Registrato Evento '{finalDef.OriginalName.Lexeme}' con ID {symbolId} in Scope {currentScopeId}.",
            BindingLog);

        return eventMember with { Event = finalDef };
    }

    /// <summary>
    /// Registra un blocco di dichiarazioni di variabili (`DIM`, `GLOBAL`) che appaiono a livello di modulo
    /// (o direttamente in un namespace). Queste variabili hanno visibilità globale o di namespace.
    /// 
    /// RESPONSABILITÀ:
    /// 1.  Flag Globali: Applica forzatamente il flag `SymbolFlags.IsGlobal` a tutte le dichiarazioni contenute,
    ///     distinguendole così dalle variabili locali.
    /// 2.  Registrazione Simbolo: Per ogni variabile nel blocco, crea un simbolo nella SymbolTable nello scope
    ///     corrente (che si presume essere un Namespace o il Globale).
    /// 3.  Inizializzazione: Timbra l'espressione di inizializzazione (`= value`) con lo `ScopeId` corrente,
    ///     ma NON la risolve (il binding delle espressioni non avviene in Fase 2).
    /// 4.  Aggiornamento: Aggiorna il simbolo registrato con il nodo di dichiarazione arricchito.
    /// 5.  Ricostruzione: Restituisce un nuovo `DeclarationStmt` contenente la lista aggiornata delle
    ///     dichiarazioni legate.
    /// 
    /// Questo metodo è fondamentale per popolare lo scope globale prima che venga analizzato il corpo
    /// delle procedure, permettendo alle funzioni di vedere le variabili globali.
    /// </summary>
    /// <param name="program">L'istanza del programma.</param>
    /// <param name="declStmt">Lo statement di dichiarazione (che può contenere multiple variabili).</param>
    /// <param name="bindExpressions">Flag per evitare il doppio binding nelle espressioni</param>
    /// <param name="isStrict">OPTION STRICT enabled or disabled flag</param>
    /// <returns>Un nuovo nodo `DeclarationStmt` con tutte le dichiarazioni registrate.</returns>
    private static DeclarationStmt RegisterGlobalVariables(BasicProgram program, DeclarationStmt declStmt,
        bool bindExpressions, bool isStrict)
    {
        var currentScopeId = program.Symbols.GetCurrentScope().Id;

        var updatedDeclarations = declStmt.Declarations.Select(decl =>
        {
            var finalFlags = decl.Flags;
            if (declStmt.Keyword.Kind == TokenKind.Dynamic)
            {
                // Le variabili PRIVATE dichiarate a livello di modulo/namespace sono comunque
                // legate allo scope dinamico, non globale/lessicale.
                finalFlags = (finalFlags & ~SymbolFlags.IsGlobal) | SymbolFlags.IsDynamicScope;
            }
            else
            {
                // Comportamento standard per GLOBAL, DIM, etc.
                finalFlags |= SymbolFlags.IsGlobal;
            }

            var existingSymbolIds = program.Symbols.FindSymbolInScope(decl.NameCleaned.Lexeme);
            int symbolId;

            // Fix collisione nome TYPE/variabile (2026-10-01): un simbolo esistente con lo
            // stesso nome case-insensitive che è una dichiarazione DI MODULO (TYPE/CLASS/
            // PROCEDURE/INTERFACE/ENUM) non è un placeholder-variabile riusabile — riusarlo
            // sovrascriverebbe il suo DeclarationNode, facendolo sparire dal registro UDT/
            // classi (bug: script top-level con `TYPE P` + `DIM p AS P` nello stesso scope di
            // modulo — dentro una procedura le variabili sono in uno scope separato, nessuna
            // collisione — vedi AiWiki Bugs/(open) - TYPE-TopLevel-NonRegistrato-Come-Tipo-
            // Modulo.md). In tal caso si registra un simbolo SEPARATO per la variabile; la
            // risoluzione dei riferimenti-valore successivi preferisce il simbolo non-modulo
            // (vedi IsModuleTypeDeclarationNode/FindSymbolIdInScopeChain in Linker.Resolver.cs).
            var reusableSymbolId = existingSymbolIds?.FirstOrDefault(id =>
                !IsModuleTypeDeclarationNode(program.Symbols.GetSymbol(id).DeclarationNode), -1) ?? -1;

            if (reusableSymbolId >= 0)
            {
                symbolId = reusableSymbolId;
                Log(
                    $"[BINDER]    -> Simbolo globale '{decl.NameSuffixed.Lexeme}' già registrato. ID recuperato: {symbolId}",
                    BindingLog);
            }
            else
            {
                symbolId = program.Symbols.RegisterSymbol(decl.NameCleaned, finalFlags,
                    decl.DataType ?? new InferredType(), decl);
                Log(
                    $"[BINDER]    -> Registrata variabile globale/dinamica '{decl.NameSuffixed.Lexeme}' con ID {symbolId} in Scope {currentScopeId}.",
                    BindingLog);
            }

            var boundInitialValue = decl.InitialValue;
            if (bindExpressions && decl.InitialValue is not null)
            {
                boundInitialValue = BindExpression(decl.InitialValue, program, isStrict);
            }

            var boundDimensions = decl.Dimensions;
            if (bindExpressions && !decl.Dimensions.IsDefaultOrEmpty)
            {
                boundDimensions = [..decl.Dimensions.Select(d => BindExpression(d, program, isStrict))];
            }

            var boundLowerBounds = decl.LowerBounds;
            if (bindExpressions && !decl.LowerBounds.IsDefaultOrEmpty)
            {
                boundLowerBounds = [..decl.LowerBounds.Select(d => BindExpression(d, program, isStrict))];
            }

            var finalDecl = decl with
            {
                SymbolId = symbolId,
                Flags = finalFlags,
                ScopeId = currentScopeId,
                InitialValue = boundInitialValue,
                Dimensions = boundDimensions,
                LowerBounds = boundLowerBounds,
            };

            program.Symbols.UpdateSymbolDeclaration(symbolId, finalDecl);

            return finalDecl;
        }).ToImmutableArray();

        return new DeclarationStmt(declStmt.Keyword, updatedDeclarations)
        {
            ScopeId = currentScopeId
        };
    }

    /// <summary>
    /// Registra un blocco di dichiarazioni di costanti (`CONST`) che appaiono a livello di modulo.
    ///
    /// RESPONSABILITÀ:
    /// 1.  Flag Costanti/Globali: Applica i flag `SymbolFlags.IsGlobal` e `SymbolFlags.IsConst`.
    /// 2.  Registrazione Simbolo: Registra ogni costante nella SymbolTable nello scope corrente.
    /// 3.  Timbratura: Assegna `ScopeId` e `SymbolId` alle dichiarazioni.
    /// 4.  Espressione Valore: Timbra l'espressione del valore con lo `ScopeId` corrente, ma non la risolve
    ///     o valuta (il Constant Folding avviene in una fase successiva).
    /// 5.  Ricostruzione: Restituisce un nuovo `ConstStmt` con le dichiarazioni legate.
    ///
    /// Questo permette di utilizzare le costanti globali in tutto il modulo o namespace.
    /// </summary>
    /// <param name="program">L'istanza del programma.</param>
    /// <param name="constStmt">Lo statement di dichiarazione costante.</param>
    /// <returns>Un nuovo nodo `ConstStmt` con le costanti registrate.</returns>
    /// <param name="isStrict">OPTION STRICT enabled or disabled flag</param>
    /// <param name="bindExpressions">Flag per evitare il doppio binding nelle espressioni</param>
    private static ConstStmt RegisterGlobalConstants(BasicProgram program, ConstStmt constStmt,
        bool bindExpressions, bool isStrict)
    {
        var currentScopeId = program.Symbols.GetCurrentScope().Id;
        var updatedDeclarations = constStmt.Declarations.Select(decl =>
        {
            var finalFlags = decl.Flags | SymbolFlags.IsGlobal | SymbolFlags.IsConst;

            // Il ConstStmt a TOP-LEVEL passa da DUE fasi del binding (BindDeclaration Fase 1 e
            // BindStatement Fase 2 — vedi i dispatch su ConstStmt): senza riuso dell'ID,
            // RegisterSymbol lanciava "già stato dichiarato" al secondo passaggio. Stesso
            // pattern di RegisterGlobalVariables (FindSymbolInScope → riuso ID).
            var existingSymbolIds = program.Symbols.FindSymbolInScope(decl.OriginalToken.Lexeme);
            int symbolId;
            // Fix collisione nome TYPE/costante (2026-10-01): stesso principio di
            // RegisterGlobalVariables sopra — non riusare un simbolo esistente che è una
            // dichiarazione di modulo (TYPE/CLASS/PROCEDURE/INTERFACE/ENUM). Il doppio-bind
            // dello STESSO CONST (commento sopra) resta invariato: al secondo passaggio il
            // simbolo esistente ha DeclarationNode = ConstDeclaration, mai escluso dal filtro.
            var reusableSymbolId = existingSymbolIds?.FirstOrDefault(id =>
                !IsModuleTypeDeclarationNode(program.Symbols.GetSymbol(id).DeclarationNode), -1) ?? -1;
            if (reusableSymbolId >= 0)
            {
                symbolId = reusableSymbolId;
            }
            else
            {
                symbolId = program.Symbols.RegisterSymbol(decl.OriginalToken, finalFlags,
                    decl.DataType ?? new InferredType(), decl);
            }

            // FIX: Skip binding se richiesto
            var boundInitialValue = decl.InitialValue;
            if (bindExpressions)
            {
                // Per le costanti, InitialValue è strutturalmente non-nullable, quindi lo leghiamo direttamente.
                boundInitialValue = BindExpression(decl.InitialValue, program, isStrict);
            }

            var finalDecl = decl with
            {
                SymbolId = symbolId,
                Flags = finalFlags,
                ScopeId = currentScopeId,
                InitialValue = boundInitialValue
            };

            program.Symbols.UpdateSymbolDeclaration(symbolId, finalDecl);
            return finalDecl;
        }).ToImmutableArray();

        return constStmt with { Declarations = updatedDeclarations, ScopeId = currentScopeId };
    }

    /// <summary>
    /// Gestisce la registrazione implicita di variabili tramite istruzioni di assegnazione (`LET`)
    /// che appaiono a livello di modulo (fuori da procedure).
    ///
    /// RESPONSABILITÀ:
    /// 1.  Analisi Assegnazioni: Itera su tutte le assegnazioni nell'istruzione `LET`.
    /// 2.  Rilevamento Nuovi Simboli: Per ogni assegnazione a una variabile semplice (`VariableTarget`)
    ///     che ha `SymbolId` non risolto (minore di 0), verifica se il simbolo esiste già nello scope corrente
    ///     o nei suoi genitori.
    /// 3.  Creazione Implicita: Se il simbolo non esiste, lo crea "al volo" come una nuova variabile
    ///     nello scope corrente (che è globale o di namespace), applicando il flag `SymbolFlags.IsGlobal`.
    /// 4.  Aggiornamento Target: Aggiorna il nodo `VariableTarget` con il nuovo `SymbolId` e `ScopeId`.
    /// 5.  Ricostruzione: Restituisce un nuovo `LetStmt` con i target aggiornati. Le espressioni di valore
    ///     vengono lasciate intatte (non risolte).
    ///
    /// Questo metodo supporta lo stile di programmazione BASIC classico dove le variabili non devono
    /// essere dichiarate esplicitamente prima dell'uso.
    /// </summary>
    /// <param name="program">L'istanza del programma.</param>
    /// <param name="letStmt">Lo statement LET da processare.</param>
    /// <returns>Un nuovo nodo `LetStmt` con le variabili implicite registrate.</returns>
    private static LetStmt RegisterLetVariables(BasicProgram program, LetStmt letStmt)
    {
        // Per ogni assegnazione, se il target è una variabile semplice che non esiste, la creiamo implicitamente.
        var currentScopeId = program.Symbols.GetCurrentScope().Id;
        var updatedAssignments = letStmt.Assignments.Select(a =>
        {
            // Se è una VariableTarget con ID negativo (non ancora risolta)
            if (a.Target is VariableTarget { SymbolId: < 0 } vt)
            {
                // Cerca nello scope corrente
                var symbolIds = program.Symbols.FindSymbolInScope(vt.NameCleaned.Lexeme);
                if (symbolIds == null || symbolIds.Count == 0)
                {
                    // Non esiste: Creazione implicita globale (o di namespace) in questo contesto
                    var symbolId = program.Symbols.RegisterSymbol(
                        vt.NameCleaned,
                        SymbolFlags.IsGlobal, // Le variabili LET implicite top-level sono globali
                        new InferredType(),
                        vt
                    );
                    var newTarget = vt with { SymbolId = symbolId, ScopeId = currentScopeId };
                    program.Symbols.UpdateSymbolDeclaration(symbolId, newTarget);
                    Log(
                        $"[BINDER]    -> Variabile Globale Implicita '{vt.NameSuffixed.Lexeme}' registrata con ID {symbolId}.",
                        BindingLog);
                    return a with { Target = newTarget };
                }
            }

            // Altrimenti lascia stare, il Resolver collegherà l'uso al simbolo esistente.
            return a;
        }).ToImmutableArray();

        return letStmt with { Assignments = updatedAssignments, ScopeId = currentScopeId };
    }

    /// <summary>
    /// Attraversa ricorsivamente un'espressione e assegna lo ScopeId corrente a ogni nodo.
    /// Non risolve i simboli, si limita a timbrare il contesto. Questa versione è stata
    /// corretta per scendere ricorsivamente in tutti i nodi figli, preservando la logica
    /// esistente per la gestione dei parametri BYREF.
    /// </summary>
    private static Expr BindExpression(Expr expr, BasicProgram program, bool isStrict)
    {
        var currentScopeId = program.Symbols.GetCurrentScope().Id;

        Log($"[BINDER-EXPR] Timbratura {expr.GetType().Name} ('{expr}') con ScopeId: {currentScopeId}", BindingLog);

        Expr rewrittenExpr;

        switch (expr)
        {
            // --- LOGICA PRE-ESISTENTE (CONSERVATA) ---
            case FunctionExpr fe:
            {
                // In Fase 2, l'ID della funzione potrebbe non essere noto, ma il nome sì.
                // Dobbiamo tentare di trovare le procedure candidate per ispezionare i parametri.
                var candidates = program.Symbols.FindSymbolsByName(fe.OriginalToken.Lexeme)
                    .Select(s => s.DeclarationNode)
                    .OfType<ProcedureDefinition>()
                    .Where(p => SymbolTable.IsProcedureSignatureCompatible(p, fe.Args.Length))
                    .ToList();

                var targetProc = candidates.Count == 1 ? candidates[0] : null;

                var boundArgs = fe.Args.Select((arg, i) =>
                {
                    var param = targetProc?.Parameters.ElementAtOrDefault(i);
                    if (param != null && param.Flags.HasFlag(SymbolFlags.IsByRef) &&
                        arg is VariableExpr { SymbolId: < 0 } ve)
                    {
                        var existingIds = program.Symbols.FindSymbolInScope(ve.NameCleaned.Lexeme);
                        if (existingIds == null || !existingIds.Any())
                        {
                            // Crea implicitamente la variabile nello scope CORRENTE del Binder.
                            // Il tipo deve essere quello del PARAMETRO BYREF (es. SUB
                            // TestByRefString(BYREF S$) chiamata con D$ mai usato: D$ nasce
                            // STRING, non InferredType — con InferredType il backend IL
                            // dichiarava un local non-STRING e il BYREF produceva garbage
                            // (2.16924184205286E-314), vedi ScopesByRef.bas sez.4.
                            var symbolId = program.Symbols.RegisterSymbol(
                                ve.NameCleaned,
                                SymbolFlags.IsLocal,
                                param.DataType is not InferredType ? param.DataType : new InferredType(),
                                ve
                            );
                            Log(
                                $"[BINDER-BYREF] Variabile implicita (BYREF) '{ve.NameCleaned.Lexeme}' registrata con ID {symbolId}.",
                                BindingLog);

                            var boundVar = ve with { SymbolId = symbolId };
                            program.Symbols.UpdateSymbolDeclaration(symbolId, boundVar);
                            // Importante: il nodo boundVar deve essere timbrato con lo scopeId corrente.
                            return (Expr)boundVar with { ScopeId = currentScopeId };
                        }
                    }

                    return BindExpression(arg, program, isStrict);
                }).ToImmutableArray();

                rewrittenExpr = new FunctionExpr(fe.SymbolId, fe.OriginalToken, boundArgs, fe.ReturnType)
                    { ResolvedDefinition = fe.ResolvedDefinition };
                break;
            }

            case ArrayAccessExpr aae:
            {
                // La stessa logica di FunctionExpr si applica qui, perché questo nodo potrebbe essere una chiamata a UDF.
                var candidates = program.Symbols.FindSymbolsByName(aae.NameCleaned.Lexeme)
                    .Select(s => s.DeclarationNode)
                    .OfType<ProcedureDefinition>()
                    .Where(p => SymbolTable.IsProcedureSignatureCompatible(p, aae.Indices.Length))
                    // IDENTITÀ PER TOKENKIND (§12, regola d'oro): il lexeme è solo la grafia.
                    // Se il nome nel sorgente ha un Kind specifico (keyword, es. `LIST` →
                    // TokenKind.List), risponde SOLO la procedura registrata con lo stesso
                    // Kind: `AI.List` (BuiltinKind AiList) non è `LIST`. Per i nomi con Kind
                    // Identifier il nome resta l'unica chiave possibile.
                    .Where(p => aae.NameCleaned.Kind == TokenKind.Identifier
                                || (p is NativeProcedureDefinition npd ? npd.BuiltinKind : p.OriginalName.Kind)
                                == aae.NameCleaned.Kind)
                    .ToList();

                var targetProc = candidates.Count == 1 ? candidates[0] : null;

                // Una variabile visibile omonima di un builtin ha la priorità sul builtin
                // stesso (es. DIM amax() vs AMAX, DIM argmax() vs MATRIX.ARGMAX): senza,
                // il nodo verrebbe timbrato con l'ID della procedura builtin e la variabile
                // diventerebbe illeggibile (bug MATRIX.ARGMAX/ARGMIN). Vale per OGNI storage
                // dichiarato — array E scalare/variante (es. `DIM voci` riempita da
                // STRINGS.SPLIT): `voci(0)` è l'elemento 0 della variabile, non una chiamata
                // alla procedura omonima (bug scoperto scrivendo l'app DigUp). Resta valido
                // per i soli builtin (Native/ExternalProcedureDefinition): una FUNCTION/SUB
                // utente omonima conserva la priorità attuale.
                if (targetProc is NativeProcedureDefinition or ExternalProcedureDefinition)
                {
                    var lookupScope = aae.ScopeId >= 0 ? aae.ScopeId : currentScopeId;
                    var shadowId = FindSymbolIdInScopeChain(aae.NameCleaned.Lexeme, lookupScope, aae.Position, program);
                    if (shadowId >= 0
                        && program.Symbols.GetSymbol(shadowId).DeclarationNode is VariableDeclaration)
                        targetProc = null;
                }

                var boundIndices = aae.Indices.Select((arg, i) =>
                {
                    var param = targetProc?.Parameters.ElementAtOrDefault(i);
                    if (param != null && param.Flags.HasFlag(SymbolFlags.IsByRef) &&
                        arg is VariableExpr { SymbolId: < 0 } ve)
                    {
                        var existingIds = program.Symbols.FindSymbolInScope(ve.NameCleaned.Lexeme);
                        if (existingIds == null || !existingIds.Any())
                        {
                            var symbolId = program.Symbols.RegisterSymbol(
                                ve.NameCleaned, SymbolFlags.IsLocal,
                                param.DataType is not InferredType ? param.DataType : new InferredType(), ve);
                            Log(
                                $"[BINDER-BYREF] Variabile implicita (BYREF in AAE) '{ve.NameCleaned.Lexeme}' registrata con ID {symbolId}.",
                                BindingLog);

                            var boundVar = ve with { SymbolId = symbolId };
                            program.Symbols.UpdateSymbolDeclaration(symbolId, boundVar);
                            return (Expr)boundVar with { ScopeId = currentScopeId };
                        }
                    }

                    return BindExpression(arg, program, isStrict);
                }).ToImmutableArray();

                var resolvedSymbolId = targetProc?.SymbolId ?? aae.SymbolId;
                // Variabile ARRAY/collezione con simbolo NON risolto in parsing (SymbolId<0,
                // es. `DIM fonts(2)` untyped letto dentro un argomento di statement come
                // `DRAW TEXT STR(fonts(0))`): risolvi per NOME nella scope chain. Senza,
                // l'emitter cade nel path dinamico con default 0 → IL invalido
                // (InvalidProgramException; 3 sample grafici, 2026-10-03).
                if (targetProc == null && resolvedSymbolId < 0)
                {
                    var lookupScope = aae.ScopeId >= 0 ? aae.ScopeId : currentScopeId;
                    var varSymId = FindSymbolIdInScopeChain(aae.NameCleaned.Lexeme, lookupScope, aae.Position, program);
                    if (varSymId >= 0) resolvedSymbolId = varSymId;
                }
                rewrittenExpr = new ArrayAccessExpr(resolvedSymbolId, aae.NameSuffixed, aae.NameCleaned, boundIndices, aae.IsSafe)
                { ScopeId = currentScopeId };
                break;
            }
            // --- FINE LOGICA PRE-ESISTENTE ---

            // --- INIZIO LOGICA DI DISCESA RICORSIVA (RIPRISTINATA E COMPLETATA) ---
            case BinaryExpr be:
                var left = BindExpression(be.Left, program, isStrict);
                var right = BindExpression(be.Right, program, isStrict);
                rewrittenExpr = new BinaryExpr(left, be.Op, right);
                break;
            case UnaryExpr ue:
                rewrittenExpr = new UnaryExpr(ue.Op, BindExpression(ue.Right, program, isStrict));
                break;
            case GroupingExpr ge:
                rewrittenExpr = new GroupingExpr(BindExpression(ge.Expression, program, isStrict));
                break;
            case MethodCallExpr mce:
                rewrittenExpr = new MethodCallExpr(
                    BindExpression(mce.Instance, program, isStrict),
                    mce.MethodName,
                    [..mce.Args.Select(a => BindExpression(a, program, isStrict))]
                ) { ResolvedDefinition = mce.ResolvedDefinition };
                break;
            case FieldAccessExpr fae:
                rewrittenExpr = new FieldAccessExpr(
                    BindExpression(fae.Structure, program, isStrict),
                    fae.Field
                )
                {
                    FieldSymbolId = fae.FieldSymbolId,
                    ResolvedType = fae.ResolvedType
                };
                break;
            case PropertyAccessExpr pae:
                rewrittenExpr =
                    new PropertyAccessExpr(BindExpression(pae.Instance, program, isStrict), pae.PropertyName)
                        { ResolvedDefinition = pae.ResolvedDefinition };
                break;
            case IndexedAccessExpr iae:
                rewrittenExpr = new IndexedAccessExpr(
                    BindExpression(iae.Base, program, isStrict),
                    [..iae.Indices.Select(i => BindExpression(i, program, isStrict))],
                    iae.IsSafe
                );
                break;
            case NewExpr ne:
                rewrittenExpr = new NewExpr(
                    ne.ParsedType,
                    // ReSharper disable once MergeConditionalExpression
                    ne.ConstructorArgs.HasValue
                        ? ne.ConstructorArgs.Value.Select(a => BindExpression(a, program, isStrict)).ToImmutableArray()
                        : null,
                    // ReSharper disable once MergeConditionalExpression
                    ne.InitialValues.HasValue
                        ? ne.InitialValues.Value.Select(v => BindExpression(v, program, isStrict)).ToImmutableArray()
                        : null,
                    ne.NewToken
                );
                break;
            case ListLiteralExpr lle:
                rewrittenExpr = new ListLiteralExpr(
                    [..lle.Elements.Select(e => BindExpression(e, program, isStrict))],
                    lle.OpenBracket
                );
                break;
            case ArrayLiteralExpr ale:
                rewrittenExpr = new ArrayLiteralExpr(
                    [..ale.Elements.Select(e => BindExpression(e, program, isStrict))],
                    ale.OpenBrace
                );
                break;
            case TupleLiteralExpr tuple:
                rewrittenExpr = new TupleLiteralExpr(
                    [..tuple.Elements.Select(e => new NamedTupleElement(BindExpression(e.Value, program, isStrict), e.Name))],
                    tuple.OpenParen,
                    tuple.CloseParen
                );
                break;
            case DictionaryLiteralExpr dle:
                rewrittenExpr = new DictionaryLiteralExpr(
                    [
                        ..dle.Entries.Select(e => new DictionaryEntry(
                            BindExpression(e.Key, program, isStrict),
                            BindExpression(e.Value, program, isStrict)
                        ))
                    ],
                    dle.OpenBrace
                );
                break;
            case BuiltinFunctionCallExpr bce:
                rewrittenExpr = new BuiltinFunctionCallExpr(
                    bce.Namespace, bce.Name,
                    [..bce.Args.Select(a => BindExpression(a, program, isStrict))],
                    bce.ReturnType
                ) { OriginalToken = bce.OriginalToken };
                break;
            case GoFunctionExpr gfe:
                rewrittenExpr = new GoFunctionExpr(
                    gfe.SymbolId, gfe.OriginalToken,
                    [..gfe.Args.Select(a => BindExpression(a, program, isStrict))],
                    gfe.WaitGroup != null ? BindExpression(gfe.WaitGroup, program, isStrict) : null,
                    gfe.ReturnType
                );
                break;
            case LambdaExpr le:
            {
                program.Symbols.EnterScope($"Lambda_{le.FuncToken.Line}_{le.FuncToken.Column}",
                    SymbolTable.ScopeType.Procedure);
                try
                {
                    var lambdaScopeId = program.Symbols.GetCurrentScope().Id;

                    var boundParameters = le.Parameters.Select(p =>
                    {
                        var paramFlags = p.Flags | SymbolFlags.IsParameter;
                        var paramId = program.Symbols.RegisterSymbol(p.OriginalName, paramFlags, p.DataType, p);
                        var finalParam = p with { SymbolId = paramId, ScopeId = lambdaScopeId };
                        program.Symbols.UpdateSymbolDeclaration(paramId, finalParam);
                        return finalParam;
                    }).ToImmutableArray();

                    var boundExprBody = le.ExpressionBody != null
                        ? BindExpression(le.ExpressionBody, program, isStrict)
                        : null;
                    var boundBlockBody = le.BlockBody.HasValue
                        ? BindListOfStatements(le.BlockBody.Value, program, isStrict)
                        : (ImmutableArray<Stmt>?)null;

                    rewrittenExpr = new LambdaExpr(boundParameters, le.ReturnType, boundBlockBody, boundExprBody,
                        le.FuncToken);
                }
                finally
                {
                    program.Symbols.ExitScope();
                }

                break;
            }
            case DirectCastExpr dce:
                rewrittenExpr = new DirectCastExpr(
                    BindExpression(dce.Expression, program, isStrict),
                    dce.TargetType,
                    dce.CastToken
                );
                break;
            case SafeCastExpr sce:
                rewrittenExpr = new SafeCastExpr(
                    BindExpression(sce.Expression, program, isStrict),
                    sce.TargetType,
                    sce.AsToken
                );
                break;
            case NullableHasValueExpr nhv:
                rewrittenExpr = new NullableHasValueExpr(
                    BindExpression(nhv.Expression, program, isStrict),
                    nhv.HasToken
                );
                break;
            case NullableValueExpr nv:
                rewrittenExpr = new NullableValueExpr(
                    BindExpression(nv.Expression, program, isStrict),
                    nv.ValueToken
                );
                break;
            case ListComprehensionExpr lce:
                return BindListComprehension(lce, program, isStrict);
            case DictComprehensionExpr dce:
                return BindDictComprehension(dce, program, isStrict);
            case SetComprehensionExpr sce:
                return BindSetComprehension(sce, program, isStrict);
            case GeneratorExpr ge:
                return BindGeneratorExpr(ge, program, isStrict);
            case AssignmentExpr ae:
            {
                var boundTarget = BindTarget(ae.Target, program, isStrict);
                var boundValue = BindExpression(ae.Value, program, isStrict);
                rewrittenExpr = new AssignmentExpr(boundTarget, boundValue, ae.Operator);
                break;
            }
            case MacroExpr me:
                var boundInnerExpression = BindExpression(me.Expression, program, isStrict);
                rewrittenExpr = new MacroExpr(boundInnerExpression, me.Token);
                break;
            case EvalExpr ee:
            {
                var boundEvalTarget = BindExpression(ee.Target, program, isStrict);
                var boundEvalArgs = ee.Args.Select(a => BindExpression(a, program, isStrict)).ToImmutableArray();
                rewrittenExpr = new EvalExpr(boundEvalTarget, boundEvalArgs, ee.Token);
                break;
            }
            case ForeignAmountExpr fx:
                rewrittenExpr = new ForeignAmountExpr(
                    BindExpression(fx.Amount, program, isStrict),
                    fx.CurrencyToken,
                    fx.ExchangeRate != null ? BindExpression(fx.ExchangeRate, program, isStrict) : null);
                break;
            case NullConditionalAccessExpr nca:
                var boundNcaExpr = BindExpression(nca.Expression, program, isStrict);
                var boundNcaArgs = nca.Arguments?
                    .Select(a => BindExpression(a, program, isStrict))
                    .ToImmutableArray();
                rewrittenExpr = new NullConditionalAccessExpr(boundNcaExpr, nca.Kind, nca.MemberToken, boundNcaArgs);
                break;
            case InterpolatedStringExpr ise:
                var boundSegments = ise.Segments.Select(seg => seg switch
                {
                    InterpolatedStringExprSegment es => (InterpolatedStringSegment)(es with
                    {
                        Expression = BindExpression(es.Expression, program, isStrict)
                    }),
                    _ => seg
                }).ToImmutableArray();
                rewrittenExpr = ise with { Segments = boundSegments };
                break;
            default:
                rewrittenExpr = expr;
                break;
        }

        return rewrittenExpr with { ScopeId = currentScopeId };
    }

    /// <summary>
    /// Attraversa ricorsivamente un Target e assegna lo ScopeId corrente a ogni nodo.
    /// </summary>
    private static Target BindTarget(Target target, BasicProgram program, bool isStrict)
    {
        var currentScopeId = program.Symbols.GetCurrentScope().Id;
        var scopedTarget = target with { ScopeId = currentScopeId };

        switch (scopedTarget)
        {
            case ArrayElementTarget aet:
                return aet with
                {
                    Indices = [..aet.Indices.Select(i => BindExpression(i, program, isStrict))]
                };

            case FieldTarget ft when ft.Structure is null:
                return ft; // WITH block implicit member — resolved at runtime via WithTarget
            case FieldTarget ft:
                return ft with { Structure = BindTarget(ft.Structure!, program, isStrict) };

            case IndexedTarget it:
                return it with
                {
                    Base = BindExpression(it.Base, program, isStrict),
                    Indices = [..it.Indices.Select(i => BindExpression(i, program, isStrict))]
                };
            case MacroTarget mt:
                Log(
                    $"[BINDER-TRACE] Binding MacroTarget in scope {currentScopeId}. Descending into its expression: {mt.Expression}",
                    BindingLog);
                return mt with { Expression = (MacroExpr)BindExpression(mt.Expression, program, isStrict) };
            default:
                return scopedTarget;
        }
    }
    
    /// <summary>
    /// Helper per il binding delle Comprensioni di Lista Annidate.
    /// Gestisce l'ingresso sequenziale negli scope per ogni clausola FOR.
    /// </summary>
    private static ListComprehensionExpr BindListComprehension(ListComprehensionExpr lce, BasicProgram program, bool isStrict)
    {
        var outerScopeId = program.Symbols.GetCurrentScope().Id;
        var scopesEntered = 0;
        var boundClauses = ImmutableArray.CreateBuilder<ComprehensionClause>();

        try
        {
            foreach (var clause in lce.Clauses)
            {
                // 1. Lega la sorgente nello scope corrente (prima di definire l'iteratore di questa clausola)
                var boundSource = BindExpression(clause.SourceExpression, program, isStrict);

                // 2. Crea un nuovo scope per la variabile iteratore
                program.Symbols.EnterScope(
                    $"CompClause_{clause.IteratorVariable.Line}_{clause.IteratorVariable.Column}",
                    SymbolTable.ScopeType.Block);
                scopesEntered++;

                // 3. Registra l'iteratore
                var iteratorSymbolId = program.Symbols.RegisterSymbol(
                    clause.IteratorVariable,
                    SymbolFlags.IsLocal,
                    new InferredType(),
                    clause.IteratorVariable
                );

                // 4. Lega la condizione nello scope dell'iteratore
                var boundCondition = clause.ConditionExpression != null
                    ? BindExpression(clause.ConditionExpression, program, isStrict)
                    : null;
                // Il binder NON ha un case VariableExpr: i riferimenti `n` nel cond restano
                // con il SymbolId/ScopeId del parser (esterno alla comprehension). Se non
                // vengono rilegati, il resolver non li trova (lo scope dell'iteratore non è
                // nel loro ScopeId) e il backend IL crea un local SEPARATO dall'iteratore del
                // loop → il cond/corpo leggono sempre 0. Rilega i riferimenti al nome
                // dell'iteratore con il SymbolId registrato in questo scope.
                if (boundCondition is not null)
                    boundCondition = ReplaceComprehensionIteratorRefs(boundCondition, clause.IteratorVariable.Lexeme, iteratorSymbolId);

                boundClauses.Add(new ComprehensionClause(
                    iteratorSymbolId,
                    clause.IteratorVariable,
                    boundSource,
                    boundCondition
                ));
            }

            // 5. Lega l'output nello scope più interno (vede tutti gli iteratori)
            var boundOutput = BindExpression(lce.OutputExpression, program, isStrict);
            // Rilega nell'output i riferimenti a TUTTI gli iteratori delle clausole (lo
            // stesso problema del cond: senza il SymbolId registrato, l'output legge un
            // local separato e la comprehension produce sempre l'ultimo/zero elemento).
            foreach (var (iterName, iterId) in boundClauses.Select(c => (c.IteratorVariable.Lexeme, c.IteratorSymbolId)))
                boundOutput = ReplaceComprehensionIteratorRefs(boundOutput, iterName, iterId);

            return new ListComprehensionExpr(
                boundOutput,
                boundClauses.ToImmutable(),
                lce.OpenBracket,
                lce.OrderByClauses
            ) { ScopeId = outerScopeId };
        }
        finally
        {
            // 6. Esce da tutti gli scope creati per ripristinare lo stato
            for (var i = 0; i < scopesEntered; i++)
            {
                program.Symbols.ExitScope();
            }
        }
    }

    /// <summary>
    /// Sostituisce ricorsivamente i riferimenti (VariableExpr) con il nome dato con il
    /// SymbolId dell'iteratore della comprehension. Senza questa rilegatura i riferimenti
    /// dell'iteratore restano con un SymbolId irrisolto (il binder non ha un case
    /// VariableExpr) e il backend IL li dichiara come local SEPARATO dal loop della
    /// comprehension → cond/corpo leggono sempre il default (0) e la comprehension
    /// produce elementi errati o vuoti. Vedere EmitComprehensionClauses.
    /// </summary>
    private static Expr ReplaceComprehensionIteratorRefs(Expr expr, string name, int iteratorSymbolId)
    {
        switch (expr)
        {
            case VariableExpr ve when ve.NameCleaned.Lexeme.Equals(name, StringComparison.OrdinalIgnoreCase):
                return ve with { SymbolId = iteratorSymbolId };
            case VariableExpr:
                return expr;
            case BinaryExpr be:
                return new BinaryExpr(
                    ReplaceComprehensionIteratorRefs(be.Left, name, iteratorSymbolId),
                    be.Op,
                    ReplaceComprehensionIteratorRefs(be.Right, name, iteratorSymbolId));
            case UnaryExpr ue:
                return new UnaryExpr(ue.Op, ReplaceComprehensionIteratorRefs(ue.Right, name, iteratorSymbolId));
            case GroupingExpr ge:
                return new GroupingExpr(ReplaceComprehensionIteratorRefs(ge.Expression, name, iteratorSymbolId));
            case ArrayAccessExpr aae:
                return new ArrayAccessExpr(aae.SymbolId, aae.NameSuffixed, aae.NameCleaned,
                    [..aae.Indices.Select(i => ReplaceComprehensionIteratorRefs(i, name, iteratorSymbolId))],
                    aae.IsSafe) { ScopeId = aae.ScopeId };
            case MethodCallExpr mce:
                return new MethodCallExpr(
                    ReplaceComprehensionIteratorRefs(mce.Instance, name, iteratorSymbolId),
                    mce.MethodName,
                    [..mce.Args.Select(a => ReplaceComprehensionIteratorRefs(a, name, iteratorSymbolId))])
                { ResolvedDefinition = mce.ResolvedDefinition };
            case IndexedAccessExpr iae:
                return new IndexedAccessExpr(
                    ReplaceComprehensionIteratorRefs(iae.Base, name, iteratorSymbolId),
                    [..iae.Indices.Select(i => ReplaceComprehensionIteratorRefs(i, name, iteratorSymbolId))],
                    iae.IsSafe);
            case BuiltinFunctionCallExpr bce:
                return new BuiltinFunctionCallExpr(
                    bce.Namespace, bce.Name,
                    [..bce.Args.Select(a => ReplaceComprehensionIteratorRefs(a, name, iteratorSymbolId))],
                    bce.ReturnType);
            default:
                return expr;
        }
    }

    private static GeneratorExpr BindGeneratorExpr(GeneratorExpr ge, BasicProgram program, bool isStrict)
    {
        var outerScopeId = program.Symbols.GetCurrentScope().Id;
        var scopesEntered = 0;
        var boundClauses = ImmutableArray.CreateBuilder<ComprehensionClause>();

        try
        {
            foreach (var clause in ge.Clauses)
            {
                var boundSource = BindExpression(clause.SourceExpression, program, isStrict);
                program.Symbols.EnterScope(
                    $"GenClause_{clause.IteratorVariable.Line}_{clause.IteratorVariable.Column}",
                    SymbolTable.ScopeType.Block);
                scopesEntered++;

                var iteratorSymbolId = program.Symbols.RegisterSymbol(
                    clause.IteratorVariable,
                    SymbolFlags.IsLocal,
                    new InferredType(),
                    clause.IteratorVariable
                );

                var boundCondition = clause.ConditionExpression != null
                    ? BindExpression(clause.ConditionExpression, program, isStrict)
                    : null;

                boundClauses.Add(new ComprehensionClause(
                    iteratorSymbolId,
                    clause.IteratorVariable,
                    boundSource,
                    boundCondition
                ));
            }

            var boundOutput = BindExpression(ge.OutputExpression, program, isStrict);

            return new GeneratorExpr(
                boundOutput,
                boundClauses.ToImmutable(),
                ge.OpenParen
            ) { ScopeId = outerScopeId };
        }
        finally
        {
            for (var i = 0; i < scopesEntered; i++) program.Symbols.ExitScope();
        }
    }

    /// <summary>
    /// Helper per il binding delle Comprensioni di Dizionario.
    /// </summary>
    private static DictComprehensionExpr BindDictComprehension(DictComprehensionExpr dce, BasicProgram program, bool isStrict)
    {
        var outerScopeId = program.Symbols.GetCurrentScope().Id;
        var scopesEntered = 0;
        var boundClauses = ImmutableArray.CreateBuilder<ComprehensionClause>();

        try
        {
            foreach (var clause in dce.Clauses)
            {
                var boundSource = BindExpression(clause.SourceExpression, program, isStrict);
                program.Symbols.EnterScope(
                    $"CompClause_{clause.IteratorVariable.Line}_{clause.IteratorVariable.Column}",
                    SymbolTable.ScopeType.Block);
                scopesEntered++;

                var iteratorSymbolId = program.Symbols.RegisterSymbol(
                    clause.IteratorVariable,
                    SymbolFlags.IsLocal,
                    new InferredType(),
                    clause.IteratorVariable
                );

                var boundCondition = clause.ConditionExpression != null
                    ? BindExpression(clause.ConditionExpression, program, isStrict)
                    : null;

                boundClauses.Add(new ComprehensionClause(
                    iteratorSymbolId,
                    clause.IteratorVariable,
                    boundSource,
                    boundCondition
                ));
            }

            var boundKey = BindExpression(dce.KeyExpression, program, isStrict);
            var boundValue = BindExpression(dce.ValueExpression, program, isStrict);

            return new DictComprehensionExpr(
                boundKey,
                boundValue,
                boundClauses.ToImmutable(),
                dce.OpenBrace
            ) { ScopeId = outerScopeId };
        }
        finally
        {
            for (var i = 0; i < scopesEntered; i++) program.Symbols.ExitScope();
        }
    }

    /// <summary>
    /// Helper per il binding delle Comprensioni di Set.
    /// </summary>
    private static SetComprehensionExpr BindSetComprehension(SetComprehensionExpr sce, BasicProgram program, bool isStrict)
    {
        var outerScopeId = program.Symbols.GetCurrentScope().Id;
        var scopesEntered = 0;
        var boundClauses = ImmutableArray.CreateBuilder<ComprehensionClause>();

        try
        {
            foreach (var clause in sce.Clauses)
            {
                var boundSource = BindExpression(clause.SourceExpression, program, isStrict);
                program.Symbols.EnterScope(
                    $"CompClause_{clause.IteratorVariable.Line}_{clause.IteratorVariable.Column}",
                    SymbolTable.ScopeType.Block);
                scopesEntered++;

                var iteratorSymbolId = program.Symbols.RegisterSymbol(
                    clause.IteratorVariable,
                    SymbolFlags.IsLocal,
                    new InferredType(),
                    clause.IteratorVariable
                );

                var boundCondition = clause.ConditionExpression != null
                    ? BindExpression(clause.ConditionExpression, program, isStrict)
                    : null;

                boundClauses.Add(new ComprehensionClause(
                    iteratorSymbolId,
                    clause.IteratorVariable,
                    boundSource,
                    boundCondition
                ));
            }

            var boundElement = BindExpression(sce.ElementExpression, program, isStrict);

            return new SetComprehensionExpr(
                boundElement,
                boundClauses.ToImmutable(),
                sce.OpenBrace
            ) { ScopeId = outerScopeId };
        }
        finally
        {
            for (var i = 0; i < scopesEntered; i++) program.Symbols.ExitScope();
        }
    }
    
}
