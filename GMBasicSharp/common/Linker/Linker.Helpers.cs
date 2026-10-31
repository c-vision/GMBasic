// FILE: Interpreter.Linker.Helpers.cs

using System.Collections.Immutable;

namespace Basic;

/// <summary>
/// Contiene i metodi del Linker per l'aggregazione e la gestione dei moduli e dei loro simboli.
/// </summary>
public static partial class Linker
{
    /*/// <summary>
    /// Confronta due DataType per equivalenza semantica, ignorando la posizione nel codice sorgente.
    /// </summary>
    private static bool AreSameType(DataType? a, DataType? b)
    {
        if (ReferenceEquals(a, b)) return true;
        if (a is null || b is null) return false;
        if (a.GetType() != b.GetType()) return false;

        return a switch
        {
            ArrayType at => AreSameType(at.ElementType, ((ArrayType)b).ElementType),
            ListType lt => AreSameType(lt.ElementType, ((ListType)b).ElementType),
            DictType dt => AreSameType(dt.KeyType, ((DictType)b).KeyType) &&
                           AreSameType(dt.ValueType, ((DictType)b).ValueType),
            // ... aggiungere altri tipi generici se necessario ...

            // Per ClassType e UserDefinedType, confrontiamo prima il SymbolId per efficienza.
            // Se fallisce, confrontiamo i nomi come fallback, gestendo il caso in cui un AST
            // non sia stato ancora completamente arricchito con gli ID.
            ClassType ct when b is ClassType cb =>
                (ct.Definition.SymbolId >= 0 && ct.Definition.SymbolId == cb.Definition.SymbolId) ||
                ct.Definition.OriginalName.Lexeme.Equals(cb.Definition.OriginalName.Lexeme,
                    StringComparison.OrdinalIgnoreCase),

            UserDefinedType udt when b is UserDefinedType udtB =>
                (udt.SymbolId >= 0 && udt.SymbolId == udtB.SymbolId) ||
                udt.OriginalName.Lexeme.Equals(udtB.OriginalName.Lexeme, StringComparison.OrdinalIgnoreCase),

            // Per tutti i tipi primitivi (Double, String, etc.) che non hanno stato extra oltre alla posizione,
            // il controllo sul GetType() fatto all'inizio è sufficiente.
            _ => true
        };
    }*/
    
    /// <summary>
    /// Confronta due DataType per equivalenza semantica, ignorando la posizione nel codice sorgente.
    /// VERSIONE CORRETTA: Risolve i tipi `UnresolvedQualifiedType` prima del confronto
    /// per gestire correttamente l'equivalenza tra diverse rappresentazioni AST dello stesso tipo.
    /// </summary>
    private static bool AreSameType(DataType? a, DataType? b, AstRewriteContext context, BasicProgram program)
    {
        // Fase 1: Risoluzione dei tipi. Se a o b sono tipi non risolti, tentiamo di risolverli.
        // Durante il rewrite di una specializzazione generica (CLASS<T> con type args noti nel
        // contesto), un `T` residuo nei corpi viene risolto al tipo concreto.
        var resolvedA = ResolveMaybeGenericType(a, context, program);
        var resolvedB = ResolveMaybeGenericType(b, context, program);
        return AreSameTypeResolved(resolvedA, resolvedB, context, program);
    }

    private static DataType? ResolveMaybeGenericType(DataType? t, AstRewriteContext context, BasicProgram program)
    {
        if (t is UnresolvedQualifiedType uq && context.GenericTypeArgs != null &&
            uq.TypeArguments.IsDefaultOrEmpty)
        {
            var nm = uq.QualifiedName.FullName;
            if (nm.Length > 0 && context.GenericTypeArgs.TryGetValue(nm, out var concrete))
                return concrete;
        }
        return t is UnresolvedQualifiedType uqt ? ResolveDataType(uqt, program, context.ModulePath) : t;
    }

    private static bool AreSameTypeResolved(DataType? resolvedA, DataType? resolvedB, AstRewriteContext context, BasicProgram program)
    {

        //Log($"[ARE-SAME-TYPE] Confronto: '{resolvedA?.ToString() ?? "null"}' vs '{resolvedB?.ToString() ?? "null"}'", false);
        
        if (ReferenceEquals(resolvedA, resolvedB)) return true;
        if (resolvedA is null || resolvedB is null) return false;
        
        // Dopo la risoluzione, i tipi concreti dovrebbero corrispondere
        if (resolvedA.GetType() != resolvedB.GetType())
        {
            //Log($"[ARE-SAME-TYPE] -> false (tipi .NET diversi dopo risoluzione: {resolvedA.GetType().Name} vs {resolvedB.GetType().Name})", true);
            return false;
        }

        var result = resolvedA switch
        {
            // --- COLLEZIONI A SINGOLO ELEMENTO ---
            ArrayType at => AreSameType(at.ElementType, ((ArrayType)resolvedB).ElementType, context, program),
            ListType lt => AreSameType(lt.ElementType, ((ListType)resolvedB).ElementType, context, program),
            HashSetType hst => AreSameType(hst.ElementType, ((HashSetType)resolvedB).ElementType, context, program),
            QueueType qt => AreSameType(qt.ElementType, ((QueueType)resolvedB).ElementType, context, program),
            StackType st => AreSameType(st.ElementType, ((StackType)resolvedB).ElementType, context, program),
            LinkedListType llt => AreSameType(llt.ElementType, ((LinkedListType)resolvedB).ElementType, context,
                program),
            DoublyLinkedListType dllt => AreSameType(dllt.ElementType, ((DoublyLinkedListType)resolvedB).ElementType,
                context, program),
            CircularLinkedListType cllt => AreSameType(cllt.ElementType,
                ((CircularLinkedListType)resolvedB).ElementType, context, program),
            BinaryHeapType bht => AreSameType(bht.ElementType, ((BinaryHeapType)resolvedB).ElementType, context,
                program) && bht.IsMaxHeap == ((BinaryHeapType)resolvedB).IsMaxHeap,
            BagType bt => AreSameType(bt.ElementType, ((BagType)resolvedB).ElementType, context, program),
            GraphType gt => AreSameType(gt.NodeType, ((GraphType)resolvedB).NodeType, context, program),
            TimelineType tlt => AreSameType(tlt.ValueType, ((TimelineType)resolvedB).ValueType, context, program),
            HashRingType hrt => AreSameType(hrt.NodeType, ((HashRingType)resolvedB).NodeType, context, program),
            RingBufferType rbt => AreSameType(rbt.ElementType, ((RingBufferType)resolvedB).ElementType, context,
                program),
            BinaryIndexedTreeType bitt => AreSameType(bitt.ElementType, ((BinaryIndexedTreeType)resolvedB).ElementType,
                context, program),
            ChannelType chant => AreSameType(chant.ValueType, ((ChannelType)resolvedB).ValueType, context, program),
            GeneratorType gent => AreSameType(gent.YieldType, ((GeneratorType)resolvedB).YieldType, context, program),

            // --- COLLEZIONI CHIAVE-VALORE ---
            DictType dt => AreSameType(dt.KeyType, ((DictType)resolvedB).KeyType, context, program) &&
                           AreSameType(dt.ValueType, ((DictType)resolvedB).ValueType, context, program),
            SortedDictType sdt => AreSameType(sdt.KeyType, ((SortedDictType)resolvedB).KeyType, context, program) &&
                                  AreSameType(sdt.ValueType, ((SortedDictType)resolvedB).ValueType, context, program),
            TreeType tt => AreSameType(tt.KeyType, ((TreeType)resolvedB).KeyType, context, program) &&
                           AreSameType(tt.ValueType, ((TreeType)resolvedB).ValueType, context, program),
            AVLTreeType avlt => AreSameType(avlt.KeyType, ((AVLTreeType)resolvedB).KeyType, context, program) &&
                                AreSameType(avlt.ValueType, ((AVLTreeType)resolvedB).ValueType, context, program),
            RedBlackTreeType rbtt =>
                AreSameType(rbtt.KeyType, ((RedBlackTreeType)resolvedB).KeyType, context, program) &&
                AreSameType(rbtt.ValueType, ((RedBlackTreeType)resolvedB).ValueType, context, program),
            CacheType ct => AreSameType(ct.KeyType, ((CacheType)resolvedB).KeyType, context, program) &&
                            AreSameType(ct.ValueType, ((CacheType)resolvedB).ValueType, context, program),
            BPlusTreeType bpt => AreSameType(bpt.KeyType, ((BPlusTreeType)resolvedB).KeyType, context, program) &&
                                 AreSameType(bpt.ValueType, ((BPlusTreeType)resolvedB).ValueType, context, program),
            KeyValuePairType kvpt =>
                AreSameType(kvpt.KeyType, ((KeyValuePairType)resolvedB).KeyType, context, program) &&
                AreSameType(kvpt.ValueType, ((KeyValuePairType)resolvedB).ValueType, context, program),
            
            ClassType ct when resolvedB is ClassType cb =>
                ct.Definition.SymbolId == cb.Definition.SymbolId,

            UserDefinedType udt when resolvedB is UserDefinedType udtB =>
                udt.SymbolId == udtB.SymbolId,
            
            InterfaceType it when resolvedB is InterfaceType ib =>
                it.Definition.SymbolId == ib.Definition.SymbolId,
                
            EnumType et when resolvedB is EnumType eb =>
                et.Name.Lexeme.Equals(eb.Name.Lexeme, StringComparison.OrdinalIgnoreCase),

            _ => true
        };
        
        //Log($"[ARE-SAME-TYPE] -> Risultato finale: {result}", false);
        return result;
    }

    /*/// <summary>
    /// Confronta due DataType per equivalenza semantica, ignorando la posizione nel codice sorgente.
    /// VERSIONE CORRETTA E COMPLETA: Gestisce il confronto tra tipi diversi (es. ClassType vs UnresolvedQualifiedType)
    /// che rappresentano lo stesso simbolo, basandosi sui loro nomi, e include tutti i tipi generici.
    /// </summary>
    private static bool AreSameType(DataType? a, DataType? b)
    {
        if (ReferenceEquals(a, b)) return true;
        if (a is null || b is null) return false;

        // --- NUOVA LOGICA: CONFRONTO SEMANTICO BASATO SUL NOME ---
        // Prima di confrontare i tipi .NET, estraiamo un nome canonico se si tratta di tipi definiti dall'utente.
        var nameA = GetComplexTypeName(a);
        var nameB = GetComplexTypeName(b);

        // Se entrambi sono tipi complessi con un nome, il loro nome DEVE corrispondere.
        if (nameA != null && nameB != null)
        {
            return nameA.Equals(nameB, StringComparison.OrdinalIgnoreCase);
        }

        // Se solo uno è un tipo complesso (es. ClassType vs StringType), non sono uguali.
        if (nameA != null || nameB != null)
        {
            return false;
        }
        
        // --- LOGICA ORIGINALE (per tipi primitivi e generici) ---
        // Se non sono tipi complessi con nome, allora i loro tipi .NET devono corrispondere.
        if (a.GetType() != b.GetType()) return false;

        // A questo punto, sappiamo che i tipi sono identici (es. entrambi ListType o entrambi DoubleType).
        // Procediamo con il confronto strutturale ricorsivo per i tipi generici.
        return a switch
        {
            // --- COLLEZIONI A SINGOLO ELEMENTO ---
            ArrayType at => AreSameType(at.ElementType, ((ArrayType)b).ElementType),
            ListType lt => AreSameType(lt.ElementType, ((ListType)b).ElementType),
            HashSetType hst => AreSameType(hst.ElementType, ((HashSetType)b).ElementType),
            QueueType qt => AreSameType(qt.ElementType, ((QueueType)b).ElementType),
            StackType st => AreSameType(st.ElementType, ((StackType)b).ElementType),
            LinkedListType llt => AreSameType(llt.ElementType, ((LinkedListType)b).ElementType),
            DoublyLinkedListType dllt => AreSameType(dllt.ElementType, ((DoublyLinkedListType)b).ElementType),
            CircularLinkedListType cllt => AreSameType(cllt.ElementType, ((CircularLinkedListType)b).ElementType),
            BinaryHeapType bht => AreSameType(bht.ElementType, ((BinaryHeapType)b).ElementType) && bht.IsMaxHeap == ((BinaryHeapType)b).IsMaxHeap,
            BagType bt => AreSameType(bt.ElementType, ((BagType)b).ElementType),
            GraphType gt => AreSameType(gt.NodeType, ((GraphType)b).NodeType),
            TimelineType tlt => AreSameType(tlt.ValueType, ((TimelineType)b).ValueType),
            HashRingType hrt => AreSameType(hrt.NodeType, ((HashRingType)b).NodeType),
            RingBufferType rbt => AreSameType(rbt.ElementType, ((RingBufferType)b).ElementType),
            BinaryIndexedTreeType bitt => AreSameType(bitt.ElementType, ((BinaryIndexedTreeType)b).ElementType),
            ChannelType chant => AreSameType(chant.ValueType, ((ChannelType)b).ValueType),
            GeneratorType gent => AreSameType(gent.YieldType, ((GeneratorType)b).YieldType),

            // --- COLLEZIONI CHIAVE-VALORE ---
            DictType dt => AreSameType(dt.KeyType, ((DictType)b).KeyType) && AreSameType(dt.ValueType, ((DictType)b).ValueType),
            SortedDictType sdt => AreSameType(sdt.KeyType, ((SortedDictType)b).KeyType) && AreSameType(sdt.ValueType, ((SortedDictType)b).ValueType),
            TreeType tt => AreSameType(tt.KeyType, ((TreeType)b).KeyType) && AreSameType(tt.ValueType, ((TreeType)b).ValueType),
            AVLTreeType avlt => AreSameType(avlt.KeyType, ((AVLTreeType)b).KeyType) && AreSameType(avlt.ValueType, ((AVLTreeType)b).ValueType),
            RedBlackTreeType rbtt => AreSameType(rbtt.KeyType, ((RedBlackTreeType)b).KeyType) && AreSameType(rbtt.ValueType, ((RedBlackTreeType)b).ValueType),
            CacheType ct => AreSameType(ct.KeyType, ((CacheType)b).KeyType) && AreSameType(ct.ValueType, ((CacheType)b).ValueType),
            BPlusTreeType bpt => AreSameType(bpt.KeyType, ((BPlusTreeType)b).KeyType) && AreSameType(bpt.ValueType, ((BPlusTreeType)b).ValueType),
            KeyValuePairType kvpt => AreSameType(kvpt.KeyType, ((KeyValuePairType)b).KeyType) && AreSameType(kvpt.ValueType, ((KeyValuePairType)b).ValueType),

            // --- ALTRI TIPI COMPOSTI ---
            FunctionPointerType fpt => AreSameType(fpt.ReturnType, ((FunctionPointerType)b).ReturnType) &&
                                      fpt.ParameterTypes.SequenceEqual(((FunctionPointerType)b).ParameterTypes, (dt1, dt2) => AreSameType(dt1, dt2)),

            // Per tutti i tipi primitivi (Double, String, etc.) e non generici (Trie, SparseSet),
            // il controllo su GetType() fatto all'inizio è sufficiente, quindi arriviamo qui.
            _ => true
        };
    }*/

    /// <summary>
    /// Helper per estrarre un nome canonico da un DataType complesso (Class, UDT, Enum, Interface, Unresolved).
    /// </summary>
    private static string? GetComplexTypeName(DataType dt)
    {
        return dt switch
        {
            ClassType ct => ct.Definition.OriginalName.Lexeme,
            UserDefinedType udt => udt.OriginalName.Lexeme,
            EnumType et => et.Name.Lexeme,
            InterfaceType it => it.Definition.OriginalName.Lexeme,
            UnresolvedQualifiedType uqt => uqt.QualifiedName.FullName,
            _ => null
        };
    }
    
    /// <summary>
    /// DEDUCE IL TIPO DI DATO DAL SUFFISSO DEL NOME DELLA VARIABILE.
    /// </summary>
    private static DataType DeduceTypeFromSuffix(string name)
    {
        if (name.EndsWith('$')) return new StringType();
        if (name.EndsWith('%')) return new IntegerType();
        if (name.EndsWith('&')) return new LongType();
        if (name.EndsWith('!')) return new SingleType();
        if (name.EndsWith('#')) return new DoubleType();
        if (name.EndsWith('@')) return new CurrencyType();
        return new DoubleType(); // Default
    }

    /// <summary>
    /// Estrae il tipo dell'elemento se il DataType fornito è una collezione (Array o List).
    /// Se il tipo è uno scalare, lo restituisce invariato.
    /// </summary>
    private static DataType GetInnerElementType(DataType type)
    {
        return type switch
        {
            ArrayType at => at.ElementType,
            ListType lt => lt.ElementType,
            _ => type
        };
    }

    /// <summary>
    /// Sincronizza il tipo e i flag di un simbolo nella SymbolTable.
    /// Assicura che il flag IsArray sia coerente con la natura del tipo (Array, List o FunctionPointer).
    /// </summary>
    private static void SyncSymbolMetadata(int symbolId, DataType finalType, bool isArrayIntent, BasicProgram program)
    {
        if (symbolId < 0) return;

        // 1. Aggiorna il tipo nella SymbolTable (Fonte di Verità)
        program.Symbols.UpdateSymbolType(symbolId, finalType);

        // 2. Determina se il simbolo deve essere considerato "indicizzabile/chiamabile" (Flag IsArray).
        // In MG-BASIC, questo flag abilita l'uso delle parentesi '()' nel Resolver.
        bool needsArrayFlag = isArrayIntent || 
                              finalType is ArrayType || 
                              finalType is ListType || 
                              finalType is FunctionPointerType;

        if (needsArrayFlag)
        {
            program.Symbols.UpdateSymbolFlags(symbolId, SymbolFlags.IsArray);
        }
        else
        {
            program.Symbols.RemoveSymbolFlags(symbolId, SymbolFlags.IsArray);
        }
    }
    
    /// <summary>
    /// Metodo helper che estrae tutte le procedure pubbliche da un elenco di procedure.
    /// Questa versione è stata aggiornata per operare su una collezione semplice,
    /// dato che `BasicProcedures` è ora una classe statica.
    /// </summary>
    /// <param name="procedures">Un elenco di definizioni di procedura.</param>
    /// <returns>Un dizionario di sola lettura contenente solo le procedure pubbliche, raggruppate per nome.</returns>
    public static IReadOnlyDictionary<string, ImmutableArray<ProcedureDefinition>> GetPublicProcedures(
        IEnumerable<ProcedureDefinition> procedures)
    {
        return procedures
            .Where(p => p.Flags.HasFlag(SymbolFlags.IsPublic))
            .GroupBy(p => p.Name.Lexeme, StringComparer.OrdinalIgnoreCase)
            .ToDictionary(g => g.Key, g => g.ToImmutableArray(), StringComparer.OrdinalIgnoreCase);
    }

    /// <summary>
    /// Metodo helper che cerca una singola procedura pubblica per nome in un BasicProcedures.
    /// Utilizzato per validare gli import selettivi.
    /// </summary>
    /// <summary>
    /// Metodo helper che cerca tutte le procedure pubbliche per nome in un BasicProcedures.
    /// Utilizzato per validare gli import selettivi e recuperare tutti gli overload.
    /// </summary>
    public static bool TryFindPublicProcedure(IEnumerable<ProcedureDefinition> allProcedures, string name,
        out ImmutableArray<ProcedureDefinition> procedures) // Modificata la firma qui
    {
        procedures =
        [
            ..allProcedures
                .Where(p => p.Name.Lexeme.Equals(name, StringComparison.OrdinalIgnoreCase) &&
                            p.Flags.HasFlag(SymbolFlags.IsPublic))
        ];

        return !procedures.IsEmpty;
    }

    /// <summary>
    /// Estrae tutti i tipi (UDT) pubblici da un dizionario di tipi.
    /// </summary>
    public static IReadOnlyDictionary<string, UserDefinedType> GetPublicTypes(
        IReadOnlyDictionary<string, UserDefinedType> allTypes)
    {
        return allTypes.Where(kvp => kvp.Value.Flags.HasFlag(SymbolFlags.IsPublic))
            .ToDictionary(kvp => kvp.Key, kvp => kvp.Value, StringComparer.OrdinalIgnoreCase);
    }

    /// <summary>
    /// Estrae tutte le classi pubbliche da un dizionario di classi.
    /// </summary>
    public static IReadOnlyDictionary<string, ClassDefinition> GetPublicClasses(
        IReadOnlyDictionary<string, ClassDefinition> allClasses)
    {
        return allClasses.Where(kvp => kvp.Value.Flags.HasFlag(SymbolFlags.IsPublic))
            .ToDictionary(kvp => kvp.Key, kvp => kvp.Value, StringComparer.OrdinalIgnoreCase);
    }

    /// <summary>
    /// Estrae tutte le costanti pubbliche da un dizionario di costanti.
    /// Poiché le costanti in un namespace sono implicitamente pubbliche, questo metodo
    /// semplicemente restituisce il dizionario originale.
    /// </summary>
    public static IReadOnlyDictionary<string, ConstDeclaration> GetPublicConstants(
        IReadOnlyDictionary<string, ConstDeclaration> allConstants)
    {
        // Attualmente, tutte le costanti a livello di namespace scoperte sono considerate pubbliche.
        // Se in futuro venissero introdotte costanti PRIVATE a livello di namespace,
        // qui andrebbe aggiunta la logica di filtraggio.
        return allConstants;
    }

    /// <summary>
    /// Metodo helper per trovare procedure candidate in base al nome e al numero di argomenti.
    /// Questa versione è stata aggiornata per utilizzare la nuova proprietà pubblica
    /// `Procedures` della classe `BasicProcedures`, eliminando completamente la reflection.
    /// </summary>
    public static List<ProcedureDefinition> FindProcedureCandidates(IEnumerable<ProcedureDefinition> allProcedures,
        string name,
        int argCount)
    {
        var candidates = new List<ProcedureDefinition>();

        var overloads = allProcedures
            .Where(p => p.Name.Lexeme.Equals(name, StringComparison.OrdinalIgnoreCase));

        candidates.AddRange(from candidate in overloads
            let requiredParams = candidate.Parameters.Count(p => !p.Flags.HasFlag(SymbolFlags.IsOptional))
            where argCount >= requiredParams && argCount <= candidate.Parameters.Length
            select candidate);

        return candidates;
    }

    /// <summary>
    /// Trova il nome del namespace a cui appartiene una data ClassDefinition.
    /// </summary>
    public static string? FindNamespaceForClass(ClassDefinition classDef, BasicProgram program)
    {
        // 1. Ottieni il SymbolId della classe.
        var symbolId = classDef.SymbolId;

        if (symbolId < 0) return null;

        // 2. Trova lo scope del simbolo e risali fino al Namespace (O(N_depth)).
        var symbol = program.Symbols.GetSymbol(symbolId);
        var nsScope = program.Symbols.FindParentScopeOfType(symbol.ScopeId, SymbolTable.ScopeType.Namespace);

        return nsScope?.Name ?? SymbolTable.GlobalScopeName;
    }

    /// <summary>
    /// Cerca ricorsivamente un metodo nella gerarchia di una classe che corrisponda
    /// al nome e sia compatibile con il numero di argomenti specificato.
    /// </summary>
    private static (MethodMember? Method, ClassDefinition? Owner) FindMethodInHierarchy(ClassDefinition startingClass,
        string methodName, int argCount)
    {
        var current = startingClass;
        while (current != null)
        {
            var candidates = current.Members.OfType<MethodMember>()
                .Where(m => m.Procedure.OriginalName.Lexeme.Equals(methodName, StringComparison.OrdinalIgnoreCase))
                .ToList();

            // Usa l'helper centralizzato per la compatibilità della firma
            var match = candidates.FirstOrDefault(m =>
                SymbolTable.IsProcedureSignatureCompatible(m.Procedure, argCount));

            if (match != null)
            {
                return (match, current);
            }

            current = current.ResolvedBaseClass;
        }

        return (null, null);
    }

    /// <summary>
    /// Cerca ricorsivamente un campo o una proprietà nella gerarchia di una classe.
    /// Restituisce il membro trovato e la classe in cui è stato definito.
    /// </summary>
    internal static (ClassMember? Member, ClassDefinition? Owner) FindFieldInHierarchy(ClassDefinition startingClass,
        string memberName)
    {
        var current = startingClass;
        while (current != null)
        {
            var member = current.Members.FirstOrDefault(m =>
                m.Name.Lexeme.Equals(memberName, StringComparison.OrdinalIgnoreCase));

            if (member != null)
            {
                return (member, current);
            }

            current = current.ResolvedBaseClass;
        }

        return (null, null);
    }

    /// <summary>
    /// Tenta di risolvere un nome di tipo in un DataType concreto (UDT, Classe, Enum o Interfaccia),
    /// interrogando direttamente la SymbolTable e applicando le regole di visibilità.
    /// QUESTA VERSIONE È CORRETTA e gestisce la visibilità a livello di modulo, la ricerca degli Enum,
    /// delle Interfacce e la risoluzione tramite namespace importati.
    /// </summary>
    /// <param name="name">Il nome non qualificato del tipo da risolvere.</param>
    /// <param name="program">L'istanza del programma che contiene la SymbolTable.</param>
    /// <param name="modulePath">Il percorso del modulo dal cui contesto parte la risoluzione.</param>
    /// <returns>Il <see cref="DataType"/> risolto se viene trovato un simbolo unico e visibile; altrimenti, <c>null</c>.</returns>
    private static DataType? TryResolveTypeName(string name, BasicProgram program, string modulePath)
    {
        const bool log = false; // Log forzato per debug
        Log($"[RESOLVE-TYPE] Inizio risoluzione tipo '{name}' dal modulo '{Path.GetFileName(modulePath)}'.", log);

        var candidates =
            new List<(DataType Type, string? FilePath, SymbolFlags Flags, string NamespaceName)>();
        var parts = name.Split('.');

        // Caso 1: Nome qualificato (es. "MioNamespace.MioTipo" o "Alias.MioTipo")
        if (parts.Length > 1)
        {
            var namespaceOrAlias = string.Join(".", parts.Take(parts.Length - 1));
            var typeName = parts.Last();
            var resolvedNamespace = namespaceOrAlias; // Di default, assume che non sia un alias

            if (program.LoadedModules.TryGetValue(modulePath, out var currentModule1))
            {
                var import = currentModule1.Imports.FirstOrDefault(i =>
                    i.Alias != null && i.Alias.Lexeme.Equals(namespaceOrAlias, StringComparison.OrdinalIgnoreCase));

                if (import != null)
                {
                    resolvedNamespace = import.NamespaceName.Lexeme;
                    Log(
                        $"[RESOLVE-TYPE]   -> Rilevato alias '{namespaceOrAlias}' risolto in namespace '{resolvedNamespace}'.",
                        log);
                }
            }

            Log(
                $"[RESOLVE-TYPE]   -> Rilevato nome qualificato. Cerco tipo '{typeName}' nel namespace '{resolvedNamespace}'.",
                log);

            var symbolsInNs = program.Symbols.GetSymbolIdsByNamespace(resolvedNamespace);
            if (symbolsInNs != null)
            {
                Log($"[RESOLVE-TYPE]   -> Trovati {symbolsInNs.Count} simboli nel namespace '{resolvedNamespace}'.",
                    log);
                foreach (var symbolId in symbolsInNs)
                {
                    var symbol = program.Symbols.GetSymbol(symbolId);
                    if (symbol.OriginalName.Equals(typeName, StringComparison.OrdinalIgnoreCase) &&
                        (symbol.DeclarationNode is DataType ||
                         symbol.Flags.HasFlag(SymbolFlags.IsTypeAlias) ||
                         symbol.Flags.HasFlag(SymbolFlags.IsEnum) ||
                         symbol.Flags.HasFlag(SymbolFlags.IsInterface))) // <-- AGGIUNTO
                    {
                        if (symbol.Flags.HasFlag(SymbolFlags.IsPublic))
                        {
                            DataType finalType;
                            if (symbol.DeclarationNode is EnumDefinition ed)
                            {
                                finalType = new EnumType(ed.Name, ed.UnderlyingType);
                            }
                            else if (symbol.DeclarationNode is ClassDefinition cd)
                            {
                                finalType = new ClassType(cd);
                            }
                            else if (symbol.DeclarationNode is InterfaceDefinition idef) // <-- AGGIUNTO
                            {
                                finalType = new InterfaceType(idef);
                            }
                            else if (symbol.DeclarationNode is DataType dt)
                            {
                                finalType = dt;
                            }
                            else
                            {
                                finalType = symbol.DataType;
                            }

                            Log($"[RESOLVE-TYPE]   -> TROVATO! Tipo qualificato pubblico risolto in '{finalType}'.",
                                log);
                            return finalType;
                        }
                    }
                }
            }

            Log(
                $"[RESOLVE-TYPE]   -> NON TROVATO. Nessun tipo pubblico '{typeName}' trovato nel namespace '{resolvedNamespace}'.",
                log);
            return null;
        }

        // Caso 2: Nome semplice (es. "MioTipo")
        Log($"[RESOLVE-TYPE]   -> Rilevato nome semplice. Cerco tutti i simboli con nome '{name}'.", log);
        var allSymbolsWithName = program.Symbols.FindSymbolsByName(name);
        Log($"[RESOLVE-TYPE]   -> Trovati {allSymbolsWithName.Count} simboli totali con questo nome.", log);

        foreach (var symbol in allSymbolsWithName)
        {
            if (!symbol.Flags.HasFlag(SymbolFlags.IsStruct) &&
                !symbol.Flags.HasFlag(SymbolFlags.IsClass) &&
                !symbol.Flags.HasFlag(SymbolFlags.IsEnum) &&
                !symbol.Flags.HasFlag(SymbolFlags.IsInterface) && // <-- AGGIUNTO
                !symbol.Flags.HasFlag(SymbolFlags.IsTypeAlias))
            {
                Log(
                    $"[RESOLVE-TYPE]      - Scartato simbolo ID {symbol.Id} ('{symbol.OriginalName}'). Non è un tipo. Flags: {symbol.Flags}",
                    log);
                continue;
            }

            DataType finalType;
            if (symbol.DeclarationNode is EnumDefinition ed)
            {
                finalType = new EnumType(ed.Name, ed.UnderlyingType);
            }
            else if (symbol.DeclarationNode is ClassDefinition cd)
            {
                finalType = new ClassType(cd);
            }
            else if (symbol.DeclarationNode is InterfaceDefinition idef) // <-- AGGIUNTO
            {
                finalType = new InterfaceType(idef);
            }
            else if (symbol.DeclarationNode is DataType dt)
            {
                finalType = dt;
            }
            else
            {
                finalType = symbol.DataType;
            }

            var symbolFilePath = program.FindFilePathForSymbol(symbol.DeclarationNode);
            var typeNamespace = program.Symbols.GetNamespaceNameForSymbol(symbol.Id);

            // Aggiungiamo sempre il candidato, anche se è un built-in (filePath == null).
            candidates.Add((finalType, symbolFilePath, symbol.Flags, typeNamespace));
        }

        if (!program.LoadedModules.TryGetValue(modulePath, out var currentModule))
        {
            Log(
                $"[RESOLVE-TYPE]   -> ERRORE: Modulo corrente '{Path.GetFileName(modulePath)}' non trovato in LoadedModules.",
                log);
            return candidates.Count == 1 ? candidates[0].Type : null;
        }

        var visibleSymbols = new List<DataType>();
        var wildcardImportedNamespaces = currentModule.Imports
            .Where(i => i.Mode == ImportMode.AllMembers)
            .Select(i => i.NamespaceName.Lexeme)
            .ToHashSet(StringComparer.OrdinalIgnoreCase);

        foreach (var defaultNs in program.Settings.DefaultNamespaces)
        {
            wildcardImportedNamespaces.Add(defaultNs);
        }

        Log(
            $"[RESOLVE-TYPE]   -> Namespace con visibilità globale (wildcard/default): [{string.Join(", ", wildcardImportedNamespaces)}]",
            log);

        foreach (var (type, filePath, flags, typeNamespace) in candidates)
        {
            if (string.Equals(filePath, modulePath, StringComparison.OrdinalIgnoreCase))
            {
                visibleSymbols.Add(type);
                continue;
            }

            if (flags.HasFlag(SymbolFlags.IsPublic) && wildcardImportedNamespaces.Contains(typeNamespace))
            {
                Log(
                    $"[RESOLVE-TYPE]     - Accettato candidato '{type}': pubblico e da namespace importato '{typeNamespace}'.",
                    log);
                visibleSymbols.Add(type);
            }
            else
            {
                Log(
                    $"[RESOLVE-TYPE]     - Scartato candidato '{type}': visibilità non sufficiente (Pubblico: " +
                    $"{flags.HasFlag(SymbolFlags.IsPublic)}, NS Importato: {wildcardImportedNamespaces.Contains(typeNamespace)})",
                    log);
            }
        }

        var distinctVisibleSymbols = visibleSymbols.Distinct().ToList();
        Log($"[RESOLVE-TYPE]   -> Trovati {distinctVisibleSymbols.Count} candidati unici e visibili.", log);

        if (distinctVisibleSymbols.Count > 1)
        {
            throw new BasicException(ErrorCode.AmbiguousCall,
                new Token(TokenKind.Identifier, name, name, SourcePosition.Unknown).Position,
                $"Il tipo '{name}' è ambiguo. Trovato in più namespace importati.");
        }

        var result = distinctVisibleSymbols.Count == 1 ? distinctVisibleSymbols[0] : null;
        Log(
            $"[RESOLVE-TYPE] Fine risoluzione. Risultato: {(result != null ? result.ToString() : "null (non trovato)")}",
            log);
        return result;
    }
}