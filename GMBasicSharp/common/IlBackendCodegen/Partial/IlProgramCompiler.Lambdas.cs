// FILE: IlProgramCompiler.Lambdas.cs
// PARTIAL — dominio LAMBDAS (scoperta/compilazione/stato closure).
// Migrato da IlProgramCompiler.cs (2026-08-25 — protocollo 3-step).

using System.Collections.Immutable;
using System.Linq;
using System.Reflection;
using System.Reflection.Emit;
using System.Reflection.Metadata;
using System.Reflection.Metadata.Ecma335;
using System.Reflection.PortableExecutable;
using Basic.IlBackend.Gpm;
using Basic.Runtime.Gpm;
using Basic;

namespace Basic.IlBackend;

/// <summary>Partial del transpiler IL: dominio LAMBDAS.</summary>
public sealed partial class IlProgramCompiler
{

    private static Dictionary<LambdaExpr, LambdaClosureInfo> DiscoverAndCompileLambdas(
        BasicProgram program, ModuleBuilder moduleBuilder, IlTypeContext types,
        Dictionary<int, TypeBuilder> classTypes, Dictionary<int, MethodBuilder> procedureMethods,
        Dictionary<int, FunctionDefinition> procedureDefinitionsBySymbol,
        Dictionary<int, FieldBuilder> classFields, Dictionary<int, FieldArrayInfo> classFieldArrays,
        Dictionary<int, List<ClassConstructorInfo>> classConstructors, Dictionary<int, ClassPropertyInfo> classProperties,
        Dictionary<(int, TokenKind, int, string), MethodBuilder> classOperators,
        TypeBuilder programType, List<TypeBuilder> closureTypesToFinalize)
    {
        var lambdaInfos = new Dictionary<LambdaExpr, LambdaClosureInfo>();

        var allStatementLists = new List<ImmutableArray<Stmt>>();
        foreach (var lineStatements in program.MainModule!.Statements.Values)
            allStatementLists.Add([.. lineStatements]);
        foreach (var proc in procedureDefinitionsBySymbol.Values)
            allStatementLists.Add(proc.Body);
        foreach (var classDef in classTypes.Keys
                     .Select(id => program.Symbols.GetSymbol(id).DeclarationNode)
                     .OfType<ClassDefinition>())
        {
            foreach (var member in classDef.Members)
            {
                switch (member)
                {
                    case MethodMember mm: allStatementLists.Add(mm.Procedure.Body); break;
                    case ConstructorMember cm: allStatementLists.Add(cm.Constructor.Body); break;
                }
            }
        }
        foreach (var udtDef in classTypes.Keys
                     .Select(id => program.Symbols.GetSymbol(id).DeclarationNode)
                     .OfType<UserDefinedType>())
        {
            foreach (var m in udtDef.Methods) allStatementLists.Add(m.Body);
            foreach (var c in udtDef.Constructors) allStatementLists.Add(c.Body);
        }

        // Fase A: scopre ogni LambdaExpr (in tutto il programma, ricorsivamente — anche
        // dentro il corpo di un'altra lambda, che riceve così una propria
        // LambdaClosureInfo indipendente, con le sue catture calcolate transitivamente,
        // vedi CollectLambdaCaptures) e dichiara subito tipo di chiusura (se servono
        // catture) + firma del metodo Invoke, SENZA corpo — stesso schema "dichiara
        // prima, compila dopo" già usato per classi/procedure,
        // necessario perché una lambda può referenziare (per NEW/CALL) un'altra lambda
        // dichiarata più avanti nel programma.
        var discovered = new List<LambdaExpr>();
        foreach (var stmts in allStatementLists)
            CollectLambdaExpressions(stmts, discovered);

        var closureSeq = 0;
        // Parametri di procedure E di lambda: una cattura che è un PARAMETRO (byvalue) resta
        // per valore — la sua promozione a box ha un percorso proprio (Fase 4 / ByRef) e
        // senza un box sorgente al call-site non è costruibile (il parametro non è un local
        // del metodo: EmitLambdaCreation non troverebbe il box da condividere).
        var allParamSymbolIds = program.Symbols.GetAllProcedures()
            .SelectMany(p => p.Parameters.Select(pp => pp.SymbolId))
            .Concat(discovered.SelectMany(l => l.Parameters).Select(pp => pp.SymbolId))
            .ToHashSet();
        // Bug closure_byref (2026-08-31): un simbolo può essere MUTATO da una lambda A e letto
        // in SOLA LETTURA da una lambda B (es. `somma = FN(x)=>totale+x` e
        // `inc = SUB(){ totale=totale+1 }` sullo STESSO `totale`) — calcolare le catture
        // mutate per-singola-lambda (come prima) lascia B con uno snapshot per valore anche
        // dopo il fix puntuale, perché "totale" non è mai un target DENTRO IL CORPO DI B.
        // Unione GLOBALE (whole-program, via SymbolId univoco per simbolo — nessun rischio di
        // falsa condivisione tra procedure diverse) delle catture mutate da QUALUNQUE lambda
        // scoperta: ogni lambda che cattura un simbolo mutato altrove riceve comunque un
        // campo ByRefBox<T>, restando sincronizzata con le mutazioni fatte da un'altra
        // closure (parity col backend nativo VMC, che promuove a box qualunque slot
        // catturato, mai solo quelli scritti da QUELLA specifica closure).
        var globallyMutatedCaptures = discovered.SelectMany(CollectMutatedSymbols)
            .Where(s => !allParamSymbolIds.Contains(s)).ToHashSet();
        /* CATTURE PER RIFERIMENTO (2026-11-03): una closure cattura la VARIABILE, non il
         * valore — parità con VM/C e C99Native, dove ogni cattura vive in una cella/box.
         * Prima SOLO le catture MUTATE ricevevano un campo ByRefBox: una variabile letta e
         * poi RIASSEGNATA dal chiamante restava lo snapshot del valore al momento della
         * creazione (`DIM ro=5 : f={||ro} : ro=42` → 5 invece di 42) e lo stesso per le
         * STRINGHE (`s="a" : f={||s} : s="b"` → a). Box per tutte le catture LOCALI;
         * parametri (percorso ByRefBox/Fase 4 proprio), globali e static restano come sono. */
        var lambdaParamSymbolIds = discovered
            .SelectMany(l => l.Parameters.Select(pp => pp.SymbolId))
            .ToHashSet();
        var boxedCaptureSymbols = discovered
            .SelectMany(CollectLambdaCaptures)
            .Where(id => id >= 0 && !lambdaParamSymbolIds.Contains(id))
            .Where(id =>
            {
                var sym = program.Symbols.GetSymbol(id);
                var fl = sym.Flags;
                if (fl.HasFlag(SymbolFlags.IsConst)) return false;
                if (fl.HasFlag(SymbolFlags.IsStaticStorage)) return false;
                if (fl.HasFlag(SymbolFlags.IsGlobal)) return false;
                // Parametri BYREF: percorso Fase 4 (ParameterInfo.IsBox), non questo.
                if (fl.HasFlag(SymbolFlags.IsParameter) && fl.HasFlag(SymbolFlags.IsByRef)) return false;
                // Array: reference type, la cattura condivide già l'oggetto.
                if (sym.DataType is ArrayType) return false;
                return true;
            })
            .ToHashSet();
        foreach (var lambda in discovered)
        {
            var captures = CollectLambdaCaptures(lambda);
            // Catture MUTATE (write-target nel body, DI QUALUNQUE lambda — vedi sopra):
            // richiedono la cattura per RIFERIMENTO — il campo diventa ByRefBox<T> (condiviso
            // con la variabile del chiamante) invece dello snapshot per valore. Escluse le
            // catture che sono PARAMETRI di una procedura (restano T: i param BYREF hanno già
            // il loro percorso box __ClosureCtx/Fase 4; i param BYVAL catturati restano
            // snapshot).
            var mutatedCaptures = boxedCaptureSymbols;
            var delegateType = DataTypeMapper.Map(
                new FunctionPointerType(lambda.Parameters.Select(p => p.DataType).ToImmutableArray(), lambda.ReturnType),
                types, classTypes);

            TypeBuilder? closureType = null;
            ConstructorBuilder? closureCtor = null;
            var captureFields = ImmutableArray<(int, DataType, FieldBuilder)>.Empty;
            MethodBuilder invokeMethod;
            var paramClrTypes = lambda.Parameters.Select(p => DataTypeMapper.Map(p.DataType, types, classTypes)).ToArray();
            var returnClrType = DataTypeMapper.Map(lambda.ReturnType, types, classTypes);

            if (captures.Count == 0)
            {
                invokeMethod = programType.DefineMethod(
                    $"Lambda_{closureSeq}", MethodAttributes.Private | MethodAttributes.Static, returnClrType, paramClrTypes);
            }
            else
            {
                closureType = moduleBuilder.DefineType($"Closure_{closureSeq}", TypeAttributes.Public | TypeAttributes.Class, types.Object);
                closureTypesToFinalize.Add(closureType);
                // DefineDefaultConstructor (non GetConstructor sul TypeBuilder dopo, che
                // lancia "The invoked member is not supported before the type is created"
                // finché CreateType() non è stato chiamato, in fondo a CompileToAssembly):
                // il ConstructorBuilder restituito qui è già direttamente usabile per
                // emettere `newobj` prima che il tipo sia finalizzato.
                closureCtor = closureType.DefineDefaultConstructor(MethodAttributes.Public);
                var builtFields = ImmutableArray.CreateBuilder<(int, DataType, FieldBuilder)>();
                foreach (var symbolId in captures)
                {
                    var captureType = program.Symbols.GetSymbol(symbolId).DataType;
                    // Cattura MUTATA (non-param): campo ByRefBox<T> — la closure condivide il
                    // box con la variabile del chiamante, le scritture dentro la lambda si
                    // propagano (bug closure_byref). Leggibilità/scrittura del box gestite dai
                    // rami IsByRefBoxFieldType di EmitLoadVariable/EmitStoreVariable.
                    /* Un PARAMETRO di LAMBDA ha ABI delegate: i numerici viaggiano come
                     * double anche se dichiarati INTEGER/LONG. Il campo di cattura deve
                     * avere lo STESSO tipo CLR del valore che riceve (`ldarg`), altrimenti
                     * `stfld` scrive i BIT di un double in un campo int64 → letture
                     * reinterpretate (mut= 4.6e18 = bit pattern di 3.0, OverflowException). */
                    var mappedType = (lambdaParamSymbolIds.Contains(symbolId)
                                      && IlConversion.IsNumeric(captureType))
                        ? types.Double
                        : DataTypeMapper.Map(captureType, types, classTypes);
                    var fieldType = mutatedCaptures.Contains(symbolId)
                        ? typeof(ByRefBox<>).MakeGenericType(mappedType)
                        : mappedType;
                    var field = closureType.DefineField(
                        "capture_" + symbolId, fieldType, FieldAttributes.Public);
                    builtFields.Add((symbolId, captureType, field));
                }
                captureFields = builtFields.ToImmutable();
                invokeMethod = closureType.DefineMethod(
                    "Invoke", MethodAttributes.Public, returnClrType, paramClrTypes);
            }

            lambdaInfos[lambda] = new LambdaClosureInfo(delegateType, closureType, closureCtor, invokeMethod, captureFields);
            closureSeq++;
        }

        // Fase B: compila ogni corpo — DOPO che TUTTE le lambda del programma hanno già
        // un Invoke dichiarato (una lambda può chiamarne un'altra).
        foreach (var lambda in discovered)
        {
            var info = lambdaInfos[lambda];
            var body = lambda.BlockBody ?? [new ReturnStmt(lambda.ExpressionBody, lambda.FuncToken)];
            IlProgramCompiler lambdaCompiler;
            if (info.ClosureType is null)
            {
                // Parametri-array BYVAL (il metodo Invoke è T[] non T[]& — vedi il ctor con
                // closure qui sotto): il parser forza IsByRef sugli array, che produrrebbe
                // Ldarg+Ldind_Ref su un T[] → InvalidProgram.
                var byValParams = lambda.Parameters
                    .Select(p => p with { Flags = p.Flags & ~SymbolFlags.IsByRef }).ToImmutableArray();
                lambdaCompiler = new IlProgramCompiler(
                    program, info.InvokeMethod.GetILGenerator(), types, procedureMethods, procedureDefinitionsBySymbol,
                    classTypes, classFields, classFieldArrays, classConstructors, classProperties, classOperators, lambdaInfos,
                    new FunctionDefinition(-1, lambda.FuncToken, byValParams, lambda.ReturnType, body, SymbolFlags.None));
            }
            else
            {
                var capturedFields = info.Captures.ToDictionary(c => c.SymbolId, c => c.Field);
                lambdaCompiler = new IlProgramCompiler(
                    program, info.InvokeMethod.GetILGenerator(), types, procedureMethods, procedureDefinitionsBySymbol,
                    classTypes, classFields, classFieldArrays, classConstructors, classProperties, classOperators, lambdaInfos,
                    lambda.Parameters, lambda.ReturnType, capturedFields);
            }
            lambdaCompiler.EmitStatementBody(body);
        }

        return lambdaInfos;
    }

    /// <summary>
    /// Attraversamento GENERICO (via reflection sulle proprietà pubbliche) dell'intero
    /// sottoalbero sintattico raggiungibile da un nodo AST, usato da tutte e tre le
    /// analisi lambda/chiusura sotto (scoperta lambda, locali dichiarati, simboli
    /// referenziati). Sostituisce tre switch manuali paralleli che coprivano solo 5 tipi
    /// di Stmt e 8 di Expr sulle centinaia esistenti nell'AST — qualunque lambda dentro
    /// un PRINT/FOR/WHILE/SELECT CASE/TRY o altro costrutto non esplicitamente elencato
    /// semplicemente non veniva scoperta (crash a valle: "lambda non scoperta da
    /// DiscoverAndCompileLambdas") o non contribuiva alle catture (chiusura con un campo
    /// mancante). <paramref name="visit"/> viene invocato per ogni <see cref="AstNode"/>
    /// incontrato (Stmt, Expr, Target, CaseTest, Parameter/Decl, ecc.); se ritorna
    /// <c>false</c> l'attraversamento NON scende nei figli di quel nodo (usato per
    /// fermarsi a una lambda annidata e delegare a <see cref="CollectLambdaCaptures"/>
    /// invece di un attraversamento generico). Non scende MAI in un <see cref="DataType"/>
    /// (i grafi dei tipi possono essere condivisi/ciclici tra definizioni — es.
    /// ClassDefinition.Parent — e i corpi di metodi/costruttori che referenziano non sono
    /// sintatticamente annidati nel nodo di partenza). <see cref="PrintArg"/> e
    /// <see cref="Assignment"/> sono gli unici wrapper non-AstNode nell'AST (record
    /// "contenitore" semplici, senza riferimenti circolari), gestiti esplicitamente — se in
    /// futuro emergesse un altro wrapper simile (nuovo costrutto sintattico con un record
    /// helper dedicato invece di un AstNode), il sintomo è identico a quello di questa
    /// sessione: "lambda non scoperta" o una cattura mancante, con il costrutto specifico
    /// nel messaggio di errore/nel test che fallisce.
    /// </summary>
    private static void WalkAst(object? node, Func<object, bool> visit)
    {
        switch (node)
        {
            case null:
            case string:
                return;
            case DataType:
                return;
            case PrintArg pa:
                if (pa.Expression is { } printExpr) WalkAst(printExpr, visit);
                return;
            case Assignment assign:
                WalkAst(assign.Target, visit);
                WalkAst(assign.Value, visit);
                return;
            case AstNode astNode:
                if (visit(astNode)) WalkAstChildren(astNode, visit);
                return;
            case System.Collections.IEnumerable enumerable:
                // Un ImmutableArray<T> mai inizializzato (default(ImmutableArray<T>), non lo
                // stesso di .Empty) lancia InvalidOperationException al primo
                // GetEnumerator/MoveNext — non c'è modo di controllarlo prima senza reflection
                // aggiuntiva specifica per T (sconosciuto qui, il valore è boxed come object);
                // equivalente a "nessun elemento", niente da attraversare.
                try
                {
                    foreach (var item in enumerable) WalkAst(item, visit);
                }
                catch (InvalidOperationException)
                {
                }
                return;
        }
    }

    private static readonly Dictionary<Type, PropertyInfo[]> AstNodePropertiesCache = new();

    private static void WalkAstChildren(object node, Func<object, bool> visit)
    {
        var type = node.GetType();
        if (!AstNodePropertiesCache.TryGetValue(type, out var props))
        {
            props = type.GetProperties(BindingFlags.Public | BindingFlags.Instance)
                .Where(p => p.GetIndexParameters().Length == 0)
                // Ciclo-safe (bug 2026-09-16 FindExplicitCallTo/stack overflow): le back-reference
                // al genitore (es. UserDefinedType.Parent, raggiungibile via LocalTypeStmt) NON
                // sono figli strutturali — seguirle fa risalire l'albero all'infinito.
                .Where(p => !Basic.AstReflectiveWalk.BackReferencePropertyNames.Contains(p.Name))
                .ToArray();
            AstNodePropertiesCache[type] = props;
        }
        foreach (var prop in props) WalkAst(prop.GetValue(node), visit);
    }

    /// <summary>
    /// Raccoglie ogni <see cref="LambdaExpr"/> in una lista di statement, ricorsivamente e
    /// GENERICAMENTE (vedi <see cref="WalkAst"/>) — anche una lambda annidata dentro il
    /// corpo di un'altra viene scoperta (si continua sempre a scendere, <c>true</c>), così
    /// da riceverne una propria LambdaClosureInfo indipendente.
    /// </summary>
    private static void CollectLambdaExpressions(IEnumerable<Stmt> statements, List<LambdaExpr> found)
    {
        foreach (var stmt in statements) CollectLambdaExpressionsInStmt(stmt, found);
    }

    private static void CollectLambdaExpressionsInStmt(Stmt stmt, List<LambdaExpr> found)
    {
        WalkAst(stmt, node =>
        {
            if (node is LambdaExpr le) found.Add(le);
            return true;
        });
    }

    /// <summary>
    /// Determina le variabili "catturate" da una lambda: ogni SymbolId referenziato nel
    /// corpo (RICORSIVAMENTE anche attraverso una lambda ANNIDATA — vedi
    /// CollectReferencedSymbolsInExpr, caso LambdaExpr, che richiama questo stesso metodo
    /// sulla lambda annidata e ne propaga le catture verso l'esterno) che non è né un
    /// parametro della lambda né dichiarato (DIM) al suo interno — vedi il commento sopra
    /// DiscoverAndCompileLambdas per la semantica "by value". Una lambda annidata i cui
    /// riferimenti sono TUTTI soddisfatti da parametri/locali di QUESTA lambda non
    /// propaga nulla (sottratti da locallyBound come qualunque altro riferimento diretto);
    /// solo ciò che la lambda annidata richiede da PIÙ IN LÀ (grandparent scope o oltre)
    /// diventa a sua volta una cattura di questa lambda — ricorsione naturale su qualunque
    /// profondità di annidamento.
    /// </summary>
    private static List<int> CollectLambdaCaptures(LambdaExpr lambda)
    {
        var body = lambda.BlockBody ?? (lambda.ExpressionBody is { } eb ? [new ReturnStmt(eb, lambda.FuncToken)] : ImmutableArray<Stmt>.Empty);

        var locallyBound = new HashSet<int>(lambda.Parameters.Select(p => p.SymbolId));
        CollectLocalDeclarations(body, locallyBound);

        var referenced = new HashSet<int>();
        CollectReferencedSymbols(body, referenced);

        return referenced.Where(id => !locallyBound.Contains(id)).ToList();
    }

    /// <summary>
    /// SymbolId delle variabili SCRITTE (target di assegnazione) nel corpo di una lambda —
    /// cattura MUTATA. Usato da DiscoverAndCompileLambdas (campo ByRefBox&lt;T&gt;) e da
    /// BoxedSymbols (la variabile del chiamante diventa un box): una closure che MUTA una
    /// variabile esterna deve condividere il box con la variabile di origine, altrimenti la
    /// scrittura resta nel campo-snapshot (bug closure_byref: SUB() { totale = totale + 1 }
    /// non propagava la modifica al chiamante su IL). Cammina genericamente (WalkAst) —
    /// copre qualunque costrutto, inclusi i target di LetStmt/AssignmentExpr.
    /// </summary>
    private static HashSet<int> CollectMutatedSymbols(LambdaExpr lambda)
    {
        var mutated = new HashSet<int>();
        var body = lambda.BlockBody ?? (lambda.ExpressionBody is { } eb
            ? [new ReturnStmt(eb, lambda.FuncToken)] : ImmutableArray<Stmt>.Empty);
        WalkAst(body.Cast<object>().ToList(), node =>
        {
            switch (node)
            {
                case VariableTarget vt: mutated.Add(vt.SymbolId); return true;
                case ArrayElementTarget aet: mutated.Add(aet.SymbolId); return true;
                default: return true;
            }
        });
        return mutated;
    }

    /// <summary>
    /// Simboli dichiarati (DIM) DIRETTAMENTE nel corpo di QUESTA lambda (a qualunque
    /// profondità di blocchi annidati — IF/FOR/WHILE/ecc., vedi <see cref="WalkAst"/>), da
    /// escludere dalle catture. Genericamente NON scende in nessuna <see cref="Expr"/>:
    /// dato che una lambda ANNIDATA vive dentro un'Expr (LambdaExpr), questo confine
    /// rispetta automaticamente lo scope della nidificata (le sue DIM interne non sono
    /// locali di QUESTA lambda) senza bisogno di un caso esplicito per LambdaExpr.
    /// </summary>
    private static void CollectLocalDeclarations(IEnumerable<Stmt> statements, HashSet<int> locallyBound)
    {
        foreach (var stmt in statements)
        {
            WalkAst(stmt, node =>
            {
                if (node is Expr) return false;
                if (node is DeclarationStmt decl)
                    foreach (var d in decl.Declarations) locallyBound.Add(d.SymbolId);
                return true;
            });
        }
    }

    /// <summary>
    /// Ogni SymbolId referenziato (letto O scritto: un target di assegnazione conta come
    /// riferimento anche lui, serve comunque un campo di chiusura per poterci scrivere)
    /// nel corpo di questa lambda, GENERICAMENTE (vedi <see cref="WalkAst"/>) — copre
    /// qualunque costrutto (PRINT, FOR, WHILE, SELECT CASE, TRY, ecc.), non solo i pochi
    /// gestiti esplicitamente prima di questo fix.
    /// </summary>
    private static void CollectReferencedSymbols(IEnumerable<Stmt> statements, HashSet<int> referenced)
    {
        foreach (var stmt in statements)
        {
            WalkAst(stmt, node =>
            {
                switch (node)
                {
                    // Una lambda ANNIDATA propaga le SUE catture verso questa lambda esterna
                    // (ricorsione su CollectLambdaCaptures stesso, non un semplice
                    // attraversamento dell'albero): ciò che la nidificata prende dal proprio
                    // scope enclosing è, dal punto di vista di QUESTA lambda, un riferimento
                    // esterno come un altro — se coincide con un parametro/locale proprio
                    // verrà comunque escluso più sotto (locallyBound), altrimenti diventa a
                    // sua volta una cattura di questa lambda (che EmitLambdaCreation della
                    // nidificata leggerà, al momento della sua NEW implicita, dal campo di
                    // chiusura o dal locale/parametro di QUESTO livello — vedi
                    // EmitLoadVariable, già corretto per costruzione). NON si scende
                    // genericamente nei suoi figli: parametri/corpo sono un ambito lessicale
                    // separato, già interamente gestito dalla ricorsione esplicita sopra.
                    case LambdaExpr nested:
                        foreach (var nestedCapture in CollectLambdaCaptures(nested)) referenced.Add(nestedCapture);
                        return false;
                    case VariableExpr v:
                        referenced.Add(v.SymbolId);
                        return true;
                    // CALL indiretta `EVAL(inner, x)` riscritta dal Linker in FunctionExpr
                    // IsIndirect col SymbolId della VARIABILE callee: va referenziata come
                    // una VariableExpr, altrimenti la cattura di function-value dentro una
                    // lambda non viene creata (NullReferenceException al call).
                    case FunctionExpr fe when fe.IsIndirect:
                        referenced.Add(fe.SymbolId);
                        return true;
                    case ArrayAccessExpr a:
                        referenced.Add(a.SymbolId);
                        return true;
                    case VariableTarget vt:
                        referenced.Add(vt.SymbolId);
                        return true;
                    case ArrayElementTarget aet:
                        referenced.Add(aet.SymbolId);
                        return true;
                    default:
                        return true;
                }
            });
        }
    }

    /// <summary>
    /// Emette il RIFERIMENTO al box di una cattura mutata (per condividerlo con una closure
    /// figlia), in ordine di precedenza:
    /// 1. CAMPO ByRefBox di una closure ESTERNA (lambda dentro lambda: il simbolo è una
    ///    cattura del genitore, non un local di questa lambda);
    /// 2. LOCAL box della procedura corrente (<see cref="BoxedSymbols"/>);
    /// 3. PARAMETRO box (BYREF promosso).
    /// Ritorna false se non c'è un box da condividere. Senza il caso 1 la closure interna
    /// riceveva un local nuovo (null) al posto del box del genitore → NullReferenceException
    /// alla prima mutazione (lambda annidata: `k = k + a + b + c` due livelli dentro).
    /// </summary>
    private bool TryEmitBoxReference(int symbolId)
    {
        if (_capturedFields.TryGetValue(symbolId, out var capturedField)
            && IsByRefBoxFieldType(capturedField.FieldType))
        {
            _il.Emit(OpCodes.Ldarg_0);
            _il.Emit(OpCodes.Ldfld, capturedField);
            return true;
        }
        if (BoxedSymbols.Contains(symbolId))
        {
            _il.Emit(OpCodes.Ldloc, GetOrDeclareLocal(symbolId));
            return true;
        }
        if (_parameters.TryGetValue(symbolId, out var boxParam) && boxParam.IsBox)
        {
            _il.Emit(OpCodes.Ldarg, boxParam.ArgIndex);
            return true;
        }
        /* Parametro BYVAL catturato, materializzato in un ByRefBox<T> all'ingresso del
         * metodo (vedi BoxCapturedParameters): il box è un LOCAL di questa istanza. */
        if (_locals.TryGetValue(symbolId, out var boxedLocal) && IsByRefBoxFieldType(boxedLocal.LocalType))
        {
            _il.Emit(OpCodes.Ldloc, boxedLocal);
            return true;
        }
        return false;
    }

    /// <summary>
    /// NEW implicita del valore lambda: nessuna cattura → delegate legato a null su un
    /// metodo STATIC (ldnull+ldftn+newobj); con catture → istanzia il tipo di chiusura,
    /// copia lo snapshot corrente di ogni cattura in un campo (letta dal contesto
    /// CORRENTE, cioè questa istanza del compilatore — mai quello della lambda stessa),
    /// poi lega il delegate a QUELL'istanza.
    /// </summary>
    private void EmitLambdaCreation(LambdaExpr lambda)
    {
        if (!_lambdaInfos.TryGetValue(lambda, out var info))
            throw new NotSupportedException("Backend IL: lambda non scoperta da DiscoverAndCompileLambdas (bug interno).");

        // Delegate generico chiuso con un TypeBuilder nei type args (es. lambda con
        // parametro di una CLASS/UDT): GetConstructor diretto fallisce — ponte selettivo
        // (vedi EmitFunctionReference/EmitNewNativeCollection).
        var delegateCtor = ResolveDelegateCtor(info.DelegateType);

        if (info.ClosureType is null)
        {
            _il.Emit(OpCodes.Ldnull);
            _il.Emit(OpCodes.Ldftn, info.InvokeMethod);
            _il.Emit(OpCodes.Newobj, delegateCtor);
            return;
        }

        _il.Emit(OpCodes.Newobj, info.ClosureCtor!);
        foreach (var (symbolId, _, field) in info.Captures)
        {
            _il.Emit(OpCodes.Dup);
            if (IsByRefBoxFieldType(field.FieldType))
            {
                // Campo BOX (cattura mutata per riferimento): passa il BOX STESSO, non il
                // valore — la closure condivide l'identità col local/param del chiamante, così
                // le scritture dentro la lambda si propagano (bug closure_byref).
                if (!TryEmitBoxReference(symbolId))
                    throw new NotSupportedException($"Backend IL: campo box senza box sorgente (bug interno): sym={symbolId} '{_program.Symbols.GetSymbol(symbolId).OriginalToken.Lexeme}' type={_program.Symbols.GetSymbol(symbolId).DataType} isGlobal={IsGlobalSymbol(symbolId)} isStatic={IsStaticSymbol(symbolId)} inParams={_parameters.ContainsKey(symbolId)} boxed={BoxedSymbols.Contains(symbolId)}");
            }
            else
            {
                EmitLoadVariable(symbolId);
            }
            _il.Emit(OpCodes.Stfld, field);
        }
        // Niente Dup qui: Ldftn non consuma lo stack (il metodo è un operando IL, non un
        // valore preso dallo stack) — l'istanza di chiusura lasciata da sopra resta
        // sotto il method pointer appena spinto, esattamente l'ordine [target, methodPtr]
        // che newobj(object, IntPtr) del delegate si aspetta. Un Dup qui lascerebbe
        // un'istanza di chiusura IN PIÙ sullo stack dopo la newobj del delegate — stack
        // sbilanciato, InvalidProgramException a runtime (verificato empiricamente).
        _il.Emit(OpCodes.Ldftn, info.InvokeMethod);
        _il.Emit(OpCodes.Newobj, delegateCtor);
    }

    /// <summary>
    /// EVAL({ |p| ... }, args...) con lambda INLINE come target: materializza la lambda
    /// (EmitLambdaCreation) e la invoca via delegate, come la CALL indiretta su variabile
    /// (il Linker lascia EvalExpr quando il target non è una variabile — vedi
    /// Linker.Rewrites.Eval.cs).
    /// </summary>
    /// <summary>Tipo funzione del target di un EVAL non-lambda (es. `EVAL(Mk(), 5)`).</summary>
    private static FunctionPointerType? ResolveEvalTargetFpt(Expr target) => target switch
    {
        FunctionExpr fe => fe.ReturnType as FunctionPointerType
            ?? (fe.ResolvedDefinition as FunctionDefinition)?.ReturnType as FunctionPointerType,
        // EVAL(EVAL(…, a), b): il target è un altro EVAL il cui risultato è un block — la
        // firma è il ReturnType della catena (ricorsivo). Senza, il 3° livello cadeva in
        // "espressione 'EvalExpr' non ancora supportata" (lambda_nested_capture).
        EvalExpr ev => ResolveEvalTargetFpt(ev.Target)?.ReturnType as FunctionPointerType,
        _ => null
    };

    private void EmitInlineLambdaEval(EvalExpr ev, LambdaExpr le)
    {
        EmitLambdaCreation(le);
        foreach (var arg in ev.Args) EmitExpression(arg);
        if (!_lambdaInfos.TryGetValue(le, out var info))
            throw new NotSupportedException("Backend IL: lambda non scoperta da DiscoverAndCompileLambdas (bug interno).");
        var invokeMethod = ResolveDelegateInvoke(info.DelegateType);
        _il.Emit(OpCodes.Callvirt, invokeMethod);
    }

    /// <summary>
    /// CALL indiretta (<see cref="FunctionExpr.IsIndirect"/>) — una variabile/parametro di
    /// tipo CALLBACK/FUNCTION() invocata come DELEGATE.Invoke(args), non una `call`
    /// diretta a un MethodBuilder noto (quella resta EmitFunctionCall).
    /// </summary>
    /// <summary>Costruttore (object, IntPtr) del tipo delegate: gestisce i delegate definiti a
    /// runtime (arity &gt; 16, vedi DynamicDelegateTypes), per cui <c>GetConstructor</c> su un
    /// TypeBuilder non ancora creato lancia, e i generici chiusi che contengono un TypeBuilder
    /// (ponte selettivo).</summary>
    private ConstructorInfo ResolveDelegateCtor(Type delegateType)
    {
        if (_types.DynamicDelegateMembers != null && _types.DynamicDelegateMembers.TryGetValue(delegateType, out var dyn))
            return dyn.Ctor;
        if (ClosedGenericContainsTypeBuilder(delegateType))
            return TypeBuilder.GetConstructor(delegateType,
                delegateType.GetGenericTypeDefinition().GetConstructor([_types.Object, typeof(IntPtr)])!);
        return delegateType.GetConstructor([_types.Object, typeof(IntPtr)])
            ?? throw new NotSupportedException($"Backend IL: costruttore del delegate '{delegateType}' non trovato.");
    }

    /// <summary>Metodo Invoke del tipo delegate (vedi <see cref="ResolveDelegateCtor"/> per i
    /// delegate definiti a runtime).</summary>
    private MethodInfo ResolveDelegateInvoke(Type delegateType)
    {
        if (_types.DynamicDelegateMembers != null && _types.DynamicDelegateMembers.TryGetValue(delegateType, out var dyn))
            return dyn.Invoke;
        return delegateType.GetMethod("Invoke")
            ?? throw new NotSupportedException($"Backend IL: metodo Invoke non trovato sul delegate '{delegateType}'.");
    }

    private LocalBuilder? FindClosureContextLocal(Type closureDeclaringType)
    {
        foreach (var kv in _locals)
        {
            var sym = _program.Symbols.GetSymbol(kv.Key);
            var dt = NormalizeInferredType(sym.DataType);
            if (dt is not ClassType and not ClassDefinition) continue;
            try
            {
                if (DataTypeMapper.Map(dt, _types, _classTypes) == closureDeclaringType)
                    return kv.Value;
            }
            catch (NotSupportedException) { /* tipo non mappabile: ignora */ }
        }
        return null;
    }

}
