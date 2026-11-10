// FILE: IlProgramCompiler.TypeSystem.cs
// PARTIAL — dominio TYPESYSTEM/INFER (deduzione e normalizzazione dei tipi CLR/BASIC).
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

/// <summary>Partial del transpiler IL: dominio TYPESYSTEM/INFER.</summary>
public sealed partial class IlProgramCompiler
{

    private static DataType NormalizeInferredType(DataType type) => type is InferredType ? new DoubleType() : type;

    /// <summary>Chiave di accesso a JSON/ANY di tipo DINAMICO: espressione ANY, oppure variabile il cui simbolo ha tipo ANY/InferredType (es. la
    /// variabile di un FOR EACH su JSON.KEYS: InferType la normalizza a DOUBLE ma il valore e' un object, per lo piu' una stringa).</summary>
    private bool IsDynamicJsonKey(Expr key)
    {
        if (NormalizeInferredType(InferType(key)) is AnyType or Basic.JsonType) return true;
        return key is VariableExpr ve && ve.SymbolId >= 0
            && _program.Symbols.GetSymbol(ve.SymbolId).DataType is InferredType or AnyType or Basic.JsonType;
    }

    /// <summary>
    /// Tipo del valore prodotto da un FunctionReferenceExpr — deve combaciare ESATTAMENTE
    /// con ciò che EmitExpression realmente emette per lo stesso nodo (vedi
    /// IlProgramCompiler.Expressions.cs, dispatch su IsCallbackFunctionReferenceCore):
    /// un vero riferimento callback (parametri > 0, o funzione locale catturata/promossa a
    /// closure) resta un delegate opaco (AnyType, coerente con EmitFunctionReference); un
    /// riferimento bruto a 0 argomenti diventa una CHIAMATA reale (EmitFunctionReferenceAsCall)
    /// e quindi il tipo va risolto sul ReturnType dichiarato del target (VoidType per una
    /// SUB) — mai un AnyType fisso, altrimenti il chiamante (EmitExpressionStatement) POPa
    /// un valore mai spinto sullo stack da una CALL void → IL invalido.
    /// </summary>
    private DataType InferFunctionReferenceType(FunctionReferenceExpr fr)
    {
        if (IsCallbackFunctionReferenceCore(fr.SymbolId, _program)) return new AnyType();
        var decl = fr.SymbolId >= 0 ? _program.Symbols.GetSymbol(fr.SymbolId).DeclarationNode : null;
        return decl switch
        {
            FunctionDefinition fd => fd.ReturnType,
            NativeProcedureDefinition npd => npd.ReturnType,
            _ => new AnyType()
        };
    }

    /// <summary>True se il symbol è un CAMPO di una classe __ClosureCtx (cattura BYVAL
    /// heap-promotata dal Linker). Le catture BYREF non sono promuovibili (campo ≠ T&amp;).</summary>
    private static bool IsSymbolInClosureContext(BasicProgram program, int symbolId)
    {
        var scope = program.Symbols.GetScope(program.Symbols.GetSymbol(symbolId).ScopeId);
        if (scope.Type != SymbolTable.ScopeType.Class) return false;
        var owner = program.Symbols.GetSymbol(scope.OwnerSymbolId);
        return owner.OriginalName.StartsWith("__ClosureCtx", StringComparison.Ordinal);
    }

    /// <summary>Unwrap di NullableType (Contatto?) → tipo elemento (Contatto), ricorsivo.</summary>
    private static DataType UnwrapNullableType(DataType type) => type is NullableType nt ? UnwrapNullableType(nt.ElementType) : type;

    /// <summary>Tipo del risultato di un binario Date/Time/TimeSpan.</summary>
    private DataType InferDateTimeBinaryType(BinaryExpr b)
    {
        var l = NormalizeInferredType(InferType(b.Left));
        var r = NormalizeInferredType(InferType(b.Right));
        // Date/DateTime - Date/DateTime → TimeSpan
        if (b.Op.Kind == TokenKind.Minus && l is DateType or DateTimeType && r is DateType or DateTimeType)
            return new TimeSpanType();
        // Date/DateTime + Time → DateTime
        if (b.Op.Kind == TokenKind.Plus && (l is DateType or DateTimeType) && r is TimeType)
            return new DateTimeType();
        // Date/DateTime ± TimeSpan → Date/DateTime
        if (l is TimeSpanType || r is TimeSpanType)
            return l is DateType or DateTimeType ? l : r is DateType or DateTimeType ? r : new TimeSpanType();
        // Date/DateTime ± numero (giorni) → Date/DateTime
        if (l is DateType or DateTimeType) return l;
        if (r is DateType or DateTimeType) return r;
        return new DateTimeType();
    }

    /// <summary>Kind per IlRuntime.DateBinary: 0=Date/DateTime, 1=Time, 2=TimeSpan, 3=numero.</summary>
    private static double TypeKindOf(DataType t) => t switch
    {
        DateType or DateTimeType => 0,
        TimeType => 1,
        TimeSpanType => 2,
        _ => 3
    };

    /// <summary>Risolve il tipo di ritorno di una chiamata builtin. Il Linker materializza il
    /// ReturnType reale quando la NativeProcedureDefinition è risolta (IMPORT del namespace o
    /// scope materializzato), ma SENZA IMPORT il ReturnType resta InferredType: il tipo sullo
    /// stack (es. long da IlRuntime.MemoryMap) non combacia con quello che la conversione
    /// successiva si aspetta (double da NormalizeInferredType) → Conv_I8 su un long non boxed
    /// → InvalidProgramException. Fallback al registry builtin per (namespace, nome, arità).</summary>
    private DataType InferBuiltinFunctionCallType(BuiltinFunctionCallExpr f)
    {
        // ABS: il risultato e SEMPRE numerico (double), anche con argomento BIT
        // (ABS(TRUE) = 1). Senza, il chiamante inferiva il tipo dell argomento (Bit) e
        // applicava la conversione bool->numerico su un double (mul double*int32 =
        // "Invalid operand types in BinaryInst", ABS(<BIT>) su GPM).
        if (f.Name.Kind is TokenKind.Abs) return new DoubleType();
        // POOL.GET(p): PCodePool.Get ritorna un `object` (valore dinamico). Dichiararlo AnyType
        // (→ System.Object) fa convertire al consumatore con il dispatch runtime sul tipo reale;
        // con il ReturnType InferredType (→ double) lo stack aveva object dove si attendeva double
        // → InvalidProgramException su `n = POOL.GET(p)`.
        if (f.Name.Kind is TokenKind.PoolGet or TokenKind.Next) return new AnyType();
        // Conversioni numeriche esplicite/implicite (CINT/CBYTE/CLNG/CDBL/...): il tipo
        // di ritorno è il tipo TARGET della conversione, NON il default Double. Senza,
        // un argomento già coercedo dal Linker (CINT(2) per un param INTEGER) veniva
        // reinferito Double -> il call site (EmitFunctionCall) emetteva una SECONDA
        // coerce double->Int16 su un valore già Int16 = Convert.ToInt16(double) con un
        // short sullo stack = InvalidProgramException (MATCH con FUNCTION AS INTEGER).
        if (f.Name.Kind is TokenKind.CInt) return new IntegerType();
        if (f.Name.Kind is TokenKind.CByte) return new ByteType();
        if (f.Name.Kind is TokenKind.CLng) return new LongType();
        if (f.Name.Kind is TokenKind.CDbl) return new DoubleType();
        // CSNG produce un FLOAT (conv.r4) — il tipo inferito deve essere SingleType, non
        // DoubleType: con DoubleType il box del PRINT era .Double → il float esteso a double
        // veniva formattato G15 ("1.23456788063049") invece di G9 del float ("1.23456788").
        // Golden TypesConversions 'CSNG(1.23456789) = 1.23456788'.
        if (f.Name.Kind is TokenKind.CSng) return new DoubleType();
        // Data/ora: TODAY()/RDD.TODAY()/CTOD() ritornano DateTime CLR (DateType). Senza,
        // una variabile implicita (double) riceveva il DateTime senza coerce → reinterpretato
        // come double denormal (RDD_Complete 'TODAY: -6.0E-266'). NOTA: TokenKind.Date è
        // DATE$() (stringa), NON una data — resta fuori da questo caso (ClipperSayTest).
        if (f.Name.Kind is TokenKind.Today or TokenKind.RddToday
            or TokenKind.Ctod or TokenKind.RddCtod)
            return new DateType();
        // DTOC() ritorna stringa (RddDtoc) — una variabile implicita 's' riceverebbe la
        // stringa senza coerce → reinterpretata come double denormal (RDD_Complete 'DTOC').
        if (f.Name.Kind is TokenKind.Dtoc or TokenKind.RddDtoc)
            return new StringType();
        // SQL.EXEC(alias, sql[, params]) ritorna i changes (double). Senza questo caso il
        // tipo del builtin restava ANY → un `DIM x = SQL.EXEC(...)` (senza AS) creava un
        // locale object e la store/read del double si corrompeva (osservato con
        // ON ERROR RESUME NEXT: valore ~4.6e18). Con AS DOUBLE funzionava già.
        if (f.Name.Kind is TokenKind.SqlExec)
            return new DoubleType();
        // Builtin di configurazione DBF/ISAM/SPEED che ritornano -1/0 (double).
        // NB: ALIAS() NON sta qui — ritorna una STRINGA (il nome dell'area, StandardLibrary
        // dichiara StringType e IlRuntime.DbfAlias ritorna string): con Alias in questa lista
        // il type system credeva "double" e PRINT convertiva il riferimento alla stringa in un
        // numero, stampando un indirizzo (es. 4406279896). § MiniTests/dbf/dbf_state.bas.
        // Stessa classe di bug per SPEED.SETDURABILITY: la StdLib dichiara StringType e
        // IlRuntime.SpeedSetDurability ritorna la modalità ("Full"), ma qui era forzato a
        // Double → PRINT convertiva il riferimento alla stringa in un numero (5163367424)
        // e l'assegnazione a una variabile STRING generava IL invalido.
        if (f.Name.Kind is TokenKind.DbfSetDurability or TokenKind.DbfFlushDurability
            or TokenKind.SpeedFlushDurability
            or TokenKind.IsamSetReplication
            or TokenKind.Used
            or TokenKind.DbfBeginGroupCommit or TokenKind.DbfEndGroupCommit
            or TokenKind.SpeedBeginGroupCommit or TokenKind.SpeedEndGroupCommit)
            return new DoubleType();
        // SETREPLICATION ritorna il PATH del log (stringa), APPLYREPLICATION ritorna un
        // contatore (StdLib: LongType, IlRuntime.DbfApplyReplication: long). Con APPLY qui
        // dentro il type system credeva "stringa": la CALL lasciava un int64 dove il
        // consumatore si aspettava un riferimento → InvalidProgramException sul backend IL
        // (§ MiniTests/dbf/dbf_replication.bas, isolato per bisect).
        // ISAM.INTEGRITY ritorna il TESTO JSON (IlRuntime.IsamIntegrity → string): con
        // DoubleType il consumatore trattava il riferimento come numero → IL invalido
        // (§ MiniTests/dbf/isam_replication.bas, `DIM checkJson AS STRING = ISAM.INTEGRITY(...)`).
        if (f.Name.Kind is TokenKind.IsamIntegrity)
            return new StringType();
        if (f.Name.Kind is TokenKind.DbfSetReplication)
            return new StringType();
        if (f.ReturnType is not InferredType)
            return NormalizeInferredType(f.ReturnType);
        if (StdLib.Instance.BuiltinProcedures.TryGetValue(f.Namespace.Lexeme, out var procs))
        {
            foreach (var p in procs)
            {
                if (p is NativeProcedureDefinition npd &&
                    npd.OriginalName.Lexeme.Equals(f.Name.Lexeme, StringComparison.OrdinalIgnoreCase) &&
                    npd.ReturnType is not VoidType &&
                    p.Parameters.Length == f.Args.Length)
                    return NormalizeInferredType(npd.ReturnType);
            }
        }
        return new DoubleType();
    }

    /// <summary>
    /// Inferenza di tipo minima per espressioni: il Linker non annota un DataType
    /// inline su ogni nodo Expr (solo su Target/chiamate — vedi ricerca preliminare
    /// in TODO Obsidian, Filone B), quindi il backend deve dedurlo strutturalmente.
    /// Copertura attuale limitata ai nodi già gestiti da EmitExpression.
    /// </summary>
    private DataType InferVariableType(VariableExpr v)
    {
        foreach (var (varName, (arrInfo, _, _)) in _forEachState)
            if (varName == v.NameCleaned.Lexeme || varName == v.NameSuffixed.Lexeme)
                return arrInfo.ElementDataType;
        // SELF/ME in un metodo/costruttore/accessor di classe o UDT: il Linker può lasciare
        // il DataType del simbolo SELF sintetico come InferredType (in particolare negli
        // accessor PROPERTY) — il tipo reale è quello della CLASSE corrente
        // (_currentClassSymbolId). Senza, EmitFieldAccess cade nel fallback "campo non
        // risolvibile" e SELF.X negli accessor UDT legge 0.
        if (TryResolveSelfArgIndex(v.SymbolId, out _) && _currentClassSymbolId is int selfClassId)
        {
            var selfUdt = _program.GetAllUserDefinedTypes()
                .FirstOrDefault(u => u.SymbolId == selfClassId);
            if (selfUdt is not null) return selfUdt;
            var selfClass = _program.GetAllClasses()
                .FirstOrDefault(c => c.SymbolId == selfClassId);
            if (selfClass is not null) return new ClassType(selfClass);
        }
        // Simbolo con local object boxed (ridefinizione $ / SafeCast AS?): il valore a
        // runtime è un boxed di tipo imprevedibile (double, string, null...) — il tipo
        // statico del simbolo NON corrisponde più al valore → AnyType (dispatch runtime
        // in EmitConvertToString/ToDouble, mai un convert statico che mismatcherebbe).
        if (_redefinedTypeSymbols.Contains(v.SymbolId)) return new AnyType();
        return NormalizeInferredType(_program.Symbols.GetSymbol(v.SymbolId).DataType);
    }

    /// <summary>
    /// Deduce i tipi di chiave e valore di un letterale dizionario, usando ANY quando
    /// le voci sono eterogenee invece di imporre il tipo della prima voce alle successive.
    /// </summary>
    /// <param name="dictionary">Letterale dizionario da analizzare.</param>
    /// <returns>Tipo dizionario compatibile con tutte le voci presenti.</returns>
    private DataType InferDictionaryLiteralType(DictionaryLiteralExpr dictionary)
    {
        if (dictionary.Entries.IsEmpty)
            return new DictType(new InferredType(), new InferredType());

        var keyType = NormalizeInferredType(InferType(dictionary.Entries[0].Key));
        var valueType = NormalizeInferredType(InferType(dictionary.Entries[0].Value));
        for (var i = 1; i < dictionary.Entries.Length; i++)
        {
            var nextKeyType = NormalizeInferredType(InferType(dictionary.Entries[i].Key));
            var nextValueType = NormalizeInferredType(InferType(dictionary.Entries[i].Value));
            if (nextKeyType.GetType() != keyType.GetType()) keyType = new AnyType();
            if (nextValueType.GetType() != valueType.GetType()) valueType = new AnyType();
        }
        return new DictType(keyType, valueType);
    }

    private DataType InferType(Expr expr)
    {
        return expr switch
        {
        NumberExpr => new DoubleType(),
        StringExpr => new StringType(),
        InterpolatedStringExpr => new StringType(),
        // (x*x FOR x IN src): la lista materializzata di oggetti (il FOR EACH la itera).
        GeneratorExpr => new ListType(new AnyType()),
        NilExpr => new AnyType(),
        AssignmentExpr ae => InferType(ae.Value),
        BooleanExpr => new BitType(),
        VariableExpr v => InferVariableType(v),
        // MakeByRefExpr (sintetico, Linker-only — Fase 4): il tipo BASIC è quello del simbolo
        // originale (T), MAI il box che lo rappresenta a livello CLR — l'inferenza qui serve
        // solo per l'eventuale coerce del chiamante (EmitFieldAssignment), che deve vedere il
        // tipo "logico" del campo, non l'incapsulamento interno.
        MakeByRefExpr mbr => _program.Symbols.GetSymbol(mbr.SymbolId).DataType,
        // NOT: BitType solo per operando booleano; per operandi numerici è bitwise
        // (~5 = -6, parità VM) -> DoubleType (box coerente in PRINT, TypesNumerics).
        // NOT produce SEMPRE double (Conv_I8; Not; Conv_R8, parità VM) — mai BitType
        // (il box bool su un double sarebbe IL invalido).
        UnaryExpr u when u.Op.Kind == TokenKind.Not => new DoubleType(),
        UnaryExpr u => InferType(u.Right),
        BinaryExpr b when b.Op.Kind == TokenKind.Plus &&
                           (InferType(b.Left) is StringType || InferType(b.Right) is StringType)
            => new StringType(),
        // `??` (null-coalescing): l'emit boxa ENTRAMBI i lati in object (il sinistro può
        // essere il risultato object di un accesso sicuro) e lascia un oggetto sullo stack.
        // Se il tipo inferito resta DoubleType il consumatore (DIM/PRINT) tratta quel
        // riferimento come double → garbage (il puntatore: misurato ~4.8e9 su
        // Collections/CollectionsSafeAccess, `d["x"]? ?? -1`). Il tipo è quindi SEMPRE
        // dinamico: il consumatore converte con ToDouble/ToString.
        BinaryExpr b when b.Op.Kind == TokenKind.QuestionMarkQuestion => new AnyType(),
        // "+" con un operando DINAMICO (AnyType, es. campo DBF nudo late-binding `BALANCE + 1`):
        // EmitBinaryExpression usa IlRuntime.DynamicAdd, che ritorna un object BOXED (numerico
        // oppure stringa, deciso a runtime). Il tipo risultante deve quindi essere AnyType e NON
        // il default DoubleType (riga sotto): con DoubleType il consumatore (PRINT/assegnazione)
        // leggeva il riferimento all'oggetto boxed come double → garbage (~4.9e9, il puntatore).
        // Il consumatore converte poi con ToDouble/ToString secondo il contesto.
        BinaryExpr b when b.Op.Kind == TokenKind.Plus &&
                           (NormalizeInferredType(InferType(b.Left)) is AnyType
                            || NormalizeInferredType(InferType(b.Right)) is AnyType)
            => new AnyType(),
        // Aritmetica Date/Time/TimeSpan: Date-Date→TimeSpan; Date+Time→DateTime; ±TimeSpan.
        BinaryExpr b when b.Op.Kind is TokenKind.Plus or TokenKind.Minus
            && (NormalizeInferredType(InferType(b.Left)) is DateType or TimeType or DateTimeType or TimeSpanType
                || NormalizeInferredType(InferType(b.Right)) is DateType or TimeType or DateTimeType or TimeSpanType)
            => InferDateTimeBinaryType(b),
        // AND/OR sono SEMPRE bitwise (parità VM, (long)&(long)): il risultato è un double
        // (Conv_R8 dopo l'opcode And/Or) — MAI BitType (prima BitType -> box Boolean su un
        // double = PrintFormatValue(-1) invece di 2, TypesNumerics).
        BinaryExpr b when IsComparisonOperator(b.Op.Kind) || b.Op.Kind is TokenKind.Is
            => new BitType(),
        BinaryExpr b when b.Op.Kind is TokenKind.And or TokenKind.Or => new DoubleType(),
        BinaryExpr => new DoubleType(),
        // Il Linker risolve già il tipo di ritorno delle funzioni builtin (a differenza
        // dei nodi Expr generici sopra) — nessuna inferenza strutturale necessaria qui.
        // MIN/MAX/CLAMP sono dichiarate con InferredType e, per il ParamArray,
        // il linker può propagare erroneamente l'ArrayType dell'argomento sintetico
        // come ReturnType. Questo backend materializza deliberatamente un double,
        // quindi il tipo sullo stack va dichiarato coerentemente anche per PRINT/LET.
        BuiltinFunctionCallExpr f when f.Name.Kind is TokenKind.Min or TokenKind.Max or TokenKind.Clamp or TokenKind.Fact or TokenKind.Perm or TokenKind.Comb or TokenKind.Avg or TokenKind.Sum => new DoubleType(),   // Sum mancava: PRINT SUM(...) generava IL non valido
        // TORCH: handle = long; scalari = double; stringhe; array. La tipizzazione è necessaria
        // perché il linker NON annota TORCH.* (i TokenKind non erano dispatchati dal codegen IL).
        BuiltinFunctionCallExpr f when f.Name.Kind is TokenKind.TorchCreate or TokenKind.TorchFrom
            or TokenKind.TorchAdd or TokenKind.TorchSub or TokenKind.TorchMul or TokenKind.TorchDiv
            or TokenKind.TorchMatmul or TokenKind.TorchRelu or TokenKind.TorchZeros or TokenKind.TorchOnes
            or TokenKind.TorchEye or TokenKind.TorchRandn or TokenKind.TorchTranspose or TokenKind.TorchReshape
            or TokenKind.TorchSigmoid or TokenKind.TorchTanh or TokenKind.TorchSoftmax or TokenKind.TorchExp
            or TokenKind.TorchLog or TokenKind.TorchSqrt or TokenKind.TorchPow or TokenKind.TorchAbs
            or TokenKind.TorchGelu or TokenKind.TorchPermute or TokenKind.TorchSlice or TokenKind.TorchConcat
            or TokenKind.TorchStack or TokenKind.TorchSplit or TokenKind.TorchFree or TokenKind.TorchSet
            or TokenKind.TorchModelCreate or TokenKind.TorchModelTrainStep or TokenKind.TorchModelForward
            or TokenKind.TorchModelParameters or TokenKind.TorchModelSave or TokenKind.TorchModelReset
            or TokenKind.TorchModelLoad
            or TokenKind.TorchModelInput or TokenKind.TorchModelOutput => new LongType(),
        BuiltinFunctionCallExpr f when f.Name.Kind is TokenKind.TorchGet or TokenKind.TorchSum
            or TokenKind.TorchMean or TokenKind.TorchMax or TokenKind.TorchMin or TokenKind.TorchSimilarity
            or TokenKind.TorchLossMse or TokenKind.TorchLossCrossEntropy or TokenKind.TorchLossBinary
            or TokenKind.TorchAccuracy => new DoubleType(),
        BuiltinFunctionCallExpr f when f.Name.Kind is TokenKind.TorchDevice or TokenKind.TorchLibPath
            or TokenKind.TorchDtype or TokenKind.TorchAvailable or TokenKind.TorchDeviceOf
            or TokenKind.TorchModelState => new StringType(),
        BuiltinFunctionCallExpr f when f.Name.Kind is TokenKind.TorchLoaded or TokenKind.TorchAllclose => new BitType(),
        BuiltinFunctionCallExpr f when f.Name.Kind is TokenKind.TorchToArray or TokenKind.TorchTopK
            or TokenKind.TorchArgsort or TokenKind.TorchOneHot or TokenKind.TorchClip
            or TokenKind.TorchNormalize or TokenKind.TorchStandardize => new ArrayType(new DoubleType()),
        BuiltinFunctionCallExpr f when f.Name.Kind == TokenKind.TorchShape => new ArrayType(new LongType()),
        // Builtin DBF numerici: il linker annota Long ma la convenzione BASIC è double (il
        // runtime IlRuntime.Dbf* ritorna double) — box Int64 su un double = IL invalido.
        // EOF incluso: ISAM.EOF/DBF.EOF/RDD.EOF usano il TokenKind EofFunc (shared con
        // FILE.EOF) e l'emitter delega a DbfEof(alias) che ritorna double (0.0/1.0), NON
        // BitType — senza, `PRINT "... EOF(Clienti) = " + CSTR(...)` boxava Boolean su un
        // double = IL invalido nei metodi grandi (DbIsamFullTests Proc_601).
        BuiltinFunctionCallExpr f when f.Name.Kind is TokenKind.Bof or TokenKind.Eof or TokenKind.Found or TokenKind.Deleted
            or TokenKind.Recno or TokenKind.Lastrec or TokenKind.Selected => new DoubleType(),
        BuiltinFunctionCallExpr f when f.Name.Kind == TokenKind.EofFunc
            && f.Namespace is { } eofNs2
            && (eofNs2.Lexeme.Equals("ISAM", StringComparison.OrdinalIgnoreCase)
                || eofNs2.Lexeme.Equals("DBF", StringComparison.OrdinalIgnoreCase)
                || eofNs2.Lexeme.Equals("RDD", StringComparison.OrdinalIgnoreCase))
            => new DoubleType(),
        // RDD RLOCK/FLOCK/BUFFER/SETDELETED come FUNZIONE: lo StdLib dichiara BitType ma
        // l'emitter (EmitRddLockCall, e i case RddBuffer/RddSetDeleted in Builtins.cs)
        // materializza il bool come double (Conv_I4; Conv_R8). Se il tipo statico resta
        // BitType, l'assegnazione a un BIT (`bResult = ISAM.RLOCK(...)`, `ok = RDD.BUFFER(...)`)
        // NON coerce (bitValueType is BitType → skip) e uno `stloc` double-in-bool = IL
        // invalido → InvalidProgramException nei metodi grandi (DbIsamFullTests Proc_601) o
        // "Invalid operand types in BinaryInst" nell'importer ILOptim (sample RDD_Buffer_Demo).
        BuiltinFunctionCallExpr f when f.Name.Kind is TokenKind.RLock or TokenKind.RddRLock
            or TokenKind.FLock or TokenKind.RddFLock
            or TokenKind.RddBuffer or TokenKind.RddSetDeleted => new DoubleType(),
        // MUTEX.TRYLOCK: lo StdLib dichiara BitType ma l'emitter (EmitObjectArgCall ->
        // IlRuntime.MutexTryLock) materializza un DOUBLE (-1.0/0.0). Il tipo statico BitType
        // faceva scegliere a EmitCoerceToTarget la coercizione bit->target (Conv_I4) su un
        // valore double -> IL incoerente: ILOptim "Invalid operand types in BinaryInst" e,
        // senza ottimizzatore, valore errato (x = MUTEX.TRYLOCK(mu) dava 1 invece di -1).
        // Stessa classe del blocco RDD RLOCK/FLOCK sopra. Inline (PRINT ...; TRYLOCK(mu))
        // non passava da EmitCoerceToTarget -> gia' corretto.
        BuiltinFunctionCallExpr f when f.Name.Kind is TokenKind.MutexTryLock
            or TokenKind.ContextCancelled => new DoubleType(),
        /* DBFILTER è una stringa; LUPDATE è una DATA → seriale (DateType): la StdLib lo dichiara
         * DateType e DbfLupdate ritorna ora il seriale. Prima era nel gruppo StringType e il
         * codegen emetteva una stringa dove il type system si aspettava un double →
         * InvalidProgramException (§ MiniTests/dbf/zz_a). */
        BuiltinFunctionCallExpr f when f.Name.Kind is TokenKind.DbFilter => new StringType(),
        // LUPDATE() ritorna la STRINGA "YYYY-MM-DD" (IlRuntime.DbfLupdate): con DoubleType il
        // consumatore trattava il riferimento come numero (stampa garbage/indirizzo).
        // JSON.ToObject(Tipo, json) con target UDT/LIST: il tipo del risultato è il Tipo target
        // (senza, InferType cade nel default → NotSupportedIlExpressionException). SOLO UDT/LIST:
        // lasciare passare gli altri target cambia la tipizzazione di altre espressioni (es.
        // member access su JSON) e rompe l'IL di minitest non correlati (member_typed).
        JsonToObjectExpr jto when jto.TargetType is UserDefinedType or ListType => jto.TargetType,
        BuiltinFunctionCallExpr f when f.Name.Kind is TokenKind.LUpdate => new StringType(),
        BuiltinFunctionCallExpr f when f.Name.Kind is TokenKind.Iif or TokenKind.Choose or TokenKind.ValType => new AnyType(),
        // Dow/Seconds: la NativeProcedure dichiara IntegerType/LongType ma IlRuntime ritorna
        // double (convenzione BASIC: i numeri senza suffisso restano DOUBLE) — coerente.
        BuiltinFunctionCallExpr f when f.Name.Kind is TokenKind.Dow or TokenKind.Seconds => new DoubleType(),
        BuiltinFunctionCallExpr f when f.Name.Kind == TokenKind.Format => new StringType(),
        // Stringify/ToJson (JSON.Stringify / x.ToJson() riscritto dal linker): ritorna una
        // stringa — il fallback Double boxa una stringa come double (IL invalido con ILOptim,
        // es. JSON.Stringify(MyBag.ToJson())).
        BuiltinFunctionCallExpr f when f.Name.Kind is TokenKind.Stringify or TokenKind.ObjectToJson => new StringType(),
        BuiltinFunctionCallExpr f when f.Name.Kind == TokenKind.StackPush => new VoidType(),
        // KEYIMPORT(pem, target) — forma COMANDO: il 2° arg è il target, il risultato è
        // già stoccato lì → void (il chiamante statement non deve popare).
        BuiltinFunctionCallExpr f when f.Name.Kind == TokenKind.StackPop && f.Args.Length > 0 => CollectionElementType(InferType(f.Args[0])),
        BuiltinFunctionCallExpr f when f.Name.Kind is TokenKind.Dequeue or TokenKind.First or TokenKind.Last && f.Args.Length == 1
            && NormalizeInferredType(InferType(f.Args[0])) is RingBufferType or CacheType or TreeType or AVLTreeType or RedBlackTreeType
                or BinaryIndexedTreeType or TrieType or BPlusTreeType or GraphType or HashRingType or SparseSetType
                or BinaryHeapType or BagType or TimelineType => new AnyType(),
        BuiltinFunctionCallExpr f when f.Name.Kind is TokenKind.Dequeue or TokenKind.First or TokenKind.Last && f.Args.Length == 1 => CollectionElementType(InferType(f.Args[0])),
        BuiltinFunctionCallExpr f when f.Name.Kind is TokenKind.GetCursorPos or TokenKind.GetTextMetrics or TokenKind.GetPaletteColor => new ArrayType(new DoubleType()),
        BuiltinFunctionCallExpr f when f.Name.Kind == TokenKind.Count && f.Args.Length == 1 && TryGetNativeCollectionType(InferType(f.Args[0]), out _) => new DoubleType(),
        // BAG.Count(item) (istanza+item, dispatch CollectionInvoke → object): risultato
        // DINAMICO, mai Double (il box double su un object = valore corrotto a runtime).
        BuiltinFunctionCallExpr f when f.Name.Kind == TokenKind.Count && f.Args.Length >= 2 && IsDynamicDispatchInstance(f.Args[0]) => new AnyType(),
        BuiltinFunctionCallExpr f when f.Name.Kind == TokenKind.ToArray && f.Args.Length == 1 => new ArrayType(CollectionElementType(InferType(f.Args[0]))),
        BuiltinFunctionCallExpr f when f.Name.Kind == TokenKind.ToList && f.Args.Length == 1 => new ListType(CollectionElementType(InferType(f.Args[0]))),
        // TODICT(dict) ritorna un DICT (non il default numerico Double): senza, `DIM x =
        // d.ToDict()` su un target AnyType → box double su un object (il dict) → InvalidProgram.
        BuiltinFunctionCallExpr f when f.Name.Kind == TokenKind.ToDict && f.Args.Length == 1 =>
            InferType(f.Args[0]) switch
            {
                DictType dt => dt,
                SortedDictType sdt => new DictType(sdt.KeyType, sdt.ValueType),
                _ => new DictType(new AnyType(), new AnyType())
            },
        BuiltinFunctionCallExpr f when f.Name.Kind == TokenKind.Keys && f.Args.Length == 1 && InferType(f.Args[0]) is DictType dict => new ArrayType(dict.KeyType),
        BuiltinFunctionCallExpr f when f.Name.Kind == TokenKind.Keys && f.Args.Length == 1 && InferType(f.Args[0]) is SortedDictType dict => new ArrayType(dict.KeyType),
        // KEYS su strutture avanzate boxed (Cache/Tree/Bag/...): dispatch RUNTIME
        // CollectionInvoke che ritorna object[] di chiavi boxed — mai il default numerico
        // Double (il box double su un object[] = InvalidProgramException).
        BuiltinFunctionCallExpr f when f.Name.Kind == TokenKind.Keys && f.Args.Length == 1
            && NormalizeInferredType(InferType(f.Args[0])) is CacheType or TreeType or RedBlackTreeType or AVLTreeType
            or BinaryIndexedTreeType or TrieType or BPlusTreeType or GraphType or HashRingType
            or SparseSetType or BinaryHeapType or BagType or TimelineType or RingBufferType => new AnyType(),
        BuiltinFunctionCallExpr f => InferBuiltinFunctionCallType(f),
        WithMemberAccessExpr member when member.Arguments is null && _withTargets.Count > 0 =>
            NormalizeInferredType(_program.Symbols.GetSymbol(ResolveFieldOrPropertySymbolIdByName(_withTargets.Peek().Type, member.MemberNameToken.Lexeme)).DataType),
        // Accesso SAFE (arr(i)?): il valore è boxed object (null se fuori range) — AnyType
        // (come il safe dict), altrimenti la coerce a valle usa l'elemento con un object
        // sullo stack → IL invalido.
        ArrayAccessExpr aSafe when aSafe.IsSafe => new AnyType(),
        ArrayAccessExpr a when _program.Symbols.GetSymbol(a.SymbolId).DataType is DictType dictAccessType => dictAccessType.ValueType,
        // "nome()" senza indici (Indices vuoto) e' l'idioma BASIC per riferirsi all'INTERO
        // array, non a un elemento — usato sia come target di riassegnazione whole-array
        // (x() = Funzione(...), gia' gestito altrove) sia come ARGOMENTO di chiamata
        // (es. FINANCE.NPV(rate, cf())). Il Linker rappresenta ENTRAMBI i casi con lo
        // stesso ArrayAccessExpr di "nome(i)" (solo Indices.Length li distingue), quindi
        // senza questo case il ramo generico sotto restituiva sempre il tipo ELEMENTO
        // (es. DOUBLE invece di ARRAY OF DOUBLE) anche per il riferimento all'intero
        // array, facendo fallire EmitDoubleArrayArgument/EmitDateTimeArrayArgument con
        // "atteso un array di..." per qualunque array, anche perfettamente valido.
        ArrayAccessExpr a when a.Indices.IsEmpty => NormalizeInferredType(_program.Symbols.GetSymbol(a.SymbolId).DataType),
        // F4: valore matrice (descrittore) indicizzato m(i,j) → l'elemento è un DOUBLE
        // (MatrixValueGetAt). Senza questo case il fallback restituiva MatrixValueType e il
        // consumatore (PRINT) non boxava il double → InvalidProgramException.
        ArrayAccessExpr a when NormalizeInferredType(_program.Symbols.GetSymbol(a.SymbolId).DataType) is MatrixValueType
            => new DoubleType(),
        // MyList(0) su LIST/DICT (non array): tipo dell'elemento / valore del dict.
        ArrayAccessExpr a when NormalizeInferredType(_program.Symbols.GetSymbol(a.SymbolId).DataType) is ListType listT
            => listT.ElementType,
        ArrayAccessExpr a when NormalizeInferredType(_program.Symbols.GetSymbol(a.SymbolId).DataType) is DictType dictT
            => dictT.ValueType,
        ArrayAccessExpr a when NormalizeInferredType(_program.Symbols.GetSymbol(a.SymbolId).DataType) is SortedDictType sdictT
            => sdictT.ValueType,
        ArrayAccessExpr a when NormalizeInferredType(_program.Symbols.GetSymbol(a.SymbolId).DataType) is AnyType
            or Basic.JsonType or FetchResultType or HttpOptionsType or HttpHeadersType or HttpConfigType
            or HttpPostedDataType or WebSocketType => new AnyType(),
        // rs("col") su SQLRESULTSET: valore della colonna della riga corrente, tipo incerto
        // (numero/testo secondo la colonna) → AnyType (dispatch di PRINT via PrintFormatValue).
        ArrayAccessExpr a when NormalizeInferredType(_program.Symbols.GetSymbol(a.SymbolId).DataType) is SqlResultSetType => new AnyType(),
        ArrayAccessExpr a when NormalizeInferredType(_program.Symbols.GetSymbol(a.SymbolId).DataType) is TreeType or AVLTreeType
            or RedBlackTreeType or BinaryIndexedTreeType or TrieType or BPlusTreeType or GraphType
            or HashRingType or SparseSetType or BinaryHeapType or BagType or CacheType or TimelineType
            or RingBufferType => new AnyType(),
        // Jagged (ArrayType di ArrayType — es. FFT double[][]): 2 indici → l'elemento INTERNO.
        ArrayAccessExpr a when _program.Symbols.GetSymbol(a.SymbolId).DataType is ArrayType { ElementType: ArrayType jaggedInner }
            && a.Indices.Length == 2 => NormalizeInferredType(jaggedInner.ElementType),
        // Accesso a un elemento di tupla con indice COSTANTE: gli elementi STRING vanno
        // tipizzati come stringa (altrimenti PRINT/interpolazione li tratta come double) e i
        // numerici come double (la lettura passa da TupleElement + ToDouble). Gli elementi non
        // scalari (es. tupla annidata) mantengono il loro tipo, così EmitToString usa
        // CollectionToString ("[1, 2]", come VM/C) invece di JsonValueToString ("System.Object[]").
        ArrayAccessExpr a when _program.Symbols.GetSymbol(a.SymbolId).DataType is TupleType tupleAccessType
            && a.Indices.Length == 1 && a.Indices[0] is NumberExpr tupleIndex
            && (int)tupleIndex.Value >= 0 && (int)tupleIndex.Value < tupleAccessType.Elements.Length
            => NormalizeInferredType(tupleAccessType.Elements[(int)tupleIndex.Value].DataType) switch
            {
                StringType => new StringType(),
                var elementType when IlConversion.IsNumeric(elementType) => new DoubleType(),
                var elementType => elementType
            },
        ArrayAccessExpr a when _program.Symbols.GetSymbol(a.SymbolId).DataType is TupleType => new DoubleType(),
        ArrayAccessExpr a => NormalizeInferredType(UnwrapArrayLevels(_program.Symbols.GetSymbol(a.SymbolId).DataType, a.Indices.Length)),
        ListLiteralExpr list => new ListType(list.Elements.IsEmpty ? new InferredType() : NormalizeInferredType(InferType(list.Elements[0]))),
        DictionaryLiteralExpr dictionary => InferDictionaryLiteralType(dictionary),
        // FFI (ExternalProcedureDefinition): i runtime helper (IlRuntime.FfiCallTyped/
        // FfiCallTypedArr) ritornano SEMPRE un CLR double, indipendentemente dal kind
        // dichiarato (LONG/INTEGER/... marshallano a int64 nativo MA il valore torna alla
        // gamba BASIC come double — vedi FfiService.cs). Dichiarare qui epd.ReturnType
        // (es. LongType) mentiva sul tipo REALMENTE sullo stack: EmitCoerceToTarget
        // confrontava "LongType == LongType" (nessuna conversione necessaria) e uno store
        // diretto (`stloc`/`box System.Int64`) su un valore double non verificabile dava
        // bit reinterpretati invece del numero (bug trovato 2026-09-28, `q = VecAdd(21,21)`
        // con q AS LONG → 4631107791820423168 invece di 42, mai esercitato prima perché
        // nessun MiniTest FFI assegnava un ritorno LONG a una variabile tipizzata). Stessa
        // causa radice già corretta per UnresolvedCallExpr sotto (RECNO/LASTREC). STRING è
        // l'eccezione reale: FfiCallTypedArrStr ritorna un vero CLR string (copy-out UTF-8),
        // quindi lì il tipo dichiarato è anche quello davvero sullo stack.
        FunctionExpr fn => NormalizeInferredType(
            fn.ResolvedDefinition is ExternalProcedureDefinition epd
                ? (epd.ReturnType is StringType or UserDefinedType ? epd.ReturnType : new DoubleType())
                : fn.ReturnType ?? new VoidType()),
        // Chiamata non risolta dal linker (es. builtin DBF risolti per nome nel P-Code):
        // i numerici senza suffisso sono SEMPRE double (convenzione BASIC) — mai il
        // ReturnType del linker (spesso Long per RECNO/LASTREC → box Int64 su un double
        // = IL invalido).
        UnresolvedCallExpr uce => new DoubleType(),
        // CATCH ex AS ERR: ex.Number/ex.Description non sono mai simboli/campi registrati
        // dal Linker (vedi EmitFieldAccess, stesso identico controllo) — fa.ResolvedType
        // non è affidabile qui (spesso un fallback generico del Linker, non specifico a
        // ERR), quindi va intercettato ESPLICITAMENTE prima di provare quel percorso, non
        // dopo: EmitFieldAccess emette Number come double (Conv_R8 dopo il getter int32
        // di ErrInstance) — deve combaciare esattamente, altrimenti un confronto come
        // "ex.Number = 53" mescolerebbe un int32 e un double sullo stack (Ceq richiede
        // operandi dello stesso tipo).
        FieldAccessExpr fa when NormalizeInferredType(InferType(fa.Structure)) is StringBuilderType
            && (fa.Field.Lexeme.Equals("LENGTH", StringComparison.OrdinalIgnoreCase)
                || CollMembers.Is(fa.Field, TokenKind.CollCapacity)) => new DoubleType(),
        FieldAccessExpr fa when TryGetNativeCollectionType(InferType(fa.Structure), out _) && CollMembers.Is(fa.Field, TokenKind.CollCount) => new DoubleType(),
        FieldAccessExpr fa when NormalizeInferredType(InferType(fa.Structure)) is SqlResultSetType
            && (CollMembers.Is(fa.Field, TokenKind.CollCount)
                || fa.Field.Lexeme.Equals("COLUMNS", StringComparison.OrdinalIgnoreCase)) => new DoubleType(),
        FieldAccessExpr fa when NormalizeInferredType(InferType(fa.Structure)) is FetchResultType => new AnyType(),
        FieldAccessExpr fa when InferType(fa.Structure) is ErrType =>
            fa.Field.Lexeme.Equals("Number", StringComparison.OrdinalIgnoreCase)
                || fa.Field.Lexeme.Equals("LineNumber", StringComparison.OrdinalIgnoreCase)
                ? new DoubleType() : new StringType(),
        FieldAccessExpr fa => NormalizeInferredType(InferFieldAccessType(fa)),
        BaseExpr => new AnyType(),
        TypeExpr te => NormalizeInferredType(te.Type),
        // SWITCH: il tipo del primo RESULT (parità VM — tutti i rami convergono nel
        // medesimo tipo statico; il default è il primo case presente).
        SwitchExpr sw => NormalizeInferredType(InferType(sw.Cases.Length > 0 ? sw.Cases[0].Result : (sw.ElseCase ?? sw.Value))),
        // Lambda letterale (es. FUNCTION = FN() AS LONG => ... — il return di una FUNCTION
        // di ordine superiore): FunctionPointerType (il tipo del delegate CLR deriva dai
        // parametri/return dichiarati — il case prima lanciava "LambdaExpr non supportata").
        LambdaExpr le => new FunctionPointerType(le.Parameters.Select(p => p.DataType).ToImmutableArray(), le.ReturnType),
        // Riferimento BRUTO a funzione a 0 argomenti (es. `CALL Foo` / `Foo()` senza
        // parametri, senza parentesi obbligatorie): EmitFunctionReferenceAsCall lo traduce
        // in una CHIAMATA REALE (mai un delegate) quando NON è un vero riferimento
        // callback (IsCallbackFunctionReferenceCore false — stesso predicato usato da
        // EmitExpression per scegliere tra EmitFunctionReference/EmitFunctionReferenceAsCall,
        // vedi IlProgramCompiler.Expressions.cs case FunctionReferenceExpr). Il tipo
        // inferito qui DEVE riflettere il tipo di ritorno EFFETTIVO di quella chiamata
        // (VoidType per una SUB) — restituire sempre AnyType faceva sì che
        // EmitExpressionStatement (che decide il POP di uno statement guardando InferType)
        // emettesse un Pop dopo una CALL a un metodo CLR void, cioè senza nulla sullo stack
        // da scartare: stack underflow → IL invalido, ILOptim.BlockState.ImportPop va in
        // underflow (IndexOutOfRangeException) e il JIT alza InvalidProgramException. Bug
        // riproducibile con QUALSIASI `CALL NomeSub` a 0 argomenti, indipendentemente da
        // GOTO/label (diagnosticato inizialmente come bug del codegen GOTO nelle SUB, ma
        // il vero innesco è la sintassi CALL-senza-parentesi, non il control flow interno).
        FunctionReferenceExpr fr3 => InferFunctionReferenceType(fr3),
        SafeCastExpr sce => new NullableType(NormalizeInferredType(sce.TargetType)),
        MethodCallExpr mc when (NormalizeInferredType(InferType(mc.Instance)) is RegexType
            || (mc.Instance is FieldAccessExpr mcRegexFa && NormalizeInferredType(InferType(mcRegexFa.Structure)) is RegexType)) =>
            InferRegexMethodReturnType(mc),
        MethodCallExpr mc when NormalizeInferredType(InferType(mc.Instance)) is StringBuilderType =>
            mc.MethodName.Lexeme.Equals("ToString", StringComparison.OrdinalIgnoreCase) ? new StringType() : new AnyType(),
        MethodCallExpr mc when TryInferNativeCollectionMethodReturn(mc, out var collectionMethodReturn) => collectionMethodReturn,
        // BASE.metodo(): l'istanza è BaseExpr (InferType → AnyType) e la chiamata è risolta
        // STATICAMENTE al metodo della base (forceNonVirtual, vedi EmitMethodCall). Senza
        // questo caso, il ramo AnyType sotto cattura la chiamata e restituisce AnyType →
        // EmitExpressionStatement emette un `Pop` spurio su un metodo VOID → stack underflow
        // → InvalidProgramException (bug ClassesHierarchy/ClassesExpressionBodied:
        // `BASE.StampaInfo()` da un override).
        MethodCallExpr mc when mc.Instance is BaseExpr && mc.ResolvedDefinition is FunctionDefinition baseFd =>
            NormalizeInferredType(baseFd.ReturnType),
        // MethodCallExpr su oggetto DINAMICO (Tree/Bag/... boxed, dispatch CollectionInvoke):
        // risultato object (AnyType) — mai VoidType (il fallback sotto lo darebbe). NB: il
        // check NON richiede ResolvedDefinition==null: il linker può risolvere il metodo
        // (es. Keys() su Cache → ReturnType Double) ma il backend lo emette comunque via
        // CollectionInvoke che ritorna object — InferType deve restare AnyType, altrimenti
        // la coerce a valle boxa un object come Double (InvalidProgramException).
        MethodCallExpr mc when NormalizeInferredType(InferType(mc.Instance)) is RedBlackTreeType or TreeType or AVLTreeType
                or BinaryIndexedTreeType or TrieType or BPlusTreeType or GraphType or HashRingType
                or SparseSetType or BinaryHeapType or BagType or CacheType or TimelineType
                or RingBufferType or AnyType or HttpRequestType or HttpResponseType
                or HttpOptionsType or HttpHeadersType or HttpConfigType or HttpPostedDataType
                or WebSocketType or SqlResultSetType or FetchResultType or CookieJarType
                // HLL: metodi dispatchati a runtime (CollectionInvoke → object).
                or HyperLogLogType
                // JSON (es. pkgs.Count() su un JSON.PARSE): dispatch runtime CollectionInvoke
                // → object. Senza, InferType cascava al fallback statico (ReturnType della
                // def risolta, es. Double) e l'aritmetica `pkgs.Count() - 1` coerzava un
                // object come double → InvalidProgramException (stesso bug di BAG.Count).
                or Basic.JsonType => new AnyType(),
        MethodCallExpr mc => NormalizeInferredType(mc.ResolvedDefinition is FunctionDefinition fd ? fd.ReturnType : new VoidType()),
        QualifiedNameExpr qne => new DoubleType(),
        NewExpr ne => ne.ParsedType,
        PropertyAccessExpr pa when TryGetNativeCollectionType(InferType(pa.Instance), out _) && CollMembers.Is(pa.PropertyName, TokenKind.CollCount) => new DoubleType(),
        PropertyAccessExpr pa => NormalizeInferredType(ResolvePropertyDefinition(pa).Type),
        // Accesso SAFE ([k]?): il valore è boxed object (null se assente) — il tipo statico
        // è dinamico (AnyType), altrimenti la coerce a valle usa il tipo dell'elemento (es.
        // int16) con un object/F8 sullo stack → IL invalido.
        IndexedAccessExpr iaSafe when iaSafe.IsSafe => new AnyType(),
        // Accesso a indice su un globale dynamic-scope per NOME (FORM/QUERY delle pagine BSP,
        // keyword non dichiarabili → SymbolId -3): il valore è dinamico (dict/JSON iniettato
        // dall'host) — AnyType, altrimenti il fallback a metodo (ResolveIndexedAccessMethod
        // Definition) lanciava NotSupportedIlExpressionException.
        IndexedAccessExpr ia when ia.Base is VariableExpr dynIv && dynIv.SymbolId < 0 => new AnyType(),
        IndexedAccessExpr ia when TryInferNativeCollectionIndexType(ia, out var collectionIndexType) => collectionIndexType,
        // Istanza.Metodo[args] su oggetto DINAMICO (AnyType — es. PricesCopy.Remove["Apple"]):
        // il risultato è object (dispatch runtime CollectionInvoke), mai risolto a una classe.
        IndexedAccessExpr ia when ia.Base is FieldAccessExpr fa3 && fa3.Structure is VariableExpr iv3
            && NormalizeInferredType(InferType(iv3)) is PortfolioResultType => new DoubleType(),
        IndexedAccessExpr ia when ia.Base is FieldAccessExpr fa3 && fa3.Structure is VariableExpr iv3
            && NormalizeInferredType(InferType(iv3)) is AnyType or Basic.JsonType => new AnyType(),
        // MyRB["c"] (Base = VariableExpr) su oggetto dinamico (RedBlack/Tree/... boxed):
        // risultato object (dispatch runtime CollectionIndexGet).
        // Indice su valore JSON annidato (es. myDict["data"][0] con myDict DICT<STRING,JSON>):
        // il primo accesso è JSON, il secondo indicizza il JSON → AnyType.
        IndexedAccessExpr ia when ia.Base is IndexedAccessExpr innerIa
            && NormalizeInferredType(InferType(innerIa)) is Basic.JsonType or AnyType or HttpRequestType or HttpResponseType => new AnyType(),
        IndexedAccessExpr ia when ia.Base is VariableExpr iv4
            && NormalizeInferredType(InferType(iv4)) is RedBlackTreeType or TreeType or AVLTreeType
                or BinaryIndexedTreeType or TrieType or BPlusTreeType or GraphType or HashRingType
                or SparseSetType or BinaryHeapType or BagType or CacheType or TimelineType
                or RingBufferType or AnyType or Basic.JsonType or FetchResultType => new AnyType(),
        // Indice annidato su espressione JSON/dinamica (es. jsonRes("json")("prodotto") —
        // Base = ArrayAccessExpr che inferisce AnyType): risultato object.
        IndexedAccessExpr ia when NormalizeInferredType(InferType(ia.Base)) is AnyType or Basic.JsonType
            or FetchResultType or HttpOptionsType or HttpHeadersType or HttpConfigType or HttpPostedDataType
            or WebSocketType or PortfolioResultType => new AnyType(),
        // Indice su ESPRESSIONE array (es. FROMHEX(hex$)(0)): l'elemento dell'array.
        IndexedAccessExpr ia when ia.Indices.Length == 1
            && NormalizeInferredType(InferType(ia.Base)) is ArrayType iaArr => NormalizeInferredType(iaArr.ElementType),
        // Indice su un array TOP-LEVEL (NUMERI(3) — Base = VariableExpr di un array
        // DIMensionato): il tipo dell'elemento (il fallback sotto risolveva come metodo →
        // NotSupportedIlExpressionException).
        IndexedAccessExpr ia when ia.Base is VariableExpr ivArr && _arrays.TryGetValue(ivArr.SymbolId, out var iaArrInfo)
            => iaArrInfo.ElementDataType,
        IndexedAccessExpr ia => TryResolveIndexedFieldArray(ia.Base, out _, out var indexedFieldArrayInfo)
            ? indexedFieldArrayInfo.ElementDataType
            : NormalizeInferredType(ResolveIndexedAccessMethodDefinition(ia, out _).ReturnType),
        GroupingExpr g => InferType(g.Expression),
        // Elemento dal PRIMO elemento (come EmitArrayLiteralExpression): un letterale di
        // stringhe (`{ c FOR c IN {"a","b"} }`) è string[], non double[] — prima hardcoded
        // double[] → la comprehension iterava con ToDouble su boxed string = InvalidProgram.
        ArrayLiteralExpr ale => new ArrayType(ale.Elements.Length == 0
            ? (DataType)new DoubleType()
            : NormalizeInferredType(InferType(ale.Elements[0]))),
        // Wrapper del Linker per un accesso a campo su un tipo INCERTO/dinamico (es.
        // "entry.Key" dove "entry" è l'entry di un FOR EACH a 1 variabile su DICT — mai un vero
        // simbolo di campo registrato) — stesso valore del tipo interno, nessuna semantica di
        // nullabilità aggiuntiva da questo backend (JsonFieldGet è già null-safe di suo).
        // "X.VALUE" dove X non è UDT/CLASS: il Linker non distingue staticamente un vero campo
        // JSON/entry chiamato "Value" (il nostro caso — vedi InferForEachElementTypes) da un
        // genuino unwrap Nullable<T>.Value (nessuna informazione "nullable" sopravvive al
        // binding), quindi impacchetta ENTRAMBI in NullableValueExpr per priorità (vedi
        // Linker.Rewrites.cs). Se la struttura è JsonType (dinamico), è il nostro caso: stesso
        // campo dinamico "Value" di un FieldAccessExpr normale, mai un vero unwrap nullable.
        NullableValueExpr nv when NormalizeInferredType(InferType(nv.Expression)) is Basic.JsonType => new Basic.JsonType(),
        NullableValueExpr nv when NormalizeInferredType(InferType(nv.Expression)) is RegexMatchType =>
            InferRegexMatchFieldType(nv.ValueToken.Lexeme ?? nv.ValueToken.ToString() ?? ""),
        NullableValueExpr nv => InferType(nv.Expression),
        NullableHasValueExpr => new IntegerType(),
        // Il valore di ritorno di &expr/EVAL è genuinamente DINAMICO (dipende da cosa produce
        // il TESTO valutato a runtime — numero, stringa, bool, qualunque cosa), non dal tipo
        // statico del testo stesso (m.Expression è tipicamente una StringExpr, es. &("1+2") →
        // m.Expression è la stringa letterale "1+2", NON il numero che quel testo produce).
        // Prima di questo fix, InferType(m.Expression) faceva ereditare a x = &("1+2") il tipo
        // STRING del testo sorgente, non il tipo del suo risultato — un'assegnazione a una
        // variabile numerica corrompeva il valore alla lettura (coercizione basata su un tipo
        // statico sbagliato). AnyType (→ object, nessuna assunzione di unboxing) è coerente con
        // come IlRuntime.MacroEvalExpr/MacroEval sono già dichiarati (object?).
        MacroExpr => new Basic.AnyType(),
        // cond ? a : b — tipo comune dei due rami; se eterogenei → Double (default numerico).
        TernaryExpr te => InferType(te.TruePart) == InferType(te.FalsePart)
            ? InferType(te.TruePart)
            : new DoubleType(),
        PrefixIncrementExpr or PostfixIncrementExpr => new DoubleType(),
        NamedArgumentExpr nae => InferType(nae.Value),
        CheckedStringExpr => new StringType(),
        ArraySliceExpr ase => InferType(ase.Base),
        DirectCastExpr dce => NormalizeInferredType(dce.TargetType),
        AwaitExpr awe => InferType(awe.Target),
        GoFunctionExpr gfe => NormalizeInferredType(gfe.ReturnType),
        TupleLiteralExpr tle => new TupleType([.. tle.Elements.Select(e => new TupleField(e.Name?.Lexeme, InferType(e.Value)))]),
        NullConditionalAccessExpr nca => InferNullConditionalType(nca),
        // G5: FETCH espressione (forma espressione) → tipo FETCHRESULT. Senza questo, un
        // `DIM r = FETCH(url, opts)` (r senza tipo esplicito, come nelle pagine BSP) falliva
        // con NotSupportedIlExpressionException in InferType — la v2 lo supporta già.
        HttpFetchExpr => new FetchResultType(),
        // EVAL({ |p| ... }, args...) con lambda INLINE o EVAL(Mk(), args) con Mk che
        // ritorna un block: tipo = ritorno della lambda/FunctionPointerType (senza,
        // PRINT non boxava il double → PrintFormatValue(object) = InvalidProgram;
        // target dinamico non riscritto dal Linker → Any, come MacroExpr).
        EvalExpr eeEval => eeEval.Target switch
        {
            LambdaExpr leEval => NormalizeInferredType(leEval.ReturnType),
            FunctionExpr feEval when ResolveEvalTargetFpt(feEval) is FunctionPointerType fptEval
                => NormalizeInferredType(fptEval.ReturnType),
            _ => new Basic.AnyType(),
        },
        // Comprehension di dict/set: tipo dalle espressioni chiave/valore/elemento (come EmitDictComprehension/EmitSetComprehension
        // quando non c'e' un DIM tipizzato), cosi' DIM tipizzato e inferenza coincidono col valore sullo stack.
        // [ expr FOR x IN src ]: array del tipo dell'output. Numerico → double[] (come un
        // ArrayLiteralExpr); output stringa (sorgente/iteratore stringa) → string[]
        // (EmitListComprehension usa List<T>/T[] in base a questo).
        ListComprehensionExpr lce => new ArrayType(
            NormalizeInferredType(InferType(lce.OutputExpression)) is StringType
                ? (DataType)new StringType()
                : new DoubleType()),
        DictComprehensionExpr dce => new DictType(NormalizeInferredType(InferType(dce.KeyExpression)), NormalizeInferredType(InferType(dce.ValueExpression))),
        SetComprehensionExpr sce => new HashSetType(NormalizeInferredType(InferType(sce.ElementExpression))),
        _ => throw new NotSupportedIlExpressionException(expr)
        };
    }

    private bool TryInferNativeCollectionMethodReturn(MethodCallExpr expr, out DataType returnType)
    {
        var instanceType = InferType(expr.Instance);
        if (!TryGetNativeCollectionType(instanceType, out var collectionType))
        {
            returnType = null!;
            return false;
        }

        // JOIN su collection (MyList.Join(", ")): non è un metodo CLR nativo — la collection
        // EmitCollectionJoin lo gestisce; qui il tipo di ritorno è string.
        if (CollMembers.Is(expr.MethodName, TokenKind.CollJoin))
        {
            returnType = new StringType();
            return true;
        }

        // Current() su CIRCULAR → tipo elemento.
        if (instanceType is CircularLinkedListType && CollMembers.Is(expr.MethodName, TokenKind.CollCurrent) && expr.Args.Length == 0)
        {
            returnType = CollectionElementType(instanceType);
            return true;
        }

        // IndexOf su collection (helper runtime CollectionIndexOf) → double.
        if (CollMembers.Is(expr.MethodName, TokenKind.CollIndexOf) && expr.Args.Length == 1)
        {
            returnType = new DoubleType();
            return true;
        }

        // .Keys()/.Values() su DICT/SORTED → array delle chiavi/valori.
        if (expr.Args.Length == 0 && (CollMembers.Is(expr.MethodName, TokenKind.CollKeys)
            || CollMembers.Is(expr.MethodName, TokenKind.CollValues)))
        {
            var kArgType = NormalizeInferredType(instanceType);
            if (kArgType is DictType kd) { returnType = new ArrayType(CollMembers.Is(expr.MethodName, TokenKind.CollKeys) ? kd.KeyType : kd.ValueType); return true; }
            if (kArgType is SortedDictType ksd) { returnType = new ArrayType(CollMembers.Is(expr.MethodName, TokenKind.CollKeys) ? ksd.KeyType : ksd.ValueType); return true; }
        }

        // F1 Incongruenze: SET/SETKEYVALUE (comandi sovrascrittura) e GET (lookup) su
        // DICT/SORTED: Dictionary non espone .Set/.Get — il backend emette il kind dispatch
        // CollectionInvokeByKind (ritorna object). InferType deve rispecchiare il VALORE
        // EMesso: SET → AnyType (object: lo scarto è del consumatore, un uso in espressione
        // lo boxa), GET → tipo del valore. Prima: il CLR-resolve falliva ("set" non è un
        // metodo Dictionary) → fallback VoidType → EmitDynamicValueBox emetteva
        // `box System.Void` = InvalidProgramException.
        var setGetInst = NormalizeInferredType(instanceType);
        if (setGetInst is DictType sgd || setGetInst is SortedDictType sgsd)
        {
            if (expr.Args.Length == 2 && (CollMembers.Is(expr.MethodName, TokenKind.CollSet)
                || CollMembers.Is(expr.MethodName, TokenKind.CollSetKeyValue)))
            {
                returnType = new AnyType();
                return true;
            }
            if (expr.Args.Length == 1 && CollMembers.Is(expr.MethodName, TokenKind.CollGet))
            {
                returnType = setGetInst is DictType sgv ? sgv.ValueType : ((SortedDictType)setGetInst).ValueType;
                return true;
            }
            // F1: MaxPair/MinPair — restituiscono la CHIAVE (stringa).
            if (expr.Args.Length == 0 && (CollMembers.Is(expr.MethodName, TokenKind.CollMaxPair)
                || CollMembers.Is(expr.MethodName, TokenKind.CollMinPair)))
            {
                returnType = new StringType();
                return true;
            }
        }
        // F1: membri LIST/QUEUE/STACK senza metodo CLR — il backend emette il kind dispatch
        // (runtime object) o un bool; InferType deve allinearsi al valore emesso (prima:
        // fallback VoidType → unbox.any/box System.Void = InvalidProgramException).
        var f1Inst = NormalizeInferredType(instanceType);
        if (expr.Args.Length == 0 && (CollMembers.Is(expr.MethodName, TokenKind.CollIsEmpty))
            && (f1Inst is ListType or QueueType or StackType or HashSetType or DictType or SortedDictType
                or LinkedListType or DoublyLinkedListType or CircularLinkedListType or RingBufferType))
        {
            returnType = new BitType();
            return true;
        }
        if (expr.Args.Length == 0 && (CollMembers.Is(expr.MethodName, TokenKind.CollFront)
            || CollMembers.Is(expr.MethodName, TokenKind.CollBack)))
        {
            if (f1Inst is ListType fl) { returnType = fl.ElementType; return true; }
            if (f1Inst is QueueType fq) { returnType = fq.ElementType; return true; }
            if (f1Inst is StackType fs) { returnType = fs.ElementType; return true; }
        }
        if ((expr.Args.Length == 1 && CollMembers.Is(expr.MethodName, TokenKind.CollAddUnique))
            || (expr.Args.Length == 2 && (CollMembers.Is(expr.MethodName, TokenKind.CollAddAfter)
                || CollMembers.Is(expr.MethodName, TokenKind.CollAddBefore))))
        {
            returnType = new AnyType();
            return true;
        }

        var methodName = instanceType is LinkedListType && CollMembers.Is(expr.MethodName, TokenKind.CollAdd)
            ? nameof(LinkedList<int>.AddLast)
            : expr.MethodName.Lexeme;
        var method = ResolveNativeCollectionMethod(collectionType, methodName, expr.Args);
        if (method is null)
        {
            // First()/Last() come property (LinkedList CLR) → tipo dell'elemento.
            if (expr.Args.Length == 0)
            {
                var propInfo = collectionType.GetProperty(expr.MethodName.Lexeme, System.Reflection.BindingFlags.Public | System.Reflection.BindingFlags.Instance);
                if (propInfo is not null)
                {
                    // CollCapacity (es. Capacity su RINGBUFFER) → emesso via
                    // CollectionInvokeByKind (object) → AnyType. Prima ritornava il tipo
                    // ELEMENTO (Capacity su RINGBUFFER<LONG> → LongType) → la coerce boxava
                    // Int64 su un object già boxato (doppio box → garbage). First/Last veri
                    // (LinkedList) restano il tipo elemento.
                    returnType = expr.MethodName.Kind == TokenKind.CollCapacity
                        ? new AnyType()
                        : CollectionElementType(InferType(expr.Instance));
                    return true;
                }
            }
            returnType = null!;
            return false;
        }

        // CollCapacity (es. RINGBUFFER.Capacity) è emesso via CollectionInvokeByKind → object;
        // un tipo primitivo statico farebbe la coerce boxare un object già boxato (doppio box
        // → garbage). NB: SOLO CollCapacity — altri CollMembers (ContainsKey/...) sono emessi
        // DIRETTAMENTE (bool non boxato) e richiedono il tipo concreto.
        if (expr.MethodName.Kind == TokenKind.CollCapacity && method.ReturnType != typeof(void))
        {
            returnType = new AnyType();
            return true;
        }

        if (method.ReturnType == typeof(void)) returnType = new VoidType();
        else if (method.ReturnType == typeof(bool)) returnType = new BitType();
        else if (method.ReturnType.IsPrimitive && method.ReturnType != typeof(bool) && method.ReturnType != typeof(char)) returnType = new DoubleType();
        else if (method.ReturnType.IsArray) returnType = new ArrayType(CollectionElementType(InferType(expr.Instance)));
        else if (method.ReturnType == collectionType) returnType = instanceType;
        else if (method.ReturnType == DataTypeMapper.Map(CollectionElementType(InferType(expr.Instance)), _types, _classTypes)) returnType = CollectionElementType(InferType(expr.Instance));
        else returnType = new VoidType();
        return true;
    }

    
private bool TryInferNativeCollectionIndexType(IndexedAccessExpr expr, out DataType valueType)
    {
        if (expr.Indices.Length != 1 || !TryGetNativeCollectionType(InferType(expr.Base), out _))
        {
            valueType = null!;
            return false;
        }
        valueType = InferType(expr.Base) switch
        {
            ListType list => list.ElementType,
            DictType dict => dict.ValueType,
            SortedDictType dict => dict.ValueType,
            ArrayType array => array.ElementType,
            _ => throw new NotSupportedException($"Backend IL: collection '{InferType(expr.Base)}' non è indicizzabile.")
        };
        return true;
    }

    private static DataType CollectionElementType(DataType collectionType) => collectionType switch
    {
        ListType list => list.ElementType,
        HashSetType set => set.ElementType,
        QueueType queue => queue.ElementType,
        StackType stack => stack.ElementType,
        LinkedListType linked => linked.ElementType,
        CircularLinkedListType circular => circular.ElementType,
        DoublyLinkedListType doubly => doubly.ElementType,
        RingBufferType ring => ring.ElementType,
        // Strutture chiave→valore: l'elemento "unico" è il VALORE (Tree/RedBlack/AVL/BPlus).
        TreeType tree => tree.ValueType,
        RedBlackTreeType rb => rb.ValueType,
        AVLTreeType avl => avl.ValueType,
        BPlusTreeType bp => bp.ValueType,
        BinaryIndexedTreeType bi => bi.ElementType,
        CacheType cache => cache.ValueType,
        TimelineType tl => tl.ValueType,
        BagType bag => bag.ElementType,
        BinaryHeapType heap => heap.ElementType,
        TrieType => new StringType(),
        SparseSetType => new LongType(),
        GraphType graph => graph.NodeType,
        HashRingType hr => hr.NodeType,
        _ => throw new NotSupportedException($"Backend IL: '{collectionType}' non ha un singolo tipo elemento.")
    };

    /// <summary>
    /// Tipo di un FieldAccessExpr quando il Linker non ha popolato FieldSymbolId (campo
    /// ANNIDATO, es. "Sc.Rif.Posizione.X": la struttura "Sc.Rif.Posizione" è essa stessa
    /// una FieldAccessExpr con FieldSymbolId proprio non necessariamente risolto) —
    /// risolve ricorsivamente via InferType(fa.Structure), stesso meccanismo di
    /// ResolveFieldOrPropertySymbolIdByName usato dal lato scrittura.
    /// </summary>
    private DataType InferFieldAccessType(FieldAccessExpr fa)
    {
        // Late-binding Clipper (campo nudo, es. `ITEM` dopo `DBF SELECT ORD` —
        // TryResolveLateBinding nel Linker): la struttura è il sentinella `_dbRecord`
        // (SymbolTable.DbfRecordBufferSymbolId), MAI un valore reale. Quale alias è
        // selezionato non è noto a compile-time in generale (dipende dal flusso a runtime)
        // → il valore resta dinamico (object), come le altre letture "sempre object"
        // (JSON/Http/...) qui sotto. PRIMA del fallback generico "DoubleType or
        // InferredType" (pensato per MODES(I).Active, non per questo caso): senza questo
        // check un campo DBF/ISAM nudo era SEMPRE inferito DoubleType, anche per campi
        // STRING, e il valore reale (letto a runtime da EmitFieldAccess via
        // IlRuntime.DbfLateBindField) veniva scartato — vedi
        // ILBackend-DbfLateBinding-BareField-AlwaysZero (AiWiki).
        if (fa.Structure is VariableExpr dbLateBindVar && dbLateBindVar.SymbolId == SymbolTable.DbfRecordBufferSymbolId)
            return new AnyType();
        // dict.Keys / dict.Values (senza parentesi): EmitFieldAccess li materializza come
        // array via Enumerable.ToArray — il tipo statico deve essere ArrayType. PRIMA del
        // FieldSymbolId: il linker registra "Keys" nello schema con ListType, incoerente
        // con l'emit ToArray.
        if ((CollMembers.Is(fa.Field, TokenKind.CollKeys) ||
             CollMembers.Is(fa.Field, TokenKind.CollValues))
            && NormalizeInferredType(InferType(fa.Structure)) is DictType dk)
            return new ArrayType(CollMembers.Is(fa.Field, TokenKind.CollKeys) ? dk.KeyType : dk.ValueType);
        if ((CollMembers.Is(fa.Field, TokenKind.CollKeys) ||
             CollMembers.Is(fa.Field, TokenKind.CollValues))
            && NormalizeInferredType(InferType(fa.Structure)) is SortedDictType sk)
            return new ArrayType(CollMembers.Is(fa.Field, TokenKind.CollKeys) ? sk.KeyType : sk.ValueType);
        // q.Peek/q.Dequeue (Queue), s.Pop (Stack) come field → tipo elemento della collection.
        if ((CollMembers.Is(fa.Field, TokenKind.CollPeek) ||
             CollMembers.Is(fa.Field, TokenKind.CollDequeue))
            && NormalizeInferredType(InferType(fa.Structure)) is QueueType qt)
            return qt.ElementType;
        if ((CollMembers.Is(fa.Field, TokenKind.CollPop) ||
             CollMembers.Is(fa.Field, TokenKind.CollPeek))
            && NormalizeInferredType(InferType(fa.Structure)) is StackType st)
            return st.ElementType;
        if (fa.FieldSymbolId >= 0 || NormalizeInferredType(InferType(fa.Structure)) is UserDefinedType or ClassType)
        {
            // CAMPO-ARRAY di CLASSE/UDT: il simbolo del campo porta il tipo ELEMENTO (es.
            // DoubleType), il rango vive nella dichiarazione (FieldArrayInfo) — senza questo
            // l'inferenza diceva "scalare" e `MATRIX.ROWCOUNT(t.m)`/`Show(t.m)` cadevano sul
            // percorso handle ("Handle matrice non valido"), divergendo da PCode/VM-C/C99.
            int faSym = fa.FieldSymbolId >= 0
                ? fa.FieldSymbolId
                : ResolveFieldOrPropertySymbolIdByName(NormalizeInferredType(InferType(fa.Structure)), fa.Field.Lexeme);
            if (faSym >= 0)
            {
                if (_classFieldArrays.TryGetValue(faSym, out var faArrayInfo))
                    return new ArrayType(faArrayInfo.ElementDataType, faArrayInfo.Rank);
                return _program.Symbols.GetSymbol(faSym).DataType;
            }
        }
        // Campo dinamico su JSON/entry DICT (vedi EmitFieldAccess) — nessun simbolo REALE da
        // risolvere, il valore resta dinamico (object) esattamente come la struttura di
        // partenza: JsonFieldGet non sa a compile-time se restituirà una stringa o un numero.
        if (InferType(fa.Structure) is Basic.JsonType) return new Basic.JsonType();
        // REGEX: r.Pattern (stringa) e m.Index/Length/Success/Value (RegexMatchValue).
        var regexFaType = NormalizeInferredType(InferType(fa.Structure));
        if (regexFaType is RegexType) return new StringType();
        // RGBA/DisplayMode: .Red/.Green/.Blue/.Alpha sono componenti numerici (double[]).
        if (regexFaType is RgbaColorType or DisplayModeType) return new DoubleType();
        if (regexFaType is RegexMatchType)
        {
            var fieldName = fa.Field.Lexeme ?? fa.Field.ToString() ?? "";
            var prop = typeof(Basic.RegexNative.RegexMatchValue).GetProperties()
                .FirstOrDefault(p => p.Name.Equals(fieldName, StringComparison.OrdinalIgnoreCase));
            if (prop is null) throw new NotSupportedException($"Backend IL: campo REGEXMATCH '{fieldName}' non trovato.");
            return prop.PropertyType == typeof(string) ? new StringType() : new DoubleType();
        }
        // HLL: `.Count`/`.Length` senza parentesi → stima della cardinalità (numerico). Non è
        // un campo/property CLR: senza questo caso la risoluzione lanciava
        // "impossibile risolvere il campo/proprietà 'Count'".
        if (NormalizeInferredType(InferType(fa.Structure)) is HyperLogLogType)
            return new DoubleType();
        // Campo su oggetto DINAMICO (AnyType/Http/WebSocket...): il valore resta object —
        // i metodi (Remove/Add/First) passano dal dispatch runtime CollectionInvoke.
        if (NormalizeInferredType(InferType(fa.Structure)) is AnyType or HttpRequestType or HttpResponseType
            or HttpOptionsType or HttpHeadersType or HttpConfigType or HttpPostedDataType
            or SqlResultSetType or WebSocketType or PortfolioResultType or FetchResultType
            or RebalanceStrategyType or BacktestResultType
            or AnovaResultType or RegressionResultType or PcaResultType or KMeansResultType
            or ArimaResultType or HoltWintersResultType or AdfTestResultType or HmmResultType or JohansenResultType or GpResultType or SvmResultType or GmmResultType or PolyFitResultType or ParticleResultType)
            return new AnyType();
        // Campo su base con tipo STATICO non-UDF (es. MODES(I).Active con MODES Double
        // assegnato da SCREENS() — test di sintassi GraphicsShowModes): il P-Code lo accetta
        // (tipi dinamici); qui i campi DISPLAYMODE sono numerici → DoubleType di default.
        if (NormalizeInferredType(InferType(fa.Structure)) is DoubleType or InferredType)
            return new DoubleType();
        var fieldSymbolId = ResolveFieldOrPropertySymbolIdByName(InferType(fa.Structure), fa.Field.Lexeme);
        return _program.Symbols.GetSymbol(fieldSymbolId).DataType;
    }


}
