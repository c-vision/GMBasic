// FILE: Interpreter.Linker.Rewrites.Utils.cs

// ReSharper disable RedundantArgumentDefaultValue

using System.Collections.Immutable;

namespace Basic;

public static partial class Linker
{
    /// <summary>
    /// [HELPER PER REWRITER] Tenta di risolvere una chiamata non qualificata come una funzione built-in,
    /// cercando in tutti i namespace che forniscono un'implementazione per quel nome di funzione.
    /// Questo metodo è a uso esclusivo del Linker/Rewriter.
    /// </summary>
    /// <param name="nameToken">Il token del nome della funzione da risolvere.</param>
    /// <param name="args">La lista di argomenti forniti nella chiamata.</param>
    /// <param name="program">L'istanza del programma per l'accesso alla SymbolTable.</param>
    /// <returns>
    /// Un `BuiltinFunctionCallExpr` se viene trovata una funzione univoca,
    /// un `UnresolvedCallExpr` se la chiamata è ambigua,
    /// o `null` se nessuna funzione built-in corrisponde.
    /// </returns>
    private static Expr? TryResolveBuiltinFunction(Token nameToken, ImmutableArray<Expr> args, BasicProgram program)
    {
        // Ottimizzazione disabilitata dopo l'estrazione del Linker: il controllo built-in
        // richiedeva StdLib. La risoluzione procede comunque correttamente tramite gli altri
        // meccanismi del Rewriter.
        return null;
    }

    /// <summary>
    /// [NUOVO HELPER] Verifica se una conversione tra tipi numerici è "widening" (non perde dati).
    /// </summary>
    private static bool IsWideningConversion(DataType source, DataType target)
    {
        // Implementazione semplificata della gerarchia di promozione
        var hierarchy = new List<Type>
        {
            typeof(ByteType), typeof(IntegerType), typeof(LongType), typeof(QuadType),
            typeof(SingleType), typeof(DoubleType), typeof(CurrencyType)
        };

        var sourceIndex = hierarchy.IndexOf(source.GetType());
        var targetIndex = hierarchy.IndexOf(target.GetType());

        // Se entrambi sono nella gerarchia, la conversione è widening se l'indice di destinazione è maggiore o uguale.
        return sourceIndex != -1 && targetIndex != -1 && targetIndex >= sourceIndex;
    }
    
    /// <summary>
    /// Determina se un valore di un certo tipo sorgente può essere assegnato o convertito
    /// implicitamente a un tipo di destinazione, secondo le regole di casting del linguaggio.
    /// </summary>
    public static bool CanCoerce(DataType sourceType, DataType targetType, AstRewriteContext context, BasicProgram program)
    {
        // 0. Nullable: assegnare il tipo sottostante a un nullable (o viceversa) è valido.
        if (targetType is NullableType targetNullable)
            return CanCoerce(sourceType, targetNullable.ElementType, context, program);
        if (sourceType is NullableType sourceNullable)
            return CanCoerce(sourceNullable.ElementType, targetType, context, program);

        // 1. Identità dei tipi: se sono identici, l'assegnazione è sempre valida.
        if (AreSameType(sourceType, targetType, context, program)) return true;

        // 2. PRIORITÀ ASSOLUTA: Gestione del tipo dinamico ANY 
        // Permette l'upcasting (qualsiasi -> ANY) e il downcasting (ANY -> specifico).
        // Questo controllo deve precedere i vincoli di OPTION STRICT per evitare errori di narrowing.
        if (targetType is AnyType || sourceType is AnyType) return true;
    
        // 3. InferredType è sempre compatibile (sarà risolto nelle passate successive).
        if (targetType is InferredType || sourceType is InferredType) return true;

        // 3b. Canali (parità Go chan): un canale generico (ValueType Any/Inferred, es. `DIM ch
        // AS CHANNEL` o `CONTEXT.DONE` che ritorna CHANNEL) è assegnabile a un canale di
        // qualunque tipo e viceversa; altrimenti il ValueType deve essere coercibile.
        if (sourceType is ChannelType sourceChan && targetType is ChannelType targetChan)
        {
            if (sourceChan.ValueType is AnyType or InferredType || targetChan.ValueType is AnyType or InferredType)
                return true;
            return CanCoerce(sourceChan.ValueType, targetChan.ValueType, context, program);
        }

        // 4. Validazione OPTION STRICT per tipi numerici (solo widening consentito).
        if (context.IsInStrictContext)
        {
            // In modalità STRICT, solo le conversioni "widening" (che non perdono dati) sono permesse.
            if (IsNumericType(sourceType) && IsNumericType(targetType))
            {
                return IsWideningConversion(sourceType, targetType);
            }
        }
        
        // 4b. Un UserDefinedType placeholder (SymbolId < 0, usato in StdLib per UDTPACKED/UDTALIGNED/SIZEOF)
        // è compatibile con qualsiasi UserDefinedType concreto.
        if (sourceType is UserDefinedType { SymbolId: < 0 } && targetType is UserDefinedType) return true;
        if (targetType is UserDefinedType { SymbolId: < 0 } && sourceType is UserDefinedType) return true;

        if (sourceType is ClassType sourceClassForInterface && targetType is InterfaceType targetInterface)
        {
            var current = sourceClassForInterface.Definition;
            while (current != null)
            {
                // Controlla se la classe corrente o una delle sue basi implementa l'interfaccia.
                if (current.ResolvedInterfaces.Any(i => i.SymbolId == targetInterface.Definition.SymbolId))
                {
                    return true;
                }
                current = current.ResolvedBaseClass;
            }
            return false;
        }

        if (sourceType is UserDefinedType udtForInterface && targetType is InterfaceType targetUDTInterface)
        {
            if (udtForInterface.ResolvedInterfaces.Any(i => i.SymbolId == targetUDTInterface.Definition.SymbolId))
                return true;
            return false;
        }
        
        if (sourceType is ClassType sourceClass && targetType is ClassType targetClass)
        {
            var current = sourceClass.Definition;
            while (current != null)
            {
                if (current.SymbolId == targetClass.Definition.SymbolId) return true;
                current = current.ResolvedBaseClass;
            }

            // Downcast implicito (necessario per c = forme(0))
            var targetBase = targetClass.Definition;
            while (targetBase != null)
            {
                if (targetBase.SymbolId == sourceClass.Definition.SymbolId) return true;
                targetBase = targetBase.ResolvedBaseClass;
            }

            return false;
        }

        if (sourceType is FunctionPointerType sourceFpt && targetType is FunctionPointerType targetFpt)
        {
            if (!AreSameType(sourceFpt.ReturnType, targetFpt.ReturnType, context, program) ||
                sourceFpt.ParameterTypes.Length != targetFpt.ParameterTypes.Length) return false;
            return !sourceFpt.ParameterTypes.Where((t, i) => !AreSameType(t, targetFpt.ParameterTypes[i], context, program)).Any();
        }

        var effectiveSourceType = sourceType is EnumType enumSource ? enumSource.UnderlyingType : sourceType;
        var effectiveTargetType = targetType is EnumType enumTarget ? enumTarget.UnderlyingType : targetType;
        if (AreSameType(effectiveSourceType, effectiveTargetType, context, program)) return true;

        var numericTypes = new List<Type>
        {
            typeof(ByteType), typeof(SByteType), typeof(IntegerType), typeof(UIntegerType), typeof(LongType),
            typeof(ULongType), typeof(QuadType), typeof(UQuadType), typeof(SingleType), typeof(DoubleType),
            typeof(CurrencyType)
        };
        var isSourceNumeric = numericTypes.Contains(effectiveSourceType.GetType()) || effectiveSourceType is BitType;
        var isTargetNumeric = numericTypes.Contains(effectiveTargetType.GetType()) || effectiveTargetType is BitType;

        if ((sourceType is EnumType || targetType is EnumType) && isSourceNumeric && isTargetNumeric) return true;
        if (isSourceNumeric && isTargetNumeric) return true;

        var isSourceDate = effectiveSourceType is DateType or TimeType or DateTimeType or TimeSpanType;
        var isTargetDate = effectiveTargetType is DateType or TimeType or DateTimeType or TimeSpanType;
        if (isSourceNumeric && isTargetDate) return true;
        if (isSourceDate && isTargetNumeric) return true;
        if (sourceType is FunctionPointerType && isTargetNumeric) return true;

        // Impedisce di assegnare una matrice (ARRAY OF ARRAY) a un vettore (ARRAY) o viceversa.
        if (sourceType is ArrayType s1 && targetType is ArrayType t1 && CanCoerce(s1.ElementType, t1.ElementType, context, program)) return true;
        
        return (sourceType, targetType) switch
        {
            (DateTimeType, DateType) => true,
            (DateTimeType, TimeType) => true,
            (DateType, DateTimeType) => true,
            (TimeType, DateTimeType) => true,
            (StringType, DateType or TimeType or DateTimeType or TimeSpanType) => true,
            (JsonType, StringType) => true,
            (StringType, JsonType) => true,

            // Gestione della covarianza verso ANY per le collezioni native.
            // Permette l'upcasting implicito se il tipo di destinazione è una collezione di ANY,
            // risolvendo gli errori di narrowing in modalità OPTION STRICT ON.
            (ListType, ListType tgt) when tgt.ElementType is AnyType => true,
            (ArrayType, ArrayType tgt) when tgt.ElementType is AnyType => true,
            (DictType, DictType tgt) when tgt.ValueType is AnyType => true,
            
            // Coercizioni tra tipi di collezione (IMPORTANTE: List -> Altro per i literal [])
            // Ogni chiamata ricorsiva a CanCoerce gestirà correttamente AnyType se presente come target.
            (ListType src, BinaryHeapType tgt) => CanCoerce(src.ElementType, tgt.ElementType, context, program),
            (ListType src, BagType tgt) => CanCoerce(src.ElementType, tgt.ElementType, context, program),
            (ArrayType src, BinaryHeapType tgt) => CanCoerce(src.ElementType, tgt.ElementType, context, program),
            (ArrayType src, BagType tgt) => CanCoerce(src.ElementType, tgt.ElementType, context, program),

            (ArrayType src, ListType tgt) => CanCoerce(src.ElementType, tgt.ElementType, context, program),
            (ListType src, ArrayType tgt) => CanCoerce(src.ElementType, tgt.ElementType, context, program),
            (ListType src, HashSetType tgt) => CanCoerce(src.ElementType, tgt.ElementType, context, program),
            (ListType src, QueueType tgt) => CanCoerce(src.ElementType, tgt.ElementType, context, program),
            (ListType src, StackType tgt) => CanCoerce(src.ElementType, tgt.ElementType, context, program),
            (ListType src, LinkedListType tgt) => CanCoerce(src.ElementType, tgt.ElementType, context, program),
            (ListType src, DoublyLinkedListType tgt) => CanCoerce(src.ElementType, tgt.ElementType, context, program),
            (ListType src, CircularLinkedListType tgt) => CanCoerce(src.ElementType, tgt.ElementType, context, program),
            (ListType src, TrieType) => CanCoerce(src.ElementType, new StringType(), context, program),
            (ArrayType src, HashSetType tgt) => CanCoerce(src.ElementType, tgt.ElementType, context, program),
            (ArrayType src, QueueType tgt) => CanCoerce(src.ElementType, tgt.ElementType, context, program),
            (ArrayType src, StackType tgt) => CanCoerce(src.ElementType, tgt.ElementType, context, program),
            (ArrayType src, TrieType) => CanCoerce(src.ElementType, new StringType(), context, program),

            // Coercizioni Dict e Mappe
            (DictType src, DictType tgt) => CanCoerce(src.KeyType, tgt.KeyType, context, program) &&
                                            CanCoerce(src.ValueType, tgt.ValueType, context, program),
            (DictType src, SortedDictType tgt) => CanCoerce(src.KeyType, tgt.KeyType, context, program) &&
                                                  CanCoerce(src.ValueType, tgt.ValueType, context, program),
            (DictType src, TreeType tgt) => CanCoerce(src.KeyType, tgt.KeyType, context, program) &&
                                            CanCoerce(src.ValueType, tgt.ValueType, context, program),
            (DictType src, AVLTreeType tgt) => CanCoerce(src.KeyType, tgt.KeyType, context, program) &&
                                               CanCoerce(src.ValueType, tgt.ValueType, context, program),
            (DictType src, RedBlackTreeType tgt) => CanCoerce(src.KeyType, tgt.KeyType, context, program) &&
                                                    CanCoerce(src.ValueType, tgt.ValueType, context, program),
            (DictType src, BPlusTreeType tgt) => CanCoerce(src.KeyType, tgt.KeyType, context, program) &&
                                                 CanCoerce(src.ValueType, tgt.ValueType, context, program),
            (DictType src, CacheType tgt) => CanCoerce(src.KeyType, tgt.KeyType, context, program) &&
                                             CanCoerce(src.ValueType, tgt.ValueType, context, program),

            (ArrayType s, ArrayType t) => CanCoerce(s.ElementType, t.ElementType, context, program),
            (ListType s, ListType t) => CanCoerce(s.ElementType, t.ElementType, context, program),

            (_, StringType) => true,
            _ => false
        };
    }

    /// <summary>
    /// Metodo helper centralizzato per trovare tutte le procedure candidate (ProcedureDefinition)
    /// per un dato nome, seguendo le regole di visibilità dello scope corrente.
    /// Esegue una ricerca a due fasi: prima risale l'albero degli scope (locale, procedura, etc.),
    /// poi, se non trova nulla, cerca nei namespace importati e di default.
    /// </summary>
    private static List<ProcedureDefinition> FindProcedureCandidatesInScope(string name, AstRewriteContext context,
        BasicProgram program)
    {
        List<ProcedureDefinition>? candidates = null;

        // --- FASE 1: Ricerca Locale (Scope-Up) ---
        // Utilizza l'overload che si basa sullo scope corrente della SymbolTable.
        var candidateIds = program.Symbols.FindSymbolInScope(name);
        if (candidateIds != null)
        {
            candidates = candidateIds
                .Select(id => program.Symbols.GetSymbol(id).DeclarationNode)
                .OfType<ProcedureDefinition>()
                .ToList();
        }

        // --- FASE 2: Ricerca nei Namespace Importati (Fallback) ---
        // Eseguita SOLO SE la ricerca locale non ha prodotto risultati.
        if (candidates != null && candidates.Count != 0)
            return candidates.Distinct().ToList(); // Aggiunto Distinct() per sicurezza
        candidates = [];
        var visibleNamespaces = new HashSet<string>(StringComparer.OrdinalIgnoreCase);

        // Aggiungi i namespace di default
        foreach (var ns in program.Settings.DefaultNamespaces)
        {
            visibleNamespaces.Add(ns);
        }

        // Aggiungi i namespace importati nel modulo corrente
        if (program.LoadedModules.TryGetValue(context.ModulePath, out var currentModule))
        {
            foreach (var import in currentModule.Imports)
            {
                switch (import.Mode)
                {
                    case ImportMode.Namespace:
                    case ImportMode.AllMembers:
                        visibleNamespaces.Add(import.NamespaceName.Lexeme);
                        break;
                    case ImportMode.SpecificMembers:
                    {
                        var memberMatch = import.SpecificMembers.FirstOrDefault(m =>
                            (m.Alias?.Lexeme ?? m.OriginalName.Lexeme).Equals(name,
                                StringComparison.OrdinalIgnoreCase));
                        if (memberMatch != null)
                        {
                            visibleNamespaces.Add(import.NamespaceName.Lexeme);
                        }

                        break;
                    }
                }
            }
        }

        // Raccogli tutti i candidati dai namespace visibili.
        foreach (var nsName in visibleNamespaces)
        {
            candidates.AddRange(program.GetProceduresForNamespace(nsName)
                .Where(p => p.OriginalName.Lexeme.Equals(name, StringComparison.OrdinalIgnoreCase) &&
                            p.Flags.HasFlag(SymbolFlags.IsPublic))); // Solo membri pubblici
        }

        return candidates.Distinct().ToList(); // Aggiunto Distinct() per sicurezza
    }

    /// <summary>
    /// Converte un'espressione AST leggibile (`Expr`) nel suo equivalente target di assegnazione scrivibile (`Target`).
    /// Questo è un helper cruciale per trasformare le espressioni parsate in L-values per istruzioni
    /// come `GET`, `INPUT` o `LET`.
    /// </summary>
    /// <param name="expr">
    /// Il nodo `Expr` da convertire. Deve rappresentare un L-value valido, come `VariableExpr`,
    /// `ArrayAccessExpr`, `FieldAccessExpr` o `IndexedAccessExpr`.
    /// </param>
    /// <returns>Un nuovo nodo `Target` che rappresenta semanticamente la stessa locazione dell' `Expr` di input.</returns>
    /// <exception cref="InvalidOperationException">
    /// Lanciata quando l'`Expr` di input non rappresenta un target assegnabile valido
    /// (es. una chiamata a funzione o un valore letterale).
    /// </exception>
    /// <remarks>
    /// Il parser produce spesso nodi `Expr` di default perché sta analizzando un'espressione generica.
    /// Tuttavia, alcune istruzioni richiedono un nodo `Target` specifico per rappresentare una destinazione di assegnazione.
    /// Questo metodo esegue quella conversione specifica, assicurando che l'AST sia semanticamente corretto prima dell'esecuzione.
    /// </remarks>
    private static Target ExpressionToTarget(Expr expr)
    {
        return expr switch
        {
            VariableExpr ve => new VariableTarget(ve.SymbolId, ve.NameSuffixed, ve.NameCleaned),
            ArrayAccessExpr aae => new ArrayElementTarget(aae.SymbolId, aae.NameSuffixed, aae.NameCleaned, aae.Indices),
            FieldAccessExpr fae => new FieldTarget(ExpressionToTarget(fae.Structure), -1, fae.Field),
            IndexedAccessExpr iae => new IndexedTarget(iae.Base, iae.Indices),
            _ => throw new InvalidOperationException(
                $"Cannot convert expression of type {expr.GetType().Name} to a valid Target.")
        };
    }

    /// <summary>
    /// Inferisce il tipo di un campo di una struttura/classe.
    /// </summary>
    private static DataType? InferFieldType(Target structureTarget, Token field, AstRewriteContext context,
        BasicProgram program)
    {
        const bool log = false;
        // Dobbiamo prima trovare il tipo della struttura stessa.
        var structureExpr = ConvertTargetToExpr(structureTarget);
        var structureType = LinkerInferExpressionType(structureExpr, context, program);

        Log($"[INFER-FIELD] Inferenza per campo '{field.Lexeme}'. Tipo della struttura base: {structureType}", log);

        // 1. Controllo nel registro degli schemi built-in (priorità ai tipi nativi)
        if (TypeSchemaRegistry.TryGetValue(structureType.GetType(), out var schema))
        {
            if (schema!.Fields.TryGetValue(field.Lexeme, out var fieldType))
            {
                return fieldType;
            }
            // Se non è nei campi, continuiamo (potrebbe essere un UDT con stesso nome .NET Type, improbabile ma sicuro).
        }

        // 2. Risoluzione campi standard (UDT e Classi definiti dall'utente)
        var result = structureType switch
        {
            UserDefinedType udt =>
                // Cerca prima tra i campi
                udt.Fields.FirstOrDefault(f => f.Name.Lexeme.Equals(field.Lexeme, StringComparison.OrdinalIgnoreCase))
                    // Se il campo ha dimensioni, restituisce ArrayType, altrimenti DataType semplice.
                    is { } fieldSymbol
                    ? (fieldSymbol.Dimensions.Any() ? new ArrayType(fieldSymbol.DataType) : fieldSymbol.DataType)
                    :
                    // Se non è un campo, cerca tra le proprietà
                    udt.Properties.FirstOrDefault(p =>
                        p.Name.Lexeme.Equals(field.Lexeme, StringComparison.OrdinalIgnoreCase))?.Type,

            ClassType ct =>
                (ct.Definition.Members.FirstOrDefault(m =>
                        m is FieldMember fm &&
                        fm.Name.Lexeme.Equals(field.Lexeme, StringComparison.OrdinalIgnoreCase)) as
                    FieldMember)
                // Gestisce i campi array anche per le classi.
                is { } fieldMember
                    ? (fieldMember.Declaration.Dimensions.Any()
                        ? new ArrayType(fieldMember.Declaration.DataType ?? new InferredType())
                        : fieldMember.Declaration.DataType)
                    : (ct.Definition.Members.FirstOrDefault(m =>
                        m is PropertyMember pm &&
                        pm.Name.Lexeme.Equals(field.Lexeme, StringComparison.OrdinalIgnoreCase)) as PropertyMember)
                    ?.Property.Type,

            InterfaceType it =>
                (it.Definition.Members.FirstOrDefault(m =>
                        m is PropertyMember pm && pm.Name.Lexeme.Equals(field.Lexeme, StringComparison.OrdinalIgnoreCase)) 
                    as PropertyMember)?.Property.Type,
            
            _ => null
        };
        Log($"[INFER-FIELD] -> Tipo finale inferito per il campo: {result}", log);
        return result;
    }

    /// <summary>
    /// Inferisce il tipo di un target. Questo metodo centralizza la logica di tipo per i target.
    /// CORREZIONE 2: Gestisce correttamente ArrayElementTarget senza indici (riferimento all'intero array),
    /// restituendo il tipo del simbolo invece del tipo dell'elemento.
    /// </summary>
    public static DataType? InferTargetType(Target target, AstRewriteContext context, BasicProgram program)
    {
        var result = target switch
        {
            VariableTarget vt => vt.ResolvedType ??
                                 (vt.SymbolId >= 0 ? program.Symbols.GetSymbol(vt.SymbolId).DataType : null),
            
            ArrayElementTarget aet => aet.ResolvedType ?? (aet.SymbolId >= 0
                ? (aet.Indices.IsEmpty 
                    // Se Indices è vuoto, stiamo accedendo all'intero array/lista
                    ? program.Symbols.GetSymbol(aet.SymbolId).DataType 
                    // Se ci sono indici, accediamo all'elemento
                    : (program.Symbols.GetSymbol(aet.SymbolId).DataType as ArrayType)?.ElementType 
                      ?? (program.Symbols.GetSymbol(aet.SymbolId).DataType as ListType)?.ElementType)
                : null),
            
            IndexedTarget it => it.ResolvedType ?? InferElementType(it.Base, context, program),
            FieldTarget ft when ft.Structure is null => ft.ResolvedType, // WITH block implicit member
            FieldTarget ft => ft.ResolvedType ?? InferFieldType(ft.Structure!, ft.OriginalToken, context, program),
            _ => null
        };
        return result;
    }

    /// <summary>
    /// Inferisce il tipo di un elemento di una collezione/array.
    /// </summary>
    private static DataType? InferElementType(Expr collectionExpr, AstRewriteContext context,
        BasicProgram program)
    {
        var collectionType = LinkerInferExpressionType(collectionExpr, context, program);
        return collectionType switch
        {
            ArrayType at => at.ElementType,
            ListType lt => lt.ElementType,
            GeneratorType gt => gt.YieldType,
            HashSetType hst => hst.ElementType,
            QueueType qt => qt.ElementType,
            StackType st => st.ElementType,
            BinaryIndexedTreeType bitt => bitt.ElementType,
            LinkedListType llt => llt.ElementType,
            DoublyLinkedListType dllt => dllt.ElementType,
            CircularLinkedListType cllt => cllt.ElementType,
            BinaryHeapType bht => bht.ElementType,
            BagType bt => bt.ElementType,
            GraphType gt => gt.NodeType,
            TimelineType tlt => tlt.ValueType,
            HashRingType hrt => hrt.NodeType,
            RingBufferType rbt => rbt.ElementType,
            SparseSetType => new LongType(),
            DictType dt => dt.ValueType,
            SortedDictType sdt => sdt.ValueType,
            TreeType tt => tt.ValueType,
            AVLTreeType avlt => avlt.ValueType,
            RedBlackTreeType rbt => rbt.ValueType,
            CacheType ct => ct.ValueType,
            BPlusTreeType bpt => bpt.ValueType,
            // ponytail: SQL cell type is dynamic — InferredType keeps rs("col") usable in PRINT/LET
            SqlResultSetType => new InferredType(),
            _ => null
        };
    }

    /// <summary>
    /// Risolve un DataType, trasformando tipi non risolti (UnresolvedQualifiedType)
    /// in tipi concreti (ClassType, UserDefinedType, etc.). Gestisce la risoluzione ricorsiva
    /// per i tipi contenitore e previene le dipendenze circolari.
    /// </summary>
    /// <param name="dataType">Il tipo di dato da risolvere.</param>
    /// <param name="program">Il programma che contiene la SymbolTable e il contesto di compilazione.</param>
    /// <param name="modulePath">Il percorso del modulo dal cui contesto parte la risoluzione.</param>
    /// <param name="visited">Set per tracciare i tipi visitati e prevenire ricorsioni infinite.</param>
    /// <returns>Il DataType concreto e risolto.</returns>
    public static DataType ResolveDataType(DataType dataType, BasicProgram program, string modulePath,
        HashSet<DataType>? visited = null)
    {
        visited ??= [];
        if (!visited.Add(dataType))
        {
            return dataType;
        }

        try
        {
            switch (dataType)
            {
                case UnresolvedQualifiedType { TypeArguments.IsDefaultOrEmpty: false } uqtGeneric:
                {
                    var fullName = uqtGeneric.QualifiedName.FullName;
                    var genericType = TryResolveTypeName(fullName, program, modulePath);
                    if (genericType is UserDefinedType { TypeParameters.IsDefaultOrEmpty: false } genericUdt)
                    {
                        return ResolveGenericUdtInstantiation(genericUdt, uqtGeneric, program, modulePath, visited);
                    }

                    if (genericType is ClassType { Definition.TypeParameters.IsDefaultOrEmpty: false } genericClassType)
                    {
                        var specializedClass = ResolveGenericClassInstantiation(genericClassType.Definition, uqtGeneric, program, modulePath, visited);
                        return new ClassType(specializedClass);
                    }

                    throw new BasicException(ErrorCode.TypeNotDefined, uqtGeneric.Position,
                        $"Il tipo '{fullName}' non è generico o non è stato trovato: impossibile applicare i type argument '<{string.Join(", ", uqtGeneric.TypeArguments)}>'.");
                }

                case UnresolvedQualifiedType uqt:
                {
                    var fullName = uqt.QualifiedName.FullName;
                    var foundType = TryResolveTypeName(fullName, program, modulePath);
                    if (foundType != null)
                    {
                        return ResolveDataType(foundType, program, modulePath, visited);
                    }

                    // --- NUOVA LOGICA PER DELEGATE ---
                    // Se TryResolveTypeName non trova un tipo (UDT/Class/Enum), 
                    // potrebbe essere un delegato. Cerchiamolo come simbolo.
                    var symbolCandidates = program.Symbols.FindSymbolsByName(fullName);
                    var delegateSymbol = symbolCandidates.FirstOrDefault(s =>
                        s.Flags.HasFlag(SymbolFlags.IsTypeAlias) && s.DataType is FunctionPointerType);

                    if (delegateSymbol != null)
                    {
                        // Trovato! Restituiamo il suo tipo sottostante.
                        return delegateSymbol.DataType;
                    }
                    // --- FINE NUOVA LOGICA ---

                    throw new BasicException(ErrorCode.TypeNotDefined, uqt.Position,
                        $"Il tipo '{fullName}' non è stato trovato o è ambiguo.");
                }

                // Aggiunta logica per recuperare l'istanza autorevole dalla SymbolTable.
                case UserDefinedType { SymbolId: > 0 } udt:
                {
                    // Se l'UDT ha già un ID valido, usiamo quell'ID per ottenere la versione
                    // "fresca" e completamente popolata dal Binder, che si trova nella SymbolTable.
                    var symbol = program.Symbols.GetSymbol(udt.SymbolId);
                    if (symbol.DeclarationNode is UserDefinedType authoritativeUdt)
                    {
                        // DIAGNOSTICA: Verifica se stiamo effettivamente arricchendo il tipo
                        if (udt.Methods.IsEmpty && !authoritativeUdt.Methods.IsEmpty)
                        {
                            Log($"[RESOLVE-DATA-TYPE] Refreshing UDT '{udt.Name.Lexeme}' (ID {udt.SymbolId}). " +
                                $"Methods count: {udt.Methods.Length} -> {authoritativeUdt.Methods.Length}",
                                false); // Log spento per default
                        }

                        // Non chiamiamo ricorsivamente ResolveDataType su authoritativeUdt per evitare loop infiniti
                        // se la SymbolTable contenesse riferimenti circolari, ma ci fidiamo che l'istanza
                        // nella SymbolTable sia quella corretta.
                        return authoritativeUdt;
                    }

                    return udt; // Fallback se il simbolo non contiene un UDT
                }

                // Aggiunta logica per recuperare l'istanza autorevole di InterfaceDefinition
                case InterfaceType { Definition.SymbolId: > 0 } it:
                {
                    var symbol = program.Symbols.GetSymbol(it.Definition.SymbolId);
                    if (symbol.DeclarationNode is InterfaceDefinition authoritativeInterfaceDef)
                    {
                        return new InterfaceType(authoritativeInterfaceDef);
                    }
                    return it; // Fallback
                }
                
                case ClassType { Definition.SymbolId: > 0 } ct:
                {
                    // Stessa logica per le Classi.
                    var symbol = program.Symbols.GetSymbol(ct.Definition.SymbolId);
                    if (symbol.DeclarationNode is ClassDefinition authoritativeClassDef)
                    {
                        return new ClassType(authoritativeClassDef);
                    }

                    return ct;
                }

                // --- CASI RICORSIVI: (invariati) ---
                case ArrayType at:
                    return at with { ElementType = ResolveDataType(at.ElementType, program, modulePath, visited) };
                case ListType lt:
                    return lt with { ElementType = ResolveDataType(lt.ElementType, program, modulePath, visited) };
                case DictType dt:
                    return new DictType(
                        ResolveDataType(dt.KeyType, program, modulePath, visited),
                        ResolveDataType(dt.ValueType, program, modulePath, visited));
                case HashSetType hst:
                    return hst with { ElementType = ResolveDataType(hst.ElementType, program, modulePath, visited) };
                case QueueType qt:
                    return qt with { ElementType = ResolveDataType(qt.ElementType, program, modulePath, visited) };
                case StackType st:
                    return st with { ElementType = ResolveDataType(st.ElementType, program, modulePath, visited) };
                case SortedDictType sdt:
                    return new SortedDictType(
                        ResolveDataType(sdt.KeyType, program, modulePath, visited),
                        ResolveDataType(sdt.ValueType, program, modulePath, visited));
                case TreeType tt:
                    return new TreeType(
                        ResolveDataType(tt.KeyType, program, modulePath, visited),
                        ResolveDataType(tt.ValueType, program, modulePath, visited));
                case AVLTreeType avlt:
                    return new AVLTreeType(
                        ResolveDataType(avlt.KeyType, program, modulePath, visited),
                        ResolveDataType(avlt.ValueType, program, modulePath, visited));
                case RedBlackTreeType rbt:
                    return new RedBlackTreeType(
                        ResolveDataType(rbt.KeyType, program, modulePath, visited),
                        ResolveDataType(rbt.ValueType, program, modulePath, visited));
                case BinaryIndexedTreeType bitt:
                    return bitt with { ElementType = ResolveDataType(bitt.ElementType, program, modulePath, visited) };
                case LinkedListType llt:
                    return llt with { ElementType = ResolveDataType(llt.ElementType, program, modulePath, visited) };
                case DoublyLinkedListType dllt:
                    return dllt with { ElementType = ResolveDataType(dllt.ElementType, program, modulePath, visited) };
                case CircularLinkedListType cllt:
                    return cllt with { ElementType = ResolveDataType(cllt.ElementType, program, modulePath, visited) };
                case BinaryHeapType bht:
                    return new BinaryHeapType(ResolveDataType(bht.ElementType, program, modulePath, visited),
                        bht.IsMaxHeap);
                case BagType bt:
                    return bt with { ElementType = ResolveDataType(bt.ElementType, program, modulePath, visited) };
                case CacheType ct:
                    return new CacheType(
                        ResolveDataType(ct.KeyType, program, modulePath, visited),
                        ResolveDataType(ct.ValueType, program, modulePath, visited));
                case GraphType gt:
                    return gt with { NodeType = ResolveDataType(gt.NodeType, program, modulePath, visited) };
                case TimelineType tlt:
                    return tlt with { ValueType = ResolveDataType(tlt.ValueType, program, modulePath, visited) };
                case ChannelType chanT:
                    return chanT with { ValueType = ResolveDataType(chanT.ValueType, program, modulePath, visited) };
                case GeneratorType genT:
                    return genT with { YieldType = ResolveDataType(genT.YieldType, program, modulePath, visited) };
                // NullableType si risolve al tipo sottostante: a runtime un nullable è
                // rappresentato dal valore boxed oppure da null (C#). Non serve un wrapper.
                // ECCEZIONE: un nullable che risolve a un UDT (es. `Contatto?`, sia già
                // UserDefinedType sia UnresolvedQualifiedType) resta NullableType — il
                // transpiler C lo mappa a gmb_val_t (boxed), perché un UDT struct non può
                // rappresentare lo stato NIL (c = NIL / NEW / ?.).
                case NullableType nt:
                {
                    var ntResolved = ResolveDataType(nt.ElementType, program, modulePath, visited);
                    return new NullableType(ntResolved);
                }
                // TypeParameter: non risolvere qui. I type parameters rimangono come placeholder
                // nella definizione generica originale. Vengono sostituiti solo durante
                // la specializzazione (SpecializeGenericFunction) quando il generico viene chiamato.
                case TypeParameter:
                    return dataType;
                case FunctionPointerType fpt:
                    return new FunctionPointerType(
                        [..fpt.ParameterTypes.Select(p => ResolveDataType(p, program, modulePath, visited))],
                        ResolveDataType(fpt.ReturnType, program, modulePath, visited));

                default:
                    return dataType;
            }
        }
        finally
        {
            visited.Remove(dataType);
        }
    }

    /// <summary>
    /// Sostituisce i riferimenta a <see cref="TypeParameter"/> nel DataType dato un dizionario di sostituzione.
    /// Usato quando un generico viene specializzato (es. LIST OF T → LIST OF INTEGER).
    /// </summary>
    /// <param name="dataType">Il tipo di dato in cui sostituire.</param>
    /// <param name="substitution">Mappa dai TypeParameter ai tipi concreti.</param>
    /// <returns>Il DataType con i parametri sostituiti.</returns>
    public static DataType SubstituteTypeParameters(
        DataType dataType,
        ImmutableDictionary<TypeParameter, DataType> substitution)
    {
        return dataType switch
        {
            TypeParameter tp when substitution.TryGetValue(tp, out var resolved) => resolved,
            TypeParameter tp => tp, // Non sostituito — lascia il parametro (caso d'errore)
            // Un riferimento a un type parameter (es. "v AS T" in un parametro/campo) resta,
            // prima del binding, un UnresolvedQualifiedType per nome (non ancora un vero
            // TypeParameter — quello esiste solo nella clausola <T> della dichiarazione
            // generica) — se il nome coincide con un parametro nella substitution map,
            // va sostituito qui: nessun secondo passo lo risolverebbe più tardi per un
            // parametro isolato come "T" (ResolveDataType lo cercherebbe come simbolo per
            // nome nella SymbolTable, dove "T" non è mai registrato).
            UnresolvedQualifiedType { TypeArguments.IsDefaultOrEmpty: true } uqt
                when substitution.Keys.FirstOrDefault(k =>
                        k.Name.Lexeme.Equals(uqt.QualifiedName.FullName, StringComparison.OrdinalIgnoreCase))
                    is { } matchedParam => substitution[matchedParam],
            UnresolvedQualifiedType uqt => uqt, // Sarà risolto da ResolveDataType
            ArrayType at => at with { ElementType = SubstituteTypeParameters(at.ElementType, substitution) },
            ListType lt => lt with { ElementType = SubstituteTypeParameters(lt.ElementType, substitution) },
            DictType dt => new DictType(
                SubstituteTypeParameters(dt.KeyType, substitution),
                SubstituteTypeParameters(dt.ValueType, substitution)),
            HashSetType hst => hst with { ElementType = SubstituteTypeParameters(hst.ElementType, substitution) },
            QueueType qt => qt with { ElementType = SubstituteTypeParameters(qt.ElementType, substitution) },
            StackType st => st with { ElementType = SubstituteTypeParameters(st.ElementType, substitution) },
            SortedDictType sdt => new SortedDictType(
                SubstituteTypeParameters(sdt.KeyType, substitution),
                SubstituteTypeParameters(sdt.ValueType, substitution)),
            TreeType tt => new TreeType(
                SubstituteTypeParameters(tt.KeyType, substitution),
                SubstituteTypeParameters(tt.ValueType, substitution)),
            AVLTreeType avlt => new AVLTreeType(
                SubstituteTypeParameters(avlt.KeyType, substitution),
                SubstituteTypeParameters(avlt.ValueType, substitution)),
            RedBlackTreeType rbt => new RedBlackTreeType(
                SubstituteTypeParameters(rbt.KeyType, substitution),
                SubstituteTypeParameters(rbt.ValueType, substitution)),
            BinaryIndexedTreeType bitt => bitt with { ElementType = SubstituteTypeParameters(bitt.ElementType, substitution) },
            LinkedListType llt => llt with { ElementType = SubstituteTypeParameters(llt.ElementType, substitution) },
            DoublyLinkedListType dllt => dllt with { ElementType = SubstituteTypeParameters(dllt.ElementType, substitution) },
            CircularLinkedListType cllt => cllt with { ElementType = SubstituteTypeParameters(cllt.ElementType, substitution) },
            BinaryHeapType bht => new BinaryHeapType(
                SubstituteTypeParameters(bht.ElementType, substitution), bht.IsMaxHeap),
            BagType bt => bt with { ElementType = SubstituteTypeParameters(bt.ElementType, substitution) },
            CacheType ct => new CacheType(
                SubstituteTypeParameters(ct.KeyType, substitution),
                SubstituteTypeParameters(ct.ValueType, substitution)),
            GraphType gt => gt with { NodeType = SubstituteTypeParameters(gt.NodeType, substitution) },
            TimelineType tlt => tlt with { ValueType = SubstituteTypeParameters(tlt.ValueType, substitution) },
            HashRingType hrt => hrt with { NodeType = SubstituteTypeParameters(hrt.NodeType, substitution) },
            RingBufferType rbt => rbt with { ElementType = SubstituteTypeParameters(rbt.ElementType, substitution) },
            BPlusTreeType bpt => new BPlusTreeType(
                SubstituteTypeParameters(bpt.KeyType, substitution),
                SubstituteTypeParameters(bpt.ValueType, substitution)),
            NullableType nt => new NullableType(SubstituteTypeParameters(nt.ElementType, substitution)),
            FunctionPointerType fpt => new FunctionPointerType(
                fpt.ParameterTypes.Select(p => SubstituteTypeParameters(p, substitution)).ToImmutableArray(),
                SubstituteTypeParameters(fpt.ReturnType, substitution)),
            UserDefinedType udt => udt with { Fields = udt.Fields.Select(f =>
                f with { DataType = SubstituteTypeParameters(f.DataType, substitution) }
            ).ToImmutableArray() },
            _ => dataType
        };
    }

    /// <summary>
    /// Risolve un'istanziazione concreta di un TYPE generico (es. <c>Box&lt;DOUBLE&gt;</c>), producendo
    /// una monomorfizzazione: una copia specializzata dell'<see cref="UserDefinedType"/> generico con
    /// i <see cref="TypeParameter"/> sostituiti dai tipi concreti forniti nei type argument.
    /// Il risultato viene cachato per nome mangled in <see cref="BasicProgram.GenericUdtInstantiations"/>,
    /// così che ogni istanziazione (es. tutte le occorrenze di <c>Box&lt;DOUBLE&gt;</c> nel programma)
    /// condivida lo stesso SymbolId e la stessa identità di tipo.
    /// </summary>
    private static UserDefinedType ResolveGenericUdtInstantiation(
        UserDefinedType genericUdt,
        UnresolvedQualifiedType uqt,
        BasicProgram program,
        string modulePath,
        HashSet<DataType> visited)
    {
        if (genericUdt.TypeParameters.Length != uqt.TypeArguments.Length)
        {
            throw new BasicException(ErrorCode.TypeMismatch, uqt.Position,
                $"Il tipo generico '{genericUdt.OriginalName.Lexeme}' richiede {genericUdt.TypeParameters.Length} " +
                $"type argument(s), ma ne sono stati forniti {uqt.TypeArguments.Length}.");
        }

        var resolvedArgs = uqt.TypeArguments
            .Select(t => ResolveDataType(t, program, modulePath, visited))
            .ToImmutableArray();

        var mangledName = $"{genericUdt.OriginalName.Lexeme}<{string.Join(",", resolvedArgs.Select(a => a.ToString()))}>";
        if (program.GenericUdtInstantiations.TryGetValue(mangledName, out var cached))
        {
            return cached;
        }

        var substitution = ImmutableDictionary.CreateBuilder<TypeParameter, DataType>();
        for (var i = 0; i < genericUdt.TypeParameters.Length; i++)
        {
            substitution[genericUdt.TypeParameters[i]] = resolvedArgs[i];
        }

        var substitutionMap = substitution.ToImmutable();
        var mangledNameToken = genericUdt.OriginalName with { Lexeme = mangledName };

        var specialized = genericUdt with
        {
            SymbolId = -1,
            Name = mangledNameToken,
            // Ogni campo, oltre al DataType nell'AST, ha una propria voce nella SymbolTable
            // (registrata da RegisterUdt in fase di binding, condivisa dal TYPE generico
            // originale). Alcuni consumatori (es. IlProgramCompiler.InferFieldAccessType)
            // leggono il tipo del campo da lì (`Symbols.GetSymbol(fieldSymbolId).DataType`),
            // non dall'AST — se riusassimo il SymbolId del generico, quella voce condivisa
            // resterebbe T non sostituito, e sarebbe pure sbagliata per qualunque ALTRA
            // istanziazione dello stesso generico (es. Box<STRING> nello stesso programma).
            // Ogni specializzazione ha quindi bisogno di simboli-campo PROPRI.
            Fields = genericUdt.Fields
                .Select(f =>
                {
                    var substitutedType = SubstituteTypeParameters(f.DataType, substitutionMap);
                    var newFieldId = program.Symbols.RegisterSymbol(
                        f.Name, SymbolFlags.IsField | SymbolFlags.IsPublic, substitutedType, f);
                    var newField = f with { SymbolId = newFieldId, DataType = substitutedType };
                    program.Symbols.UpdateSymbolDeclaration(newFieldId, newField);
                    return newField;
                })
                .ToImmutableArray(),
            Methods = genericUdt.Methods
                .Select(m => SubstituteInProcedureSignature(m, substitutionMap))
                .ToImmutableArray(),
            Constructors = genericUdt.Constructors
                .Select(c => c with
                {
                    Parameters = c.Parameters
                        .Select(p => p with { DataType = SubstituteTypeParameters(p.DataType, substitutionMap) })
                        .ToImmutableArray()
                })
                .ToImmutableArray(),
            TypeParameters = ImmutableArray<TypeParameter>.Empty
        };
        specialized = specialized with { OriginalName = mangledNameToken };

        // Ri-ancora SELF: i metodi/costruttori clonati puntano ancora, via Parent, al TYPE
        // generico originale (impostato dal Binder in fase 2). Senza questo fixup, SELF nei
        // corpi dei metodi risolverebbe i campi contro il TYPE generico invece che contro
        // questa specializzazione (stesso field, ma proprietario sbagliato).
        foreach (var method in specialized.Methods)
        {
            method.Parent = specialized;
        }
        foreach (var ctor in specialized.Constructors)
        {
            ctor.Parent = specialized;
        }

        var specId = program.Symbols.RegisterSymbol(mangledNameToken, genericUdt.Flags, specialized, specialized);
        specialized = specialized with { SymbolId = specId };
        program.Symbols.UpdateSymbolDeclaration(specId, specialized);

        program.GenericUdtInstantiations[mangledName] = specialized;
        return specialized;
    }

    /// <summary>
    /// Risolve un'istanziazione concreta di una CLASS generica (es. <c>Box&lt;DOUBLE&gt;</c>), stessa
    /// meccanica di <see cref="ResolveGenericUdtInstantiation"/> ma per <see cref="ClassDefinition"/>.
    /// Scope MVP (lancia esplicito oltre questo): nessuna ereditarietà/interfaccia sulla classe
    /// generica, e solo membri <see cref="FieldMember"/>/<see cref="MethodMember"/>/
    /// <see cref="ConstructorMember"/> — stessa restrizione già in vigore per i TYPE generici
    /// (PROPERTY/OPERATOR/DELETE non ancora supportati lì, e qui il backend IL non supporta
    /// comunque PROPERTY/OPERATOR/DELETE/ereditarietà/interfacce insieme ai generici).
    /// </summary>
    private static ClassDefinition ResolveGenericClassInstantiation(
        ClassDefinition genericClass,
        UnresolvedQualifiedType uqt,
        BasicProgram program,
        string modulePath,
        HashSet<DataType> visited)
    {
        if (genericClass.TypeParameters.Length != uqt.TypeArguments.Length)
        {
            throw new BasicException(ErrorCode.TypeMismatch, uqt.Position,
                $"La classe generica '{genericClass.OriginalName.Lexeme}' richiede {genericClass.TypeParameters.Length} " +
                $"type argument(s), ma ne sono stati forniti {uqt.TypeArguments.Length}.");
        }

        if (genericClass.BaseClassSymbolId is not null || !genericClass.ImplementedInterfaceNames.IsDefaultOrEmpty)
        {
            throw new BasicException(ErrorCode.TypeNotInstantiable, uqt.Position,
                $"Classe generica '{genericClass.OriginalName.Lexeme}': ereditarietà e interfacce non ancora " +
                "supportate insieme ai generici.");
        }

        var unsupportedMember = genericClass.Members.FirstOrDefault(m =>
            m is not (FieldMember or MethodMember or ConstructorMember));
        if (unsupportedMember is not null)
        {
            throw new BasicException(ErrorCode.TypeNotInstantiable, uqt.Position,
                $"Classe generica '{genericClass.OriginalName.Lexeme}': membro '{unsupportedMember.GetType().Name}' " +
                "non ancora supportato insieme ai generici (solo campi, metodi, costruttori).");
        }

        var resolvedArgs = uqt.TypeArguments
            .Select(t => ResolveDataType(t, program, modulePath, visited))
            .ToImmutableArray();

        var mangledName = $"{genericClass.OriginalName.Lexeme}<{string.Join(",", resolvedArgs.Select(a => a.ToString()))}>";
        if (program.GenericClassInstantiations.TryGetValue(mangledName, out var cached))
        {
            return cached;
        }

        var substitution = ImmutableDictionary.CreateBuilder<TypeParameter, DataType>();
        for (var i = 0; i < genericClass.TypeParameters.Length; i++)
        {
            substitution[genericClass.TypeParameters[i]] = resolvedArgs[i];
        }

        var substitutionMap = substitution.ToImmutable();
        var mangledNameToken = genericClass.OriginalName with { Lexeme = mangledName };

        // Ogni PARAMETRO con un tipo generico ha bisogno di un proprio simbolo in SymbolTable
        // (non quello condiviso del template) — stessa ragione dei campi sotto: il corpo
        // (riscritto più avanti per QUESTA specializzazione) referenzia il parametro per
        // SymbolId, e quel SymbolId nella SymbolTable ha ancora DataType=T se non
        // rialloccato qui. paramSymbolMap sostituisce ogni VariableExpr che referenzia il
        // SymbolId originale con uno che referenzia il nuovo, durante il rewrite del corpo.
        var paramSymbolMap = new Dictionary<int, Expr>();

        ImmutableArray<Parameter> SpecializeParameters(ImmutableArray<Parameter> parameters) =>
            parameters.Select(p =>
            {
                var substitutedType = SubstituteTypeParameters(p.DataType, substitutionMap);
                if (p.SymbolId < 0) return p with { DataType = substitutedType };
                var newParamId = program.Symbols.RegisterSymbol(p.Name, p.Flags, substitutedType, p);
                paramSymbolMap[p.SymbolId] = new VariableExpr(newParamId, p.Name, p.OriginalName);
                return p with { SymbolId = newParamId, DataType = substitutedType };
            }).ToImmutableArray();

        // Ogni campo ha bisogno di un proprio simbolo in SymbolTable (non quello condiviso
        // della classe generica) — stessa ragione di ResolveGenericUdtInstantiation:
        // InferFieldAccessType legge il tipo del campo da lì, non dall'AST.
        var specializedMembers = genericClass.Members.Select(member => member switch
        {
            FieldMember fieldMember => SpecializeGenericClassField(fieldMember, substitutionMap, program),
            MethodMember methodMember => methodMember with
            {
                Procedure = methodMember.Procedure switch
                {
                    FunctionDefinition fd => fd with
                    {
                        Parameters = SpecializeParameters(fd.Parameters),
                        ReturnType = SubstituteTypeParameters(fd.ReturnType, substitutionMap)
                    },
                    _ => methodMember.Procedure with { Parameters = SpecializeParameters(methodMember.Procedure.Parameters) }
                }
            },
            ConstructorMember ctorMember => ctorMember with
            {
                Constructor = ctorMember.Constructor with
                {
                    Parameters = SpecializeParameters(ctorMember.Constructor.Parameters)
                }
            },
            _ => member // inammissibile: già filtrato sopra da unsupportedMember
        }).ToImmutableArray();

        var specialized = genericClass with
        {
            SymbolId = -1,
            Name = mangledNameToken,
            Members = specializedMembers,
            TypeParameters = ImmutableArray<TypeParameter>.Empty
        };
        specialized = specialized with { OriginalName = mangledNameToken };

        // Ri-ancora Parent (sia sul ClassMember sia sulla ProcedureDefinition/ConstructorDefinition
        // incapsulata): i membri clonati puntano ancora, via Parent, alla CLASS generica originale
        // (impostato dal Parser in FASE 1) — senza questo fixup SELF nei corpi risolverebbe i campi
        // contro la classe generica invece che contro questa specializzazione. Stessa causa già
        // vista e corretta per i TYPE generici.
        foreach (var member in specialized.Members)
        {
            member.Parent = specialized;
            switch (member)
            {
                case MethodMember mm: mm.Procedure.Parent = specialized; break;
                case ConstructorMember cm: cm.Constructor.Parent = specialized; break;
            }
        }

        // Riscrive i corpi di Method/Constructor per QUESTA specializzazione — il template
        // generico non viene mai riscritto (RewriteClassDefinition lo salta esplicitamente,
        // è condiviso da tutte le istanziazioni), quindi il corpo clonato sopra referenzia
        // ancora, per SymbolId statico, i campi/SELF del template. L'Executor se la cava lo
        // stesso (risolve SELF.Campo per DataType dinamico a runtime), ma il backend IL
        // risolve per SymbolId statico — senza questo passo la compilazione fallisce a
        // valle su un SymbolId di SELF mai risolto. Stesso meccanismo già in uso per
        // Operators/Constructors/Methods di una CLASS/UDT non generica
        // (Linker.cs, ResolveAndRewriteModuleAsts) — RewriteStatementList con
        // CurrentClass impostato risolve i FieldTarget per nome contro
        // context.CurrentClass.Members, qui puntato sulla ClassDefinition monomorfizzata.
        var rewriteContext = new AstRewriteContext(modulePath, null, new DbfContext(), false, false, null)
        {
            CurrentClass = specialized,
            SymbolMap = paramSymbolMap.Count > 0 ? paramSymbolMap : null,
            GenericTypeArgs = genericClass.TypeParameters
                .Zip(resolvedArgs, (tp, dt) => new KeyValuePair<string, DataType>(tp.Name.Lexeme, dt))
                .ToDictionary(kv => kv.Key, kv => kv.Value, StringComparer.OrdinalIgnoreCase)
        };
        specialized = specialized with
        {
            Members = specialized.Members.Select(member => member switch
            {
                MethodMember mm => mm with
                {
                    Procedure = mm.Procedure with
                    {
                        Body = RewriteStatementList(mm.Procedure.Body, rewriteContext, null, program).Rewritten
                    }
                },
                ConstructorMember cm => cm with
                {
                    Constructor = cm.Constructor with
                    {
                        Body = RewriteStatementList(cm.Constructor.Body, rewriteContext, null, program).Rewritten
                    }
                },
                _ => member
            }).ToImmutableArray()
        };
        // Ri-ancora Parent di nuovo: i `with` sopra copiano Method/Constructor come nuovi
        // oggetti (stesso Parent dell'originale per costruzione .NET, già specialized —
        // ma il nuovo `specialized` esterno è un'istanza DIVERSA dal precedente dopo il
        // `with` sulla lista Members, quindi il fixup va ripetuto contro QUESTA istanza).
        foreach (var member in specialized.Members)
        {
            member.Parent = specialized;
            switch (member)
            {
                case MethodMember mm: mm.Procedure.Parent = specialized; break;
                case ConstructorMember cm: cm.Constructor.Parent = specialized; break;
            }
        }

        var specId = program.Symbols.RegisterSymbol(mangledNameToken, genericClass.Flags, specialized, specialized);
        specialized = specialized with { SymbolId = specId };
        program.Symbols.UpdateSymbolDeclaration(specId, specialized);

        program.GenericClassInstantiations[mangledName] = specialized;
        return specialized;
    }

    /// <summary>
    /// Specializza un singolo <see cref="FieldMember"/> di una CLASS generica, registrando un
    /// SymbolId di campo dedicato alla specializzazione (vedi commento in
    /// <see cref="ResolveGenericClassInstantiation"/>).
    /// </summary>
    private static FieldMember SpecializeGenericClassField(
        FieldMember fieldMember, ImmutableDictionary<TypeParameter, DataType> substitution, BasicProgram program)
    {
        var declaration = fieldMember.Declaration;
        var dataType = declaration.DataType ?? throw new InvalidOperationException("Field declaration has null DataType");
        var substitutedType = SubstituteTypeParameters(dataType, substitution);
        // Item 15 (audit rewrite non uniforme): un campo come `DIM Items AS T = NEW T()`
        // manteneva T non sostituito nell'InitialValue — solo il DataType del campo veniva
        // specializzato. Dimensions/LowerBounds non necessitano di sostituzione (sono
        // espressioni numeriche di dimensione array, non riferimenti a tipo).
        var substitutedInitialValue = declaration.InitialValue != null
            ? SubstituteTypeParametersInExpr(declaration.InitialValue, substitution)
            : null;
        var newFieldId = program.Symbols.RegisterSymbol(
            declaration.NameCleaned, SymbolFlags.IsField | SymbolFlags.IsPublic, substitutedType, declaration);
        var newDeclaration = declaration with
        {
            SymbolId = newFieldId, DataType = substitutedType, InitialValue = substitutedInitialValue
        };
        program.Symbols.UpdateSymbolDeclaration(newFieldId, newDeclaration);
        return fieldMember with { Declaration = newDeclaration };
    }

    /// <summary>
    /// Sostituisce i type parameters generici nei pochi punti in cui un'espressione porta
    /// direttamente un DataType (NEW T(...), CAST AS T): usato per gli initializer dei campi
    /// di una CLASS generica specializzata (item 15) — non è un rewrite generico completo
    /// dell'espressione, solo la sostituzione del tipo nei nodi che lo imbarcano.
    /// </summary>
    private static Expr SubstituteTypeParametersInExpr(
        Expr expr, ImmutableDictionary<TypeParameter, DataType> substitution)
    {
        return expr switch
        {
            NewExpr ne => ne with
            {
                ParsedType = SubstituteTypeParameters(ne.ParsedType, substitution),
                ConstructorArgs = ne.ConstructorArgs?.Select(a => SubstituteTypeParametersInExpr(a, substitution))
                    .ToImmutableArray(),
                InitialValues = ne.InitialValues?.Select(a => SubstituteTypeParametersInExpr(a, substitution))
                    .ToImmutableArray()
            },
            SafeCastExpr sce => sce with
            {
                Expression = SubstituteTypeParametersInExpr(sce.Expression, substitution),
                TargetType = SubstituteTypeParameters(sce.TargetType, substitution)
            },
            _ => expr
        };
    }

    /// <summary>
    /// Sostituisce i type parameters nella firma (parametri + return type, se presente) di una procedura
    /// membro di un TYPE generico. Il corpo non viene riscritto: per i pattern comuni (es. `GETVALUE = SELF.Campo`)
    /// il tipo corretto viene comunque risolto a runtime tramite il tipo concreto di SELF.
    /// </summary>
    private static ProcedureDefinition SubstituteInProcedureSignature(
        ProcedureDefinition proc, ImmutableDictionary<TypeParameter, DataType> substitution)
    {
        var newParameters = proc.Parameters
            .Select(p => p with { DataType = SubstituteTypeParameters(p.DataType, substitution) })
            .ToImmutableArray();

        return proc switch
        {
            FunctionDefinition fd => fd with
            {
                Parameters = newParameters,
                ReturnType = SubstituteTypeParameters(fd.ReturnType, substitution)
            },
            _ => proc with { Parameters = newParameters }
        };
    }

    /// <summary>
    /// Verifica che un tipo concreto soddisfi un vincolo di tipo.
    /// Per ora, permissivo: assume che tutti i tipi primitivi soddisfino IComparable.
    /// </summary>
    private static bool SatisfiesConstraint(DataType concreteType, DataType constraint)
    {
        // Per ora, permissivo — in futuro verificare che il tipo implementi l'interfaccia.
        return true;
    }

    /// <summary>
    /// Trova il DeclarationStmt originale nell'AST del modulo e lo aggiorna con il tipo risolto.
    /// Questo è necessario perché il Linker opera su una lista di Statement (che deve essere modificata in-place
    /// per la persistenza del tipo risolto).
    /// </summary>
    private static void UpdateDeclarationStatementInModule(BasicProgram program, Token originalNameToken,
        DataType newType)
    {
        const bool log = false;
        Log(
            $"[UPDATE-DECL] Inizio ricerca per il DeclarationStmt di '{originalNameToken.Lexeme}' con nuovo tipo: {newType}",
            log);

        // Ricerca in tutti i moduli
        foreach (var module in program.LoadedModules.Values)
        {
            // Ricerca in tutte le liste di statements per quel modulo (righe numerate)
            foreach (var statementsList in module.Statements.Values)
            {
                for (var i = 0; i < statementsList.Count; i++)
                {
                    if (statementsList[i] is not DeclarationStmt declarationStmt) continue;

                    var declarations = declarationStmt.Declarations.ToList();
                    var declIndex = declarations.FindIndex(d => d.NameCleaned.Equals(originalNameToken));

                    if (declIndex == -1) continue;

                    // Trovato!
                    Log(
                        $"[UPDATE-DECL] Trovata dichiarazione di '{originalNameToken.Lexeme}' nel modulo '{Path.GetFileName(module.FilePath)}' alla linea {declarationStmt.Line}.",
                        log);

                    // 1. Clona e modifica la dichiarazione specifica
                    var oldDecl = declarations[declIndex];
                    var newDecl = oldDecl with { DataType = newType };
                    declarations[declIndex] = newDecl;

                    // 2. Crea un nuovo DeclarationStmt con l'array immutabile corretto
                    var newDeclarationStmt = declarationStmt with { Declarations = [..declarations] };

                    // 3. Sostituisce il Statement nella lista in-place
                    statementsList[i] = newDeclarationStmt;
                    Log($"[UPDATE-DECL] Sostituito Statement indice {i} per la persistenza del tipo.", log);
                    return; // Trovato e aggiornato, saiamo.
                }
            }
        }

        Log(
            $"[UPDATE-DECL] Fine ricerca. Il DeclarationStmt per '{originalNameToken.Lexeme}' non è stato trovato o aggiornato.",
            log);
    }

    /// <summary>
    /// Inferisce o verifica il tipo di ritorno di una funzione definita dall'utente (UDF).
    /// Questo metodo è un componente chiave del loop di stabilizzazione del Linker.
    /// Analizza il corpo di una funzione per trovare tutte le istruzioni `RETURN`, inferisce
    /// il tipo di dato delle espressioni restituite e ne garantisce la coerenza.
    /// 
    /// Oltre all'inferenza standard per i tipi non dichiarati (`InferredType`), questo metodo
    /// implementa una logica di promozione intelligente ("Smart Promotion") per i puntatori a funzione:
    /// se una funzione è dichiarata esplicitamente come `AS INTEGER` o `AS LONG` (pratica comune
    /// nei dialetti BASIC storici per i callback), ma il corpo restituisce un riferimento a una
    /// procedura (`FunctionPointerType`), il Linker promuove il tipo di ritorno della definizione
    /// a `FunctionPointerType`. Questo permette al Rewriter di riconoscere e validare correttamente
    /// le chiamate indirette (es. `MyFuncPtr(arg)`), risolvendo l'ambiguità sintattica con gli array.
    /// 
    /// Se il tipo di ritorno viene risolto o promosso, aggiorna la SymbolTable e segnala al
    /// contesto di riscrittura che è avvenuta una modifica, innescando un'ulteriore passata
    /// del loop di stabilizzazione per propagare il nuovo tipo ai chiamanti.
    /// </summary>
    /// <param name="udf">La definizione della funzione (FunctionDefinition) da analizzare.</param>
    /// <param name="context">Il contesto di riscrittura corrente, usato per l'inferenza ricorsiva e per segnalare le modifiche.</param>
    /// <param name="program">L'istanza del programma che contiene la SymbolTable.</param>
    /// <returns>
    /// Il <see cref="DataType"/> risolto per la funzione. Se la funzione non ha istruzioni RETURN o se
    /// i tipi restituiti sono incoerenti o non ancora inferibili, restituisce <see cref="InferredType"/>.
    /// </returns>
    /// <exception cref="BasicException">Lanciata se una funzione contiene istruzioni RETURN con tipi di dato incoerenti (e non è in atto una promozione valida).</exception>
    private static DataType InferUdfReturnType(FunctionDefinition udf, AstRewriteContext context, BasicProgram program)
    {
        const bool log = false; // Log forzato per debug
        Log($"[INFER-UDF] Inizio inferenza per '{udf.OriginalName.Lexeme}'", log);

        // Trova tutte le istruzioni RETURN nel corpo della funzione.
        var returnStatements = udf.Body.OfType<ReturnStmt>().ToList();

        if (returnStatements.Count == 0)
        {
            Log($"[INFER-UDF]   -> Nessun RETURN. Uso tipo dichiarato o Inferred.", log);
            // Se il tipo è già esplicito, usalo. Altrimenti InferredType.
            return udf.ReturnType is not InferredType
                ? ResolveDataType(udf.ReturnType, program, context.ModulePath)
                : new InferredType();
        }

        DataType? inferredReturnType = null;

        // Analizza i tipi di tutti i RETURN
        foreach (var returnStmt in returnStatements)
        {
            if (returnStmt.Value == null) continue;

            var returnType = LinkerInferExpressionType(returnStmt.Value, context, program);
            Log($"[INFER-UDF]   -> RETURN trovato. Tipo inferito espressione: {returnType}", log);

            if (returnType is InferredType)
            {
                Log($"[INFER-UDF]   -> Tipo espressione non inferibile. Abort.", log);
                if (udf.ReturnType is not InferredType) return udf.ReturnType;
                return new InferredType();
            }

            if (inferredReturnType == null)
            {
                inferredReturnType = returnType;
            }
            else if (!inferredReturnType.Equals(returnType))
            {
                Log($"[INFER-UDF]   -> Mismatch tipi RETURN: {inferredReturnType} vs {returnType}", log);
                if (udf.ReturnType is not InferredType) return udf.ReturnType;
                throw new BasicException(returnStmt,
                    $"Tutte le istruzioni RETURN nella funzione '{udf.Name.Lexeme}' devono restituire lo stesso tipo di dato.",
                    ErrorCode.TypeMismatch);
            }
        }

        inferredReturnType ??= new InferredType();
        Log($"[INFER-UDF]   -> Tipo inferito complessivo dai RETURN: {inferredReturnType}", log);

        // Controllo e Promozione del Tipo
        var currentDeclaredType = ResolveDataType(udf.ReturnType, program, context.ModulePath);
        Log($"[INFER-UDF]   -> Tipo dichiarato attuale: {currentDeclaredType}", log);

        if (currentDeclaredType is not InferredType)
        {
            // Se il tipo dichiarato è Integer/Long ma il corpo restituisce un FunctionPointer,
            // promuoviamo il tipo della funzione a FunctionPointer.
            if ((currentDeclaredType is IntegerType || currentDeclaredType is LongType) &&
                inferredReturnType is FunctionPointerType)
            {
                Log(
                    $"[INFER-UDF]   -> PROMOZIONE ATTIVATA: Dichiarato {currentDeclaredType} -> Promosso a {inferredReturnType}.",
                    log);

                UpdateProcedureReturnTypeInSymbolTable(program, udf, inferredReturnType);
                context.OnTypeInferred?.Invoke();
                return inferredReturnType;
            }

            Log($"[INFER-UDF]   -> Nessuna promozione necessaria o possibile. Mantengo dichiarato.", log);
            return currentDeclaredType;
        }

        // Se il tipo non era dichiarato (InferredType), adottiamo quello inferito dai RETURN.
        if (inferredReturnType is not InferredType)
        {
            Log($"[INFER-UDF]   -> TIPO RISOLTO! Da InferredType a {inferredReturnType}. Segnalo cambiamento.", log);
            context.OnTypeInferred?.Invoke();
            UpdateProcedureReturnTypeInSymbolTable(program, udf, inferredReturnType);
        }

        return inferredReturnType;
    }

    /// <summary>
    /// Helper per aggiornare il tipo di ritorno di una procedura nella SymbolTable.
    /// QUESTA VERSIONE È ARCHITETTURALMENTE CORRETTA:
    /// 1. Utilizza il SymbolId della procedura per un accesso diretto O(1).
    /// 2. Modifica direttamente il nodo di dichiarazione del simbolo, che è la fonte di verità unica.
    /// </summary>
    private static void UpdateProcedureReturnTypeInSymbolTable(BasicProgram program, FunctionDefinition procToUpdate,
        DataType newReturnType)
    {
        // Fase 1: Ottieni l'ID univoco dalla definizione della procedura.
        var symbolId = procToUpdate.SymbolId;

        // Validazione di sicurezza: se l'ID non è valido, non fare nulla.
        if (symbolId < 0)
        {
            // Potrebbe accadere per procedure fittizie o non ancora completamente registrate.
            // In questo contesto, è un'operazione sicura ignorare la richiesta.
            return;
        }

        // Fase 2: Recupera il simbolo in O(1).
        var symbol = program.Symbols.GetSymbol(symbolId);

        // Fase 3: Esegui il cast e l'aggiornamento.
        // È fondamentale assicurarsi che il nodo di dichiarazione sia del tipo corretto.
        if (symbol.DeclarationNode is FunctionDefinition currentProcDef)
        {
            // Crea una nuova istanza della definizione della procedura con il tipo di ritorno aggiornato.
            var updatedProc = currentProcDef with { ReturnType = newReturnType };

            // Fase 4: Aggiorna il nodo di dichiarazione nella SymbolTable.
            // Questa è l'unica operazione di scrittura permessa sul simbolo.
            program.Symbols.UpdateSymbolDeclaration(symbolId, updatedProc);
        }
        else
        {
            // Questo indicherebbe un'incoerenza grave nel compilatore, dove un SymbolId
            // di una FunctionDefinition punta a un nodo di tipo diverso.
            throw new InvalidOperationException(
                $"Internal Compiler Error: Symbol ID '{symbolId}' for procedure '{procToUpdate.Name.Lexeme}' " +
                $"does not point to a FunctionDefinition node. Found '{symbol.DeclarationNode.GetType().Name}' instead.");
        }
    }

    /// <summary>
    /// Recupera l'istanza più aggiornata di una FunctionDefinition dalla SymbolTable.
    /// QUESTA VERSIONE È ARCHITETTURALMENTE CORRETTA:
    /// Utilizza il SymbolId per un accesso diretto O(1) alla fonte di verità.
    /// </summary>
    private static FunctionDefinition? GetCurrentProcedureDefinition(BasicProgram program,
        FunctionDefinition procToFind)
    {
        // Fase 1: Ottieni l'ID univoco dalla definizione della procedura.
        var symbolId = procToFind.SymbolId;

        // Validazione di sicurezza: se l'ID non è valido, il simbolo non esiste nella tabella.
        if (symbolId < 0)
        {
            return null;
        }

        // Fase 2: Accedi direttamente al simbolo in O(1).
        var symbol = program.Symbols.GetSymbol(symbolId);

        // Fase 3: Esegui il cast e restituisci il nodo di dichiarazione aggiornato.
        // Se il nodo non è del tipo atteso, restituisce null per indicare un'incoerenza.
        return symbol.DeclarationNode as FunctionDefinition;
    }

/// <summary>
    /// Helper per inferire il tipo di ritorno di funzioni polimorfiche generiche come ABS, FLOOR, etc.
    /// Questa versione è stata corretta per essere auto-sufficiente: recupera la definizione della
    /// procedura nativa dalla SymbolTable utilizzando le informazioni presenti nel nodo AST.
    /// VERSIONE CORRETTA: Supporta la specializzazione dei tipi per il namespace ARRAYS (es. ASORT)
    /// e per le funzioni di sistema (SCREENS, DIR, MMAPREADBYTES) risolvendo i fallimenti di inferenza.
    /// </summary>
    private static DataType InferGenericPolymorphicReturnType(BuiltinFunctionCallExpr bce,
        AstRewriteContext context, BasicProgram program)
    {
        var resolvedProc = program.GetProceduresForNamespace(bce.Namespace.Lexeme)
            .OfType<NativeProcedureDefinition>()
            .FirstOrDefault(p => p.BuiltinKind == bce.Name.Kind);

        if (resolvedProc == null) return new InferredType();

        // --- GESTIONE SPECIALE: POOL.GET ---
        // Ritorna il VALORE custodito nel pool (dinamico, object a runtime), mai il POOL stesso: senza,
        // il "Caso 1" generico sotto restituiva il tipo del primo argomento (POOL) e
        // `n = POOL.GET(p)` con `n AS LONG` falliva «Impossibile assegnare un valore di tipo 'POOL'».
        if (resolvedProc.BuiltinKind == TokenKind.PoolGet) return new AnyType();

        // --- GESTIONE SPECIALE: AMAP ---
        if (resolvedProc.BuiltinKind == TokenKind.AMap && bce.Args is [_, LambdaExpr lambdaMap, ..])
        {
            if (lambdaMap.ReturnType is not InferredType)
                return new ArrayType(lambdaMap.ReturnType);
            return new ArrayType(new InferredType());
        }

        // --- GESTIONE SPECIALE: AREDUCE ---
        if (resolvedProc.BuiltinKind == TokenKind.AReduce && bce.Args.Length >= 2)
        {
            if (bce.Args.Length >= 3)
            {
                var initialType = LinkerInferExpressionType(bce.Args[2], context, program);
                if (initialType is not InferredType) return initialType;
            }

            if (bce.Args[1] is LambdaExpr { ReturnType: not InferredType } lambdaRed) return lambdaRed.ReturnType;
            // Variabile DELEGATE (es. AREDUCE(arr, FunzioneSommaPari) — la lambda è stata
            // assegnata a una variabile): il tipo dell'accumulatore è il ReturnType del
            // FunctionPointerType della variabile. Senza, il default ArrayType per le
            // funzioni array rendeva `DIM x = AREDUCE(...)` un array → cast (double[]) su
            // uno scalare → AccessViolation (FunctionsLambdaClosures).
            if (bce.Args[1] is VariableExpr vRed && bce.Args.Length == 2)
            {
                var redSym = program.Symbols.GetSymbol(vRed.SymbolId).DataType;
                if (redSym is FunctionPointerType fptRed && fptRed.ReturnType is not InferredType)
                    return fptRed.ReturnType;
            }
            // Fallback: lambda NON tipizzata (ReturnType InferredType, es. `FN(acc,x)=>acc+x`)
            // o acc non risolto → l'accumulatore di AREDUCE è SCALARE (l'elemento dell'array),
            // MAI l'array stesso. Senza, il "Caso 1" generico sotto ritornava il tipo del primo
            // argomento (ARRAY OF DOUBLE) → `DIM r = AREDUCE(a, FN(acc,x)=>acc+x)` tipizzato
            // array (bug arrays5: cast double[] su uno scalare / box mancante → InvalidProgram).
            {
                var srcT = LinkerInferExpressionType(bce.Args[0], context, program);
                if (srcT is ArrayType atRed)
                    return atRed.ElementType is not InferredType ? atRed.ElementType : new DoubleType();
                return new DoubleType();
            }
        }

        // --- GESTIONE SPECIALE: AFLAT ---
        // A differenza di ASORT/AADD/ecc. (stessa forma del sorgente, gestito dal "Caso 2"
        // generico sotto: elementSource = inputType.ElementType, poi ArrayType(elementSource)),
        // AFLAT RIDUCE la nesting a UN SOLO livello (appiattisce array annidati di qualunque
        // profondità) — il "Caso 2" generico avrebbe ritornato lo STESSO tipo annidato del
        // sorgente (per un input jagged, elementSource è ancora un ArrayType), quindi
        // `DIM flat = AFLAT(nested)` restava erroneamente tipizzato come l'array jagged
        // originale invece del vero risultato piatto — bug reale: il cast IL a valle
        // (nel codegen) e AJOIN(flat, ...) a valle rifiutavano il "falso" array multi-rank
        // (ArraysFluent.bas). Srotola TUTTI i livelli ArrayType fino al tipo FOGLIA.
        if (resolvedProc.BuiltinKind == TokenKind.AFlat && bce.Args.Length > 0)
        {
            var flatInputType = LinkerInferExpressionType(bce.Args[0], context, program);
            var leafType = flatInputType;
            while (leafType is ArrayType flatAt) leafType = flatAt.ElementType;
            return new ArrayType(leafType is InferredType ? new DoubleType() : leafType);
        }

        // --- Funzioni di sistema con tipo di ritorno collezione fisso ---
        // Queste funzioni sono registrate come Inferred o generiche ma hanno un tipo di ritorno statico.
        switch (bce.Name.Kind)
        {
            case TokenKind.Screens:
                // Coerente con la VM (PCodeProcessor.Builtins.cs: SCREENS ritorna 1.0 — numero
                // di schermi). La vecchia regola ArrayType(DisplayModeType) disallineava il
                // linker alla VM (mai catturata: GraphicsShowModes è SDL2, SKIP) e l'IL
                // falliva con "ArrayType non mappato" su SCREENS().
                return new DoubleType();
            case TokenKind.MemoryMapReadBytes:
                return new ArrayType(new ByteType());
            case TokenKind.DirFunc:
                return new ArrayType(new StringType());
            // FIX: CONTRACT restituisce un proxy dinamico (ANY), non deve inferire dal primo argomento (indirizzo stringa).
            case TokenKind.Contract:
                return new AnyType();
        }

        var declaredReturnType = resolvedProc.ReturnType;

        switch (declaredReturnType)
        {
            // --- Caso 1: Funzioni che restituiscono un valore scalare basato sull'input (es. ABS) ---
            case InferredType when bce.Args.Length > 0:
            {
                // Len e Asc sono casi speciali che restituiscono sempre Long, indipendentemente dall'input.
                if (bce.Name.Kind is TokenKind.Len or TokenKind.Asc) return new LongType();
                // AVG/MIN/MAX/... con ParamArray (values()): ritornano il valore SCALARE
                // (media/min/max), MAI l'array — prima inferito come ARRAY OF <tipo> dal
                // primo argomento (un array), rompendo `DIM x = AVG(v)` (cast array→double).
                if (resolvedProc.Parameters.Any(p => p.Flags.HasFlag(SymbolFlags.IsParamArray)))
                {
                    var paramArgType = LinkerInferExpressionType(bce.Args[0], context, program);
                    return paramArgType is ArrayType pat ? (pat.ElementType is not InferredType ? pat.ElementType : new DoubleType())
                        : paramArgType is not InferredType ? paramArgType : new DoubleType();
                }
                
                var argumentType = LinkerInferExpressionType(bce.Args[0], context, program);
                return argumentType is not InferredType ? argumentType : declaredReturnType;
            }

            // --- Caso 2: Funzioni che restituiscono un array generico (es. ASORT, AADD, AINS) ---
            case ArrayType { ElementType: InferredType } when bce.Args.Length > 0:
            {
                var inputType = LinkerInferExpressionType(bce.Args[0], context, program);

                DataType? elementSource = inputType switch
                {
                    ArrayType at => at.ElementType,
                    ListType lt => lt.ElementType,
                    _ => null
                };

                if (elementSource != null && elementSource is not InferredType)
                {
                    return new ArrayType(elementSource);
                }

                break;
            }
        }

        return declaredReturnType;
    }

    /// <summary>
    /// Converte un Target in un'espressione equivalente.
    /// Funzione helper per ParseTarget, necessaria per le assegnazioni a catena.
    /// </summary>
    private static Expr ConvertTargetToExpr(Target target)
    {
        return target switch
        {
            VariableTarget vt => new VariableExpr(vt.SymbolId, vt.NameSuffixed, vt.NameCleaned),
            ArrayElementTarget aet => new ArrayAccessExpr(aet.SymbolId, aet.NameSuffixed, aet.NameCleaned, aet.Indices),
            FieldTarget ft when ft.Structure is null => new WithMemberAccessExpr(ft.OriginalToken, new Token(TokenKind.Dot, ".", null, ft.OriginalToken.Position)),
            FieldTarget ft => new FieldAccessExpr(ConvertTargetToExpr(ft.Structure!), ft.OriginalToken),
            IndexedTarget it => new IndexedAccessExpr(it.Base, it.Indices),
            _ => throw new InvalidOperationException("Conversione da Target a Expr non supportata per questo tipo.")
        };
    }

    /// <summary>
    /// [NUOVO HELPER] Cerca un simbolo non qualificato nei namespace resi visibili dalle direttive IMPORT.
    /// Gestisce sia gli import selettivi (`IMPORT NS.{Membro}`) che quelli wildcard (`IMPORT NS`).
    /// </summary>
    private static List<SymbolTable.Symbol> FindSymbolInImportedNamespaces(string name, AstRewriteContext context,
        BasicProgram program)
    {
        const bool log = false;
        var candidates = new List<SymbolTable.Symbol>();

        if (!program.LoadedModules.TryGetValue(context.ModulePath, out var currentModule))
        {
            Log(
                $"[REWRITE-SEARCH] ATTENZIONE: Impossibile trovare il modulo '{context.ModulePath}' per la ricerca negli import.",
                log);
            return candidates;
        }

        Log(
            $"[REWRITE-SEARCH] Inizio ricerca per '{name}' nei namespace importati dal modulo '{Path.GetFileName(context.ModulePath)}'.",
            log);

        // --- FASE 0: Ricerca Aggressiva Globale/Locale per Nome (Forward References) ---
        // FIX ROBUSTO: Invece di indovinare il namespace, cerchiamo direttamente il simbolo per nome
        // e filtriamo quelli pertinenti (definiti in questo modulo o globali).
        var symbolsByName = program.Symbols.FindSymbolsByName(name);

        // Rimossa verifica null: il metodo restituisce sempre una lista (eventualmente vuota).
        foreach (var symbol in symbolsByName)
        {
            // Un simbolo è candidato se:
            // 1. È definito nello stesso file (modulo) del chiamante (accesso privato/interno OK).
            // 2. È globale/pubblico e visibile (ma questo è gestito meglio dagli import dopo).
            // Qui ci concentriamo sui simboli LOCALI al modulo che potrebbero essere stati persi.

            var symbolFilePath = program.FindFilePathForSymbol(symbol.DeclarationNode);

            // Se il simbolo è definito nello stesso file, è sicuramente visibile e candidato.
            if (symbolFilePath != null && symbolFilePath.Equals(context.ModulePath, StringComparison.OrdinalIgnoreCase))
            {
                Log($"[REWRITE-SEARCH]     -> TROVATO (Stesso File)! Simbolo '{symbol.OriginalName}' (ID {symbol.Id}).",
                    log);
                candidates.Add(symbol);
            }
            // Se è nel namespace globale "_global" (e non è lo stesso file, ma è comunque globale)
            else
            {
                var nsName = program.Symbols.GetNamespaceNameForSymbol(symbol.Id);
                if (nsName == SymbolTable.GlobalScopeName)
                {
                    Log(
                        $"[REWRITE-SEARCH]     -> TROVATO (Global Scope)! Simbolo '{symbol.OriginalName}' (ID {symbol.Id}).",
                        log);
                    candidates.Add(symbol);
                }
            }
        }

        // --- FASE 1: Ricerca negli IMPORT SELETTIVI ---
        Log("[REWRITE-SEARCH]   -> Fase 1: Controllo import selettivi (IMPORT NS.{...})", log);
        foreach (var import in currentModule.Imports.Where(i => i.Mode == ImportMode.SpecificMembers))
        {
            var memberMatch = import.SpecificMembers.FirstOrDefault(m =>
                (m.Alias?.Lexeme ?? m.OriginalName.Lexeme).Equals(name, StringComparison.OrdinalIgnoreCase));
            if (memberMatch != null)
            {
                var originalNameToFind = memberMatch.OriginalName.Lexeme;
                Log(
                    $"[REWRITE-SEARCH]     -> Trovata corrispondenza per '{name}' (cercando '{originalNameToFind}') nell'import di '{import.NamespaceName.Lexeme}'.",
                    log);

                var nsSymbolIds = program.Symbols.GetSymbolIdsByNamespace(import.NamespaceName.Lexeme);
                if (nsSymbolIds != null)
                {
                    var foundSymbol = nsSymbolIds
                        .Select(id => program.Symbols.GetSymbol(id))
                        .FirstOrDefault(s =>
                            s.OriginalName.Equals(originalNameToFind, StringComparison.OrdinalIgnoreCase) &&
                            s.IsPublic);

                    if (foundSymbol != null)
                    {
                        Log(
                            $"[REWRITE-SEARCH]       -> TROVATO! Simbolo '{foundSymbol.OriginalName}' (ID {foundSymbol.Id}, Tipo: {foundSymbol.DeclarationNode.GetType().Name}). Aggiunto ai candidati.",
                            log);
                        candidates.Add(foundSymbol);
                    }
                }
            }
        }

        // --- FASE 2: Ricerca negli IMPORT WILDCARD e di DEFAULT ---
        Log("[REWRITE-SEARCH]   -> Fase 2: Controllo import wildcard (IMPORT NS) e di default.", log);
        var visibleNamespaces = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
        foreach (var ns in program.Settings.DefaultNamespaces) visibleNamespaces.Add(ns);
        foreach (var import in currentModule.Imports.Where(i => i.Mode == ImportMode.AllMembers))
        {
            visibleNamespaces.Add(import.NamespaceName.Lexeme);
        }

        Log($"[REWRITE-SEARCH]     -> Namespace visibili: [{string.Join(", ", visibleNamespaces)}]", log);
        foreach (var nsName in visibleNamespaces)
        {
            var nsSymbolIds = program.Symbols.GetSymbolIdsByNamespace(nsName);
            if (nsSymbolIds == null) continue;

            foreach (var id in nsSymbolIds)
            {
                var symbol = program.Symbols.GetSymbol(id);
                // Qui cerchiamo per nome esatto nel namespace, e deve essere PUBLIC.
                if (symbol.OriginalName.Equals(name, StringComparison.OrdinalIgnoreCase) && symbol.IsPublic)
                {
                    Log(
                        $"[REWRITE-SEARCH]       -> Trovato candidato '{symbol.OriginalName}' (ID {symbol.Id}) nel namespace wildcard '{nsName}'.",
                        log);
                    candidates.Add(symbol);
                }
            }
        }

        var distinctCandidates = candidates.Distinct().ToList();
        Log($"[REWRITE-SEARCH] Ricerca completata per '{name}'. Trovati {distinctCandidates.Count} candidati unici.",
            log);
        return distinctCandidates;
    }
    
    // Funzione helper locale per ottenere l'ID del simbolo da un target.
    // Necessaria per la logica di inferenza.
    private static int GetSymbolId(Target t)
    {
        while (true)
        {
            switch (t)
            {
                case VariableTarget vt: return vt.SymbolId;
                case ArrayElementTarget aet: return aet.SymbolId;
                case FieldTarget ft when ft.Structure is null:
                    return -1; // WITH block implicit member — no static symbol
                case FieldTarget ft:
                    t = ft.Structure!;
                    continue;
                case IndexedTarget it:
                    // Prova a convertire la base in un Target per la ricorsione
                    if (it.Base is VariableExpr || it.Base is ArrayAccessExpr || it.Base is FieldAccessExpr)
                    {
                        t = ExpressionToTarget(it.Base);
                        continue;
                    }
                    return -1;
                default: return -1;
            }
        }
    }

    // Funzione helper locale per ottenere il tipo risolto da un target.
    // Necessaria per la logica di inferenza.
    private static DataType? GetResolvedType(Target t) => t switch
    {
        VariableTarget vt => vt.ResolvedType,
        ArrayElementTarget aet => aet.ResolvedType,
        FieldTarget ft => ft.ResolvedType,
        IndexedTarget it => it.ResolvedType,
        _ => null
    };

    /// <summary>
    /// Helper per recuperare il rank (numero di dimensioni) di un simbolo dalla SymbolTable.
    /// </summary>
    private static int GetMatrixRankFromSymbol(int symbolId, BasicProgram program)
    {
        if (symbolId < 0) return 0; // Simbolo non risolto, non possiamo determinare il rank.
        var symbol = program.Symbols.GetSymbol(symbolId);

        // Il rank è definito nel nodo di dichiarazione originale.
        if (symbol.DeclarationNode is VariableDeclaration varDecl)
        {
            return varDecl.Dimensions.Length;
        }

        return 0; // Non è una variabile dichiarata come array (o non è una variabile).
    }

    /// <summary>
    /// Helper per recuperare il rank di un'espressione che rappresenta un array.
    /// </summary>
    private static int GetMatrixRankFromExpression(Expr expr, BasicProgram program)
    {
        return expr switch
        {
            // Se l'espressione è un riferimento diretto a una variabile/array...
            VariableExpr ve => GetMatrixRankFromSymbol(ve.SymbolId, program),
            ArrayAccessExpr aae => GetMatrixRankFromSymbol(aae.SymbolId, program),
            // Se è un literal, il rank è sempre 1.
            ArrayLiteralExpr => 1,
            ListLiteralExpr => 1,
            // Per altre espressioni (es. chiamata a funzione che restituisce array),
            // dovremmo inferire dal tipo di ritorno, ma per ora ci concentriamo sui casi base.
            _ => 0
        };
    }

    /// <summary>
    /// Helper per recuperare il rank di un Target, navigando la sua struttura.
    /// </summary>
    private static int GetMatrixRankFromTarget(Target target, BasicProgram program)
    {
        return target switch
        {
            VariableTarget vt => GetMatrixRankFromSymbol(vt.SymbolId, program),
            // Un ArrayElementTarget non è un array, è un elemento, quindi il suo rank è 0.
            ArrayElementTarget => 0,
            FieldTarget ft => GetMatrixRankFromExpression(ConvertTargetToExpr(ft), program),
            _ => 0
        };
    }
    
}