// FILE: IlBackend/Partial/IlProgramCompiler.ArrayOps.cs
// PARTIAL — dominio ARRAY OPERATIONS (A-* statement, array helpers, indici).
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

/// <summary>Partial del transpiler IL: dominio ARRAY OPERATIONS.</summary>
public sealed partial class IlProgramCompiler
{


    /// <summary>
    /// AADD(array, valore): Array.Resize (stesso metodo generico aperto usato da REDIM
    /// PRESERVE) muta il locale IN PLACE via ref, copiando già il contenuto esistente — non
    /// serve un Array.Copy manuale come per AINS/ADEL, che invece spostano un blocco a un
    /// offset diverso da zero.
    /// </summary>

    /// <summary>
    /// AINS(array, posizione): alloca un nuovo array di lunghezza+1, copia [0,posizione) e
    /// [posizione, oldLen) con Array.Copy(Array,int,Array,int,int) shiftando il secondo
    /// blocco di +1, lasciando lo slot a `posizione` al default CLR (Newarr azzera già tutto
    /// — mai scritto esplicitamente, stessa semantica "elemento vuoto" dell'Interpreter).
    /// </summary>

    /// <summary>ADEL(array, posizione): speculare a AINS — nuovo array di lunghezza-1, il blocco dopo la posizione si sposta indietro di 1.</summary>

    /// <summary>
    /// AFILL(array, valore [, start] [, count]): mutazione IN PLACE (mai una riassegnazione
    /// di riferimento, quindi nessun writeback BYREF necessario — il chiamante vede già la
    /// mutazione attraverso il riferimento condiviso). Il valore è valutato UNA SOLA VOLTA
    /// prima del loop, mai ri-eseguito per ogni elemento (importante se l'espressione ha
    /// effetti collaterali).
    /// </summary>

    /// <summary>
    /// ACOPY(sorgente, destinazione [, sorgStart] [, count] [, destStart]): un blocco di
    /// elementi copiato via Array.Copy(Array,int,Array,int,int) — mai una riassegnazione di
    /// riferimento (la destinazione resta lo stesso array, muta in place), quindi nessun
    /// writeback BYREF necessario. Count di default: il minimo tra lo spazio restante nella
    /// sorgente e nella destinazione dai rispettivi start (calcolato via branch, non
    /// Math.Min, per evitare un'altra dipendenza BCL cross-"mondo").
    /// </summary>

    /// <summary>
    /// Tipo di ritorno di una lambda letterale o variabile FUNCTION-pointer passata come
    /// callback — mai InferType(Expr) direttamente su una LambdaExpr (non gestita, lancia).
    /// Condiviso da AMAP e ASORT (compareFunction).
    /// </summary>
    private DataType GetCallbackReturnType(Expr arg) => arg switch
    {
        LambdaExpr le => le.ReturnType,
        FunctionReferenceExpr fr => _program.Symbols.GetSymbol(fr.SymbolId).DeclarationNode is FunctionDefinition frDef
            ? frDef.ReturnType
            : throw new NotSupportedException("Backend IL: riferimento a funzione non risolvibile come callback."),
        VariableExpr v when _program.Symbols.GetSymbol(v.SymbolId).DataType is FunctionPointerType fpt => fpt.ReturnType,
        _ => throw new NotSupportedException("Backend IL: atteso una lambda letterale o una variabile FUNCTION-pointer come callback.")
    };

    /// <summary>Arità (numero di parametri) di una lambda letterale o variabile FUNCTION-pointer — usato da AEVAL per scegliere tra Action&lt;T&gt; (1 parametro) e Action&lt;T,Long&gt; (2, con indice).</summary>
    private int GetCallbackArity(Expr arg) => arg switch
    {
        LambdaExpr le => le.Parameters.Length,
        FunctionReferenceExpr fr2 => _program.Symbols.GetSymbol(fr2.SymbolId).DeclarationNode is FunctionDefinition frDef2
            ? frDef2.Parameters.Length
            : throw new NotSupportedException("Backend IL: riferimento a funzione non risolvibile come callback."),
        VariableExpr v when _program.Symbols.GetSymbol(v.SymbolId).DataType is FunctionPointerType fpt => fpt.ParameterTypes.Length,
        _ => throw new NotSupportedException("Backend IL: atteso una lambda letterale o una variabile FUNCTION-pointer come callback.")
    };

    /// <summary>
    /// ASORT(array [, compareFunction] [, order$]): in-place, mai una riassegnazione di
    /// riferimento — nessun writeback BYREF necessario. order$ è valutato a RUNTIME (non
    /// richiede una costante compile-time: "D" letterale o una variabile stringa vanno
    /// bene ugualmente) e confrontato con "D" per il flag descending; la presenza o assenza
    /// di CompareFunction invece è nota a compile-time (decide quale overload chiamare).
    /// </summary>

    /// <summary>ASHUFFLE array: mescola l'array originale in place (Fisher-Yates).</summary>

    /// <summary>Spinge un bool: true se order$ (se presente) vale "D" (Descending), false altrimenti (anche se assente).</summary>

    /// <summary>
    /// AEVAL(array, azione): itera invocando una SUB/lambda per ogni elemento — 1 parametro
    /// (solo il valore) o 2 (valore + indice Long 0-based), scelto in base all'arità
    /// dichiarata del callback (nota a compile-time), stesso dualismo dell'Interpreter
    /// (Closure.Procedure.Parameters.Length == 2).
    /// </summary>

    /// <summary>
    /// Valuta un'espressione come intero costante NOTO A COMPILE-TIME quando possibile
    /// (usato per il LOWER BOUND di DIM/REDIM, che resta sempre compile-time — vedi
    /// EmitArrayDeclaration/EmitAxisLength). Copertura: letterali numerici, CLNG(...)
    /// inserito implicitamente dal Linker (vedi EmitClngCall), e negazione unaria.
    /// </summary>


    /// <summary>
    /// Emette sullo stack IL la LUNGHEZZA di un asse di DIM/REDIM (per Newarr/il
    /// costruttore dell'array rettangolare/Array.Resize, che vogliono tutti un int32
    /// sullo stack, costante o no). Se il limite superiore è noto a compile-time (caso
    /// comune: letterale/CLNG/negazione) spinge una costante come prima — nessun cambio
    /// di IL generato per il caso già supportato. Altrimenti valuta l'espressione a
    /// RUNTIME (es. <c>DIM A(N)</c> con N variabile dopo un INPUT/calcolo) e calcola
    /// length = CInt(upperBoundExpr) - lowerBound + 1 con aritmetica IL. Il lowerBound è
    /// sempre l'intero compile-time già risolto (mai un'espressione qui).
    /// </summary>


    /// <summary>
    /// Carica sullo stack il riferimento all'array e, per ciascun asse, l'indice
    /// (shiftato del limite inferiore se diverso da 0 — vedi EmitArrayDeclaration).
    /// Condiviso da lettura (Ldelem/Get) e scrittura (Stelem/Set): entrambe vogliono
    /// [array, idx0, ..., idxN-1] sullo stack prima dell'operazione finale.
    /// </summary>

    /// <summary>Carica il riferimento all'array: dal LoadRef (array STATIC nel bag) oppure
    /// dal local registrato in <see cref="ArrayInfo"/>.</summary>

    /// <summary>
    /// Come sopra, ma prende un delegato per caricare il riferimento all'array invece di
    /// un LocalBuilder fisso — serve per i campi array di classe (Ldfld su un'istanza
    /// invece di Ldloc su un locale, vedi TryEmitFieldArrayIndexedAccess/
    /// EmitFieldArrayIndexedAssignment), senza duplicare la logica di shift degli indici.
    /// </summary>


    /// <summary>
    /// Variante dell'assegnazione a elemento array in cui il valore arriva da una
    /// sorgente runtime (READ, in futuro INPUT) anziché da una Expr. Gli indici vengono
    /// valutati una sola volta e restano sotto il valore sullo stack, nell'ordine esatto
    /// richiesto da stelem/Array.Set.
    /// </summary>

    /// <summary>
    /// Coppia (ldelem, stelem) per array RANK 1 per ciascun tipo elemento
    /// supportato — accesso diretto, elementi unboxed, nessuna indirezione.
    /// Array rank N usano invece i metodi Get/Set dell'array rettangolare
    /// (anch'essi senza boxing per il tipo elemento reale).
    /// </summary>
    /// <summary>
    /// Emette il singolo `ldelem` giusto per rank 1. I tipi scalari noti hanno un opcode
    /// specializzato dedicato (niente token di tipo, il più veloce possibile); STRING/
    /// CLASS/INTERFACE (tutti reference type CLR) condividono `Ldelem_Ref` — un riferimento
    /// è un riferimento, nessun token di tipo necessario per caricarlo. Ogni altro tipo
    /// (in pratica: un UDT, un VALUE TYPE la cui dimensione non è fissa/nota a priori come
    /// per i primitivi elencati) usa la forma GENERICA `ldelem &lt;token&gt;`, l'unica che
    /// IL offre per un value type arbitrario in un array — scoperto testando gli array di
    /// UDT, mai esercitato prima (nessun array di CLASS/UDT testato in precedenza).
    /// </summary>

    /// <summary>Simmetrico a EmitArrayElementLoad, per `stelem` — vedi lì per il perché del case `default`.</summary>

    /// <summary>
    /// Un DICT top-level (es. <c>result("chiave")</c>) usa la STESSA sintassi a parentesi
    /// di un array (ArrayAccessExpr/ArrayElementTarget) — il Linker non li distingue per
    /// tipo di nodo, solo il DataType del simbolo li discrimina. Instrada verso l'indexer
    /// CLR (Dictionary&lt;TKey,TValue&gt;.Item), riusando la stessa infrastruttura già
    /// esistente per l'indicizzazione di collezioni via IndexedAccessExpr (vedi
    /// TryEmitNativeCollectionIndexedAccess/Assignment) — qui applicata a un simbolo
    /// semplice invece che a un'espressione Base.
    /// </summary>


    private void EmitExpressionStatement(ExpressionStmt stmt)
    {
        var resultType = InferType(stmt.Expression);
        EmitExpression(stmt.Expression);
        // Metodo dinamico (CollectionInvoke — istanza HTTP/AnyType/collezione boxed):
        // il runtime ritorna object ma come STATEMENT è void-like — pop SEMPRE
        // (InferType ora dà AnyType, che NON farebbe scattare il pop sotto).
        if (resultType is not VoidType
            || stmt.Expression is MethodCallExpr mcDyn && mcDyn.ResolvedDefinition is null && IsDynamicDispatchInstance(mcDyn.Instance))
            _il.Emit(OpCodes.Pop);
    }

    private bool IsDynamicDispatchInstance(Expr instance)
    {
        return NormalizeInferredType(InferType(instance)) is AnyType or HttpRequestType or HttpResponseType
            or HttpOptionsType or HttpHeadersType or HttpConfigType or HttpPostedDataType
            or WebSocketType or SqlResultSetType or FetchResultType or CookieJarType
            or TreeType or AVLTreeType or RedBlackTreeType or BinaryIndexedTreeType or TrieType
            or BPlusTreeType or GraphType or HashRingType or SparseSetType or BinaryHeapType
            or BagType or CacheType or TimelineType or RingBufferType;
    }

    /// <summary>
    /// Emette una CONDIZIONE come valore booleano int32 valido per brtrue/brfalse.
    /// Il valore su cui si salta deve essere int32 (o reference): un `double` boxato
    /// restante sullo stack dà IL invalido che il JIT a volte tollera nei metodi corti ma
    /// rifiuta nei metodi grandi (es. `IF ISAM.FOUND(x) THEN` — DbfFound/IsamFound
    /// ritornano double -1/0; Proc_601 di DbIsamFullTests esplodeva proprio qui).
    /// Una condizione numerica viene normalizzata a int32: 0 → false, != 0 → true.
    /// </summary>
    /// <summary>`IlGpmRuntime.IsTruthy(object)`: verità BASIC su un valore riferimento
    /// (null→falso, stringa vuota→falso, bool → -1/0, altrimenti numero ≠ 0).</summary>
    private static readonly MethodInfo TruthyMethod =
        typeof(IlGpmRuntime).GetMethod(nameof(IlGpmRuntime.IsTruthy))!;

    /// <summary>True se il valore emesso per il simbolo è un RIFERIMENTO (local/param object
    /// boxato) e non un value type grezzo: decide se una condizione va normalizzata via
    /// <c>IlRuntime.ToDouble</c> invece di essere testata con <c>brtrue/brfalse</c>.</summary>
    private bool EmittedValueIsObject(int symbolId)
        => (_locals.TryGetValue(symbolId, out var local) && local.LocalType == _types.Object)
           || (_parameters.TryGetValue(symbolId, out var param) && param.IsBox);

    private void EmitConditionBool(Expr condition)
    {
        EmitExpression(condition);
        var condType = NormalizeInferredType(InferType(condition));

        // Condizioni su un valore RIFERIMENTO: la verità va decisa dal VALORE, mai dalla
        // presenza del riferimento. Vale per stringhe (`IF s$ THEN`: "" è FALSO), per i valori
        // DINAMICI (ANY/JSON) e per un BIT finito in uno storage OBJECT (variabile con tipo
        // "ridefinito": `DIM d AS INTEGER` poi assegnata da un builtin BIT come
        // IO.DIREXISTS/FILEEXISTS → bool boxato). Prima `brfalse` testava il riferimento —
        // sempre non-null, quindi sempre VERO: `IF s$ THEN` con "" entrava nel ramo THEN e
        // `IF d THEN` con d = 0 pure (bug app DigUp: indice a 0 file su IL). Semantica
        // allineata a PCode/C99Native via `IlGpmRuntime.IsTruthy` (null→falso, stringa vuota
        // →falso, bool → -1/0, altrimenti numero ≠ 0).
        if (condType is StringType or AnyType or Basic.JsonType
            || (condType is BitType && condition is VariableExpr bitVar && EmittedValueIsObject(bitVar.SymbolId)))
        {
            _il.Emit(OpCodes.Call, TruthyMethod);
            return;
        }
        if (condType is not StringType and not Basic.JsonType and not AnyType
            and not BitType
            and not UnresolvedQualifiedType)
        {
            var clr = DataTypeMapper.Map(condType, _types, _classTypes);
            if (clr == typeof(double) || clr == typeof(float) || clr == typeof(int) || clr == typeof(long)
                || clr == typeof(short) || clr == typeof(byte) || clr == typeof(decimal))
            {
                // double → int32: value != 0. (0.0 = false, != 0 = true)
                IlConversion.EmitConvert(_il, _types, condType, clr, _types.Double);
                _il.Emit(OpCodes.Ldc_R8, 0.0);
                _il.Emit(OpCodes.Ceq);
                _il.Emit(OpCodes.Ldc_I4_0);
                _il.Emit(OpCodes.Ceq);
            }
        }
    }

    private void EmitIfStatement(IfStmt stmt)
    {
        var endLabel = _il.DefineLabel();
        foreach (var clause in stmt.IfClauses)
        {
            var nextClauseLabel = _il.DefineLabel();
            EmitConditionBool(clause.Condition);
            _il.Emit(OpCodes.Brfalse, nextClauseLabel);
            foreach (var s in clause.Statements) EmitStatement(s);
            _il.Emit(OpCodes.Br, endLabel);
            _il.MarkLabel(nextClauseLabel);
        }

        if (stmt.ElseClause is not null)
            foreach (var s in stmt.ElseClause.Statements) EmitStatement(s);

        _il.MarkLabel(endLabel);
    }

    /// <summary>
    /// SELECT CASE: l'espressione viene valutata UNA SOLA VOLTA (stesso motivo di
    /// FOR Start/End/Step — evita side-effect ripetuti e rivalutazioni costose) e salvata
    /// in un locale temporaneo; ogni CaseClause può avere PIÙ test (es. "CASE 1, 2, 3" —
    /// <see cref="CaseClause.Tests"/>), OR-ati tra loro: se UNO qualsiasi combacia si salta
    /// al corpo. Nessun fall-through tra clausole (ogni corpo termina con un salto alla
    /// fine dello statement) — stessa semantica del p-code Compiler
    /// (Compiler.ControlFlow.cs, VisitSelectStatement).
    /// </summary>
    private void EmitSelectStatement(SelectStmt stmt)
    {
        EmitExpression(stmt.Expression);
        _il.Emit(OpCodes.Conv_R8);
        var selectLocal = _il.DeclareLocal(typeof(double));
        _il.Emit(OpCodes.Stloc, selectLocal);

        var isString = InferType(stmt.Expression) is StringType;
        var endLabel = _il.DefineLabel();
        foreach (var caseClause in stmt.CaseClauses)
        {
            var bodyLabel = _il.DefineLabel();
            var nextClauseLabel = _il.DefineLabel();
            foreach (var test in caseClause.Tests)
            {
                EmitCaseTestMatches(test, selectLocal, isString);
                _il.Emit(OpCodes.Brtrue, bodyLabel);
            }
            _il.Emit(OpCodes.Br, nextClauseLabel);

            _il.MarkLabel(bodyLabel);
            foreach (var s in caseClause.Statements) EmitStatement(s);
            _il.Emit(OpCodes.Br, endLabel);

            _il.MarkLabel(nextClauseLabel);
        }

        if (stmt.ElseClause is not null)
            foreach (var s in stmt.ElseClause.Statements) EmitStatement(s);

        _il.MarkLabel(endLabel);
    }

    /// <summary>
    /// MATCH...END MATCH: stessa struttura logica di SELECT CASE (espressione valutata
    /// una sola volta, nessun fall-through), ma UN solo test per clausola (mai una lista
    /// OR-ata) con più forme possibili — vedi <see cref="MatchCaseClause"/>. Copertura
    /// attuale: valore semplice, intervallo, comparazione (stesso nucleo di SELECT CASE,
    /// <see cref="EmitCaseTestMatches"/>), <c>CASE IS NOTHING</c> (confronto diretto con
    /// null — solo per un tipo reference, un value type non passerebbe mai comunque il
    /// binding a runtime) e <c>CASE _</c> (wildcard, sempre vero). ESPLICITAMENTE non
    /// supportati: pattern di TIPO (<c>CASE IS Tipo [AS var]</c>) e pattern di valore con
    /// cattura di variabile (<c>CASE valore AS var</c>) — entrambi richiederebbero
    /// `isinst`/binding di una nuova variabile locale con scope limitato alla clausola,
    /// non ancora progettato in questo backend; lanciano esplicito piuttosto che
    /// approssimare silenziosamente (a differenza del p-code Compiler, che per
    /// CaseTypePattern oggi it fa match SEMPRE senza controllare il tipo — comportamento
    /// noto ma non replicato qui di proposito).
    /// </summary>
    private void EmitMatchStatement(MatchStmt stmt)
    {
        if (stmt.Cases.Any(c => c.TypePattern is not null || c.ValuePattern is not null))
            throw new NotSupportedException("Backend IL: MATCH con CASE IS <tipo> o CASE valore AS variabile (binding di variabile) non ancora supportato.");

        var matchType = InferType(stmt.Expression);
        var isString = matchType is StringType;
        EmitExpression(stmt.Expression);
        // Normalizza il subject del MATCH a double se NUMERICO: i CASE (es. literal 4)
        // sono valutati come double, quindi un subject Int16 (es. FUNCTION AS INTEGER)
        // confrontato via ceq con un double = IL invalido (ceq richiede operandi dello
        // stesso tipo) — ConstStructuralCanary/CanaryMatchTuple.
        if (matchType is not StringType && DataTypeMapper.Map(matchType, _types, _classTypes) != _types.Double
            && IlConversion.IsNumeric(matchType))
        {
            _il.Emit(OpCodes.Conv_R8);
            matchType = new DoubleType();
        }
        var matchLocal = _il.DeclareLocal(DataTypeMapper.Map(matchType, _types, _classTypes));
        _il.Emit(OpCodes.Stloc, matchLocal);

        var endLabel = _il.DefineLabel();
        foreach (var mc in stmt.Cases)
        {
            var isWildcard = mc.WildcardToken is not null && mc.WildcardToken.Kind != TokenKind.Empty;
            if (isWildcard)
            {
                foreach (var s in mc.Statements) EmitStatement(s);
                _il.Emit(OpCodes.Br, endLabel);
                continue;
            }

            var nextCaseLabel = _il.DefineLabel();
            if (mc.NullPattern is not null)
            {
                _il.Emit(OpCodes.Ldloc, matchLocal);
                _il.Emit(OpCodes.Ldnull);
                _il.Emit(OpCodes.Ceq);
            }
            else if (mc.ValueTest is not null) EmitCaseTestMatches(mc.ValueTest, matchLocal, isString);
            else if (mc.RangeTest is not null) EmitCaseTestMatches(mc.RangeTest, matchLocal, isString);
            else if (mc.ComparisonTest is not null) EmitCaseTestMatches(mc.ComparisonTest, matchLocal, isString);
            else throw new NotSupportedException("Backend IL: clausola CASE del MATCH senza alcun pattern riconosciuto.");

            _il.Emit(OpCodes.Brfalse, nextCaseLabel);
            foreach (var s in mc.Statements) EmitStatement(s);
            _il.Emit(OpCodes.Br, endLabel);

            _il.MarkLabel(nextCaseLabel);
        }

        _il.MarkLabel(endLabel);
    }

    /// <summary>
    /// Nucleo condiviso da SELECT CASE (una lista di test OR-ati per clausola) e MATCH (un
    /// solo test per clausola): lascia un BOOLEANO (0/1) sullo stack che indica se
    /// <paramref name="valueLocal"/> soddisfa <paramref name="test"/> — mai un branch
    /// diretto, cosi' il chiamante decide da sé come combinarlo (Brtrue per un singolo OR
    /// in SELECT CASE, Brfalse per l'unico test di una clausola MATCH).
    /// <see cref="CaseTypePattern"/>/<see cref="CaseValuePattern"/> non supportati qui
    /// (nessun binding di variabile in questo backend) — lanciano esplicito, il chiamante
    /// MATCH li intercetta comunque prima con un controllo dedicato per un messaggio più
    /// chiaro; SELECT CASE non li produce mai (non fanno parte della sua grammatica).
    /// </summary>
}
