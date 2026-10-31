// FILE: IlProgramCompiler.Members.cs
// PARTIAL — dominio MEMBRI/NATIVE-COLLECTIONS (accesso campi/proprietà/metodi/indice,
// native collection resolution, operatori di classe).
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

/// <summary>Partial del transpiler IL: dominio MEMBRI (field/property/method/index/native-collections).</summary>
public sealed partial class IlProgramCompiler
{

    private bool TryEmitNativeCollectionFieldAccess(FieldAccessExpr expr)
    {
        if (!TryGetNativeCollectionType(InferType(expr.Structure), out var collectionType)) return false;

        // dict.Keys / dict.Values (senza parentesi): Dictionary.Keys/Values → array via
        // Enumerable.ToArray (come il metodo .Keys()/.Values()).
        if (expr.Field.Lexeme.Equals("KEYS", StringComparison.OrdinalIgnoreCase) ||
            expr.Field.Lexeme.Equals("VALUES", StringComparison.OrdinalIgnoreCase))
        {
            var isKeys = expr.Field.Lexeme.Equals("KEYS", StringComparison.OrdinalIgnoreCase);
            var dictType = NormalizeInferredType(InferType(expr.Structure));
            var elemClr = dictType switch
            {
                DictType dd => DataTypeMapper.Map(isKeys ? dd.KeyType : dd.ValueType, _types, _classTypes),
                SortedDictType sd => DataTypeMapper.Map(isKeys ? sd.KeyType : sd.ValueType, _types, _classTypes),
                _ => throw new NotSupportedException($"Backend IL: Keys/Values richiede un DICT/SORTED.")
            };
            var prop = collectionType.GetProperty(isKeys ? "Keys" : "Values");
            var propGetter = prop?.GetGetMethod()
                ?? throw new NotSupportedException($"Backend IL: collection '{collectionType}' non espone {(isKeys ? "Keys" : "Values")}.");
            EmitExpression(expr.Structure);
            _il.Emit(OpCodes.Callvirt, propGetter);
            _il.Emit(OpCodes.Call, EnumerableToArrayMethod.MakeGenericMethod(elemClr));
            return true;
        }

        // q.Peek / q.Dequeue (Queue), s.Pop (Stack) come FIELD (senza parentesi): sono
        // METODI CLR — emit call diretto.
        if (expr.Field.Lexeme.Equals("PEEK", StringComparison.OrdinalIgnoreCase) ||
            expr.Field.Lexeme.Equals("DEQUEUE", StringComparison.OrdinalIgnoreCase) ||
            expr.Field.Lexeme.Equals("POP", StringComparison.OrdinalIgnoreCase))
        {
            var m = collectionType.GetMethod(expr.Field.Lexeme, Type.EmptyTypes)
                ?? throw new NotSupportedException($"Backend IL: collection '{collectionType}' non espone {expr.Field.Lexeme}.");
            EmitExpression(expr.Structure);
            _il.Emit(OpCodes.Callvirt, m);
            if (m.ReturnType == typeof(int) || m.ReturnType == typeof(long)) _il.Emit(OpCodes.Conv_R8);
            return true;
        }

        if (!expr.Field.Lexeme.Equals("COUNT", StringComparison.OrdinalIgnoreCase))
            throw new NotSupportedException($"Backend IL: membro collection '{expr.Field.Lexeme}' non ancora supportato.");

        MethodInfo? getterF = null;
        bool closedF = true;
        try { closedF = ClosedOverTypeBuilder(collectionType); } catch { closedF = true; }
        if (!closedF)
            try { getterF = collectionType.GetProperty(nameof(List<int>.Count))?.GetGetMethod(); } catch { getterF = null; }
        if (getterF is null)
        {
            // List<Udt> (TypeBuilder): reflection sul Count non disponibile → dispatch runtime.
            EmitExpression(expr.Structure);
            if (collectionType.IsValueType) _il.Emit(OpCodes.Box, collectionType);
            _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.CollectionCount), typeof(object)));
            _il.Emit(OpCodes.Conv_R8);
            return true;
        }
        EmitExpression(expr.Structure);
        _il.Emit(OpCodes.Callvirt, getterF);
        _il.Emit(OpCodes.Conv_R8);
        return true;
    }

    /// <summary>
    /// Chiama un accessor get_/set_ di una PROPERTY: `callvirt` se astratto (proprietà
    /// d'interfaccia — nessun corpo `call`-abile direttamente per un metodo astratto),
    /// `call` diretta altrimenti (nessun supporto a PROPERTY VIRTUAL/OVERRIDE su classi
    /// concrete in questo MVP).
    /// </summary>
    private void EmitPropertyAccessorCall(MethodBuilder accessor) =>
        _il.Emit(accessor.Attributes.HasFlag(MethodAttributes.Abstract) ? OpCodes.Callvirt : OpCodes.Call, accessor);

    /// <summary>
    /// Scrittura di un campo O una PROPERTY, statico o d'istanza — vedi EmitFieldAccess
    /// per la simmetria in lettura. Campo statico: valuta solo il valore, poi Stsfld
    /// (nessuna istanza). Campo d'istanza: valuta istanza, poi valore, poi Stfld.
    /// Property con SET: come campo d'istanza ma chiama il setter (`call`, mai
    /// `callvirt` — nessun supporto a PROPERTY VIRTUAL in questo MVP; proprietà STATIC
    /// non ancora supportate).
    /// </summary>
    private bool TryGetStructureDataType(Target? structure, out DataType dataType)
    {
        switch (structure)
        {
            case VariableTarget vt:
                dataType = Basic.SymbolTable.UnresolvedSymbolId == vt.SymbolId ? new InferredType() : _program.Symbols.GetSymbol(vt.SymbolId).DataType;
                return true;
            case ArrayElementTarget aet:
                dataType = _program.Symbols.GetSymbol(aet.SymbolId).DataType is ArrayType at ? at.ElementType : new InferredType();
                return true;
            case FieldTarget ft when ft.Structure is not null:
                return TryGetStructureDataType(ft.Structure, out dataType);
            case IndexedTarget it when it.Base is VariableExpr itVar:
                // Users[id].nome — l'elemento di un DICT → il tipo del VALORE (JSON).
                var itType = _program.Symbols.GetSymbol(itVar.SymbolId).DataType;
                dataType = itType switch
                {
                    DictType d => d.ValueType,
                    SortedDictType d => d.ValueType,
                    _ => new InferredType()
                };
                return true;
            default:
                dataType = new InferredType();
                return false;
        }
    }

    /// <summary>Tipo (DataType) di un Target di CAMPO, risolto ricorsivamente: `a.b.c` → il
    /// tipo del campo `c` (dal tipo del campo `b`, a sua volta dal tipo di `a`). Serve al
    /// resolver dei campi annidati (`ResolveFieldTargetSymbolId`). Supporta basi Variable
    /// (incl. SELF di classe) e FieldTarget di UDT/CLASS.</summary>
    private bool TryGetFieldType(Target t, out DataType dt)
    {
        switch (t)
        {
            case VariableTarget vt:
                // SELF/ME di una CLASSE (per nome: in un costruttore/metodo di classe il tipo
                // di SELF non è sempre leggibile dal simbolo). Per un metodo di UDT (value
                // type) "self" è già gestito da TryResolveSelfArgIndex altrove.
                if (_currentClassSymbolId is int cid && _program.Symbols.GetSymbol(cid) is { } csym
                    && (vt.NameCleaned.Lexeme.Equals("SELF", StringComparison.OrdinalIgnoreCase)
                        || vt.NameCleaned.Lexeme.Equals("ME", StringComparison.OrdinalIgnoreCase)))
                {
                    dt = csym.DataType;
                    return true;
                }
                dt = _program.Symbols.GetSymbol(vt.SymbolId).DataType;
                return true;
            case FieldTarget ft:
                if (!TryGetFieldType(ft.Structure, out var baseDt)) { dt = new InferredType(); return false; }
                var b = NormalizeInferredType(baseDt);
                // Un campo di un TYPE il cui DataType è rimasto un UnresolvedQualifiedType
                // (nome non ancora legato al UserDefinedType): risolvi per nome, come
                // ResolveOperatorReturnType. Senza, `stazione.pos.lat` non risolveva il campo.
                if (b is UnresolvedQualifiedType uqt)
                {
                    var nm = uqt.QualifiedName.FullName;
                    b = _program.GetAllUserDefinedTypes()
                        .FirstOrDefault(u => u.OriginalName.Lexeme.Equals(nm, StringComparison.OrdinalIgnoreCase)) ?? b;
                }
                DataType fdt;
                if (b is UserDefinedType u)
                    fdt = u.Fields.FirstOrDefault(f => f.Name.Lexeme.Equals(ft.OriginalToken.Lexeme, StringComparison.OrdinalIgnoreCase))?.DataType ?? new InferredType();
                else if (b is ClassType c)
                {
                    // Campi EREDITATI, con lo STESSO cammino canonico a 3 passi del linker
                    // (Linker.Resolver.cs: risalita ResolvedBaseClass → BaseClassSymbolId →
                    // OriginalBaseClassName). Il backend IL non deve dipendere dal fatto che la
                    // base sia già stata risolta: nel bridge MVC `Response`/`Request` sono
                    // dichiarati nella base cross-module `MVC.HttpController` (modulo IMPORT MVC)
                    // e con solo `ResolvedBaseClass` la risalita si fermava alla classe derivata →
                    // tipo InferredType → «impossibile risolvere il campo 'StatusCode' per
                    // un'istanza di tipo 'FieldTarget'» (i sample MVC non giravano su IL).
                    fdt = new InferredType();
                    for (var current = c.Definition; current is not null;)
                    {
                        var inherited = current.Members.OfType<FieldMember>()
                            .FirstOrDefault(m => m.Declaration.NameCleaned.Lexeme.Equals(ft.OriginalToken.Lexeme, StringComparison.OrdinalIgnoreCase));
                        if (inherited is not null)
                        {
                            fdt = inherited.Declaration.DataType ?? new InferredType();
                            break;
                        }
                        if (current.ResolvedBaseClass is not null)
                            current = current.ResolvedBaseClass;
                        else if (current.BaseClassSymbolId is int baseSymId)
                            current = _program.Symbols.GetSymbol(baseSymId)?.DeclarationNode as ClassDefinition;
                        else if (current.OriginalBaseClassName is { } baseTok)
                        {
                            // Nome qualificato (MVC.HttpController) o semplice (HttpController).
                            var baseName = baseTok.Lexeme;
                            current = _program.Symbols.FindSymbolsByName(baseName)
                                .FirstOrDefault(s => s.DeclarationNode is ClassDefinition)?.DeclarationNode as ClassDefinition
                                ?? (baseName.Contains('.')
                                    ? _program.Symbols.FindSymbolsByName(baseName[(baseName.LastIndexOf('.') + 1)..])
                                        .FirstOrDefault(s => s.DeclarationNode is ClassDefinition)?.DeclarationNode as ClassDefinition
                                    : null);
                        }
                        else current = null;
                    }
                }
                else { dt = new InferredType(); return false; }
                // Il campo di un TYPE ha spesso DataType = UnresolvedQualifiedType (nome non
                // ancora legato): risolvilo per nome, altrimenti `SELF.stazione` non era
                // riconosciuto come value type (EmitInstanceValue invece di ...Address → IL
                // invalido per il nesting a 2 livelli).
                if (fdt is UnresolvedQualifiedType uqtF)
                {
                    var nmF = uqtF.QualifiedName.FullName;
                    fdt = _program.GetAllUserDefinedTypes()
                        .FirstOrDefault(x => x.OriginalName.Lexeme.Equals(nmF, StringComparison.OrdinalIgnoreCase)) ?? fdt;
                }
                dt = fdt;
                return true;
            default:
                dt = new InferredType();
                return false;
        }
    }

    private void EmitFieldAssignment(FieldTarget target, Expr value)
    {
        // WEBSOCKET: ws.OnMessage/OnClose/OnError = "NomeSub" — il NOME della SUB viene risolto a
        // COMPILE-TIME in un symbolId (i metodi generati sono Proc_<symbolId>, il nome BASIC non è
        // risolvibile a runtime nel modello IL), stesso pattern di HTTP ROUTE/USE. Senza questa
        // risoluzione il campo resterebbe una stringa inerte e nessun handler partirebbe mai
        // (no-op silenzioso → rifiutato). Il runtime (IlRuntime.WsSetHandler) collega il symbolId
        // all'handle; il loop di ricezione lo usa per la dispatch.
        if (target.Structure is not null && TryGetStructureDataType(target.Structure, out var wsStructType)
            && wsStructType is WebSocketType
            && target.OriginalToken.Lexeme is "OnMessage" or "OnClose" or "OnError")
        {
            if (value is not StringExpr wsNameExpr)
                throw new NotSupportedException("Backend IL: gli handler WebSocket (ws.OnMessage/OnClose/OnError) richiedono il NOME della SUB come stringa costante.");
            var wsNormalized = wsNameExpr.Value.Replace("_", "");
            var wsDef = _procedureDefinitions.Values
                .FirstOrDefault(d => d.OriginalName.Lexeme.Replace("_", "").Equals(wsNormalized, StringComparison.OrdinalIgnoreCase));
            if (wsDef is null)
                throw new NotSupportedException($"Backend IL: procedura '{wsNameExpr.Value}' non trovata per l'handler WebSocket.");
            EmitInstanceValue(target.Structure);
            var wsStructClr = DataTypeMapper.Map(NormalizeInferredType(wsStructType), _types, _classTypes);
            if (wsStructClr.IsValueType) _il.Emit(OpCodes.Box, wsStructClr);
            _il.Emit(OpCodes.Ldstr, target.OriginalToken.Lexeme);
            _il.Emit(OpCodes.Ldc_I4, wsDef.SymbolId);
            _il.Emit(OpCodes.Call, IlRuntimeWsSetHandlerMethod);
            return;
        }

        // Scrittura di un CAMPO DINAMICO (risposta.status = "ok" su JSON/oggetto dinamico):
        // stessa condizione di EmitFieldAccess — struttura JSON/ANY/tipi Http → JsonFieldSet.
        // `SELF.Response.StatusCode = 404` (bridge MVC): la struttura è un FieldTarget (campo di
        // classe), non una variabile → TryGetStructureDataType ricava il tipo della CLASSE
        // (ArticoliController) e non del campo, la condizione NON scatta e
        // ResolveFieldTargetSymbolId lancia «impossibile risolvere il campo 'StatusCode' per
        // un'istanza di tipo 'FieldTarget'» (i sample MVC non giravano su IL). Per un FieldTarget
        // il tipo va risolto con TryGetFieldType (che risale anche i campi EREDITATI).
        DataType? fieldStoreStructType = null;
        if (target.Structure is not null)
        {
            if (target.Structure is FieldTarget && TryGetFieldType(target.Structure, out var storeFieldType))
                fieldStoreStructType = storeFieldType;
            else if (TryGetStructureDataType(target.Structure, out var storeStructType))
                fieldStoreStructType = storeStructType;
        }
        if (target.Structure is not null
            && fieldStoreStructType is Basic.JsonType or AnyType or HttpRequestType or HttpResponseType
                or HttpOptionsType or HttpHeadersType or HttpConfigType or HttpPostedDataType
                or SqlResultSetType or WebSocketType or PortfolioResultType
                or RebalanceStrategyType or BacktestResultType)
        {
            EmitInstanceValue(target.Structure);
            var structClr = DataTypeMapper.Map(NormalizeInferredType(fieldStoreStructType!), _types, _classTypes);
            if (structClr.IsValueType) _il.Emit(OpCodes.Box, structClr);
            _il.Emit(OpCodes.Ldstr, target.OriginalToken.Lexeme);
            EmitExpression(value);
            var valueClr = DataTypeMapper.Map(NormalizeInferredType(InferType(value)), _types, _classTypes);
            if (valueClr.IsValueType) _il.Emit(OpCodes.Box, valueClr);
            // NON JsonFieldSet: sui tipi HTTP la risposta è un oggetto CLR (PCodeHttpResponse) e
            // `StatusCode` ha setter privato ⇒ quel path era un no-op SILENZIO (la risposta tornava
            // 200 invece del 404 chiesto dal controller MVC). DynamicMemberSet copre dict,
            // property (anche non pubbliche) e l'idioma Set<Membro> (SetStatus).
            _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.DynamicMemberSet), typeof(object), typeof(string), typeof(object)));
            return;
        }

        // REGEX: r.Pattern = "\d+" — il campo CLR della RegexWrapper (Stfld sull'istanza).
        if (target.Structure is VariableTarget vtRegex && NormalizeInferredType(_program.Symbols.GetSymbol(vtRegex.SymbolId).DataType) is RegexType)
        {
            var patternProp = typeof(Basic.RegexNative.RegexWrapper).GetProperty("Pattern")!;
            EmitLoadVariable(vtRegex.SymbolId);
            EmitStringArg(value);
            _il.Emit(OpCodes.Callvirt, patternProp.GetSetMethod()!);
            return;
        }

        var fieldSymbolId = ResolveFieldTargetSymbolId(target);

        if (_classFields.TryGetValue(fieldSymbolId, out var fieldBuilder))
        {
            if (fieldBuilder.IsStatic)
            {
                EmitExpression(value);
                EmitCoerceTypes(NormalizeInferredType(InferType(value)), NormalizeInferredType(_program.Symbols.GetSymbol(fieldSymbolId).DataType));
                _il.Emit(OpCodes.Stsfld, fieldBuilder);
                return;
            }
            // Un campo di un VALUE TYPE (UDT) richiede l'INDIRIZZO dell'istanza per Stfld
            // (Ldloca/Ldelema), mai il VALORE (Ldloc/Ldelem) — scrivere in una copia sullo
            // stack non avrebbe alcun effetto sull'originale (locale o elemento
            // dell'array). Un campo di CLASS (reference type) vuole invece il riferimento
            // (Ldloc/Ldelem_Ref va bene: il riferimento stesso è il "valore"). SELF/ME non
            // si presenta mai qui per un UDT in questo MVP (nessun metodo/costruttore UDT
            // supportato).
            // `lista(i).campo = v` — elemento di LISTA di UDT (value type): la lista non
            // espone un indirizzo stabile dell'elemento (List<T>[i] ritorna una COPIA),
            // quindi il percorso Ldelema degli array non è applicabile. Round-trip
            // get → modifica della copia boxed → set. (Prima: GetElementType() su una
            // generic List<T> → NotSupportedException.)
            if (fieldBuilder.DeclaringType!.IsValueType
                && target.Structure is IndexedTarget itList
                && TryEmitUdtListElementFieldAssignment(target, itList, value, fieldBuilder, fieldSymbolId))
                return;
            if (fieldBuilder.DeclaringType!.IsValueType)
                EmitInstanceAddress(target.Structure);
            else
                EmitInstanceValue(target.Structure);
            // BYREF catturato da una closure (piano Closure-ByRef-Box-Linker-Definitivo,
            // Fase 4): il campo __ClosureCtx è un ByRefBox<T> — scrivi ATTRAVERSO il box
            // (Ldfld il campo per ottenere il box, POI Stfld su Value), mai Stfld sul campo
            // stesso (sovrascriverebbe il RIFERIMENTO al box, perdendo l'identità col box
            // del chiamante). Specchio di EmitFieldAccess in lettura e di EmitStoreVariable
            // ramo _capturedFields/param/local box.
            var isByRefBoxField = !fieldBuilder.IsStatic && IsByRefBoxFieldType(fieldBuilder.FieldType);
            // Seeding Fase 4 (piano Closure-ByRef-Box-Linker-Definitivo): il valore è il BOX
            // stesso (MakeByRefExpr spinge il box del parametro) → assegna il box al campo
            // (Stfld sul campo), NON scrivere "attraverso" campo.Value (che presuppone un
            // box già nel campo, ma in questo momento il campo è null/da inizializzare).
            // Per QUALUNQUE altro valore (espressione normale) su un campo box, scrivi
            // ATTRAVERSO il box esistente (Stfld su Value), specchio di EmitFieldAccess.
            var isBoxSeeding = isByRefBoxField && value is MakeByRefExpr;
            if (isByRefBoxField && !isBoxSeeding)
                _il.Emit(OpCodes.Ldfld, fieldBuilder);
            EmitExpression(value);
            var fieldDataType = NormalizeInferredType(_program.Symbols.GetSymbol(fieldSymbolId).DataType);
            // Parità VM: campo STRING * N — l'assegnazione PADDA/TRONCA al N (prima
            // EmitCoerceTypes usciva subito su stringa senza applicare il padding, quindi
            // la lettura successiva restituiva la stringa corta invece del campo fisso —
            // golden 'Test Cliente                  , Saldo:' su Nome AS STRING * 30).
            if (fieldDataType is StringType fieldStr && fieldStr.FixedLength is > 0 and var fieldLen)
            {
                _il.Emit(OpCodes.Ldc_I4, fieldLen);
                _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.AdjustFixedString)));
            }
            else if (isBoxSeeding)
            {
                // Seeding Fase 4: il valore è già ByRefBox<T>, nessuna coerce al tipo logico
                // (Int16) — coerce un box a Int16 produrrebbe IL invalido.
            }
            else
            {
                // Campo BIT (bool CLR): EmitConvert(double→bool) non è gestito e il DataType
                // normalizzato del symbol del campo è DoubleType (non BitType) — EmitCoerceTypes
                // usciva senza istruzioni lasciando il double (ldc.r8 -1.0) su Stfld bool →
                // il campo diventava false. Il BIT è int32 1/0/-1: conv.i4 → -1/1 → true,
                // 0 → false. Golden TypesUdtGlobal 'Attivo:-1' (P1.Attivo = -1).
                var valueDynType = NormalizeInferredType(InferType(value));
                // Il CAMPO è object (HTTPREQUEST/HTTPRESPONSE/JSON/ANY → object: DataTypeMapper
                // li mappa a object): il valore va memorizzato TALE E QUALE — coercizione
                // numerica di un oggetto è esattamente il difetto del bridge MVC
                // (`SELF.Request = req` / `SELF.Response = res`): CoerceDynamicNumber su un
                // Dictionary → «Unable to cast Dictionary to IConvertible» (risposta 204) e,
                // sulla rotta 404, double in un campo object SENZA box → «invalid program».
                var fieldClrIsObject = DataTypeMapper.Map(fieldDataType, _types, _classTypes) == _types.Object;
                if (!fieldClrIsObject
                    && DataTypeMapper.Map(valueDynType, _types, _classTypes) == _types.Object)
                {
                    // Valore DINAMICO (JsonFieldGet — campo JSON/ANY): un object boxed va
                    // risolto al tipo del campo. DATE (DateTime) → data (seriale/ISO);
                    // numerico/BIT → numero. Senza: il reference finiva in un campo value-type
                    // (garbage). Bug reale: mapping JSON→UDT del report LEDGER.
                    if (fieldDataType is DateType)
                    {
                        _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.CoerceDynamicDate), typeof(object)));
                    }
                    else
                    {
                        _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.CoerceDynamicNumber), typeof(object)));
                        if (fieldBuilder.FieldType == typeof(bool)) _il.Emit(OpCodes.Conv_I4);
                        else EmitCoerceTypes(new DoubleType(), fieldDataType);
                    }
                }
                else if (fieldBuilder.FieldType == typeof(bool))
                    _il.Emit(OpCodes.Conv_I4);
                else
                    EmitCoerceTypes(valueDynType, fieldDataType);
            }
            // Bug regressione Fase 4 (commit 93f51e16): la riga sotto applicava
            // FieldType.GetField("Value") a QUALUNQUE campo non-seeding, anche i campi
            // NORMALI (STRING/INTEGER/UDT/...) che non hanno un membro "Value" — GetField
            // ritornava null e Emit(Stfld, null) lanciava ArgumentNullException("field") per
            // OGNI assegnazione a campo su un'istanza non-box (es. Persone(0).Nome = "Mario").
            // Lo sbox va applicato SOLO quando il campo è davvero un ByRefBox<T>.
            var targetFieldForWrite = isBoxSeeding
                ? fieldBuilder
                : isByRefBoxField
                    ? fieldBuilder.FieldType.GetField("Value")!
                    : fieldBuilder;
            _il.Emit(OpCodes.Stfld, targetFieldForWrite);
            return;
        }

        if (_classProperties.TryGetValue(fieldSymbolId, out var propInfo) && propInfo.Setter is not null)
        {
            EmitInstanceValue(target.Structure);
            EmitExpression(value);
            EmitCoerceTypes(NormalizeInferredType(InferType(value)), NormalizeInferredType(_program.Symbols.GetSymbol(fieldSymbolId).DataType));
            EmitPropertyAccessorCall(propInfo.Setter);
            return;
        }

        // Assegnazione WHOLE-ARRAY a un campo array di UDT (Cfg.Ports = {80, 443}):
        // l'istanza UDT è un value type → indirizzo (Ldloca) + l'array tipizzato + Stfld.
        if (_classFieldArrays.TryGetValue(fieldSymbolId, out var fieldArrayInfo))
        {
            EmitInstanceAddress(target.Structure);
            if (value is ArrayLiteralExpr arrLit)
                EmitTypedArrayLiteral(arrLit.Elements, fieldArrayInfo.ElementDataType);
            else
            {
                EmitExpression(value);
                EmitCoerceTypes(NormalizeInferredType(InferType(value)), fieldArrayInfo.ElementDataType);
            }
            _il.Emit(OpCodes.Stfld, fieldArrayInfo.Field);
            return;
        }

        throw new NotSupportedException($"Backend IL: campo/proprietà '{target.OriginalToken.Lexeme}' non trovato/a, o proprietà senza SET.");
    }

    /// <summary>
    /// Emette il VALORE dell'istanza indicata da un target di assegnazione (Structure di un
    /// FieldTarget) — una variabile semplice (Ldloc/Ldarg), un elemento di array
    /// (Ldelem/Ldelem_Ref), o ricorsivamente un ALTRO FieldTarget (es. "Sc.Rif" dentro
    /// "Sc.Rif.Posizione = ..." — interoperabilità CLASS/UDT, un campo CLASS dentro un UDT
    /// o viceversa). Usato per l'istanza di un campo/PROPERTY di CLASS (reference type: il
    /// riferimento stesso è il "valore"), mai per un value type che debba poi ricevere
    /// Stfld (vedi EmitInstanceAddress).
    /// </summary>
    private void EmitInstanceValue(Target? structure)
    {
        switch (structure)
        {
            case null when _withTargets.Count > 0: _il.Emit(OpCodes.Ldloc, _withTargets.Peek().Local); return;
            case VariableTarget vt: EmitLoadVariable(vt.SymbolId); return;
            case ArrayElementTarget aet: EmitArrayAccess(new ArrayAccessExpr(aet.SymbolId, aet.NameSuffixed, aet.NameCleaned, aet.Indices)); return;
            case FieldTarget ft: EmitNestedFieldTargetValue(ft); return;
            case IndexedTarget it when it.Base is VariableExpr itVar:
                // Users[id] come istanza (Users[id].nome = ...): il valore dell'elemento
                // del DICT/collezione — stesso percorso di lettura di EmitIndexedAccess.
                EmitIndexedAccess(new IndexedAccessExpr(itVar, it.Indices));
                return;
            default: throw new NotSupportedException($"Backend IL: valore dell'istanza non supportato per un target di tipo '{structure?.GetType().Name ?? "null"}'.");
        }
    }

    /// <summary>Spinge l'INDIRIZZO (managed pointer) di un UDT GLOBALE boxed nel bag.
    /// Il bag è condiviso tra metodi; se il box è assente (globale mai assegnato) il runtime
    /// ne crea uno di default e lo salva. Sequenza SENZA label (helper
    /// <see cref="IlRuntime.GetOrCreateGlobalBox"/>) — una versione con Brtrue/Brfalse
    /// definiva label che il GPM v2 stepper non riusciva a marcare.</summary>
    private void EmitGlobalUdtAddress(int symbolId, UserDefinedType udt)
    {
        var clr = DataTypeMapper.Map(udt, _types, _classTypes);
        _il.Emit(OpCodes.Ldc_I4, symbolId);
        EmitDefaultValue(udt);
        _il.Emit(OpCodes.Box, clr);
        _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.GetOrCreateGlobalBox), typeof(int), typeof(object)));
        _il.Emit(OpCodes.Unbox, clr);
    }

    /// <summary>
    /// Emette l'INDIRIZZO dell'istanza indicata da un target di assegnazione — richiesto
    /// per Stfld su un campo di un VALUE TYPE (UDT): scrivere dopo aver caricato solo il
    /// VALORE (Ldloc/Ldelem) muterebbe una copia scartata sullo stack, senza alcun effetto
    /// sull'originale (locale, elemento dell'array, o campo esterno — vedi
    /// EmitFieldAssignment/EmitAddressOf, stessa distinzione già usata per BYREF). Per un
    /// FieldTarget ricorsivo, l'indirizzo del campo ESTERNO dipende dal SUO tipo: se è un
    /// reference type (CLASS), Ldflda su un riferimento già ottenuto va bene (l'oggetto è
    /// sull'heap, il suo indirizzo di campo resta valido); se è ANCH'ESSO un value type
    /// (UDT dentro UDT), serve prima l'indirizzo di QUELLO (ricorsione).
    /// </summary>
    private void EmitInstanceAddress(Target? structure)
    {
        switch (structure)
        {
            case null when _withTargets.Count > 0: _il.Emit(OpCodes.Ldloca, _withTargets.Peek().Local); return;
            // SELF in un costruttore/metodo di UDT: arg 0 È GIÀ un managed pointer (T&)
            // per convenzione CLR sui metodi d'istanza di un value type — Ldarg diretto,
            // MAI Ldloca su un locale (GetOrDeclareLocal creerebbe un locale nuovo e
            // sbagliato per SELF, che non è un locale).
            case VariableTarget vt when TryResolveSelfArgIndex(vt.SymbolId, out var selfArgIndex):
                _il.Emit(OpCodes.Ldarg, selfArgIndex); return;
            case VariableTarget vt when _parameters.TryGetValue(vt.SymbolId, out var vtParam):
                // Parametro BYREF: l'arg CIL È già un managed pointer (T&) → Ldarg diretto.
                // Parametro BYVAL: Ldarga (indirizzo della copia). Mai GetOrDeclareLocal
                // (creerebbe un locale scollegato per un parametro — UDT BYREF P AS Persona,
                // campo access P.Nome = ... lanciava 'uso di un parametro in una posizione
                // che richiede un vero locale'). Golden TypesUdtGlobal.
                _il.Emit(vtParam.IsByRef ? OpCodes.Ldarg : OpCodes.Ldarga, vtParam.ArgIndex);
                return;
            // `c AS Contatto?` (NullableType su UDT): il local è OBJECT boxed (DataTypeMapper:
            // NullableType → object, vedi EmitStoredValue) — l'indirizzo del CAMPO va preso
            // DENTRO il box esistente (Unbox restituisce un puntatore ai dati del box), MAI
            // l'indirizzo dello SLOT locale stesso. Ldloca su un local object dava object&
            // (indirizzo di tutt'altro tipo): Stfld ci scriveva sopra corrompendo il
            // riferimento — il local finiva col CONTENERE il valore assegnato invece del box
            // originale, e la lettura successiva (Unbox_Any) lanciava InvalidCastException sul
            // tipo sbagliato ora contenuto nel box — bug AiWiki
            // NullConditionalAccess-Nullable-UDT-Divergenza.
            case VariableTarget vt when NormalizeInferredType(_program.Symbols.GetSymbol(vt.SymbolId).DataType) is NullableType nvt:
                // GLOBAL/STATIC: il valore vero vive nel bag (StoreGlobal/StoreStatic — vedi
                // EmitStoreVariable, che per un simbolo NullableType non-UDT non scrive MAI nel
                // local qui letto altrimenti) — TryEmitLoadGlobal/TryEmitLoadStatic per un CLR
                // "object" (NullableType, sempre) non fanno Unbox_Any/Castclass e lasciano il
                // riferimento grezzo, esattamente ciò che serve qui sotto. Nessuno dei due per
                // un simbolo locale/parametro (già intercettati dai case sopra): fallback al
                // local CIL.
                if (!TryEmitLoadGlobal(vt.SymbolId) && !TryEmitLoadStatic(vt.SymbolId))
                    _il.Emit(OpCodes.Ldloc, GetOrDeclareLocal(vt.SymbolId));
                _il.Emit(OpCodes.Unbox, DataTypeMapper.Map(NormalizeInferredType(UnwrapNullableType(nvt)), _types, _classTypes));
                return;
            // UDT GLOBALE (struct, boxed nel bag): l'indirizzo del campo va preso DENTRO il
            // box esistente — Unbox restituisce un managed pointer ai dati del box, così lo
            // Stfld successivo modifica il box IN PLACE e ogni lettura (LoadGlobal+Unbox_Any)
            // vede l'aggiornamento. Se il box non esiste ancora (globale mai assegnato) se ne
            // materializza uno di default e lo si deposita nel bag, evitando l'NRE.
            case VariableTarget vt when IsGlobalSymbol(vt.SymbolId)
                && NormalizeInferredType(_program.Symbols.GetSymbol(vt.SymbolId).DataType) is UserDefinedType vtUdt:
                EmitGlobalUdtAddress(vt.SymbolId, vtUdt);
                return;
            case VariableTarget vt: _il.Emit(OpCodes.Ldloca, GetOrDeclareLocal(vt.SymbolId)); return;
            case ArrayElementTarget aet: EmitArrayElementAddress(new ArrayAccessExpr(aet.SymbolId, aet.NameSuffixed, aet.NameCleaned, aet.Indices)); return;
            case FieldTarget ft: EmitNestedFieldTargetAddress(ft); return;
            case IndexedTarget itx: EmitIndexedTargetAddress(itx); return;
            default: throw new NotSupportedException($"Backend IL: indirizzo dell'istanza non supportato per un target di tipo '{structure?.GetType().Name ?? "null"}'.");
        }
    }

    /// <summary>Emette l'INDIRIZZO (managed pointer) dell'elemento di un array per un
    /// <see cref="IndexedTarget"/> (es. `arr(i).campo = …` con `arr` array di UDT): la base è
    /// un campo-array di CLASSE (SELF.arr) o una variabile locale/globale. Serve Ldelema
    /// (indirizzo dell'elemento value-type) per lo Stfld del campo `campo`.</summary>
    private void EmitIndexedTargetAddress(IndexedTarget it)
    {
        // Caso 1: base = campo-array di CLASSE (SELF.<campo> / <campo>) — sta in
        // _classFieldArrays (i campi NON-array stanno in _classFields).
        if (it.Base is FieldAccessExpr fae && fae.FieldSymbolId >= 0)
        {
            FieldBuilder? fldB = null;
            if (_classFieldArrays.TryGetValue(fae.FieldSymbolId, out var fai)) fldB = fai.Field;
            else if (_classFields.TryGetValue(fae.FieldSymbolId, out var fb)) fldB = fb;
            if (fldB is not null)
            {
                _il.Emit(OpCodes.Ldarg_0);
                _il.Emit(OpCodes.Ldfld, fldB);
                var faType = fldB.FieldType;
                var fRank = it.Indices.Length;
                // JAGGED (T[][], ...): ldelem.ref sugli assi intermedi, ldelema sull'ultimo.
                var fJagged = fRank > 1 && faType.GetElementType() is { IsArray: true };
                for (var axis = 0; axis < fRank; axis++)
                {
                    EmitInt32Arg(it.Indices[axis]);
                    if (fJagged && axis < fRank - 1) _il.Emit(OpCodes.Ldelem_Ref);
                }
                if (fJagged)
                {
                    var eClr = faType;
                    for (var i = 0; i < fRank; i++) eClr = eClr.GetElementType()!;
                    _il.Emit(OpCodes.Ldelema, eClr);
                    return;
                }
                var elem = faType.GetElementType();
                if (elem is not null) { _il.Emit(OpCodes.Ldelema, elem); return; }
            }
        }
        // Caso 2: base = variabile (array locale/globale/param)
        if (it.Base is VariableExpr ve)
        {
            EmitLoadVariable(ve.SymbolId);
            foreach (var ix in it.Indices) EmitInt32Arg(ix);
            if (NormalizeInferredType(_program.Symbols.GetSymbol(ve.SymbolId).DataType) is ArrayType at)
            {
                _il.Emit(OpCodes.Ldelema, DataTypeMapper.Map(at.ElementType, _types, _classTypes));
                return;
            }
        }
        throw new NotSupportedException(
            $"Backend IL: indirizzo dell'elemento non supportato per base '{it.Base?.GetType().Name ?? "null"}'.");
    }

    /// <summary>
    /// `lista(i).campo = v` dove `lista` è una LIST OF UDT (value type): non esiste un
    /// indirizzo stabile dell'elemento (List&lt;T&gt;[i] ritorna una COPIA), quindi Ldelema
    /// degli array non è applicabile. Round-trip: eleva la lista a IList, legge l'elemento
    /// (boxed), lo unboxa (managed pointer), scrive il campo, e riscrive con set_Item.
    /// </summary>
    private bool TryEmitUdtListElementFieldAssignment(
        FieldTarget target, IndexedTarget it, Expr value, FieldBuilder fieldBuilder, int fieldSymbolId)
    {
        if (it.Indices.Length != 1) return false;
        if (NormalizeInferredType(InferType(it.Base)) is not ListType lt) return false;
        var elemDt = NormalizeInferredType(lt.ElementType);
        if (elemDt is UnresolvedQualifiedType uqt)
        {
            elemDt = _program.GetAllUserDefinedTypes().FirstOrDefault(u =>
                u.OriginalName.Lexeme.Equals(uqt.QualifiedName.FullName, StringComparison.OrdinalIgnoreCase)) ?? elemDt;
        }
        if (elemDt is not UserDefinedType udt) return false;

        var elemClr = DataTypeMapper.Map(udt, _types, _classTypes);
        var itemProp = typeof(System.Collections.IList).GetProperty("Item")!;
        var ilistGet = itemProp.GetGetMethod()!;
        var ilistSet = itemProp.GetSetMethod()!;

        var locList = _il.DeclareLocal(typeof(System.Collections.IList));
        var locElem = _il.DeclareLocal(typeof(object));
        var locIdx = _il.DeclareLocal(_types.Int32);

        EmitExpression(it.Base);
        _il.Emit(OpCodes.Castclass, typeof(System.Collections.IList));
        _il.Emit(OpCodes.Stloc, locList);

        EmitInt32Argument(it.Indices[0]);
        _il.Emit(OpCodes.Stloc, locIdx);

        _il.Emit(OpCodes.Ldloc, locList);
        _il.Emit(OpCodes.Ldloc, locIdx);
        _il.Emit(OpCodes.Callvirt, ilistGet);
        _il.Emit(OpCodes.Stloc, locElem);

        // Scrive il campo nella COPIA boxed (Unbox → managed pointer alla struct interna).
        _il.Emit(OpCodes.Ldloc, locElem);
        _il.Emit(OpCodes.Unbox, elemClr);
        var fieldDataType = NormalizeInferredType(_program.Symbols.GetSymbol(fieldSymbolId).DataType);
        var valueDynType = NormalizeInferredType(InferType(value));
        if (fieldDataType is StringType fs && fs.FixedLength is > 0 and var fl)
        {
            EmitExpression(value);
            _il.Emit(OpCodes.Ldc_I4, fl);
            _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.AdjustFixedString)));
        }
        else if (DataTypeMapper.Map(valueDynType, _types, _classTypes) == _types.Object)
        {
            EmitExpression(value);
            if (fieldDataType is DateType)
                _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.CoerceDynamicDate), typeof(object)));
            else
            {
                _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.CoerceDynamicNumber), typeof(object)));
                if (fieldBuilder.FieldType == typeof(bool)) _il.Emit(OpCodes.Conv_I4);
                else EmitCoerceTypes(new DoubleType(), fieldDataType);
            }
        }
        else
        {
            EmitExpression(value);
            if (fieldBuilder.FieldType == typeof(bool)) _il.Emit(OpCodes.Conv_I4);
            else EmitCoerceTypes(valueDynType, fieldDataType);
        }
        _il.Emit(OpCodes.Stfld, fieldBuilder);

        // Riscrive l'elemento nella lista.
        _il.Emit(OpCodes.Ldloc, locList);
        _il.Emit(OpCodes.Ldloc, locIdx);
        _il.Emit(OpCodes.Ldloc, locElem);
        _il.Emit(OpCodes.Callvirt, ilistSet);
        return true;
    }

    private int ResolveFieldTargetSymbolId(FieldTarget target) =>
        target.FieldSymbolId >= 0
            ? target.FieldSymbolId
            : target.Structure is null && _withTargets.Count > 0
                ? ResolveFieldOrPropertySymbolIdByName(_withTargets.Peek().Type, target.OriginalToken.Lexeme)
            : target.Structure is VariableTarget vt
                // Un'istanza UDT (incluso SELF in un costruttore/metodo UDT) risolve per
                // DataType (con il fallback anti-snapshot-stale di
                // ResolveFieldOrPropertySymbolIdByName); ogni altro caso (istanza di
                // classe, SELF di classe, o il NOME di una classe per un membro STATIC)
                // passa dal risolutore di classe esistente.
                ? _program.Symbols.GetSymbol(vt.SymbolId).DataType is UserDefinedType udtType
                    ? ResolveFieldOrPropertySymbolIdByName(udtType, target.OriginalToken.Lexeme)
                    : UnwrapNullableType(_program.Symbols.GetSymbol(vt.SymbolId).DataType) is UserDefinedType udtNullable
                        ? ResolveFieldOrPropertySymbolIdByName(udtNullable, target.OriginalToken.Lexeme)
                    : _program.Symbols.GetSymbol(vt.SymbolId).DataType is InterfaceType ifaceVarType
                        ? ResolveFieldOrPropertySymbolIdByName(ifaceVarType, target.OriginalToken.Lexeme)
                    : ResolveMemberSymbolIdByName(ResolveClassDefinitionFromSymbol(vt.SymbolId), target.OriginalToken.Lexeme)
                // Struttura ANNIDATA (`a.b.c`): il campo appartiene al tipo (UDT/CLASS) della
                // struttura intermedia — risolvi per nome su quel tipo.
                : target.Structure is not null && TryGetFieldType(target.Structure, out var structDt)
                    && NormalizeInferredType(structDt) is UserDefinedType structUdt
                    ? ResolveFieldOrPropertySymbolIdByName(structUdt, target.OriginalToken.Lexeme)
                : throw new NotSupportedException(
                    $"Backend IL: impossibile risolvere il campo '{target.OriginalToken.Lexeme}' per un'istanza di tipo '{target.Structure?.GetType().Name ?? "null"}'.");

    /// <summary>Legge il VALORE di un FieldTarget annidato (mai per l'assegnazione al campo stesso, solo come istanza esterna di un ALTRO campo — vedi EmitInstanceValue).</summary>
    private void EmitNestedFieldTargetValue(FieldTarget target)
    {
        // Campo DINAMICO annidato (es. opts.Headers.ContentType — la struttura esterna è
        // un oggetto HTTPOPTIONS/JSON): lettura via JsonFieldGet (ResolveFieldTargetSymbolId
        // lancerebbe 'impossibile risolvere la classe' per un tipo object/dict).
        if (target.Structure is not null && TryGetStructureDataType(target.Structure, out var nestedOuterType)
            && nestedOuterType is Basic.JsonType or AnyType or HttpOptionsType or HttpHeadersType
                or HttpConfigType or HttpPostedDataType or FetchResultType or PortfolioResultType
                or RebalanceStrategyType or BacktestResultType)
        {
            EmitInstanceValue(target.Structure);
            _il.Emit(OpCodes.Ldstr, target.OriginalToken.Lexeme);
            _il.Emit(OpCodes.Call, IlJsonFieldGetMethod);
            return;
        }
        var fieldSymbolId = ResolveFieldTargetSymbolId(target);
        if (_classFields.TryGetValue(fieldSymbolId, out var fieldBuilder))
        {
            if (fieldBuilder.IsStatic) { _il.Emit(OpCodes.Ldsfld, fieldBuilder); return; }
            EmitInstanceValue(target.Structure);
            _il.Emit(OpCodes.Ldfld, fieldBuilder);
            // BYREF catturato da una closure (piano Closure-ByRef-Box-Linker-Definitivo,
            // Fase 4): il campo è un ByRefBox<T> — un ulteriore Ldfld su Value dereferenzia
            // il box, specchio di EmitFieldAccess/EmitFieldAssignment. Senza questo, un
            // accesso annidato `a.b.c` con `b` box-catturato userebbe il box stesso come
            // istanza per leggere `.c` invece del valore puntato.
            if (IsByRefBoxFieldType(fieldBuilder.FieldType))
                _il.Emit(OpCodes.Ldfld, fieldBuilder.FieldType.GetField("Value")!);
            return;
        }
        if (_classProperties.TryGetValue(fieldSymbolId, out var propInfo) && propInfo.Getter is not null)
        {
            EmitInstanceValue(target.Structure);
            EmitPropertyAccessorCall(propInfo.Getter);
            return;
        }
        throw new NotSupportedException($"Backend IL: campo/proprietà '{target.OriginalToken.Lexeme}' non trovato/a, o proprietà senza GET.");
    }

    /// <summary>Come EmitNestedFieldTargetValue ma per l'INDIRIZZO (Ldflda) — un campo VALUE TYPE annidato dentro un altro campo, mai una PROPERTY (nessun indirizzo di ritorno di un accessor).</summary>
    private void EmitNestedFieldTargetAddress(FieldTarget target)
    {
        var fieldSymbolId = ResolveFieldTargetSymbolId(target);
        if (!_classFields.TryGetValue(fieldSymbolId, out var fieldBuilder))
            throw new NotSupportedException($"Backend IL: indirizzo di '{target.OriginalToken.Lexeme}' non supportato (solo campi, mai PROPERTY).");
        if (fieldBuilder.IsStatic) { _il.Emit(OpCodes.Ldsflda, fieldBuilder); return; }
        // Il campo ESTERNO ("Structure") va caricato per VALORE se il SUO tipo è un
        // reference type (Ldflda su un riferimento resta valido, l'oggetto è sull'heap) o
        // per INDIRIZZO se è un value type (UDT) — qualunque sia la sua rappresentazione:
        // una VARIABILE semplice (VariableTarget), un elemento di array, o un ALTRO campo
        // annidato (ricorsione). Bug reale (2026-09-29): il controllo precedente guardava
        // SOLO il caso `target.Structure is FieldTarget`, trascurando il caso più comune —
        // una variabile semplice di tipo UDT (es. "o" in "o.I.X = ..." — target.Structure
        // qui è un VariableTarget, non un FieldTarget) — per cui si cadeva nel ramo
        // EmitInstanceValue(o), che per un value type fa Ldloc (una COPIA per valore)
        // invece di Ldloca (l'indirizzo dell'originale): il successivo Ldflda su un VALORE
        // anziché un puntatore produce IL non verificabile
        // (System.InvalidProgramException A RUNTIME, mai un errore di compilazione C#, dato
        // che Reflection.Emit non verifica l'IL emesso finché non viene eseguito) — vedi
        // AiWiki Bugs/(solved) - UDT-Campo-Annidato-Scrittura-Encadenata-Non-Persiste.md.
        // TryGetStructureDataType già generalizza su TUTTI i tipi di Target (Variable/
        // ArrayElement/Field/Indexed) — riusato qui invece del controllo ristretto.
        // Il tipo da valutare è quello del CAMPO/struttura ESTERNA (`target.Structure`), NON
        // la radice della catena: `TryGetFieldType` risolve il tipo del campo (es. `SELF.stazione`
        // → struct Stazione, value type) — `TryGetStructureDataType` risaliva alla RADICE
        // (SELF → ClassType) e per una catena annidata dentro una CLASSE valutava per VALORE
        // un value type → Ldflda su un valore → IL non verificabile (InvalidProgramException
        // a runtime, "aggiustato" solo per caso da ILOptim).
        var outerIsValueType = TryGetFieldType(target.Structure, out var outerDataType)
            && NormalizeInferredType(outerDataType) is UserDefinedType;
        if (outerIsValueType) EmitInstanceAddress(target.Structure);
        else EmitInstanceValue(target.Structure);
        _il.Emit(OpCodes.Ldflda, fieldBuilder);
    }

    /// <summary>
    /// Risolve la ClassDefinition a cui appartiene un membro, a partire dal SymbolId
    /// della "struttura" davanti al punto (es. SELF/ME, un nome di variabile, o il nome
    /// STESSO di una classe per l'accesso a un membro STATIC — "Forma.NextId" risolve
    /// "Forma" a un simbolo il cui DeclarationNode È la ClassDefinition, non una
    /// ClassType che la avvolge). Prova entrambe le rappresentazioni possibili.
    /// </summary>
    private ClassDefinition ResolveClassDefinitionFromSymbol(int symbolId)
    {
        var symbol = _program.Symbols.GetSymbol(symbolId);
        if (symbol.DeclarationNode is ClassDefinition fromDeclaration) return fromDeclaration;
        if (symbol.DataType is ClassDefinition fromDataTypeDirect) return fromDataTypeDirect;
        if (symbol.DataType is ClassType classType) return classType.Definition;
        throw new NotSupportedException($"Backend IL: impossibile risolvere la classe per il simbolo '{symbol.OriginalName}'.");
    }

    /// <summary>Cerca un campo o una PROPERTY per nome BASIC (case-insensitive) nella gerarchia di classi, risalendo da <paramref name="classDef"/>.</summary>
    private static int ResolveMemberSymbolIdByName(ClassDefinition classDef, string memberName)
    {
        for (var current = classDef; current is not null; current = current.ResolvedBaseClass)
        {
            foreach (var member in current.Members)
            {
                switch (member)
                {
                    case FieldMember fm when fm.Declaration.NameCleaned.Lexeme.Equals(memberName, StringComparison.OrdinalIgnoreCase):
                        return fm.Declaration.SymbolId;
                    case PropertyMember pm when pm.Property.OriginalName.Lexeme.Equals(memberName, StringComparison.OrdinalIgnoreCase):
                        return pm.Property.SymbolId;
                }
            }
        }
        throw new NotSupportedException($"Backend IL: campo/proprietà '{memberName}' non trovato/a nella classe '{classDef.OriginalName.Lexeme}' o nelle sue basi.");
    }

    /// <summary>
    /// Generalizza ResolveMemberSymbolIdByName a QUALUNQUE tipo "struttura" (CLASS o UDT),
    /// risolvendo il tipo dell'espressione via InferType invece di richiedere una
    /// VariableExpr semplice — necessario per un accesso a campo ANNIDATO (es.
    /// "Sc.Rif.Posizione.X": la struttura "Sc.Rif.Posizione" è essa stessa una
    /// FieldAccessExpr, non una variabile) o per un campo di interoperabilità CLASS/UDT.
    /// </summary>
    private int ResolveFieldOrPropertySymbolIdByName(DataType structureType, string fieldName)
    {
        if (structureType is ClassType classType)
            return ResolveMemberSymbolIdByName(classType.Definition, fieldName);
        if (structureType is InterfaceType ifaceType)
        {
            // Variabile dichiarata AS UnaInterfaccia (es. `DIM formaDisegnabile AS IDisegnabile`):
            // i membri sono METHOD/PROPERTY astratti — risolvi l'accessor/property per nome
            // (il dispatch a runtime è callvirt sull'interfaccia, vedi EmitFieldAccess/Assignment).
            foreach (var member in ifaceType.Definition.Members)
            {
                switch (member)
                {
                    case PropertyMember ipm when ipm.Property.OriginalName.Lexeme.Equals(fieldName, StringComparison.OrdinalIgnoreCase):
                        return ipm.Property.SymbolId;
                    case FieldMember ifm when ifm.Declaration.NameCleaned.Lexeme.Equals(fieldName, StringComparison.OrdinalIgnoreCase):
                        return ifm.Declaration.SymbolId;
                }
            }
            throw new NotSupportedException($"Backend IL: proprietà/campo '{fieldName}' non trovato nell'interfaccia '{ifaceType.Definition.OriginalName.Lexeme}'.");
        }
        // Il tipo del base può essere rimasto un UnresolvedQualifiedType (nome non ancora
        // legato al UserDefinedType — es. `stazione.pos.lat` dove `pos` ha tipo "Coord"):
        // risolvi per nome contro gli UDT del programma (come ResolveOperatorReturnType).
        if (structureType is UnresolvedQualifiedType uqtBase)
        {
            var uNm = uqtBase.QualifiedName.FullName;
            var resolvedUdt = _program.GetAllUserDefinedTypes()
                .FirstOrDefault(u => u.OriginalName.Lexeme.Equals(uNm, StringComparison.OrdinalIgnoreCase));
            if (resolvedUdt is not null) structureType = resolvedUdt;
        }
        if (structureType is UserDefinedType udt)
        {
            var field = udt.Fields.FirstOrDefault(f => f.Name.Lexeme.Equals(fieldName, StringComparison.OrdinalIgnoreCase));
            if (field is { SymbolId: >= 0 }) return field.SymbolId;
            // PROPERTY di UDT (es. SELF.Magnitude): i campi non la contengono — risolvi
            // per nome tra le Properties dell'UDT.
            var udtPropField = udt.Properties.FirstOrDefault(p => p.OriginalName.Lexeme.Equals(fieldName, StringComparison.OrdinalIgnoreCase));
            if (udtPropField is { SymbolId: >= 0 }) return udtPropField.SymbolId;
            // Snapshot STALE del tipo (stessa famiglia del bug già documentato per SELF
            // di CLASS): il simbolo sintetico SELF di un costruttore/metodo UDT porta come
            // DataType la copia PRE-binding dell'UDT, i cui campi hanno ancora SymbolId=-1
            // — mai la copia autorevole aggiornata dal Binder. Ri-risolvi per NOME contro
            // la dichiarazione autorevole del modulo (la stessa lista da cui Pass -0.8/0.2
            // hanno registrato i FieldBuilder in _classFields).
            var authoritativeUdt = _program.MainModule!.Declarations.OfType<UserDefinedType>()
                .FirstOrDefault(u => u.OriginalName.Lexeme.Equals(udt.OriginalName.Lexeme, StringComparison.OrdinalIgnoreCase));
            var authoritativeField = authoritativeUdt?.Fields.FirstOrDefault(f => f.Name.Lexeme.Equals(fieldName, StringComparison.OrdinalIgnoreCase));
            if (authoritativeField is { SymbolId: >= 0 }) return authoritativeField.SymbolId;
            var authoritativeProp = authoritativeUdt?.Properties.FirstOrDefault(p => p.OriginalName.Lexeme.Equals(fieldName, StringComparison.OrdinalIgnoreCase));
            if (authoritativeProp is { SymbolId: >= 0 }) return authoritativeProp.SymbolId;
            throw new NotSupportedException($"Backend IL: campo/proprietà '{fieldName}' non trovato nell'UDT '{udt.OriginalName.Lexeme}'.");
        }
        throw new NotSupportedException($"Backend IL: impossibile risolvere il campo/proprietà '{fieldName}' per il tipo '{structureType.GetType().Name}'.");
    }

    /// <summary>Lettura di una PROPERTY (es. SELF.Nome): valuta l'istanza, poi chiama il getter.</summary>
    private void EmitPropertyAccess(PropertyAccessExpr expr)
    {
        if (TryEmitNativeCollectionProperty(expr)) return;

        // t.legs(i).campo — CAMPO di un elemento di campo lista UDT: `.campo` è un FIELD
        // (non property) → risolto qui invece di lanciare NotSupportedIlExpressionException.
        if (expr.Instance is IndexedAccessExpr iaUdtP && InferType(iaUdtP) is UserDefinedType udtEP)
        {
            var fldP = udtEP.Fields.FirstOrDefault(f => f.Name.Lexeme.Equals(expr.PropertyName.Lexeme, StringComparison.OrdinalIgnoreCase));
            if (fldP is not null && _classFields.TryGetValue(fldP.SymbolId, out var fbP))
            {
                EmitExpression(iaUdtP);
                _il.Emit(OpCodes.Ldfld, fbP);
                return;
            }
        }

        // sb.Length (StringBuilder): getter della proprietà CLR Length.
        if (NormalizeInferredType(InferType(expr.Instance)) is StringBuilderType
            && expr.PropertyName.Lexeme.Equals("LENGTH", StringComparison.OrdinalIgnoreCase))
        {
            var lenGetter = typeof(System.Text.StringBuilder).GetProperty(nameof(System.Text.StringBuilder.Length))!.GetGetMethod()!;
            EmitExpression(expr.Instance);
            _il.Emit(OpCodes.Callvirt, lenGetter);
            _il.Emit(OpCodes.Conv_R8);
            return;
        }

        var property = ResolvePropertyDefinition(expr);
        if (!_classProperties.TryGetValue(property.SymbolId, out var propInfo) || propInfo.Getter is null)
            throw new NotSupportedException($"Backend IL: proprietà '{expr.PropertyName.Lexeme}' non trovata, o senza GET.");

        // Getter di una PROPERTY di UDT (value type): l'accessor d'istanza vuole `this` come
        // managed pointer (T&) — si spinge l'INDIRIZZO dell'istanza (Ldloca/Ldarg), non il
        // valore. Per una property di CLASS (reference) basta il riferimento.
        if (NormalizeInferredType(InferType(expr.Instance)) is UserDefinedType && expr.Instance is VariableExpr pVe)
            EmitInstanceAddress(new VariableTarget(pVe.SymbolId, pVe.NameSuffixed, pVe.NameCleaned));
        else
            EmitExpression(expr.Instance);
        // FETCH espresso asincrono (§E.5): la variabile istanza è un future-target (object,
        // ri-tipizzata da CollectFutureTargets); EmitLoadVariable ha fatto l'implicit-await
        // (TryReadFuture) e lascia il FetchResult boxed come object — il getter CLR della
        // proprietà vuole FetchResult, quindi serve il castclass PRIMA del callvirt.
        if (_gpmStepperMode && expr.Instance is VariableExpr iv && _gpmFutureTargets.Contains(iv.SymbolId))
            _il.Emit(OpCodes.Castclass, typeof(CommonLib.Http.FetchResult));
        EmitPropertyAccessorCall(propInfo.Getter);
    }

    /// <summary>
    /// Risolve la PropertyDefinition di un PropertyAccessExpr. Il Linker non popola
    /// sempre ResolvedDefinition per una lettura di proprietà a livello top-level
    /// (scoperto testando questa feature — un gap del Linker, non di questo backend):
    /// se assente, risale il tipo statico dell'istanza (via InferType) e cerca il nome
    /// della proprietà nella gerarchia di classi, stesso pattern di
    /// FindVirtualMethodInHierarchy.
    /// </summary>
    private PropertyDefinition ResolvePropertyDefinition(PropertyAccessExpr expr)
    {
        if (expr.ResolvedDefinition is not null) return expr.ResolvedDefinition;

        if (InferType(expr.Instance) is UserDefinedType udt)
        {
            var udtProp = udt.Properties.FirstOrDefault(p =>
                p.OriginalName.Lexeme.Equals(expr.PropertyName.Lexeme, StringComparison.OrdinalIgnoreCase));
            if (udtProp is not null) return udtProp;
            throw new NotSupportedIlExpressionException(expr);
        }

        if (InferType(expr.Instance) is not ClassType classType)
            throw new NotSupportedIlExpressionException(expr);

        for (var current = classType.Definition; current is not null; current = current.ResolvedBaseClass)
            foreach (var member in current.Members)
                if (member is PropertyMember propMember && propMember.Property.OriginalName.Lexeme.Equals(expr.PropertyName.Lexeme, StringComparison.OrdinalIgnoreCase))
                    return propMember.Property;

        throw new NotSupportedIlExpressionException(expr);
    }

    /// <summary>
    /// Chiamata a un metodo d'istanza (es. SELF.Metodo(args)): valuta l'istanza (diventa
    /// l'argomento 0 implicito), poi gli argomenti, poi la chiamata vera e propria.
    /// VIRTUAL/OVERRIDE usano `callvirt` — dispatch dinamico basato sul tipo REALE
    /// dell'istanza a runtime, il vtable nativo del CLR (mai un vtable fatto a mano, vedi
    /// TODO Obsidian, "Domande aperte" #5). Un metodo non-virtual usa `call` diretta,
    /// nessuna indirezione — resta il default per la stragrande maggioranza dei metodi
    /// (nessuna ereditarietà = nessun bisogno di dispatch dinamico).
    /// </summary>
    private void EmitMethodCall(MethodCallExpr expr)
    {
        // BASE.metodo(): il linker ha GIÀ risolto ResolvedDefinition al metodo della base
        // (Linker.Rewrites RewriteMethodCall, case BaseExpr) — il dispatch dinamico sotto
        // (InferType(BaseExpr) è AnyType) lo farebbe finire in CollectionInvoke, chiamando
        // l'OVERRIDE invece del metodo della base (ricorsione/infinito).
        if (expr.Instance is BaseExpr)
        {
            if (expr.ResolvedDefinition is not FunctionDefinition baseMethodDef)
                throw new NotSupportedException($"Backend IL: metodo BASE '{expr.MethodName.Lexeme}' non risolto.");
            EmitMethodCallCore(expr.Instance, baseMethodDef, expr.Args, forceNonVirtual: true);
            return;
        }

        if (TryEmitNativeCollectionMethodCall(expr)) return;

        if (NormalizeInferredType(InferType(expr.Instance)) is StringBuilderType)
        {
            EmitStringBuilderMethodCall(expr);
            return;
        }
        // REGEX nativo (r.IsMatch/Match/Matches/Replace/Split): metodi CLR della
        // RegexWrapper (System.Text.RegularExpressions — reale, a differenza della VM no-op).
        if (NormalizeInferredType(InferType(expr.Instance)) is RegexType)
        {
            EmitRegexMethodCall(expr);
            return;
        }
        // FetchResult.SaveToFile(path): salva il BodyBytes su file (come il VM
        // PCodeFetchResult.SaveToFile — il CommonLib FetchResult è proprietà-only).
        if (NormalizeInferredType(InferType(expr.Instance)) is FetchResultType
            && expr.MethodName.Lexeme.Equals("SaveToFile", StringComparison.OrdinalIgnoreCase))
        {
            if (expr.Args.Length != 1)
                throw new NotSupportedException("Backend IL: FetchResult.SaveToFile richiede 1 argomento (path).");
            EmitExpression(expr.Instance);
            EmitStringArg(expr.Args[0]);
            _il.Emit(OpCodes.Call, IlRuntimeFetchResultSaveToFileMethod);
            return;
        }

        if (expr.ResolvedDefinition is not FunctionDefinition methodDef)
            throw new NotSupportedException($"Backend IL: metodo '{expr.MethodName.Lexeme}' non trovato o non ancora supportato.");
        EmitMethodCallCore(expr.Instance, methodDef, expr.Args);
    }

    private DataType InferRegexMatchFieldType(string fieldName)
    {
        var prop = typeof(Basic.RegexNative.RegexMatchValue).GetProperties()
            .FirstOrDefault(p => p.Name.Equals(fieldName, StringComparison.OrdinalIgnoreCase));
        if (prop is null) throw new NotSupportedException($"Backend IL: campo REGEXMATCH '{fieldName}' non trovato.");
        return prop.PropertyType == typeof(string) ? new StringType() : new DoubleType();
    }

    private DataType InferRegexMethodReturnType(MethodCallExpr expr)
    {
        var methodName = expr.MethodName.Lexeme;
        var method = typeof(Basic.RegexNative.RegexWrapper).GetMethods().FirstOrDefault(m =>
                m.Name.Equals(methodName, StringComparison.OrdinalIgnoreCase)
                && m.GetParameters().Length == expr.Args.Length);
        if (method is null) return new AnyType();
        if (method.ReturnType == typeof(string)) return new StringType();
        if (method.ReturnType == typeof(string[])) return new ArrayType(new StringType());
        if (method.ReturnType == typeof(double)) return new DoubleType();
        if (method.ReturnType == typeof(List<Basic.RegexNative.RegexMatchValue>)) return new ListType(new RegexMatchType());
        return new RegexMatchType();
    }

    private void EmitRegexMethodCall(MethodCallExpr expr)
    {
        var wrapperType = typeof(Basic.RegexNative.RegexWrapper);
        var methodName = expr.MethodName.Lexeme;
        var method = wrapperType.GetMethods().FirstOrDefault(m =>
                m.Name.Equals(methodName, StringComparison.OrdinalIgnoreCase)
                && m.GetParameters().Length == expr.Args.Length)
            ?? throw new NotSupportedException($"Backend IL: metodo REGEX '{methodName}' non trovato.");
        EmitExpression(expr.Instance);
        var parameters = method.GetParameters();
        for (var i = 0; i < parameters.Length; i++)
        {
            var argType = NormalizeInferredType(InferType(expr.Args[i]));
            var argClr = DataTypeMapper.Map(argType, _types, _classTypes);
            if (argClr == typeof(string)) EmitStringArg(expr.Args[i]);
            else
            {
                EmitExpression(expr.Args[i]);
                IlConversion.EmitConvert(_il, _types, argType, argClr, parameters[i].ParameterType);
            }
        }
        _il.Emit(OpCodes.Callvirt, method);
    }

    private bool TryEmitNativeCollectionProperty(PropertyAccessExpr expr)
    {
        if (!TryGetNativeCollectionType(InferType(expr.Instance), out var collectionType)) return false;
        if (!expr.PropertyName.Lexeme.Equals("COUNT", StringComparison.OrdinalIgnoreCase))
            throw new NotSupportedException($"Backend IL: proprietà collection '{expr.PropertyName.Lexeme}' non ancora supportata.");

        MethodInfo? getter = null;
        bool closedOverBuilder = true;
        try { closedOverBuilder = ClosedOverTypeBuilder(collectionType); } catch { closedOverBuilder = true; }
        if (!closedOverBuilder)
            try { getter = collectionType.GetProperty(nameof(List<int>.Count))?.GetGetMethod(); } catch { getter = null; }
        if (getter is null)
        {
            // List<Udt> (elementi TypeBuilder) o reflection non disponibile per il Count:
            // dispatch runtime CollectionCount(object).
            EmitExpression(expr.Instance);
            if (collectionType.IsValueType) _il.Emit(OpCodes.Box, collectionType);
            _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.CollectionCount), typeof(object)));
            _il.Emit(OpCodes.Conv_R8);
            return true;
        }
        EmitExpression(expr.Instance);
        _il.Emit(OpCodes.Callvirt, getter);
        _il.Emit(OpCodes.Conv_R8); // convenzione BASIC: i numeri senza suffisso restano DOUBLE
        return true;
    }

        /// <summary>Boxa gli argomenti in un object[] sullo stack (parity AnyType path) —
    /// usato dal kind-dispatch per SET/GET su map-like (Dictionary non espone .Get/.Set).</summary>
    private void EmitKvArgsBoxed(MethodCallExpr expr)
    {
        _il.Emit(OpCodes.Ldc_I4, expr.Args.Length);
        _il.Emit(OpCodes.Newarr, _types.Object);
        for (var i = 0; i < expr.Args.Length; i++)
        {
            _il.Emit(OpCodes.Dup);
            _il.Emit(OpCodes.Ldc_I4, i);
            EmitExpression(expr.Args[i]);
            var aClr = DataTypeMapper.Map(NormalizeInferredType(InferType(expr.Args[i])), _types, _classTypes);
            if (aClr.IsValueType) _il.Emit(OpCodes.Box, aClr);
            _il.Emit(OpCodes.Stelem_Ref);
        }
    }

    private bool TryEmitNativeCollectionMethodCall(MethodCallExpr expr)
    {
        var instanceType = InferType(expr.Instance);
        // AnyType + tipi strutture avanzate (Tree/AVL/Trie/Heap/Bag/Cache/Timeline/Ring/
        // Graph/HashRing/SparseSet/BPlusTree — mappati a object nel DataTypeMapper): il
        // valore reale è una collection boxed — dispatch RUNTIME via CollectionInvoke.
        if (instanceType is AnyType or Basic.JsonType or TreeType or AVLTreeType or RedBlackTreeType
            or BinaryIndexedTreeType or TrieType or BPlusTreeType or GraphType or HashRingType
            or SparseSetType or BinaryHeapType or BagType or CacheType or TimelineType
            or RingBufferType
            // HttpRequest/HttpResponse (server HTTP): metodi come SetCookie/SendJSON
            // dispatchati a runtime sul tipo reale (il VM li gestisce in OP_CALL_MEMBER).
            or HttpRequestType or HttpResponseType or HttpOptionsType or HttpHeadersType
            or HttpConfigType or HttpPostedDataType or WebSocketType or SqlResultSetType
            or FetchResultType or CookieJarType)
        {
            // F1 Incongruenze: dispatch per CHIAVE CANONICA (TokenKind Coll*) quando il
            // membro è in tabella — CollectionInvokeByKind(int, object, object[]) risolve
            // il lexeme legacy internamente; fallback lexeme per membri fuori tabella.
            // NB: push order = (kind, instance, args) — la firma è (int, object, object[]).
            if (CollMembers.IsCollMember(expr.MethodName.Kind))
            {
                _il.Emit(OpCodes.Ldc_I4, (int)expr.MethodName.Kind);
                EmitExpression(expr.Instance);
                _il.Emit(OpCodes.Ldc_I4, expr.Args.Length);
                _il.Emit(OpCodes.Newarr, _types.Object);
                for (var i = 0; i < expr.Args.Length; i++)
                {
                    _il.Emit(OpCodes.Dup);
                    _il.Emit(OpCodes.Ldc_I4, i);
                    EmitExpression(expr.Args[i]);
                    var aClr = DataTypeMapper.Map(NormalizeInferredType(InferType(expr.Args[i])), _types, _classTypes);
                    if (aClr.IsValueType) _il.Emit(OpCodes.Box, aClr);
                    // Discrepanza InferType vs valore reale (es. target out `TryGetValue(k, v$)`
                    // con v$ tipizzata String dal suffisso ma local int32): il valore emesso è un
                    // value type grezzo → box del LocalType reale (altrimenti stelem int32 in un
                    // object[] = IL invalido con ILOptim).
                    else if (expr.Args[i] is VariableExpr aV && _locals.TryGetValue(aV.SymbolId, out var aLb) && aLb.LocalType.IsValueType)
                        _il.Emit(OpCodes.Box, aLb.LocalType);
                    _il.Emit(OpCodes.Stelem_Ref);
                }
                _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.CollectionInvokeByKind), typeof(int), typeof(object), typeof(object[])));
                return true;
            }
            // Push order = (instance, methodName, args) — la firma è (object, string,
            // object[]), ESATTAMENTE come il sibling CollectionInvokeByKind sopra (kind,
            // instance, args). Qui erano invertiti (Ldstr PRIMA di EmitExpression(Instance)):
            // il runtime riceveva l'ISTANZA (es. un WebSocket) nello slot "methodName" e la
            // STRINGA nello slot "instance" — CollectionInvoke chiama methodName.ToUpperInvariant()
            // sul valore ricevuto, quindi trattava l'istanza come se fosse una string (type
            // confusion silenziosa: nessun errore a compile-time, IL "verificabile" perché
            // `object` accetta qualunque reference) — crash nativo intermittente in ICU
            // (GlobalizationNative_ChangeCaseInvariant) o OverflowException a seconda del
            // layout di memoria dell'oggetto scambiato. Trovato con `ws.Send("ping")`
            // (WebSocketType, non in CollMembers → cade in questo fallback).
            EmitExpression(expr.Instance);
            _il.Emit(OpCodes.Ldstr, expr.MethodName.Lexeme);
            _il.Emit(OpCodes.Ldc_I4, expr.Args.Length);
            _il.Emit(OpCodes.Newarr, _types.Object);
            for (var i = 0; i < expr.Args.Length; i++)
            {
                _il.Emit(OpCodes.Dup);
                _il.Emit(OpCodes.Ldc_I4, i);
                EmitExpression(expr.Args[i]);
                var aClr = DataTypeMapper.Map(NormalizeInferredType(InferType(expr.Args[i])), _types, _classTypes);
                if (aClr.IsValueType) _il.Emit(OpCodes.Box, aClr);
                // Discrepanza InferType vs valore reale (es. target out `TryGetValue(k, v$)`
                // con v$ tipizzata String dal suffisso ma local int32): il valore emesso è un
                // value type grezzo → box del LocalType reale (altrimenti stelem int32 in un
                // object[] = IL invalido con ILOptim).
                else if (expr.Args[i] is VariableExpr aV && _locals.TryGetValue(aV.SymbolId, out var aLb) && aLb.LocalType.IsValueType)
                    _il.Emit(OpCodes.Box, aLb.LocalType);
                _il.Emit(OpCodes.Stelem_Ref);
            }
            _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.CollectionInvoke), typeof(object), typeof(string), typeof(object[])));
            return true;
        }
        if (!TryGetNativeCollectionType(instanceType, out var collectionType)) return false;

        // BASIC espone Add in modo uniforme; LinkedList CLR lo chiama AddLast.
        // L'alias conserva la semantica di append del resto delle collection native.
        // .Keys()/.Values() su un DICT: non sono metodi CLR (Dictionary ha property
        // Keys/Values, non metodi) — emette le chiavi/valori come array (Enumerable.ToArray).
        if (expr.Args.Length == 0 && (expr.MethodName.Lexeme.Equals("KEYS", StringComparison.OrdinalIgnoreCase)
            || expr.MethodName.Lexeme.Equals("VALUES", StringComparison.OrdinalIgnoreCase)))
        {
            var dictArgType = NormalizeInferredType(InferType(expr.Instance));
            var isKeys = expr.MethodName.Lexeme.StartsWith("KEY", StringComparison.OrdinalIgnoreCase);
            var elemClr = dictArgType switch
            {
                DictType dd => DataTypeMapper.Map(isKeys ? dd.KeyType : dd.ValueType, _types, _classTypes),
                SortedDictType sd => DataTypeMapper.Map(isKeys ? sd.KeyType : sd.ValueType, _types, _classTypes),
                _ => throw new NotSupportedException($"Backend IL: {expr.MethodName.Lexeme}() richiede un DICT/SORTED.")
            };
            // .Keys()/.Values() con parentesi: stessa semantica del FIELD Keys/Values —
            // legge la property CLR (Keys/Values del Dictionary) PRIMA di convertire ad
            // array; passare il dict stesso a Enumerable.ToArray<T> è un bug (il dict non
            // è IEnumerable<T> dei valori) → InvalidCast/EntryPointNotFound a runtime.
            var prop = collectionType.GetProperty(isKeys ? "Keys" : "Values");
            var propGetter = prop?.GetGetMethod()
                ?? throw new NotSupportedException($"Backend IL: collection '{collectionType}' non espone {(isKeys ? "Keys" : "Values")}.");
            EmitExpression(expr.Instance);
            _il.Emit(OpCodes.Callvirt, propGetter);
            _il.Emit(OpCodes.Call, EnumerableToArrayMethod.MakeGenericMethod(elemClr));
            return true;
        }

        // JOIN su una collection: MyList.Join(", ") → String.Join(sep, list) — non è un
        // metodo CLR nativo della collezione, va gestito prima della risoluzione per nome.
        if (expr.MethodName.Lexeme.Equals("JOIN", StringComparison.OrdinalIgnoreCase))
        {
            if (expr.Args.Length != 1)
                throw new NotSupportedException("Backend IL: JOIN su collection richiede 1 argomento (separatore).");
            EmitExpression(expr.Instance);
            EmitExpression(expr.Args[0]);
            _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.CollectionJoin), typeof(object), typeof(object)));
            return true;
        }

        // HashSet.Union/Intersect/Except/SymmetricExcept: il BASIC li usa come espressione
        // che ritorna un NUOVO set; UnionWith/IntersectWith (CLR) modificano in-place e
        // ritornano void → `DIM x = SetA.Union(SetB)` sarebbe InvalidProgram. Helper runtime.
        var setName = expr.MethodName.Lexeme.ToUpperInvariant();
        if (instanceType is HashSetType && expr.Args.Length == 1
            && setName is "UNION" or "INTERSECT" or "EXCEPT" or "SYMMETRICEXCEPT")
        {
            EmitObjectBoxedArgument(expr.Instance);
            EmitObjectBoxedArgument(expr.Args[0]);
            var setOp = setName switch
            {
                "UNION" => nameof(IlRuntime.SetUnion),
                "INTERSECT" => nameof(IlRuntime.SetIntersect),
                "SYMMETRICEXCEPT" => nameof(IlRuntime.SetSymmetricExcept),
                _ => nameof(IlRuntime.SetExcept)
            };
            _il.Emit(OpCodes.Call, RuntimeMethod(setOp, typeof(object), typeof(object)));
            // SetUnion/Intersect/Except ritornano HashSet<object>/List<object> (boxed), ma
            // il BASIC li usa come espressione tipizzata: `DIM U = A.Union(B)` alloca un
            // local HashSet<T> col tipo ELEMENTO del BASIC. Senza la conversione, stoccare
            // l'object nel local HashSet<T> è IL invalido (EntryPointNotFound a runtime) e
            // castclass a HashSet<T> fallisce su HashSet<object> (InvalidCastException).
            var setElemClr = DataTypeMapper.Map(CollectionElementType(instanceType), _types, _classTypes);
            if (setElemClr != _types.Object)
                _il.Emit(OpCodes.Call, ToTypedSetMethod.MakeGenericMethod(setElemClr));
            return true;
        }

        var upperMember = expr.MethodName.Lexeme.ToUpperInvariant();
        // SET/SETKEYVALUE/GET su map-like: Dictionary non espone .Set/.Get — kind dispatch
        // (push order = (kind, instance, args), firma CollectionInvokeByKind(int, object, object[])).
        if (upperMember is "SET" or "SETKEYVALUE" && expr.Args.Length == 2)
        {
            _il.Emit(OpCodes.Ldc_I4, (int)TokenKind.CollSet);
            EmitExpression(expr.Instance);
            EmitKvArgsBoxed(expr);
            _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.CollectionInvokeByKind), typeof(int), typeof(object), typeof(object[])));
            // NO pop qui: il linker tipizza SET come InferredType — lo scarto lo fa il
            // consumatore (EmitExpressionStatement per lo statement, EmitDynamicValueBox
            // per un uso in espressione). Un Pop qui spezzava l'uso come argomento PRINT.
            return true;
        }
        if (upperMember is "GET" && expr.Args.Length == 1)
        {
            _il.Emit(OpCodes.Ldc_I4, (int)TokenKind.CollGet);
            EmitExpression(expr.Instance);
            EmitKvArgsBoxed(expr);
            _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.CollectionInvokeByKind), typeof(int), typeof(object), typeof(object[])));
            // GET → linker: dt.ValueType (es. Integer) — il runtime restituisce object:
            // unbox al tipo CLR atteso dal consumatore (Conv_R8/Stloc su object = IL invalido).
            var callClr = DataTypeMapper.Map(NormalizeInferredType(InferType(expr)), _types, _classTypes);
            if (callClr.IsValueType) _il.Emit(OpCodes.Unbox_Any, callClr);
            return true;
        }
        var methodName = instanceType is LinkedListType or DoublyLinkedListType
                && expr.MethodName.Lexeme.Equals("ADD", StringComparison.OrdinalIgnoreCase)
            ? nameof(LinkedList<int>.AddLast)
            : instanceType is HashSetType && expr.MethodName.Lexeme.Equals("UNION", StringComparison.OrdinalIgnoreCase)
                ? nameof(HashSet<int>.UnionWith)
            : instanceType is HashSetType && expr.MethodName.Lexeme.Equals("INTERSECT", StringComparison.OrdinalIgnoreCase)
                ? nameof(HashSet<int>.IntersectWith)
            : instanceType is HashSetType && expr.MethodName.Lexeme.Equals("EXCEPT", StringComparison.OrdinalIgnoreCase)
                ? nameof(HashSet<int>.ExceptWith)
            : instanceType is HashSetType && expr.MethodName.Lexeme.Equals("SYMMETRICEXCEPT", StringComparison.OrdinalIgnoreCase)
                ? nameof(HashSet<int>.SymmetricExceptWith)
            : expr.MethodName.Lexeme;
        var method = ResolveNativeCollectionMethod(collectionType, methodName, expr.Args);
        // F1 Incongruenze: membri di collection con TokenKind Coll* NON risolvibili come
        // metodo CLR (Set/Get su dict, Front/Back/IsEmpty/AddUnique/AddAfter/AddBefore su
        // list/queue, MaxPair/MinPair su dict) → dispatch RUNTIME per kind. Prima: il
        // fall-through CLR sollevava NotSupportedException (il membro esisteva solo nella
        // VM/C). L'errore esplicito resta per i membri che né CLR né IlRuntime conoscono.
        if (method is null && CollMembers.IsCollMember(expr.MethodName.Kind))
        {
            _il.Emit(OpCodes.Ldc_I4, (int)expr.MethodName.Kind);
            EmitExpression(expr.Instance);
            EmitKvArgsBoxed(expr);
            _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.CollectionInvokeByKind), typeof(int), typeof(object), typeof(object[])));
            var callClr2 = DataTypeMapper.Map(NormalizeInferredType(InferType(expr)), _types, _classTypes);
            if (callClr2.IsValueType) _il.Emit(OpCodes.Unbox_Any, callClr2);
            return true;
        }
        // IndexOf su collection senza metodo CLR (LinkedList/Doubly/Circular): helper runtime.
        if (method is null && expr.MethodName.Lexeme.Equals("SORT", StringComparison.OrdinalIgnoreCase) && expr.Args.Length == 0)
        {
            EmitExpression(expr.Instance);
            _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.CollectionSort), typeof(object)));
            return true;
        }
        if (method is null && expr.MethodName.Lexeme.Equals("REVERSE", StringComparison.OrdinalIgnoreCase) && expr.Args.Length == 0)
        {
            EmitExpression(expr.Instance);
            _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.CollectionReverse), typeof(object)));
            return true;
        }
        if (method is null && instanceType is CircularLinkedListType
            && expr.MethodName.Lexeme.Equals("CURRENT", StringComparison.OrdinalIgnoreCase) && expr.Args.Length == 0)
        {
            EmitExpression(expr.Instance);
            _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.CircularCurrent), typeof(object)));
            return true;
        }
        if (method is null && instanceType is CircularLinkedListType
            && expr.MethodName.Lexeme.Equals("ROTATE", StringComparison.OrdinalIgnoreCase) && expr.Args.Length == 1)
        {
            EmitExpression(expr.Instance);
            EmitDoubleArgument(expr.Args[0]);
            _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.CircularRotate), typeof(object), typeof(double)));
            return true;
        }
        if (method is null && expr.MethodName.Lexeme.Equals("INDEXOF", StringComparison.OrdinalIgnoreCase) && expr.Args.Length == 1)
        {
            EmitExpression(expr.Instance);
            EmitExpression(expr.Args[0]);
            var iClr = DataTypeMapper.Map(NormalizeInferredType(InferType(expr.Args[0])), _types, _classTypes);
            if (iClr.IsValueType) _il.Emit(OpCodes.Box, iClr);
            _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.CollectionIndexOf), typeof(object), typeof(object)));
            return true;
        }
        if (method is null)
        {
            // Fallback LINQ: metodi di estensione Enumerable su collection CLR
            // (IndexOf/Contains/First/Last/ElementAt su LinkedList, ...).
            var linqCandidates = typeof(System.Linq.Enumerable).GetMethods(
                    System.Reflection.BindingFlags.Public | System.Reflection.BindingFlags.Static)
                .Where(m => m.Name.Equals(expr.MethodName.Lexeme, StringComparison.OrdinalIgnoreCase)
                            && m.GetParameters().Length == expr.Args.Length + 1)
                .ToList();
            if (linqCandidates.Count == 1)
            {
                var elementClr2 = DataTypeMapper.Map(CollectionElementType(instanceType), _types, _classTypes);
                var openLinq = linqCandidates[0];
                if (openLinq.IsGenericMethod)
                {
                    // I generic parameters del metodo (TSource, ...) vanno chiusi UNO per
                    // tipo (Enumerable.IndexOf<TSource> ha TSource in 2 parametri ma 1 argo
                    // generico!) — dedup per posizione.
                    var gps = openLinq.GetGenericArguments();
                    var typeArgs = new Type[gps.Length];
                    for (var gi = 0; gi < gps.Length; gi++)
                    {
                        // Default: il primo generic param è il tipo ELEMENTO della collection.
                        typeArgs[gi] = elementClr2;
                    }
                    openLinq = openLinq.MakeGenericMethod(typeArgs);
                }
                EmitExpression(expr.Instance);
                foreach (var arg in expr.Args) EmitExpression(arg);
                _il.Emit(OpCodes.Call, openLinq);
                if (openLinq.ReturnType == typeof(int) || openLinq.ReturnType == typeof(long)) _il.Emit(OpCodes.Conv_R8);
                return true;
            }

            // Fallback: First()/Last() come PROPRIETÀ (LinkedList CLR espone First/Last come
            // property, non metodo — il BASIC li usa come metodi).
            if (expr.Args.Length == 0)
            {
                var propInfo = collectionType.GetProperty(expr.MethodName.Lexeme, System.Reflection.BindingFlags.Public | System.Reflection.BindingFlags.Instance);
                if (propInfo is not null && propInfo.GetGetMethod() is { } propGetter)
                {
                    EmitExpression(expr.Instance);
                    _il.Emit(OpCodes.Callvirt, propGetter);
                    var propClr = propInfo.PropertyType;
                    if (propClr.IsGenericType && typeof(LinkedListNode<>).IsAssignableFrom(propClr.GetGenericTypeDefinition().IsGenericType ? propClr : propClr))
                    {
                        // LinkedList<T>.First è un LinkedListNode<T> — il BASIC vuole il VALORE.
                        var valueGetter = propClr.GetProperty("Value")!.GetGetMethod()!;
                        _il.Emit(OpCodes.Callvirt, valueGetter);
                    }
                    else if (propClr != typeof(string) && !propClr.IsValueType)
                    {
                        // SingleLinkedList<T>.First/Last (CommonLib) ritornano T direttamente.
                    }
                    return true;
                }
            }
            throw new NotSupportedException($"Backend IL: metodo collection '{expr.MethodName.Lexeme}' con {expr.Args.Length} argomenti non supportato per '{InferType(expr.Instance)}'.");
        }
        if (method.GetParameters().Any(parameter => parameter.ParameterType.IsByRef))
            throw new NotSupportedException($"Backend IL: metodo collection '{expr.MethodName.Lexeme}' con parametri BYREF non ancora supportato.");

        EmitExpression(expr.Instance);
        var parameters = method.GetParameters();
        var collectionGenericArgs = collectionType.IsGenericType ? collectionType.GetGenericArguments() : null;
        for (var i = 0; i < expr.Args.Length; i++)
        {
            EmitExpression(expr.Args[i]);
            var sourceType = NormalizeInferredType(InferType(expr.Args[i]));
            var paramType = parameters[i].ParameterType;
            // Un metodo risolto tramite TypeBuilder.GetMethod(closedType, openMethod) — vedi
            // ResolveNativeCollectionMethod — restituisce parametri il cui ParameterType è
            // ANCORA il generic parameter placeholder aperto (T), MAI sostituito col vero tipo
            // chiuso (es. Person): un limite di Reflection.Emit, il bridge serve a ottenere un
            // MethodInfo valido per Callvirt, non per l'introspezione completa. Va sostituito a
            // mano usando gli argomenti generici REALI del tipo chiuso della collezione — bug
            // scoperto con `customers.Add(c1)` su una LIST OF Person: EmitConvert riceveva
            // toClr="T" (IsValueType=false, un generic parameter libero), decideva quindi di
            // BOXARE l'argomento invece di lasciarlo value-type diretto → InvalidProgramException.
            if (paramType.IsGenericParameter && collectionGenericArgs != null &&
                paramType.GenericParameterPosition < collectionGenericArgs.Length)
                paramType = collectionGenericArgs[paramType.GenericParameterPosition];
            else if (paramType.ContainsGenericParameters && collectionGenericArgs != null)
                paramType = SubstituteGenericParameters(paramType, collectionGenericArgs);
            IlConversion.EmitConvert(_il, _types, sourceType, DataTypeMapper.Map(sourceType, _types, _classTypes), paramType);
        }
        _il.Emit(OpCodes.Callvirt, method);
        // Alcuni metodi CLR sono fluenti (LinkedList.AddFirst/AddLast restituiscono
        // LinkedListNode<T>) mentre il linguaggio li dichiara come comandi VOID. Il
        // valore CLR non può rimanere sullo stack: un ExpressionStmt VOID non emette
        // il Pop esterno e il metodo generato diventerebbe IL non valido al ret.
        if (TryInferNativeCollectionMethodReturn(expr, out var basicReturn) && basicReturn is VoidType && method.ReturnType != typeof(void))
            _il.Emit(OpCodes.Pop);
        else
            EmitNativeCollectionReturnCoercion(method.ReturnType);
        return true;
    }

    private void EmitNativeCollectionReturnCoercion(Type returnType)
    {
        // Il backend rappresenta i risultati integrali CLR come DOUBLE BASIC: senza
        // questa conversione metodi quali IndexOf/RemoveAt restituirebbero int32 su uno
        // stack che il resto del compilatore considera double (PRINT, LET e confronti).
        if (returnType == typeof(int) || returnType == typeof(uint) || returnType == typeof(short) ||
            returnType == typeof(ushort) || returnType == typeof(byte) || returnType == typeof(sbyte) ||
            returnType == typeof(long) || returnType == typeof(ulong))
            _il.Emit(OpCodes.Conv_R8);
    }

    /// <summary>Sostituisce i generic parameter (placeholder aperti `!0`/`!1`...) presenti
    /// DENTRO un tipo di parametro col tipo chiuso reale della collection (es. il parametro
    /// `IEnumerable&lt;!0&gt;` di List&lt;T&gt;.AddRange diventa `IEnumerable&lt;Class_514&gt;`).
    /// Necessario perché TypeBuilder.GetMethod NON sostituisce gli argomenti TypeBuilder:
    /// senza, il Callvirt riferisce un tipo aperto → BadImageFormat a load.</summary>
    private static Type SubstituteGenericParameters(Type type, Type[] closedArgs)
    {
        if (type.IsGenericParameter)
            return type.GenericParameterPosition < closedArgs.Length ? closedArgs[type.GenericParameterPosition] : type;
        if (type.IsGenericType)
        {
            var originalArgs = type.GetGenericArguments();
            var newArgs = originalArgs.Select(a => SubstituteGenericParameters(a, closedArgs)).ToArray();
            if (newArgs.Where((a, i) => !ReferenceEquals(a, originalArgs[i])).Any())
                return type.GetGenericTypeDefinition().MakeGenericType(newArgs);
        }
        return type;
    }

    private MethodInfo? ResolveNativeCollectionMethod(Type collectionType, string methodName, ImmutableArray<Expr> args)
    {
        // GetMethods diretto su un generico chiuso che CONTIENE un TypeBuilder come argomento
        // (es. List<Person> con Person = TYPE utente) fallisce con "TypeBuilder generic
        // instantiation does not support resolving members" — stesso limite di Reflection.Emit
        // di EmitNewNativeCollection (vedi lì e ClosedGenericContainsTypeBuilder), qui per la
        // risoluzione dei METODI invece del costruttore: richiede di guardare i metodi sul tipo
        // generico APERTO, poi collegarli al tipo chiuso con TypeBuilder.GetMethod. Per un
        // generico chiuso "normale" (es. List<string>, nessun TypeBuilder coinvolto) vale
        // l'inverso: il ponte stesso fallisce ("must be or must contain a TypeBuilder"), va
        // usato il percorso diretto originale. Nota: sui candidati APERTI i parametri sono
        // ancora il generic parameter placeholder (T), mai il tipo chiuso — il confronto esatto
        // sotto (parameter.ParameterType == sourceClrTypes[...]) è quindi solo un'euristica di
        // disambiguazione best-effort per overload genuinamente ambigui (es. LinkedList.AddLast),
        // non l'unico modo per trovare il metodo giusto quando ce n'è UNO solo per arità (il
        // caso comune, gestito prima da candidates.Length <= 1).
        var needsBridge = ClosedGenericContainsTypeBuilder(collectionType);
        var openType = needsBridge ? collectionType.GetGenericTypeDefinition() : collectionType;
        MethodInfo Bind(MethodInfo openMethod) => needsBridge
            ? TypeBuilder.GetMethod(collectionType, openMethod)
            : openMethod;

        var candidates = openType.GetMethods(BindingFlags.Instance | BindingFlags.Public)
            .Where(candidate => candidate.Name.Equals(methodName, StringComparison.OrdinalIgnoreCase))
            .Where(candidate => candidate.GetParameters().Length == args.Length)
            .ToArray();
        if (candidates.Length == 0) return null;
        if (candidates.Length == 1) return Bind(candidates[0]);

        // Alcune API CLR hanno overload semanticamente diversi ma con la stessa arità
        // (LinkedList.AddLast(T) / AddLast(LinkedListNode<T>)). Seleziona prima quello
        // la cui firma coincide con i tipi BASIC degli argomenti; resta un fallback
        // numerico per conversioni BASIC DOUBLE -> INTEGER/Long.
        var sourceClrTypes = args.Select(arg => DataTypeMapper.Map(NormalizeInferredType(InferType(arg)), _types, _classTypes)).ToArray();
        var exact = candidates.FirstOrDefault(candidate => candidate.GetParameters()
                .Select((parameter, index) => parameter.ParameterType == sourceClrTypes[index])
                .All(match => match))
            ?? candidates.FirstOrDefault(candidate => candidate.GetParameters()
                .Select((parameter, index) => (IsNativeNumericType(sourceClrTypes[index]) && IsNativeNumericType(parameter.ParameterType)) || parameter.ParameterType.IsAssignableFrom(sourceClrTypes[index]))
                .All(match => match));
        return exact != null ? Bind(exact) : null;
    }

    private static bool IsNativeNumericType(Type type) => type == typeof(double) || type == typeof(float) ||
        type == typeof(long) || type == typeof(ulong) || type == typeof(int) || type == typeof(uint) ||
        type == typeof(short) || type == typeof(ushort) || type == typeof(byte) || type == typeof(sbyte);

    private bool TryGetNativeCollectionType(DataType dataType, out Type collectionType)
    {
        switch (NormalizeInferredType(dataType))
        {
            case ListType or DictType or HashSetType or QueueType or StackType or SortedDictType or LinkedListType
                or CircularLinkedListType or DoublyLinkedListType or RingBufferType:
                collectionType = DataTypeMapper.Map(dataType, _types, _classTypes);
                return true;
            // ARRAY: Count/Join/Max/Min/Sort/Clear su un double[] (es. la comprehension
            // FROM...SELECT produce double[] via EmitListComprehension). Prima ArrayType NON
            // era riconosciuto -> Count(arr) cadeva al fallback stringa ("Count richiede 2
            // argomenti", LoopsForEachComprehensive).
            case ArrayType:
                collectionType = DataTypeMapper.Map(dataType, _types, _classTypes);
                return true;
            default:
                collectionType = null!;
                return false;
        }
    }

    /// <summary>Getter della property Count su un tipo collection. Su un'istanza generica
    /// chiusa su un TypeBuilder (es. List&lt;Udt_xxx&gt;) il GetProperty diretto lancia
    /// "Specified method is not supported" e TypeBuilder.GetMethod produce IL NON verificabile
    /// (BadImageFormat) — per quei casi i chiamanti usano il dispatch runtime
    /// IlRuntime.CollectionCount(object) invece di questo getter.</summary>
    private static MethodInfo GetCollectionCountGetter(Type collectionType)
        => collectionType.GetProperty(nameof(List<int>.Count))?.GetGetMethod()
            ?? throw new NotSupportedException($"Backend IL: collection '{collectionType}' non espone Count.");

    /// <summary>Vero se il tipo è (o contiene) un TypeBuilder non ancora finalizzato — i
    /// GetProperty/GetMethod di Reflection.Emit su queste chiusure generiche non funzionano.</summary>
    private static bool ClosedOverTypeBuilder(Type t)
    {
        if (t is TypeBuilder) return true;
        return t.IsGenericType && t.GetGenericArguments().Any(ClosedOverTypeBuilder);
    }

    /// <summary>
    /// Emette istanza, argomenti (con supporto BYREF) e la chiamata vera e propria — nucleo
    /// condiviso da EmitMethodCall e EmitIndexedAccess (quest'ultimo copre il caso in cui
    /// il Linker non riscrive "Istanza.Metodo(args)" in un vero MethodCallExpr — vedi lì).
    /// VIRTUAL/OVERRIDE/interfaccia usano `callvirt` (dispatch dinamico sul tipo REALE
    /// dell'istanza a runtime, il vtable nativo del CLR — mai uno fatto a mano, vedi TODO
    /// Obsidian, "Domande aperte" #5); un metodo non-virtual usa `call` diretta.
    /// </summary>
}
