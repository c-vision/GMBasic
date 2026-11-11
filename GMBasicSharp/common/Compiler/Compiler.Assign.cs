// FILE: Interpreter.Compiler.Compiler.Assign.cs



namespace Compiler;

/// <summary>
/// Contiene i metodi di visita per le istruzioni "core" del linguaggio:
/// flusso di controllo, gestione scope, assegnazioni, dichiarazioni.
/// </summary>
public sealed partial class Compiler
{
    /// <summary>
    /// Emette il P-Code per un'assegnazione, rispettando pienamente le regole di
    /// scope definite nella guida di riferimento finale.
    /// </summary>
    private void AssignVariable(Target target, bool isLet, Expr? valueExpr = null)
    {
        if (target.SourceLineIndex >= 0) _currentSourceLineIndex = target.SourceLineIndex;

        switch (target)
        {
            case VariableTarget vt:
            {
                var name = vt.NameCleaned.Lexeme;

                // SCOPE DINAMICO (DYNAMIC): la scrittura va instradata sullo scope dinamico per
                // NOME (colpisce il frame di chiamata più interno che lo definisce). Vedi
                // Compiler.Scope.Dynamic.cs. Prima di locale/globale, come per la lettura.
                if (ShouldUseDynamicScope(NormalizeIdentifierName(name)))
                {
                    EmitDynamicStore(name);
                    return;
                }

                // ResolveUpvalue disabilitato — necessita SymbolId-based lookup

                if (isLet)
                {
                    var upvalueIdx = ResolveUpvalueByName(name);
                    if (upvalueIdx != -1)
                    {
                        EmitBytes((byte)OpCode.OP_STORE_UPVALUE, (byte)upvalueIdx);
                        return;
                    }
                    var localIndex = ResolveLocal(name);
                    if (localIndex != -1)
                    {
                        // Una riassegnazione rimette "vivo" il local: se era già stato
                        // finalizzato esplicitamente da uno USING precedente sullo stesso slot
                        // (BUG-8, _locals[i].AlreadyFinalized), il NUOVO valore assegnato qui
                        // NON lo è — senza questo reset, EndScope/EmitFinalizeBodyLocals
                        // salterebbero la finalizzazione del nuovo valore a fine scope/SUB.
                        _locals[localIndex].AlreadyFinalized = false;
                        EmitAdjustStringForLocal(localIndex);
                        EmitByte((byte)OpCode.OP_STORE_LOCAL); EmitShort((ushort)localIndex);
                    }
                    else if (_staticVars.TryGetValue(name, out var staticId))
                    {
                        EmitByte((byte)OpCode.OP_STORE_STATIC_FIELD);
                        EmitShort((ushort)staticId);
                    }
                    else
                    {
                        AddImplicitLocal(vt.NameCleaned, vt.NameSuffixed);
                        EmitByte((byte)OpCode.OP_STORE_LOCAL); EmitShort((ushort)(_locals.Count - 1));
                    }
                }
                else
                {
                    var upvalueIdx = ResolveUpvalueByName(name);
                    if (upvalueIdx != -1)
                    {
                        EmitBytes((byte)OpCode.OP_STORE_UPVALUE, (byte)upvalueIdx);
                        return;
                    }
                    var localIndex = ResolveLocal(name);
                    if (localIndex != -1)
                    {
                        // Una riassegnazione rimette "vivo" il local: se era già stato
                        // finalizzato esplicitamente da uno USING precedente sullo stesso slot
                        // (BUG-8, _locals[i].AlreadyFinalized), il NUOVO valore assegnato qui
                        // NON lo è — senza questo reset, EndScope/EmitFinalizeBodyLocals
                        // salterebbero la finalizzazione del nuovo valore a fine scope/SUB.
                        _locals[localIndex].AlreadyFinalized = false;
                        EmitAdjustStringForLocal(localIndex);
                        EmitByte((byte)OpCode.OP_STORE_LOCAL); EmitShort((ushort)localIndex);
                    }
                    else if (_staticVars.TryGetValue(name, out var staticId))
                    {
                        EmitByte((byte)OpCode.OP_STORE_STATIC_FIELD);
                        EmitShort((ushort)staticId);
                    }
                    else if (TryResolveOrRegisterGlobal(name, out var globalIndex))
                    {
                        // BUG-7: una riassegnazione (`g = NEW X()`, non `DIM g = ...`, che passa
                        // dal ramo OP_DEFINE_GLOBAL sopra) finalizza (DESTRUCTOR) la VECCHIA
                        // istanza del global PRIMA di sovrascriverla con quella nuova — prima
                        // d'ora la vecchia istanza restava "viva" per sempre (mai finalizzata,
                        // nemmeno a fine programma, perché il global ormai punta solo alla
                        // nuova). No-op innocuo se il global non contiene un'istanza con
                        // DESTRUCTOR (FinalizeInstance esce subito per valori non-dict).
                        EmitByte((byte)OpCode.OP_FINALIZE_GLOBAL);
                        EmitShort(globalIndex);

                        EmitCoerceDateForGlobal(globalIndex, valueExpr);
                        EmitAdjustStringForGlobal(globalIndex);
                        EmitByte((byte)OpCode.OP_STORE_GLOBAL);
                        EmitShort(globalIndex);
                    }
                    else
                    {
                        // Se non trovata come locale/globale, verifica se è un campo
                        // del contesto di closure (__context / __ClosureCtx_*)
                        int? ctxLocalIdx = null;
                        for (int ci = 0; ci < _locals.Count; ci++)
                        {
                            var l = _locals[ci];
                            if (l.Name.Lexeme.StartsWith("__context") || l.Name.Lexeme.StartsWith("__ClosureCtx"))
                            { ctxLocalIdx = ci; break; }
                        }
                        if (ctxLocalIdx.HasValue)
                        {
                            // Stack: [..., value]. Push context on top: [..., value, context]
                            EmitByte((byte)OpCode.OP_LOAD_LOCAL); EmitShort((ushort)ctxLocalIdx.Value);
                            // La LETTURA dello stesso campo (RewriteExpression del linker per
                            // PerformHeapPromotion produce un FieldAccessExpr esplicito, letto
                            // via LoadVariable/AssignField) usa OP_STORE_MEMBER con l'offset da
                            // _fieldOffsets — MAI OP_STORE_FIELD con il nome. Le due chiavi
                            // sono DIVERSE: un incremento scritto qui con STORE_FIELD-by-name
                            // era invisibile alla lettura successiva via LOAD_MEMBER-by-offset.
                            // Serve lo STESSO meccanismo offset-based.
                            var ctxTypeName = _locals[ctxLocalIdx.Value].Type is ClassType ctxCt
                                ? ctxCt.Definition.Name.Lexeme
                                : _locals[ctxLocalIdx.Value].Name.Lexeme;
                            if (_fieldOffsets.TryGetValue($"{ctxTypeName}.{name}", out var ctxOffset))
                            {
                                EmitByte((byte)OpCode.OP_STORE_MEMBER);
                                EmitShort(ctxOffset);
                            }
                            else
                            {
                                var fnIdx = AddConstant(name);
                                EmitByte((byte)OpCode.OP_STORE_FIELD);
                                EmitShort(fnIdx);
                            }
                        }
                        else if (_scopeDepth > 0)
                        {
                            AddImplicitLocal(vt.NameCleaned, vt.NameSuffixed);
                            EmitByte((byte)OpCode.OP_STORE_LOCAL); EmitShort((ushort)(_locals.Count - 1));
                        }
                        else
                        {
                            var newGlobalIndex = DefineGlobal(name);
                            EmitAdjustStringForGlobal(newGlobalIndex);
                            EmitByte((byte)OpCode.OP_STORE_GLOBAL);
                            EmitShort(newGlobalIndex);
                        }
                    }
                }
                break;
            }

            case ArrayElementTarget aet:
                if (aet.Indices.IsEmpty)
                    AssignVariable(new VariableTarget(-1, aet.NameSuffixed, aet.NameCleaned), isLet);
                else
                {
                    LoadVariable(new VariableTarget(-1, aet.NameSuffixed, aet.NameCleaned));
                    foreach (var index in aet.Indices)
                        VisitExpression(index);
                    EmitBytes((byte)OpCode.OP_STORE_ELEMENT, (byte)aet.Indices.Length);
                }
                break;

            case FieldTarget ft: AssignField(ft); break;

            case IndexedTarget it:
                VisitExpression(it.Base);
                foreach (var index in it.Indices)
                    VisitExpression(index);
                EmitBytes((byte)OpCode.OP_STORE_ELEMENT, (byte)it.Indices.Length);
                break;

            default:
                throw new NotImplementedException($"Assignment to target '{target.GetType().Name}' not implemented.");
        }
    }

    /// <summary>
    /// Compila un'istruzione di dichiarazione (DIM, LOCAL, GLOBAL, STATIC),
    /// garantendo l'assegnazione esplicita e corretta del valore iniziale
    /// e utilizzando nomi di variabili normalizzati per le ricerche.
    /// </summary>
    /// <summary>Emette l'inizializzatore di un DIM/global rispettando il tipo di COLLEZIONE
    /// dichiarato (es. `DIM H AS HEAP<INTEGER> = [4,1,7,3]` / `DIM hs AS HASHSET&lt;INTEGER&gt;
    /// = [3,1,4,1,5]`): OP_NEW col typeCode della collezione + OP_INIT_ARRAY per popolare
    /// (che nella VM dispatcha correttamente su HashSet.Add/Stack.Push/ecc. secondo il tipo
    /// REALE dell'istanza). Un `VisitExpression` diretto del letterale `[...]` (il fallback
    /// generico) chiama SEMPRE VisitListLiteralExpression, che emette INCONDIZIONATAMENTE
    /// OP_MK_LIST — una LIST generica, ignorando il tipo dichiarato: `DIM hs AS
    /// HASHSET&lt;INTEGER&gt; = [3,1,4,1,5]` diventava una List (niente dedup, `[3,1,4,1,5]`
    /// invece di un vero set) e `DIM st AS STACK&lt;STRING&gt; = [...]` restava in ordine di
    /// inserimento invece di LIFO (bug reale: ArraysPrintFormat.bas, PCodeVM). Condivisa da
    /// VisitDeclarationStatement (DIM locali/globali nel corpo) e CompileGlobalInitializers
    /// (globali con inizializzatore, hoisted PRIMA del corpo — path SEPARATO che prima
    /// chiamava VisitExpression direttamente, senza questa logica).</summary>
    private void EmitCollectionAwareInitializer(DataType finalVariableType, Expr initialValue)
    {
        // ARRAY (DIM d() = [...] / LET su array): crea un ARRAY NATIVO via CREATE_ARRAY
        // (parità IL, che ha T[]). Prima l'inizializzatore era emesso come lista letterale:
        // il runtime non aveva alcun marker di array → TYPENAME "LIST`1" (VM/C) invece di
        // "ARRAY". CREATE_ARRAY(0) = array dinamico (ndims=0, UBOUND per conteggio).
        if (finalVariableType is ArrayType)
        {
            if (initialValue is ArrayLiteralExpr or ListLiteralExpr)
            {
                var elems = initialValue switch
                {
                    ArrayLiteralExpr a => a.Elements,
                    ListLiteralExpr l => l.Elements,
                    _ => System.Collections.Immutable.ImmutableArray<Expr>.Empty
                };
                EmitConstant((double)(elems.Length - 1));
                EmitByte((byte)OpCode.OP_CALL_BUILTIN);
                EmitShort((ushort)BuiltInFunction.CREATE_ARRAY);
                EmitByte((byte)1);
                foreach (var el in elems) VisitExpression(el);
                EmitByte((byte)OpCode.OP_INIT_ARRAY);
                EmitShort((ushort)elems.Length);
                return;
            }
            VisitExpression(initialValue);
            return;
        }
        // Inizializzatore su collezione tipizzata: il tipo è nella AS della DIM
        // (es. `DIM H AS HEAP<INTEGER> = [4,1,7,3]`) → OP_NEW con il typeCode della
        // collezione + OP_INIT_ARRAY per popolare. PRIMA solo HashSet/Queue/Stack
        // erano gestiti; le altre collezioni (HEAP/BAG/BITREE/TIMELINE/...) cadevano
        // sul fallback che compilava `[...]` come ARRAY NATIVO — il tipo dichiarato
        // veniva ignorato (DIM H AS HEAP = [4,1,7,3] creava un List/array, non un Heap).
        if (IsLiteralInitializableCollection(finalVariableType))
        {
            // Se l'inizializzatore è già un NewExpr (`DIM x AS T = NEW T(...)`), il
            // NewExpr emette GIÀ il suo OP_NEW (con gli eventuali argomenti costruttore):
            // NON emettere qui un OP_NEW preliminare, altrimenti il bytecode contiene
            // DUE OP_NEW consecutivi e la prima istanza resta orfana sullo stack
            // (bug scoperto 2026-08-18 verificando i costruttori built-in: il disasm
            // mostrava OP_NEW(20),OP_NEW(20) per NEW RINGBUFFER(3)).
            if (initialValue is NewExpr)
            {
                VisitExpression(initialValue);
                return;
            }
            // Inizializzatore NON letterale (comprehension, chiamata che ritorna una
            // collezione): il valore prodotto da VisitExpression È GIÀ la collezione
            // risultante — emettere qui un OP_NEW preliminare crea una collezione
            // ORFANA sullo stack (e il DIM finiva per ricevere quella vuota: la set
            // comprehension DIM s AS HASHSET = {x FOR x IN c} dava s=[] su VM/C,
            // ArraysComprehensionFull.bas). Stesso motivo del ramo NewExpr sopra.
            if (initialValue is not ArrayLiteralExpr and not ListLiteralExpr and not DictionaryLiteralExpr)
            {
                VisitExpression(initialValue);
                return;
            }
            var tc = finalVariableType switch
            {
                ListType => 1, DictType => 2, HashSetType => 3, QueueType => 4,
                StackType => 5, SortedDictType => 6, TreeType => 7, AVLTreeType => 8,
                RedBlackTreeType => 9, BinaryIndexedTreeType => 10, TrieType => 11,
                LinkedListType => 12, DoublyLinkedListType => 12, CircularLinkedListType => 13, BinaryHeapType { IsMaxHeap: true } => 27, BinaryHeapType => 14,
                BagType => 15, CacheType => 16, GraphType => 17, TimelineType => 18,
                HashRingType => 19, RingBufferType => 20, SparseSetType => 21, BPlusTreeType => 26,
                HyperLogLogType => 30,   // 28 = STRINGBUILDER, 29 = REGEX in PCode/VM-C
                _ => 1
            };
            EmitConstant(0.0); // 0 argomenti costruttore (OP_NEW legge count+args)
            EmitByte((byte)OpCode.OP_NEW);
            EmitShort(tc);
            // Collect initial values from the literal
            var initElements = initialValue switch
            {
                ArrayLiteralExpr ale => ale.Elements,
                ListLiteralExpr lle => lle.Elements,
                _ => ImmutableArray<Expr>.Empty
            };
            // REDBLACK/TREE/AVL con dict-init { k:v, ... }: DictionaryLiteralExpr —
            // emette OP_NEW + MK_DICT (la VM popola il tipo K/V). Senza, il dict
            // init su RedBlackTree creava un Dictionary generico (ToArray fallisce).
            if (initialValue is DictionaryLiteralExpr dictInit &&
                finalVariableType is RedBlackTreeType or TreeType or AVLTreeType or SortedDictType)
            {
                // DICT-INIT { k:v, ... } su collezione K/V: popola l'istanza creata
                // da OP_NEW tramite MK_DICT (la VM scrive nella collezione in cima
                // allo stack). initElements resta vuoto → il blocco sotto NON deve
                // ricompilare il dict (prima lo faceva: 2° MK_DICT inutile e
                // l'assegnazione finale prendeva il dict generico, non la collezione).
                foreach (var entry in dictInit.Entries)
                {
                    VisitExpression(entry.Key);
                    VisitExpression(entry.Value);
                }
                EmitByte((byte)OpCode.OP_MK_DICT);
                EmitShort(dictInit.Entries.Length);
            }
            else if (initElements.Any())
            {
                foreach (var el in initElements)
                    VisitExpression(el);
                EmitByte((byte)OpCode.OP_INIT_ARRAY);
                EmitShort((ushort)initElements.Length);
            }
            else
                VisitExpression(initialValue);
        }
        else
            VisitExpression(initialValue);
    }

    /// <summary>True per i tipi COLLEZIONE (non array) il cui letterale `[...]` va costruito con
    /// OP_NEW del typeCode + OP_INIT_ARRAY invece che come LISTA generica (OP_MK_LIST). Unica
    /// sorgente del predicato: la usano il DIM/global-init (EmitCollectionAwareInitializer) e la
    /// RIASSEGNAZIONE fuori da DIM (`H = [9, 5, 2]`, VisitLetStatement).</summary>
    private static bool IsLiteralInitializableCollection(DataType t) =>
        t is HashSetType or QueueType or StackType or
            ListType or SortedDictType or TreeType or AVLTreeType or RedBlackTreeType or
            BinaryIndexedTreeType or TrieType or CircularLinkedListType or BinaryHeapType or
            BagType or CacheType or GraphType or TimelineType or HashRingType or RingBufferType or
            SparseSetType or BPlusTreeType or LinkedListType or DoublyLinkedListType;

    /// <summary>True per i tipi scalari NUMERICI (default 0.0): usato per il fill degli array
    /// numerici DIM senza inizializzatore (parità IL/C99, vedi VisitDeclarationStatement).
    /// STRING/UDT/classe/collezioni restano fuori (default null/dict, invariati).</summary>
    private static bool IsNumericScalarElement(DataType? dt) =>
        dt is SingleType or DoubleType or CurrencyType or BitType or ByteType or SByteType
           or IntegerType or UIntegerType or LongType or ULongType or QuadType or UQuadType
           or DateType or TimeType or DateTimeType or TimeSpanType;

    private void VisitDeclarationStatement(DeclarationStmt stmt)
    {
        foreach (var declaration in stmt.Declarations)
        {
            /* STATIC dentro una procedura (2026-11-11): lo storage non è lo slot di frame ma
             * l'area STATICA del runtime (la stessa dei campi statici di CLASS: chiave =
             * SymbolId, persistence fra le chiamate). Qui si apre il blocco "inizializza una
             * volta sola": se lo slot è ancora NIL il valore (default del tipo o
             * inizializzatore) viene valutato e memorizzato; altrimenti si salta — così un
             * secondo ingresso nella procedura NON riesegue l'inizializzatore.
             * Il resto della dichiarazione (calcolo del valore) è identico al DIM: il case
             * `TokenKind.Static` in fondo memorizza nello storage statico e chiude il salto. */
            int? staticInitSkipJump = null;
            if (stmt.Keyword.Kind == TokenKind.Static && _scopeDepth > 0)
            {
                EmitByte((byte)OpCode.OP_LOAD_STATIC_FIELD);
                EmitShort((ushort)declaration.SymbolId);
                EmitByte((byte)OpCode.OP_PUSH_NIL);
                EmitByte((byte)OpCode.OP_CMP_EQ);
                staticInitSkipJump = EmitJump((byte)OpCode.OP_JUMP_IF_FALSE);
                _staticVars[declaration.NameCleaned.Lexeme] = declaration.SymbolId;
            }

            var hasExplicitDimensions = declaration.Dimensions.Any();
            var hasInitialValue = declaration.InitialValue is not null;

            DataType finalVariableType;
            if (declaration.DataType is not null and not InferredType)
            {
                var baseType = declaration.DataType;
                if (hasExplicitDimensions || declaration.IsImplicitArray || baseType is ArrayType)
                    finalVariableType = baseType is ArrayType ? baseType : new ArrayType(baseType);
                else
                    finalVariableType = baseType;
            }
            else if (hasInitialValue)
            {
                finalVariableType = CompilerInferExpressionType(declaration.InitialValue!);
            }
            else
            {
                var baseElementType = DeduceTypeFromSuffix(declaration.NameSuffixed.Lexeme);
                if (hasExplicitDimensions || declaration.IsImplicitArray || baseElementType is ArrayType)
                {
                    finalVariableType = baseElementType is ArrayType ? baseElementType : new ArrayType(baseElementType);
                }
                else
                {
                    finalVariableType = baseElementType;
                }
            }

            if (finalVariableType is InferredType)
                finalVariableType = new AnyType();

            if (hasExplicitDimensions)
            {
                // For UDT arrays, push the class descriptor first so CREATE_ARRAY can init elements
                var extraArgs = 0;
                var toDims = 0;
                if (finalVariableType is ArrayType { ElementType: UserDefinedType udtElem } &&
                    _compiledClassDescriptors.TryGetValue(udtElem.Name.Lexeme + "#" + udtElem.SymbolId, out var udtDesc))
                {
                    extraArgs = 1;
                    var descIdx = AddConstant(udtDesc);
                    EmitConstant((double)descIdx);
                }
                // Push dimension count first so CREATE_ARRAY receives it as first arg
                EmitConstant((double)declaration.Dimensions.Length);
                for (int di = 0; di < declaration.Dimensions.Length; di++)
                {
                    // If lower bound exists (from 1 TO n syntax), push it first
                    if (di < declaration.LowerBounds.Length)
                    {
                        VisitExpression(declaration.LowerBounds[di]);
                        toDims++;
                    }
                    VisitExpression(declaration.Dimensions[di]);
                }
                EmitByte((byte)OpCode.OP_CALL_BUILTIN);
                EmitShort((ushort)BuiltInFunction.CREATE_ARRAY);
                // +1 for dim count prefix
                EmitByte((byte)(declaration.Dimensions.Length + extraArgs + toDims + 1));

                // Default dell'elemento per array NUMERICI: 0.0 (parità IL/C99: le loro
                // array nascono a 0; PCode/VM-C allocano `object?[]` → null, che si STAMPA
                // vuoto invece di "0" — visibile es. `FOR EACH row IN m` su una riga mai
                // assegnata). L'elemento STRING resta null (= "" in lettura/stampa) e l'UDT
                // ha il default dict. AFILL ora gestisce ogni rank (flat fill), quindi il
                // fill vale anche per le matrici N-D (prima solo 1-D).
                if (!hasInitialValue && declaration.Dimensions.Length >= 1
                    && finalVariableType is ArrayType { ElementType: var elemFillT }
                    && (IsNumericScalarElement(elemFillT) || elemFillT is AnyType))
                {
                    int fillTmp = _locals.Count;
                    AddLocal(new Token(TokenKind.Identifier, "__dimfill" + fillTmp, null, declaration.NameSuffixed.Position));
                    EmitByte((byte)OpCode.OP_STORE_LOCAL); EmitShort((ushort)fillTmp);
                    EmitByte((byte)OpCode.OP_PUSH_REF_LOCAL); EmitShort((ushort)fillTmp);
                    EmitConstant(0.0);                    // valore
                    EmitByte((byte)OpCode.OP_PUSH_NIL);   // start (default 0)
                    EmitByte((byte)OpCode.OP_PUSH_NIL);   // count (default = lunghezza)
                    EmitByte((byte)OpCode.OP_CALL_BUILTIN); EmitShort((ushort)BuiltInFunction.AFILL); EmitByte((byte)4);
                    EmitByte((byte)OpCode.OP_POP);
                    EmitByte((byte)OpCode.OP_LOAD_LOCAL); EmitShort((ushort)fillTmp);
                }

                if (hasInitialValue)
                {
                    // Inizializzatore letterale: `{..}` (ArrayLiteralExpr) oppure `[..]`
                    // (ListLiteralExpr — forma ammessa per gli array).
                    var literalElems = declaration.InitialValue switch
                    {
                        ArrayLiteralExpr ale => ale.Elements,
                        ListLiteralExpr lle => lle.Elements,
                        _ => throw new Exception(
                            $"Initializer for array '{declaration.NameSuffixed.Lexeme}' must be an array literal {{...}}."),
                    };

                    var declaredSize = -1;
                    if (declaration.Dimensions is [NumberExpr num]) declaredSize = (int)num.Value + 1;
                    if (declaredSize != -1 && literalElems.Length > declaredSize)
                        throw new Exception($"Too many initializers for array '{declaration.NameSuffixed.Lexeme}'.");

                    foreach (var element in literalElems) VisitExpression(element);
                    EmitByte((byte)OpCode.OP_INIT_ARRAY);
                    EmitShort(literalElems.Length);
                }
            }
            else if (hasInitialValue)
            {
                EmitCollectionAwareInitializer(finalVariableType, declaration.InitialValue!);

                // Vedi commento in VisitLetStatement: un UDT (value type) va sempre copiato per
                // valore quando assegnato a un target UserDefinedType, anche in un
                // inizializzatore di DIM (`DIM P2 AS T = P1`), non solo in un LET esplicito.
                if (finalVariableType is UserDefinedType)
                    EmitByte((byte)OpCode.OP_CLONE_UDT);
            }
            else
            {
                if (finalVariableType is ArrayType)
                {
                    EmitByte((byte)OpCode.OP_CALL_BUILTIN);
                    EmitShort((ushort)BuiltInFunction.CREATE_ARRAY);
                    EmitByte((byte)(0));
                }
                else if (finalVariableType is ListType or DictType or HashSetType or QueueType or StackType or
                    LinkedListType or SortedDictType or TreeType or AVLTreeType or RedBlackTreeType or
                    BinaryIndexedTreeType or TrieType or CircularLinkedListType or BinaryHeapType or
                    BagType or CacheType or GraphType or TimelineType or HashRingType or RingBufferType or
                    SparseSetType or BPlusTreeType or StringBuilderType or RegexType or HyperLogLogType)
                {
                    // DIM x AS LIST OF T (senza inizializzatore) cadeva nel fallback generico
                    // OP_PUSH_NIL sotto — x restava null invece di una collezione vuota. Un
                    // successivo x.Add(...) su null non lanciava (dispatch silenzioso), ma se lo
                    // stesso local slot era stato in precedenza occupato da un altro valore (es.
                    // il temp __arr di un FOR EACH su array precedente nello stesso scope), x.Add
                    // finiva per operare su QUELL'oggetto invece che su null — un vero array a
                    // dimensione fissa, "Collection was of a fixed size" (bug scoperto con
                    // ForEachComprehensive.bas: LIST OF Person dopo un FOR EACH su un array UDT).
                    // Stesso typeCode di OP_NEW/VisitNewExpression (Compiler.Expressions.cs).
                    var tc = finalVariableType switch
                    {
                        // Mappa COMPLETA dei typeCode (allineata a VisitNewExpression in
                        // Compiler.Expressions.cs). Prima: GraphType e le altre collezioni
                        // avanzate cadevano su `_ => 1` (creata come LIST vuota) invece del
                        // loro typeCode → DIM x AS GRAPH<STRING> (senza NEW) dava una List,
                        // non un Graph (metodi no-op / tipo sbagliato).
                        ListType => 1, DictType => 2, HashSetType => 3, QueueType => 4,
                        StackType => 5, SortedDictType => 6, TreeType => 7, AVLTreeType => 8,
                        RedBlackTreeType => 9, BinaryIndexedTreeType => 10, TrieType => 11,
                        LinkedListType => 12, DoublyLinkedListType => 12, CircularLinkedListType => 13, BinaryHeapType { IsMaxHeap: true } => 27, BinaryHeapType => 14,
                        BagType => 15, CacheType => 16, GraphType => 17, TimelineType => 18,
                        HashRingType => 19, RingBufferType => 20, SparseSetType => 21, BPlusTreeType => 26,
                HyperLogLogType => 30,   // 28 = STRINGBUILDER, 29 = REGEX in PCode/VM-C
                        StringBuilderType => 28,
                        RegexType => 29,
                        _ => 1
                    };
                    EmitConstant(0.0); // 0 argomenti costruttore (OP_NEW legge count+args)
                    EmitByte((byte)OpCode.OP_NEW);
                    EmitShort((ushort)tc);
                }
                else if (finalVariableType is Basic.WaitGroupType)
                {
                    EmitByte((byte)OpCode.OP_NEW_WAITGROUP);
                }
                else if (finalVariableType is Basic.MutexType)
                {
                    EmitByte((byte)OpCode.OP_NEW_MUTEX);
                }
                else if (finalVariableType is Basic.RwMutexType)
                {
                    EmitByte((byte)OpCode.OP_NEW_RWMUTEX);
                }
                else if (finalVariableType is Basic.OnceType)
                {
                    EmitByte((byte)OpCode.OP_NEW_ONCE);
                }
                else if (finalVariableType is Basic.AtomicType)
                {
                    EmitByte((byte)OpCode.OP_NEW_ATOMIC);
                }
                else if (finalVariableType is Basic.PoolType)
                {
                    EmitByte((byte)OpCode.OP_NEW_POOL);
                }
                else if (finalVariableType is Basic.CondType)
                {
                    EmitByte((byte)OpCode.OP_NEW_COND);
                }
                else if (finalVariableType is Basic.ConcurrentMapType)
                {
                    EmitByte((byte)OpCode.OP_NEW_CONCURRENTMAP);
                }
                else if (finalVariableType is Basic.ChannelType chanType)
                {
                    // Parità Go make(chan T, n): la capacity (opzionale, default 0 → runtime usa 10)
                    // è un operando u8 dopo OP_NEW_CHANNEL.
                    EmitByte((byte)OpCode.OP_NEW_CHANNEL);
                    EmitByte((byte)Math.Clamp(chanType.Capacity, 0, 255));
                }
                else
                {
                    // Push default value for numeric/date types
                    if (finalVariableType is SingleType or DoubleType or CurrencyType or IntegerType or LongType or ByteType or QuadType or BitType or DateType or TimeType or DateTimeType)
                        EmitConstant(0.0);
                    else if (finalVariableType is UserDefinedType udtType)
                    {
                        var key = udtType.Name.Lexeme + "#" + udtType.SymbolId;
                        if (_compiledClassDescriptors.TryGetValue(key, out var udtDesc))
                        {
                            var descIdx = AddConstant(udtDesc);
                            // Senza questa registrazione la VTable del descrittore resta con i
                            // nomi-metodo NON patchati (stringhe tipo "Box<DOUBLE>.GetValue"
                            // invece dell'indirizzo numerico) — OP_CALL_MEMBER su un'istanza
                            // creata da un semplice `DIM b AS Box<T>` (senza NEW/costruttore)
                            // non trova mai il metodo. VisitNewExpression registra sempre
                            // questo patching per le istanze create con NEW; questo ramo
                            // (DIM senza inizializzatore) se ne dimenticava.
                            _classDescriptorsToPatch.Add((descIdx, udtType));
                            EmitConstant(0.0);
                            EmitByte((byte)OpCode.OP_NEW_UDT_INSTANCE);
                            EmitShort(descIdx);
                        }
                        else
                        {
                            // Fallback: il SymbolId dell'USO del tipo (es. `DIM p AS Persona`
                            // dentro un blocco GOSUB, o classi dichiarate dopo il main con
                            // attributi) può differire dal SymbolId della DEFINIZIONE usato
                            // come chiave in _compiledClassDescriptors ("Nome#SymbolId").
                            // Cercare per NOME evita il silenzioso OP_PUSH_NIL che lascia
                            // l'istanza null e fa perdere ogni assegnazione di campo
                            // (p.Nome = "Mario" -> OP_STORE_MEMBER su nil, no-op).
                            Dictionary<string, object?>? byName = null;
                            foreach (var kv in _compiledClassDescriptors)
                            {
                                if (kv.Value != null &&
                                    kv.Value.TryGetValue("Name", out var nm) &&
                                    nm is string nmS &&
                                    string.Equals(nmS, udtType.Name.Lexeme, StringComparison.OrdinalIgnoreCase))
                                { byName = kv.Value; break; }
                            }
                            if (byName != null)
                            {
                                var descIdx2 = AddConstant(byName);
                                _classDescriptorsToPatch.Add((descIdx2, udtType));
                                EmitConstant(0.0);
                                EmitByte((byte)OpCode.OP_NEW_UDT_INSTANCE);
                                EmitShort(descIdx2);
                            }
                            else { EmitByte((byte)OpCode.OP_PUSH_NIL); }
                        }
                    }
                    else if (finalVariableType is ClassType clsType && clsType.Definition != null)
                    {
                        // DIM AS <CLASS>: stesso identico problema di UserDefinedType — il
                        // SymbolId dell'uso può differire dalla definizione, e prima questo
                        // ramo emetteva SEMPRE OP_PUSH_NIL (istanza null: ogni assegnazione
                        // di campo via OP_STORE_MEMBER era un no-op silenzioso). Cerca il
                        // descrittore per chiave esatta e poi per nome.
                        var clsKey = clsType.Definition.Name.Lexeme + "#" + clsType.Definition.SymbolId;
                        Dictionary<string, object?>? clsDesc = null;
                        if (_compiledClassDescriptors.TryGetValue(clsKey, out var cd0)) clsDesc = cd0;
                        else
                        {
                            foreach (var kv in _compiledClassDescriptors)
                            {
                                if (kv.Value != null &&
                                    kv.Value.TryGetValue("Name", out var nm2) &&
                                    nm2 is string nmS2 &&
                                    string.Equals(nmS2, clsType.Definition.Name.Lexeme, StringComparison.OrdinalIgnoreCase))
                                { clsDesc = kv.Value; break; }
                            }
                        }
                        if (clsDesc != null)
                        {
                            var clsDescIdx = AddConstant(clsDesc);
                            _classDescriptorsToPatch.Add((clsDescIdx, clsType.Definition));
                            EmitConstant(0.0);
                            EmitByte((byte)OpCode.OP_NEW_UDT_INSTANCE);
                            EmitShort(clsDescIdx);
                        }
                        else { EmitByte((byte)OpCode.OP_PUSH_NIL); }
                    }
                    else
                        EmitByte((byte)OpCode.OP_PUSH_NIL);
                }
            }

            switch (stmt.Keyword.Kind)
            {
                // SCOPE DINAMICO (DYNAMIC): il valore (inizializzatore o NIL) è già in cima allo
                // stack → dichiaralo nel frame di chiamata corrente. Vedi
                // Compiler.Scope.Dynamic.cs.
                case TokenKind.Dynamic:
                    EmitDynamicDeclare(declaration.NameCleaned.Lexeme);
                    break;

                case TokenKind.Global:
                case TokenKind.Dim when _scopeDepth == 0:
                    var globalIndex = DefineGlobal(declaration.NameCleaned.Lexeme, finalVariableType);
                    // Store in local slot first (if variable is also accessed as local)
                    var localIdx = ResolveLocal(declaration.NameCleaned.Lexeme);
                    if (localIdx != -1)
                    {
                        EmitByte((byte)OpCode.OP_DUP);
                        EmitByte((byte)OpCode.OP_STORE_LOCAL);
                        EmitShort((ushort)localIdx);
                    }
                    EmitByte((byte)OpCode.OP_DEFINE_GLOBAL);
                    EmitShort(globalIndex);
                    break;

                case TokenKind.Static when _scopeDepth == 0:
                    /* STATIC fuori da una procedura: non ha nulla da "persistere" oltre al
                     * programma stesso → è un GLOBALE. IL e C99Native lo accettano già così;
                     * prima PCode/VM-C lanciavano un'eccezione (divergenza trovata dalla
                     * verifica 2026-11-11). */
                    var staticGlobalIndex = DefineGlobal(declaration.NameCleaned.Lexeme, finalVariableType);
                    var staticLocalIdx = ResolveLocal(declaration.NameCleaned.Lexeme);
                    if (staticLocalIdx != -1)
                    {
                        EmitByte((byte)OpCode.OP_DUP);
                        EmitByte((byte)OpCode.OP_STORE_LOCAL);
                        EmitShort((ushort)staticLocalIdx);
                    }
                    EmitByte((byte)OpCode.OP_DEFINE_GLOBAL);
                    EmitShort(staticGlobalIndex);
                    break;

                case TokenKind.Static:
                    if (IsMacroScope)
                        throw new Exception("STATIC is not allowed inside EVAL/macro: eval scope is transient.");
                    /* Il valore (default del tipo o inizializzatore) è sullo stack: va nello
                     * STORAGE STATICO, non in uno slot di frame — è ciò che lo fa persistere
                     * fra le chiamate. Poi si chiude il salto del blocco "inizializza una volta"
                     * aperto in testa alla dichiarazione. */
                    EmitByte((byte)OpCode.OP_STORE_STATIC_FIELD);
                    EmitShort((ushort)declaration.SymbolId);
                    if (staticInitSkipJump is { } staticSkip)
                        PatchJump(staticSkip);
                    break;

                case TokenKind.Let:
                case TokenKind.Local:
                case TokenKind.Dim:
                    if (stmt.Keyword.Kind == TokenKind.Local && _scopeDepth == 0)
                        throw new Exception("LOCAL can only be used inside a SUB/FUNCTION.");

                    AddLocal(declaration.NameCleaned, finalVariableType);
                    
                    var localIndex = ResolveLocal(declaration.NameCleaned.Lexeme);

                    if (hasExplicitDimensions || hasInitialValue)
                    {
                        // Store the value (from CREATE_ARRAY or initializer) to the local slot
                        if (localIndex != -1)
                        {
                            EmitByte((byte)OpCode.OP_STORE_LOCAL); EmitShort((ushort)localIndex);
                        }
                        else
                        {
                            throw new Exception(
                                $"Internal Compiler Error: Could not resolve local variable '{declaration.NameSuffixed.Lexeme}' immediately after declaration.");
                        }
                    }
                    else
                    {
                        // Nessun initializer esplicito: un valore di DEFAULT è comunque stato
                        // spinto sullo stack sopra (0.0 per i numerici, OP_NEW_UDT_INSTANCE/
                        // OP_PUSH_NIL per UDT, OP_NEW_WAITGROUP/OP_NEW_CHANNEL, OP_PUSH_NIL per
                        // tutto il resto) e va SEMPRE salvato nello slot locale — non solo per
                        // WaitGroup/Channel (unico caso gestito qui in precedenza). Senza questo
                        // store, il valore di default resta orfano sullo stack P-code E la
                        // variabile locale resta mai inizializzata (letta come null/vuota):
                        // bug scoperto con `DIM v AS SomeUdt` (nessun inizializzatore) dentro una
                        // SUB/FUNCTION — `v.Campo = x` seguito da `PRINT v.Campo` stampava vuoto,
                        // mentre lo stesso DIM a livello di modulo (scope 0, ramo
                        // TokenKind.Dim/Global sopra) funzionava già correttamente perché quel
                        // ramo fa sempre OP_DEFINE_GLOBAL incondizionatamente.
                        if (localIndex != -1)
                            EmitByte((byte)OpCode.OP_STORE_LOCAL); EmitShort((ushort)localIndex);
                    }

                    break;
            }
        }
    }

    /// <summary>
    /// Compila un'istruzione LET (assegnazione esplicita o implicita).
    /// </summary>
    private void VisitLetStatement(LetStmt stmt)
    {
        foreach (var assignment in stmt.Assignments)
        {
            // Fusion nativo: `acc = acc + f(I) * m` dentro un FOR → un solo opcode C nativo.
            // Additivo: se il pattern non matcha, si procede con l'emissione normale.
            if (TryEmitNativeLoopAccumulator(assignment))
                continue;
            // Fusion nativo: `s$ = s$ + term` dentro un FOR → opcode stringa nativo.
            if (TryEmitNativeStringAppendLoop(assignment))
                continue;
            // Fusion nativo: `x = f(y)` (builtin pura su variabile) fuori dai loop → un opcode.
            if (TryEmitNativeBuiltinAssign(assignment))
                continue;

            // RIASSEGNAZIONE di un contenitore TIPIZZATO con un letterale (`H = [9, 5, 2]` fuori
            // da DIM): si usa la STESSA emissione del DIM — EmitCollectionAwareInitializer →
            // OP_NEW del typeCode + elementi + OP_INIT_ARRAY = collezione NUOVA, che lo store
            // sotto sostituisce alla precedente. Prima il letterale compilava a OP_MK_LIST (una
            // LISTA generica): il contenitore perdeva il tipo dichiarato (su P-Code
            // `H.Peek()`/`TR.StartsWith()` → nil e `B.Count(item)` → totale) e su VM/C si
            // popolava l'oggetto trovato sotto sullo stack. Solo per una VARIABILE con tipo di
            // collezione dichiarato e letterale di lista: array/ANY/scalari e i letterali dict
            // restano sul percorso generico di prima. Una sola sorgente del predicato di tipo
            // (IsLiteralInitializableCollection), condivisa con EmitCollectionAwareInitializer.
            var letTargetType = InferExpressionTypeFromTarget(assignment.Target);
            if (assignment.Target is VariableTarget
                && assignment.Value is ListLiteralExpr or ArrayLiteralExpr
                && letTargetType is not null && IsLiteralInitializableCollection(letTargetType))
            {
                EmitCollectionAwareInitializer(letTargetType, assignment.Value);
                AssignVariable(assignment.Target, stmt.LetToken is not null, assignment.Value);
                continue;
            }

            VisitExpression(assignment.Value);
            // Macro &/EVAL che rende NIL su target NUMERICO: coercizione a 0 (parità IL, dove
            // il CLR converte null→0 nello store tipizzato). Senza, PCode lasciava null nello
            // slot → PRINT vuoto (bug ClipperMacroLocalVar local-eval-noleak: PCode stampava
            // vuoto mentre IL/VM-C/C99Native stampavano 0). Target stringa: null→"" è già la
            // stampa corretta (parità IL).
            if (assignment.Value is MacroExpr or EvalExpr
                && IsNumericScalarElement(InferExpressionTypeFromTarget(assignment.Target)))
            {
                EmitByte((byte)OpCode.OP_CALL_BUILTIN);
                EmitShort((ushort)BuiltInFunction.VAL);
                EmitByte((byte)1);
            }
            // Gli UDT (TYPE...END TYPE) sono value type: `P2 = P1` deve copiare i campi, non
            // condividere lo stesso dizionario sottostante (a differenza di una CLASS, sempre
            // per riferimento — da cui il controllo specifico su UserDefinedType, non su
            // "qualunque tipo strutturato"). Senza questa copia, modificare P2 dopo
            // l'assegnazione modificava anche P1 (stesso Dictionary<string,object?> condiviso
            // — vedi 02-Knowledge/GMBasic-PCODE-UDT-Field-Access-Bug.md). Il target, non il
            // valore RHS, decide: assegnare un'espressione UDT a un campo/variabile UserDefinedType
            // clona sempre, indipendentemente da come il valore RHS è stato prodotto.
            if (InferExpressionTypeFromTarget(assignment.Target) is UserDefinedType)
                EmitByte((byte)OpCode.OP_CLONE_UDT);
            AssignVariable(assignment.Target, stmt.LetToken is not null, assignment.Value);
        }
    }

    /// <summary>
    /// Fusion nativo: riconosce un accumulatore puro dentro un FOR a trip count noto ed emette un
    /// singolo opcode OP_NATIVE_LOOP_ACC che la VM/C esegue in C nativo. Forme supportate:
    ///   acc = acc OP f(I) * m      (term_mode 0, acc_op da OP)
    ///   acc = acc OP f(I)          (term_mode 1)
    ///   acc = acc OP I * I         (term_mode 2)
    ///   acc = acc OP I * m         (term_mode 3)
    ///   acc = acc OP I             (term_mode 4)
    /// con OP ∈ {+,-,*,/}, f builtin matematica pura dalla whitelist, I = contatore del FOR.
    /// ADDITIVO: se il pattern non matcha, ritorna false e la compilazione procede come prima.
    /// </summary>
    private bool TryEmitNativeLoopAccumulator(Assignment assignment)
    {
        // L'opcode fuso esegue TUTTE le iterazioni e consuma lo stato del FOR: ammesso SOLO se
        // questa assegnazione è l'INTERO corpo del ciclo (vedi Compiler.NativeLoopFusion.cs).
        // Con altre istruzioni nel corpo (es. un PRINT), quelle verrebbero eseguite una volta
        // sola e il contatore salterebbe al valore finale — silenziosamente. Non si tocca il
        // lowering del FOR: si rifiuta la fusione (emissione normale = semantica di riferimento).
        if (!IsWholeBodyNativeFusion(assignment)) return false;

        // 1. Deve essere dentro un FOR con contatore noto (counterSlot != -1).
        if (_forLoopContexts.Count == 0) return false;
        var (_, _, counterSlot) = _forLoopContexts.Peek();
        if (counterSlot == -1) return false;

        // 2. Target deve essere una variabile locale semplice.
        if (assignment.Target is not VariableTarget vt) return false;
        var accName = vt.NameCleaned.Lexeme;
        var accSlot = ResolveLocal(accName);
        if (accSlot == -1) return false;

        // 3. RHS deve essere: acc OP term, oppure (acc + f(I)) + g(I) per il multi-termine
        //    (associatività sinistra: ACC = ACC + SQR(I) + COS(I) → ((ACC+SQR(I))+COS(I))).
        if (assignment.Value is not BinaryExpr outer) return false;

        // 3b. Operazione di accumulo: + - * / → acc_op 0/1/2/3.
        byte accOp = outer.Op.Kind switch
        {
            TokenKind.Plus => 0,
            TokenKind.Minus => 1,
            TokenKind.Multiply => 2,
            TokenKind.Divide => 3,
            _ => byte.MaxValue,
        };
        if (accOp == byte.MaxValue) return false;

        // 4. Termine: f(I)*m | f(I) | I*I | I*m | I | f(I)+g(I).
        // term_mode: 0=f(I)*m, 1=f(I), 2=I*I, 3=I*m, 4=I, 5=f(I)+g(I).
        byte termMode;
        int fCode = 0;
        int fCode2 = 0;   /* per term_mode 5 (f(I)+g(I)) */
        double multiplier = 1.0;
        bool IsLoopCounter(Expr e) =>
            e is VariableExpr ve && ResolveLocal(ve.NameCleaned.Lexeme) == counterSlot;
        bool IsCounterTerm(Expr e) =>
            e is BuiltinFunctionCallExpr b && IsPureBuiltin(b, out _)
            && b.Args.Length == 1 && IsLoopCounter(b.Args[0]);

        // Multi-termine: `acc = acc + f(I) + g(I)` arriva gia' scorporato ((acc+f(I))+g(I)).
        // Lo scorporo e' legale SOLO se ENTRAMBI i termini sono builtin puri sul contatore e
        // l'operatore e' '+' in entrambe le posizioni: altrimenti il sotto-terminale viene
        // semplicemente IGNORATO dall'opcode fuso. Prima di questo controllo il ramo accettava
        // anche `acc + I + I` (sotto-terminale non builtin) e perdeva il termine interno:
        // PCodeVM/VM-C davano 6 dove IL/C99 davano 12 (divergenza tra backend, 2026-09-29,
        // vedi samples/Loops/LoopsAccumulatorPatterns.bas sezione 4).
        Expr outerLeft = outer.Left;
        if (outer.Op.Kind == TokenKind.Plus
            && outerLeft is BinaryExpr innerAcc && innerAcc.Op.Kind == TokenKind.Plus
            && innerAcc.Left is VariableExpr iav
            && IsCounterTerm(innerAcc.Right) && IsCounterTerm(outer.Right))
        {
            outerLeft = iav;
        }
        if (outerLeft is not VariableExpr accVar) return false;
        if (!accVar.NameCleaned.Lexeme.Equals(accName, StringComparison.OrdinalIgnoreCase))
            return false;

        switch (outer.Right)
        {
            case BuiltinFunctionCallExpr bf when IsPureBuiltin(bf, out var code) && IsLoopCounter(bf.Args[0]):
                // f(I) — term_mode 1. Se outer.Left è (acc + g(I)) → multi-termine (term_mode 5).
                if (outer.Left is BinaryExpr innerL && innerL.Op.Kind == TokenKind.Plus
                    && innerL.Left is VariableExpr iav2
                    && iav2.NameCleaned.Lexeme.Equals(accName, StringComparison.OrdinalIgnoreCase)
                    && innerL.Right is BuiltinFunctionCallExpr bfInner
                    && IsPureBuiltin(bfInner, out var cInner) && IsLoopCounter(bfInner.Args[0]))
                {
                    termMode = 5; fCode = cInner; fCode2 = code;
                }
                else { termMode = 1; fCode = code; }
                break;

            case BinaryExpr plus when plus.Op.Kind == TokenKind.Plus
                && plus.Left is BuiltinFunctionCallExpr bf1 && IsPureBuiltin(bf1, out var c1) && IsLoopCounter(bf1.Args[0])
                && plus.Right is BuiltinFunctionCallExpr bf2 && IsPureBuiltin(bf2, out var c2) && IsLoopCounter(bf2.Args[0]):
                // f(I) + g(I) — term_mode 5.
                termMode = 5; fCode = c1; fCode2 = c2;
                break;

            case BinaryExpr mul when mul.Op.Kind == TokenKind.Multiply:
                // f(I)*m | m*f(I) | I*I | I*m | m*I
                if (mul.Left is BuiltinFunctionCallExpr bfL && IsPureBuiltin(bfL, out var cL)
                    && IsLoopCounter(bfL.Args[0]) && mul.Right is NumberExpr nR)
                {
                    termMode = 0; fCode = cL; multiplier = nR.Value;
                }
                else if (mul.Right is BuiltinFunctionCallExpr bfR && IsPureBuiltin(bfR, out var cR)
                    && IsLoopCounter(bfR.Args[0]) && mul.Left is NumberExpr nL)
                {
                    termMode = 0; fCode = cR; multiplier = nL.Value;
                }
                else if (IsLoopCounter(mul.Left) && IsLoopCounter(mul.Right))
                {
                    termMode = 2; // I*I
                }
                else if (IsLoopCounter(mul.Left) && mul.Right is NumberExpr nr)
                {
                    termMode = 3; multiplier = nr.Value; // I*m
                }
                else if (mul.Left is NumberExpr nl && IsLoopCounter(mul.Right))
                {
                    termMode = 3; multiplier = nl.Value; // m*I
                }
                else return false;
                break;

            case VariableExpr ve when IsLoopCounter(ve):
                termMode = 4; // I
                break;

            default:
                return false;
        }

        // 5. Moltiplicatore deve essere un numero finito (valido solo per mode 0/3).
        if (double.IsNaN(multiplier) || double.IsInfinity(multiplier)) return false;

        // 6. Emette l'opcode composito.
        // Formato: [OP_NATIVE_LOOP_ACC] [acc_slot u8] [ctr_slot u8] [acc_op u8] [term_mode u8]
        //          [f_code u16] [f_code2 u16] [m_idx u16]  = 11 byte.
        if (vt.SourceLineIndex >= 0) _currentSourceLineIndex = vt.SourceLineIndex;
        EmitByte((byte)OpCode.OP_NATIVE_LOOP_ACC);
        EmitShort((ushort)accSlot);
        EmitShort((ushort)counterSlot);
        EmitByte(accOp);
        EmitByte(termMode);
        EmitShort((ushort)fCode);
        EmitShort((ushort)fCode2);
        EmitShort((ushort)AddConstant(multiplier));
        return true;
    }

    /// <summary>
    /// Fusion nativo: riconosce il self-append `s$ = s$ + term` dentro un FOR a trip count
    /// noto ed emette un singolo opcode OP_NATIVE_STRING_APPEND_LOOP che la VM/C (e il
    /// PCodeProcessor) esegue nativamente su un buffer con capacità di scorta — O(1)
    /// ammortizzato invece di O(n²) da concatenazione per iterazione. Forme supportate:
    ///   s$ = s$ + "letterale"     (term_mode 0, const_idx)
    ///   s$ = s$ + STR$(I)/CSTR(I) (term_mode 1, f_code)
    /// Nota: '&' NON è un operatore binario in GMBASIC (è l'operatore macro/EVAL unario),
    /// quindi la concatenazione è solo '+'. Il prepend (`s$ = expr + s$`) NON è supportato
    /// (crescere il buffer all'inizio è costoso): in quel caso fallback. ADDITIVO: se il
    /// pattern non matcha, ritorna false.
    /// </summary>
    private bool TryEmitNativeStringAppendLoop(Assignment assignment)
    {
        // Come TryEmitNativeLoopAccumulator: l'opcode fuso esegue TUTTE le iterazioni e consuma
        // lo stato del FOR — ammesso SOLO se l'assegnazione è l'INTERO corpo del ciclo
        // (vedi Compiler.NativeLoopFusion.cs; bug PCode-For-In-Function-Corpo-Loop).
        if (!IsWholeBodyNativeFusion(assignment)) return false;

        // 1. Deve essere dentro un FOR con contatore noto (counterSlot != -1).
        if (_forLoopContexts.Count == 0) return false;
        var (_, _, counterSlot) = _forLoopContexts.Peek();
        if (counterSlot == -1) return false;

        // 2. Target variabile locale semplice di tipo stringa (suffisso $).
        if (assignment.Target is not VariableTarget vt) return false;
        if (!vt.NameSuffixed.Lexeme.EndsWith('$')) return false;
        var accName = vt.NameCleaned.Lexeme;
        var accSlot = ResolveLocal(accName);
        if (accSlot == -1) return false;

        // 3. RHS: s$ + term con accumulatore a SINISTRA (append a destra).
        if (assignment.Value is not BinaryExpr outer) return false;
        if (outer.Op.Kind is not TokenKind.Plus) return false;
        if (outer.Left is not VariableExpr accVar) return false;
        if (!accVar.NameCleaned.Lexeme.Equals(accName, StringComparison.OrdinalIgnoreCase))
            return false;

        // 4. Termine: letterale stringa (mode 0) o builtin pura num→str sul contatore (mode 1).
        byte termMode;
        int fCode = 0;
        int constIdx = 0;
        bool IsLoopCounter(Expr e) =>
            e is VariableExpr ve && ResolveLocal(ve.NameCleaned.Lexeme) == counterSlot;

        switch (outer.Right)
        {
            case StringExpr lit:
                termMode = 0;
                constIdx = AddConstant(lit.Value);
                break;

            case BuiltinFunctionCallExpr bf when IsPureStringBuiltin(bf, out var code)
                && bf.Args.Length == 1 && IsLoopCounter(bf.Args[0]):
                termMode = 1;
                fCode = code;
                break;

            default:
                return false;
        }

        // 5. Emette l'opcode composito.
        // Formato: [OP_NATIVE_STRING_APPEND_LOOP] [acc_slot u8] [ctr_slot u8] [term_mode u8]
        //          [f_code u16] [const_idx u16]  = 8 byte.
        if (vt.SourceLineIndex >= 0) _currentSourceLineIndex = vt.SourceLineIndex;
        EmitByte((byte)OpCode.OP_NATIVE_STRING_APPEND_LOOP);
        EmitByte((byte)accSlot);
        EmitByte((byte)counterSlot);
        EmitByte(termMode);
        EmitShort((ushort)fCode);
        EmitShort((ushort)constIdx);
        return true;
    }

    /// <summary>Ritorna true se la builtin è nella whitelist stringa pura e ne estrae il codice.</summary>
    private bool IsPureStringBuiltin(BuiltinFunctionCallExpr bf, out int fCode)
    {
        fCode = 0;
        if (!BuiltInFunctionMap.TryGetValue(bf.Name.Kind, out var code)) return false;
        if (!NativeLoopPureStringBuiltins.Contains(code)) return false;
        fCode = (int)code;
        return true;
    }

    /// <summary>Builtin num→str pure ammesse nel self-append nativo (nessun side-effect).</summary>
    private static readonly System.Collections.Generic.HashSet<BuiltInFunction> NativeLoopPureStringBuiltins =
        new()
        {
            BuiltInFunction.STR,
            BuiltInFunction.CSTR,
        };

    /// <summary>
    /// Fusion nativo: riconosce `x = f(y)` con f builtin matematica pura e y variabile locale
    /// (fuori dai loop — per i loop c'è già OP_NATIVE_LOOP_ACC). Emette UN opcode che carica y,
    /// chiama f e salva x in C nativo, evitando i 3 dispatch LOAD/CALL_BUILTIN/STORE separati.
    /// ADDITIVO: se non matcha, ritorna false e la compilazione procede come prima.
    /// </summary>
    private bool TryEmitNativeBuiltinAssign(Assignment assignment)
    {
        // Deve essere: x = f(y), target variabile, RHS builtin pura su una variabile locale.
        if (assignment.Target is not VariableTarget vt) return false;
        var dstSlot = ResolveLocal(vt.NameCleaned.Lexeme);
        if (dstSlot == -1) return false;
        if (assignment.Value is not BuiltinFunctionCallExpr bf) return false;
        if (!IsPureBuiltin(bf, out var fCode)) return false;
        if (bf.Args[0] is not VariableExpr srcVar) return false;
        var srcSlot = ResolveLocal(srcVar.NameCleaned.Lexeme);
        if (srcSlot == -1) return false;

        // Emette [OP_NATIVE_BUILTIN_ASSIGN] [src_slot u8] [dst_slot u8] [f_code u16].
        if (vt.SourceLineIndex >= 0) _currentSourceLineIndex = vt.SourceLineIndex;
        EmitByte((byte)OpCode.OP_NATIVE_BUILTIN_ASSIGN);
        EmitShort((ushort)srcSlot);
        EmitShort((ushort)dstSlot);
        EmitShort((ushort)fCode);
        return true;
    }

    /// <summary>Ritorna true se la builtin è nella whitelist pura e ne estrae il codice.</summary>
    private bool IsPureBuiltin(BuiltinFunctionCallExpr bf, out int fCode)
    {
        fCode = 0;
        if (bf.Args.Length != 1) return false;
        if (!BuiltInFunctionMap.TryGetValue(bf.Name.Kind, out var code)) return false;
        if (!NativeLoopPureBuiltins.Contains(code)) return false;
        fCode = (int)code;
        return true;
    }

    /// <summary>Builtin matematiche pure ammesse nella fusione loop nativo (nessun side-effect).</summary>
    private static readonly System.Collections.Generic.HashSet<BuiltInFunction> NativeLoopPureBuiltins =
        new()
        {
            BuiltInFunction.SQR,
            BuiltInFunction.ABS,
            BuiltInFunction.EXP,
            BuiltInFunction.LOG,
            BuiltInFunction.LOG10,
            BuiltInFunction.SIN,
            BuiltInFunction.COS,
            BuiltInFunction.TAN,
            BuiltInFunction.ATN,
            BuiltInFunction.INT,
            BuiltInFunction.ROUND,
            BuiltInFunction.FIX,
        };

    /// <summary>
    /// "Compila" un'istruzione CONST.
    /// Questa operazione non emette P-Code, ma popola la tabella delle costanti
    /// del compilatore per il "constant folding" successivo.
    /// </summary>
    private void VisitConstStatement(ConstStmt stmt)
    {
        foreach (var declaration in stmt.Declarations)
        {
            // Valuta l'espressione iniziale. Per ora, supportiamo solo letterali.
            object constValue = declaration.InitialValue switch
            {
                NumberExpr n => n.Value,
                StringExpr s => s.Value,
                BooleanExpr b => b.Value,
                _ => throw new Exception($"Initializer for CONST '{declaration.OriginalToken.Lexeme}' must be a literal value.")
            };

            // Aggiungi la costante alla mappa del compilatore.
            // Registriamo SIA il lessema grezzo (es. "%K") SIA il nome pulito ("K").
            // Il lookup in VisitExpression usa NameCleaned.Lexeme: se in futuro il cleaning
            // togliesse anche il prefisso '%', la chiave pulita resta coperta.
            // (Twin-port GMBasicAI — senza, il fallback _constants.TryGetValue in
            // VisitExpression restava morto per le costanti in stile PowerBASIC %NOME.)
            _constants[declaration.OriginalToken.Lexeme] = constValue;
            var cleanedName = declaration.OriginalToken.Lexeme.TrimStart('%', '$', '!', '#', '&');
            if (cleanedName != declaration.OriginalToken.Lexeme)
                _constants[cleanedName] = constValue;
        }
    }

    private void EmitAdjustStringForLocal(int localIndex)
    {
        if (localIndex >= 0 && localIndex < _locals.Count)
        {
            var localType = _locals[localIndex].Type;
            if (localType is StringType { FixedLength: int len } && len > 0)
            {
                EmitByte((byte)OpCode.OP_ADJUST_STRING);
                EmitShort((ushort)len);
            }
        }
    }

    private void EmitAdjustStringForGlobal(int globalIndex)
    {
        if (globalIndex >= 0 && globalIndex < _globalInfos.Count)
        {
            var globalType = _globalInfos[globalIndex].Type;
            if (globalType is StringType { FixedLength: int len } && len > 0)
            {
                EmitByte((byte)OpCode.OP_ADJUST_STRING);
                EmitShort((ushort)len);
            }
        }
    }

    /// <summary>Parità con IL (IlProgramCompiler.Operators.cs, EmitCoerceTypes): TODAY()/
    /// RDD.TODAY()/CTOD()/RDD.CTOD() ritornano un DateTime CLR. IL applica ToOADate SOLO
    /// quando il target è NUMERICO (IlConversion.IsNumeric — una variabile IMPLICITA,
    /// nessun DIM, ha target double di default; un target STRING o DATE dichiarato esplicito
    /// resta non-numerico e NON viene coercizzato). PCode invece salvava sempre l'oggetto
    /// DateTime grezzo as-is: `d = RDD.TODAY() : PRINT d` stampava "46276" su IL e
    /// "11/09/2026 00:00:00" su PCode (Bugs/(open) - Data-Implicita-Coercizione-IL-vs-PCode.md).
    /// Qui si replica la STESSA condizione (controllo sintattico sul TokenKind della builtin,
    /// non full type-inference: PCode non ne ha una) e si applica CDBL (BuiltInFunction già
    /// esistente, SafeDouble gestisce DateTime→OADate) SOLO se il global è implicito (nessuna
    /// voce in _globalInfos) o dichiarato con un tipo NUMERICO esplicito — MAI se dichiarato
    /// STRING/DATE/altro non-numerico (regressione misurata e corretta: `DIM t AS STRING`
    /// coercizzava anch'esso prima di questo fix, mentre deve restare invariato).</summary>
    private static bool IsNumericGlobalType(DataType t) =>
        t is DoubleType or SingleType or LongType or ULongType or QuadType or UQuadType
            or IntegerType or UIntegerType or ByteType or SByteType or CurrencyType or BitType;

    private void EmitCoerceDateForGlobal(int globalIndex, Expr? valueExpr)
    {
        if (valueExpr is not BuiltinFunctionCallExpr { Name.Kind: TokenKind.Today or TokenKind.RddToday
            or TokenKind.Ctod or TokenKind.RddCtod }) return;
        var isImplicit = globalIndex < 0 || globalIndex >= _globalInfos.Count;
        if (!isImplicit && !IsNumericGlobalType(_globalInfos[globalIndex].Type)) return;
        EmitByte((byte)OpCode.OP_CALL_BUILTIN);
        EmitShort((ushort)BuiltInFunction.CDBL);
        EmitByte(1);
    }
}