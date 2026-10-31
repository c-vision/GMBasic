// FILE: IlBackend/Partial/IlProgramCompiler.BuiltinsDb.cs
// PARTIAL — dominio BUILTIN DATABASE (dbf/rdd/scope/speed-key/udt-copy).
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

/// <summary>Partial del transpiler IL: dominio BUILTIN DATABASE.</summary>
public sealed partial class IlProgramCompiler
{

    private void EmitDbfUseStatement(DbfUseStmt stmt)
    {
        // `DBF USE <file> ... AS Tipo`: se il file manca, crealo dalla struttura del TIPO
        // (comportamento documentato). Senza, il sample Clipper che apre un file ancora
        // inesistente con AS <TYPE> falliva con FileNotFoundException.
        if (stmt.TypeSymbolId > 0 && stmt.OriginalTypeName is not null)
        {
            var useUdt = _program.Symbols.FindSymbolsByName(stmt.OriginalTypeName.Lexeme)
                .Select(s => s.DataType).OfType<UserDefinedType>().FirstOrDefault();
            if (useUdt is not null)
            {
                var useDefs = string.Join(",",
                    useUdt.Fields.Select(f => $"{f.Name.Lexeme}={DbfCreateTypeName(f.DataType)}={DbfCreateFieldLen(f.DataType)}"));
                EmitExpression(stmt.FileName);
                _il.Emit(OpCodes.Ldstr, useDefs);
                _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.DbfCreateIfMissing), typeof(string), typeof(string)));
            }
        }
        EmitExpression(stmt.FileName);
        // indexPaths: array object[] (count + paths), come il compiler P-Code.
        _il.Emit(OpCodes.Ldc_I4, stmt.IndexPaths.Length);
        _il.Emit(OpCodes.Newarr, _types.Object);
        for (var i = 0; i < stmt.IndexPaths.Length; i++)
        {
            _il.Emit(OpCodes.Dup);
            _il.Emit(OpCodes.Ldc_I4, i);
            EmitExpression(stmt.IndexPaths[i]);
            var pClr = DataTypeMapper.Map(NormalizeInferredType(InferType(stmt.IndexPaths[i])), _types, _classTypes);
            if (pClr.IsValueType) _il.Emit(OpCodes.Box, pClr);
            _il.Emit(OpCodes.Stelem_Ref);
        }
        _il.Emit(OpCodes.Ldc_I4, (int)stmt.Locking);
        _il.Emit(stmt.IsUnsafe ? OpCodes.Ldc_I4_1 : OpCodes.Ldc_I4_0);
        if (stmt.OriginalAliasToken is not null) _il.Emit(OpCodes.Ldstr, stmt.OriginalAliasToken.Lexeme);
        else _il.Emit(OpCodes.Ldnull);
        if (stmt.TypeSymbolId > 0) { _il.Emit(OpCodes.Ldc_I4, stmt.TypeSymbolId); _il.Emit(OpCodes.Box, _types.Int32); }
        else _il.Emit(OpCodes.Ldnull);
        if (stmt.OriginalTypeName is not null) _il.Emit(OpCodes.Ldstr, stmt.OriginalTypeName.Lexeme);
        else _il.Emit(OpCodes.Ldnull);
        _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.DbfUse), typeof(string), typeof(object[]), typeof(int), typeof(bool), typeof(string), typeof(object), typeof(string)));
        _il.Emit(OpCodes.Pop);
    }

    /// <summary>RDD USE file VIA driver ALIAS a — driver "DBF"/"ISAM" → stessa area DBF.</summary>
    private void EmitRddUseStatement(RddUseStmt stmt)
    {
        EmitExpression(stmt.FileName);
        _il.Emit(OpCodes.Ldc_I4_0);
        _il.Emit(OpCodes.Newarr, _types.Object);
        _il.Emit(OpCodes.Ldc_I4_0); // locking shared
        _il.Emit(OpCodes.Ldc_I4_0); // unsafe false
        if (stmt.AliasToken is not null) _il.Emit(OpCodes.Ldstr, stmt.AliasToken.Lexeme);
        else _il.Emit(OpCodes.Ldnull);
        _il.Emit(OpCodes.Ldnull); // typeId
        EmitExpression(stmt.Driver); // typeName (driver)
        var dClr = DataTypeMapper.Map(NormalizeInferredType(InferType(stmt.Driver)), _types, _classTypes);
        if (dClr.IsValueType) _il.Emit(OpCodes.Box, dClr);
        _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.RddUse), typeof(string), typeof(object[]), typeof(int), typeof(bool), typeof(string), typeof(object), typeof(string)));
        _il.Emit(OpCodes.Pop);
    }

    /// <summary>Emette RDD DELETE/RECALL con scope → IlRuntime.RddDelete/RddRecall.
    /// Firma: (int scopeType, object? countOrArg, object? condition, string? alias).</summary>
    private void EmitRddScopedStatement(DbfScopeClause? scope, string runtimeMethod)
    {
        EmitScopeClauseArgs(scope);
        _il.Emit(OpCodes.Ldnull); // alias
        _il.Emit(OpCodes.Call, RuntimeMethod(runtimeMethod, typeof(int), typeof(object), typeof(object), typeof(string)));
        _il.Emit(OpCodes.Pop); // RddDelete/RddRecall restituiscono long
    }

    /// <summary>Emette DBF DELETE/RECALL [scope] → IlRuntime.DbfDeleteScoped/DbfRecallScoped
    /// (int scopeType, object? countOrArg, object? condition) — come EmitRddScopedStatement
    /// ma senza l'argomento alias (le varianti Dbf* usano sempre l'alias DBF corrente).</summary>
    private void EmitDbfScopedStatement(DbfScopeClause? scope, string runtimeMethod)
    {
        EmitScopeClauseArgs(scope);
        _il.Emit(OpCodes.Call, RuntimeMethod(runtimeMethod, typeof(int), typeof(object), typeof(object)));
        _il.Emit(OpCodes.Pop); // DbfDeleteScoped/DbfRecallScoped restituiscono long
    }

    /// <summary>Emette RDD REPLACE field WITH value [, ...] [scope] →
    /// IlRuntime.RddReplace(object fields, int scopeType, object? countOrArg,
    /// object? condition, string? alias). Costruisce il dizionario campo→valore a runtime.
    /// Il dizionario è per NOME campo (non per FieldSymbolId): quest'ultimo, quando risolto
    /// da RewriteTarget via CurrentUdtScopeId, è l'ID GLOBALE del campo nel Linker — per
    /// driver DBF/Speed RddFieldInfo.SymbolId è invece l'indice POSIZIONALE 0-based, quindi
    /// il confronto in CoreDb.BuildContext/ScopedReplace non trovava mai match (stesso bug/
    /// causa già risolto per DBF REPLACE, vedi DbfReplaceByName). Senza `AS Type` su RDD USE
    /// (late-binding puro), il target resta un VariableTarget nudo mai wrappato in
    /// FieldTarget: va gestito anche quello, altrimenti REPLACE resta un no-op silenzioso.</summary>
    private void EmitRddReplaceStatement(RddReplaceStmt stmt)
    {
        _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.RddNewReplaceDict)));
        foreach (var r in stmt.Replacements)
        {
            string fieldName;
            if (r.Target is FieldTarget replFt) fieldName = replFt.OriginalToken.Lexeme;
            else if (r.Target is VariableTarget replVt) fieldName = replVt.NameCleaned.Lexeme;
            else continue;
            _il.Emit(OpCodes.Dup); // dict
            _il.Emit(OpCodes.Ldstr, fieldName);
            EmitExpression(r.Value);
            var rvClr = DataTypeMapper.Map(NormalizeInferredType(InferType(r.Value)), _types, _classTypes);
            if (rvClr.IsValueType) _il.Emit(OpCodes.Box, rvClr);
            _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.RddReplaceDictSet),
                typeof(System.Collections.Generic.Dictionary<string, object?>), typeof(string), typeof(object)));
        }
        EmitScopeClauseArgs(stmt.Scope);
        _il.Emit(OpCodes.Ldnull); // alias
        _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.RddReplace),
            typeof(object), typeof(int), typeof(object), typeof(object), typeof(string)));
        _il.Emit(OpCodes.Pop); // RddReplace restituisce long
    }

    /// <summary>Spinge gli argomenti di scope: (int scopeType, object? countOrArg, object? condition).
    /// ScopeType: 0=All, 1=Next, 2=Record, 3=Rest, 4=Current, 5=For, 6=While.
    /// La condizione FOR/WHILE è emessa come stringa valutata a runtime
    /// (pattern DBF SETFILTER) e risolta da RuntimeExpressionEvaluator per-record.</summary>
    /// <summary>Serializza l'espressione della clausola FOR/WHILE in una stringa di filtro
    /// compatibile con RuntimeExpressionEvaluator (`campo OP valore [AND/OR ...]`): i
    /// riferimenti ai CAMPI non sono variabili risolte — valutarli come espressione
    /// produrrebbe `ldc.r8 0` (campo → 0) e la condizione sarebbe calcolata a compile-time
    /// (bug RDD_ScopeCommands: FOR Saldo > 100 diventava "0 > 100" → "False").</summary>
    private void EmitScopeClauseArgs(DbfScopeClause? scope)
    {
        if (scope == null)
        {
            _il.Emit(OpCodes.Ldc_I4_4); // Current
            _il.Emit(OpCodes.Ldnull);   // countOrArg
            _il.Emit(OpCodes.Ldnull);   // condition
            return;
        }
        switch (scope)
        {
            case DbfAllClause:
                _il.Emit(OpCodes.Ldc_I4_0); // All
                _il.Emit(OpCodes.Ldnull);
                _il.Emit(OpCodes.Ldnull);
                break;
            case DbfNextClause next:
                _il.Emit(OpCodes.Ldc_I4_1); // Next
                EmitExpression(next.Count);
                // countOrArg è object? — il runtime si aspetta un double boxed
                // (RddScopeData: countOrArg is double n). Senza box, il JIT rifiuta il
                // metodo (double su un parametro object) → InvalidProgram.
                _il.Emit(OpCodes.Box, _types.Double);
                _il.Emit(OpCodes.Ldnull);
                break;
            case DbfRecordClause recordClause:
                _il.Emit(OpCodes.Ldc_I4_2); // Record
                EmitExpression(recordClause.RecordNumber);
                _il.Emit(OpCodes.Box, _types.Double);
                _il.Emit(OpCodes.Ldnull);
                break;
            case DbfRestClause:
                _il.Emit(OpCodes.Ldc_I4_3); // Rest
                _il.Emit(OpCodes.Ldnull);
                _il.Emit(OpCodes.Ldnull);
                break;
            case DbfForClause forClause:
                _il.Emit(OpCodes.Ldc_I4_5); // For → All + forCond
                _il.Emit(OpCodes.Ldnull);
                _il.Emit(OpCodes.Ldstr, SerializeFilterCondition(forClause.Condition));
                break;
            case DbfWhileClause whileClause:
                _il.Emit(OpCodes.Ldc_I4_6); // While → All + whileCond
                _il.Emit(OpCodes.Ldnull);
                _il.Emit(OpCodes.Ldstr, SerializeFilterCondition(whileClause.Condition));
                break;
            default:
                _il.Emit(OpCodes.Ldc_I4_4); // Current
                _il.Emit(OpCodes.Ldnull);
                _il.Emit(OpCodes.Ldnull);
                break;
        }
    }

    /// <summary>Costruisce lo schema ISAM JSON (typeName, campi UDT, chiave primaria,
    /// indici) per il servizio — pattern PCodeVM ISAM_USE.</summary>
    /// <summary>
    /// Un TYPE dichiarato NEL CORPO di una SUB/FUNCTION (es. `TYPE Cliente ... END TYPE`
    /// dentro SUB Main, come in DbSpeedFullTests.bas) non compare in
    /// _program.MainModule.Declarations — solo nella SymbolTable (stesso problema già
    /// documentato per udtDefinitions in Compile(), vedi "extraUdts"). Con la sola ricerca
    /// in MainModule.Declarations, la lookup restava sempre null per questi TYPE locali →
    /// schema JSON senza campi (SpeedOpenSchema/IsamOpenSchema creavano una tabella a ZERO
    /// campi, IndexOutOfRangeException in EmitCopyObjectArrayToTarget). Si cerca quindi anche
    /// nella SymbolTable, non solo nelle Declarations dei moduli. Condiviso da
    /// BuildIsamSchemaJson e BuildSpeedQueryPlanJson (SELECT * / CREATE VIEW).
    /// </summary>
    private UserDefinedType? FindUdtByName(string typeName)
    {
        var udt = _program.MainModule?.Declarations.OfType<UserDefinedType>()
            .FirstOrDefault(u => u.OriginalName.Lexeme.Equals(typeName, StringComparison.OrdinalIgnoreCase));
        if (udt is not null) return udt;
        var symCount = _program.Symbols.Count;
        for (var si = 0; si < symCount; si++)
        {
            if (_program.Symbols.GetSymbol(si).DeclarationNode is UserDefinedType bodyUdt
                && bodyUdt.OriginalName.Lexeme.Equals(typeName, StringComparison.OrdinalIgnoreCase))
                return bodyUdt;
        }
        return null;
    }

    private void EmitSpeedKeyArg(SpeedPutStmt stmt)
    {
        // SPEED PUT alias, {key, ...values}? — usa il Source come array; qui il primo
        // elemento è la chiave. Record UDT → il primo campo è la chiave primaria.
        if (stmt.Source is ArrayLiteralExpr spLit && spLit.Elements.Length >= 1)
            EmitStringArg(spLit.Elements[0]);
        else if (stmt.Source is VariableExpr spPutUdt
            && NormalizeInferredType(_program.Symbols.GetSymbol(spPutUdt.SymbolId).DataType) is UserDefinedType spPutUdtT)
            EmitFirstUdtFieldAsString(spPutUdt, spPutUdtT);
        else _il.Emit(OpCodes.Ldstr, "");
    }

    private void EmitSpeedKeyArg(SpeedDeleteStmt stmt)
    {
        if (stmt.Source is ArrayLiteralExpr spDelLit && spDelLit.Elements.Length >= 1)
            EmitStringArg(spDelLit.Elements[0]);
        else if (stmt.Source is VariableExpr spDelUdt
            && NormalizeInferredType(_program.Symbols.GetSymbol(spDelUdt.SymbolId).DataType) is UserDefinedType spDelUdtT)
            EmitFirstUdtFieldAsString(spDelUdt, spDelUdtT);
        else if (stmt.Source is not null) EmitStringArg(stmt.Source);
        else _il.Emit(OpCodes.Ldstr, "");
    }

    /// <summary>Emette il PRIMO campo di un record UDT come stringa (convenzione SPEED:
    /// la chiave primaria è il primo campo del TYPE, es. SPEED DELETE Clienti, clienteRec).</summary>
    private void EmitFirstUdtFieldAsString(VariableExpr udtVe, UserDefinedType udt)
    {
        var firstField = udt.Fields.FirstOrDefault(f => f.SymbolId >= 0);
        if (firstField is not null && _classFields.TryGetValue(firstField.SymbolId, out var fb))
        {
            _il.Emit(OpCodes.Ldloca, GetOrDeclareLocal(udtVe.SymbolId));
            _il.Emit(OpCodes.Ldfld, fb);
            EmitConvertToString(NormalizeInferredType(_program.Symbols.GetSymbol(firstField.SymbolId).DataType));
        }
        else _il.Emit(OpCodes.Ldstr, "");
    }

    private void EmitSpeedKeyArg(SpeedSeekStmt stmt)
    {
        EmitStringArg(stmt.KeyValue);
    }

    private void EmitObjectArrayArg(Expr source)
    {
        // object[] dalla sorgente (array/UDT): materializza un object[].
        if (source is ArrayLiteralExpr arrLit)
        {
            _il.Emit(OpCodes.Ldc_I4, arrLit.Elements.Length);
            _il.Emit(OpCodes.Newarr, _types.Object);
            for (var i = 0; i < arrLit.Elements.Length; i++)
            {
                _il.Emit(OpCodes.Dup);
                _il.Emit(OpCodes.Ldc_I4, i);
                EmitExpression(arrLit.Elements[i]);
                var eClr = DataTypeMapper.Map(NormalizeInferredType(InferType(arrLit.Elements[i])), _types, _classTypes);
                if (eClr.IsValueType) _il.Emit(OpCodes.Box, eClr);
                _il.Emit(OpCodes.Stelem_Ref);
            }
            return;
        }
        if (source is VariableExpr srcUdtVe
            && NormalizeInferredType(_program.Symbols.GetSymbol(srcUdtVe.SymbolId).DataType) is UserDefinedType uSrcUdt)
        {
            // Record UDT (struct): estrai i campi in un object[] (per ISAM APPEND).
            var fields2 = uSrcUdt.Fields.Where(f => f.SymbolId >= 0)
                .Select(f => _classFields.TryGetValue(f.SymbolId, out var fb) ? fb : null)
                .Where(f => f is not null).ToArray();
            _il.Emit(OpCodes.Ldc_I4, fields2.Length);
            _il.Emit(OpCodes.Newarr, _types.Object);
            // UDT GLOBALE (programma top-level, es. `U` di DbIsamMinimalCrud): il valore STRUCT
            // vive BOXED nel bag globale (LoadGlobal), NON in una local. Prima si usava
            // GetOrDeclareLocal → una local NUOVA non inizializzata → ISAM APPEND con campi
            // vuoti. Si copia il globale in un local tipizzato e si leggono i campi da lì.
            LocalBuilder? globalTmp = null;
            if (IsGlobalSymbol(srcUdtVe.SymbolId))
            {
                var gclr = GlobalClrType(srcUdtVe.SymbolId);
                _il.Emit(OpCodes.Ldc_I4, srcUdtVe.SymbolId);
                _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.LoadGlobal), typeof(int)));
                _il.Emit(OpCodes.Unbox_Any, gclr);
                globalTmp = _il.DeclareLocal(gclr);
                _il.Emit(OpCodes.Stloc, globalTmp);
            }
            for (var i = 0; i < fields2.Length; i++)
            {
                _il.Emit(OpCodes.Dup);
                _il.Emit(OpCodes.Ldc_I4, i);
                if (globalTmp is not null) _il.Emit(OpCodes.Ldloc, globalTmp);
                else _il.Emit(OpCodes.Ldloca, GetOrDeclareLocal(srcUdtVe.SymbolId));
                _il.Emit(OpCodes.Ldfld, fields2[i]!);
                _il.Emit(OpCodes.Box, fields2[i]!.FieldType);
                _il.Emit(OpCodes.Stelem_Ref);
            }
            return;
        }
        if (source is VariableExpr srcVe)
        {
            EmitLoadVariable(srcVe.SymbolId);
            // Variabile array → object[] (cast del riferimento se già object[]).
            _il.Emit(OpCodes.Castclass, typeof(object[]));
        }
        else
        {
            _il.Emit(OpCodes.Ldc_I4_0);
            _il.Emit(OpCodes.Newarr, _types.Object);
        }
    }

    /// <summary>
    /// Prima faceva uno Stloc DIRETTO dell'`object[]` (ritornato da SpeedGet, un elemento
    /// per campo — stesso ordine di <see cref="EmitObjectArrayArg"/> usato da SPEED PUT,
    /// verificato: `DatabaseService.Commercial.SpeedInsert`/`SpeedGet` sono entrambi
    /// posizionali per indice) direttamente su una variabile UDT (STRUCT/value type) — un
    /// mismatch di tipo CLR (`object[]` vs la struct del target) che non dava un errore di
    /// verifica normale ma corrompeva il calcolo del layout struct visto dal JIT, causando
    /// un SIGSEGV NATIVO in RyuJIT (`Lowering::SpillStructCallResult`/`ClassLayout::Create`)
    /// — un crash a questo livello non è mai intercettabile da un try/catch .NET, quindi
    /// l'intero processo moriva senza alcun errore visibile e senza flush dell'output già
    /// stampato (bug `DbSpeedFullTests.bas`, isolato con bisezione binaria su `SPEED GET`).
    /// SPEED PUT fa correttamente il percorso INVERSO (struct → object[]); questo è la
    /// controparte mancante: copia ogni campo dell'`object[]` nel campo corrispondente
    /// della struct, nello stesso ordine di dichiarazione UDT usato da PUT.
    /// </summary>
    private void EmitCopyObjectArrayToTarget(Target target)
    {
        if (target is not VariableTarget dstVar) { _il.Emit(OpCodes.Pop); return; }

        var dstType = NormalizeInferredType(_program.Symbols.GetSymbol(dstVar.SymbolId).DataType);
        if (dstType is UserDefinedType dstUdt)
        {
            var tempArr = _il.DeclareLocal(typeof(object[]));
            _il.Emit(OpCodes.Castclass, typeof(object[]));
            _il.Emit(OpCodes.Stloc, tempArr);

            var fields = dstUdt.Fields.Where(f => f.SymbolId >= 0)
                .Select(f => _classFields.TryGetValue(f.SymbolId, out var fb) ? fb : null)
                .Where(f => f is not null).ToArray();

            for (var i = 0; i < fields.Length; i++)
            {
                if (_parameters.TryGetValue(dstVar.SymbolId, out var param))
                    _il.Emit(param.IsByRef ? OpCodes.Ldarg : OpCodes.Ldarga, param.ArgIndex);
                else
                    _il.Emit(OpCodes.Ldloca, GetOrDeclareLocal(dstVar.SymbolId));
                _il.Emit(OpCodes.Ldloc, tempArr);
                _il.Emit(OpCodes.Ldc_I4, i);
                _il.Emit(OpCodes.Ldelem_Ref);
                // I motori serializzano i numerici con la LORO larghezza on-disk (es. SPEED
                // LONG = 4 byte → boxed Int32) ma il campo della struct UDT è il CLR del
                // linguaggio (LONG → Int64): Unbox_Any(Int64) su un Int32 boxato lancia
                // InvalidCastException. Coerce via ToDouble(object) + conv (stesso pattern
                // del Receive isola) per i numerici integrali/floating; Decimal (Currency) e
                // Bool (Bit) sono già esatti (il motore li serializza con la stessa larghezza
                // CLR) e restano Unbox_Any.
                var fieldClr = fields[i]!.FieldType;
                if (fieldClr.IsValueType && fieldClr != typeof(decimal) && fieldClr != typeof(bool))
                {
                    _il.Emit(OpCodes.Call, _types.IlRuntimeToDoubleMethod);
                    IlConversion.EmitConvert(_il, _types, new DoubleType(), _types.Double, fieldClr);
                }
                else
                    _il.Emit(OpCodes.Unbox_Any, fieldClr);
                _il.Emit(OpCodes.Stfld, fields[i]!);
            }
            return;
        }

        // Target non-UDT (es. variabile object[]/array generico): comportamento preesistente.
        EmitStoreVariable(dstVar.SymbolId, () => { });
    }

    /// <summary>
    /// Copia il risultato di `IlRuntime.SpeedSelect` (object?[][] — una riga per ogni record,
    /// già riordinata per campo di destinazione, vedi BuildSpeedQueryPlanJson/DestFields) in un
    /// array DIM'd di struct UDT (`INTO arr() OF TypeName`). Stesso schema per-campo di
    /// EmitCopyObjectArrayToTarget (Unbox_Any con coerce ToDouble per i numerici, dato che i
    /// valori aggregati SPEED/motore possono arrivare con una larghezza CLR diversa da quella
    /// dello struct — es. SUM ritorna decimal, COUNT ritorna long), ma ripetuto per ELEMENTO
    /// di un array invece che per un singolo record: alloca `new TypeName[righe.Length]` e,
    /// per ciascuna riga, prende l'indirizzo dell'elemento struct (Ldelema, un value type in
    /// un array richiede l'indirizzo per Stfld, non il valore) e vi copia i campi.
    /// </summary>
    private void EmitCopyObjectArrayArrayToUdtArrayTarget(Target target, UserDefinedType destUdt)
    {
        if (target is not VariableTarget dstVar) { _il.Emit(OpCodes.Pop); return; }

        var tempRows = _il.DeclareLocal(typeof(object[][]));
        _il.Emit(OpCodes.Castclass, typeof(object[][]));
        _il.Emit(OpCodes.Stloc, tempRows);

        var fields = destUdt.Fields.Where(f => f.SymbolId >= 0)
            .Select(f => _classFields.TryGetValue(f.SymbolId, out var fb) ? fb : null)
            .Where(f => f is not null).ToArray();
        if (!_classTypes.TryGetValue(destUdt.SymbolId, out var udtClrType))
            throw new NotSupportedException($"Backend IL: UDT '{destUdt.OriginalName.Lexeme}' non ancora registrato come tipo CLR.");

        var lenLocal = _il.DeclareLocal(typeof(int));
        _il.Emit(OpCodes.Ldloc, tempRows);
        _il.Emit(OpCodes.Ldlen);
        _il.Emit(OpCodes.Conv_I4);
        _il.Emit(OpCodes.Stloc, lenLocal);

        var arrLocal = _il.DeclareLocal(udtClrType.MakeArrayType());
        _il.Emit(OpCodes.Ldloc, lenLocal);
        _il.Emit(OpCodes.Newarr, udtClrType);
        _il.Emit(OpCodes.Stloc, arrLocal);

        var iLocal = _il.DeclareLocal(typeof(int));
        _il.Emit(OpCodes.Ldc_I4_0);
        _il.Emit(OpCodes.Stloc, iLocal);

        var loopStart = _il.DefineLabel();
        var loopCheck = _il.DefineLabel();
        _il.Emit(OpCodes.Br, loopCheck);
        _il.MarkLabel(loopStart);

        var rowLocal = _il.DeclareLocal(typeof(object[]));
        _il.Emit(OpCodes.Ldloc, tempRows);
        _il.Emit(OpCodes.Ldloc, iLocal);
        _il.Emit(OpCodes.Ldelem_Ref);
        _il.Emit(OpCodes.Stloc, rowLocal);

        for (var j = 0; j < fields.Length; j++)
        {
            _il.Emit(OpCodes.Ldloc, arrLocal);
            _il.Emit(OpCodes.Ldloc, iLocal);
            _il.Emit(OpCodes.Ldelema, udtClrType);
            _il.Emit(OpCodes.Ldloc, rowLocal);
            _il.Emit(OpCodes.Ldc_I4, j);
            _il.Emit(OpCodes.Ldelem_Ref);
            var fieldClr = fields[j]!.FieldType;
            if (fieldClr.IsValueType && fieldClr != typeof(decimal) && fieldClr != typeof(bool))
            {
                _il.Emit(OpCodes.Call, _types.IlRuntimeToDoubleMethod);
                IlConversion.EmitConvert(_il, _types, new DoubleType(), _types.Double, fieldClr);
            }
            else
                _il.Emit(OpCodes.Unbox_Any, fieldClr);
            _il.Emit(OpCodes.Stfld, fields[j]!);
        }

        _il.Emit(OpCodes.Ldloc, iLocal);
        _il.Emit(OpCodes.Ldc_I4_1);
        _il.Emit(OpCodes.Add);
        _il.Emit(OpCodes.Stloc, iLocal);

        _il.MarkLabel(loopCheck);
        _il.Emit(OpCodes.Ldloc, iLocal);
        _il.Emit(OpCodes.Ldloc, lenLocal);
        _il.Emit(OpCodes.Blt, loopStart);

        _il.Emit(OpCodes.Ldloc, arrLocal);
        EmitStoreVariable(dstVar.SymbolId, () => { });
    }

    private void EmitAssignTargetFromTop(Target target, DataType valueType)
    {
        if (target is not VariableTarget vt) { _il.Emit(OpCodes.Pop); return; }
        // Coerce del valore (prodotto da una call runtime) al tipo del target PRIMA di
        // qualsiasi store: senza, un double in un target INTEGER/LONG/BIT (temp int32/int64)
        // è IL invalido → InvalidProgramException.
        EmitCoerceTypes(NormalizeInferredType(valueType), NormalizeInferredType(_program.Symbols.GetSymbol(vt.SymbolId).DataType));
        EmitAssignTargetFromTop(target);
    }

    private void EmitAssignTargetFromTop(Target target)
    {
        if (target is not VariableTarget vt) return;
        // Il valore è GIÀ sullo stack (risultato della call precedente). Per un GLOBALE,
        // EmitStoreVariable(int, action vuota) pusha la key DOPO il valore già presente →
        // StoreGlobal legge chiave/valore INVERTITI (il decompilato mostrava
        // StoreGlobal((int)LedgerRegister(...), (object)551)). Si salva il valore in un
        // local temp e si emette key PRIMA del valore.
        if (IsGlobalSymbol(vt.SymbolId))
        {
            var targetType = NormalizeInferredType(_program.Symbols.GetSymbol(vt.SymbolId).DataType);
            // Target STRING[] che riceve object?[] (dict JSON dal runtime LedgerRegister/
            // Balance/Get): converti ogni elemento a stringa JSON (DbLedgerFullTests
            // 'JSON.Stringify(tuttiConti$)' su STRING[]). JSON[] resta invariato.
            if (targetType is ArrayType { ElementType: StringType })
                _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.LedgerStringifyArray), typeof(object[])));
            var clr = GlobalClrType(vt.SymbolId);
            var tmp = _il.DeclareLocal(clr);
            _il.Emit(OpCodes.Stloc, tmp);
            _il.Emit(OpCodes.Ldc_I4, vt.SymbolId);
            _il.Emit(OpCodes.Ldloc, tmp);
            if (clr.IsValueType) _il.Emit(OpCodes.Box, clr);
            _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.StoreGlobal), typeof(int), typeof(object)));
            return;
        }
        EmitStoreVariable(vt.SymbolId, () => { });
    }

    private void EmitDouble(double value)
        => _il.Emit(OpCodes.Ldc_R8, value);

}
