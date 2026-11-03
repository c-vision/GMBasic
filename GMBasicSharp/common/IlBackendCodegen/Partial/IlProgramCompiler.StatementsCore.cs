// FILE: IlBackend/Partial/IlProgramCompiler.StatementsCore.cs
// PARTIAL — dominio STATEMENTS CORE (const/read/data/swap/return/gosub/exit/continue).
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

/// <summary>Partial del transpiler IL: dominio STATEMENTS CORE.</summary>
public sealed partial class IlProgramCompiler
{

    private void EmitConstStatement(ConstStmt stmt)
    {
        foreach (var declaration in stmt.Declarations)
            EmitStoreVariable(declaration.SymbolId, declaration.InitialValue);
    }

    private void EmitReadStatement(ReadStmt stmt)
    {
        foreach (var target in stmt.Variables)
        {
            switch (target)
            {
                case VariableTarget variable:
                    EmitStoreVariable(variable.SymbolId, () => EmitReadDataValue(_program.Symbols.GetSymbol(variable.SymbolId).DataType));
                    break;
                case ArrayElementTarget arrayElement:
                    EmitArrayElementStore(arrayElement, () => EmitReadDataValue(GetArrayInfo(arrayElement.SymbolId, arrayElement.Indices.Length).ElementDataType));
                    break;
                default:
                    throw new NotSupportedException($"Backend IL: READ non supporta ancora il target '{target.GetType().Name}'.");
            }
        }
    }

    private void EmitRestoreStatement(RestoreStmt stmt)
    {
        var position = 0;
        if (stmt.Target is not null)
        {
            int targetLine;
            if (stmt.Target.Kind == TokenKind.Number && int.TryParse(stmt.Target.Lexeme, out var line))
                targetLine = line;
            else if (stmt.Target.Kind == TokenKind.Identifier && _namedLabelToLine.TryGetValue(stmt.Target.Lexeme, out var labelLine))
                targetLine = labelLine;
            else
                throw new NotSupportedException(
                    $"Backend IL: RESTORE {stmt.Target.Lexeme} richiede un numero di riga o un'etichetta.");
            position = GetDataPositionAtOrAfterLine(targetLine);
        }

        _il.Emit(OpCodes.Ldc_I4, position);
        _il.Emit(OpCodes.Call, IlRuntimeRestoreDataMethod);
    }

    private void EmitRandomizeStatement(RandomizeStmt stmt)
    {
        if (stmt.Seed is null)
        {
            _il.Emit(OpCodes.Call, IlRuntimeRandomizeMethod);
            return;
        }

        EmitExpression(stmt.Seed);
        var seedType = NormalizeInferredType(InferType(stmt.Seed));
        var seedClr = DataTypeMapper.Map(seedType, _types, _classTypes);
        IlConversion.EmitConvert(_il, _types, seedType, seedClr, _types.Double);
        _il.Emit(OpCodes.Call, IlRuntimeRandomizeSeedMethod);
    }

    private void EmitSystemShellStatement(SystemShellStmt stmt)
    {
        EmitExpression(stmt.Command);
        var cmdType = NormalizeInferredType(InferType(stmt.Command));
        var cmdClr = DataTypeMapper.Map(cmdType, _types, _classTypes);
        IlConversion.EmitConvert(_il, _types, cmdType, cmdClr, _types.String);
        _il.Emit(OpCodes.Call, IlRuntimeSystemShellMethod);
        _il.Emit(OpCodes.Pop); // SystemShell ritorna double: scarta
    }

    private void EmitKeyGenStatement(KeyGenStmt stmt)
    {
        var handles = _il.DeclareLocal(typeof(long[]));
        EmitExpression(stmt.Algorithm);
        if (stmt.KeySize is not null) EmitInt64Argument(stmt.KeySize);
        else { _il.Emit(OpCodes.Ldc_I4, 2048); _il.Emit(OpCodes.Conv_I8); }
        _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.KeyGen)));
        _il.Emit(OpCodes.Stloc, handles);
        EmitKeyHandleStore(stmt.PrivateKeyHandle, () => { _il.Emit(OpCodes.Ldloc, handles); _il.Emit(OpCodes.Ldc_I4_0); _il.Emit(OpCodes.Ldelem_I8); });
        EmitKeyHandleStore(stmt.PublicKeyHandle, () => { _il.Emit(OpCodes.Ldloc, handles); _il.Emit(OpCodes.Ldc_I4_1); _il.Emit(OpCodes.Ldelem_I8); });
    }

    private void EmitKeyCloseStatement(KeyCloseStmt stmt)
    {
        EmitInt64Argument(stmt.KeyHandle);
        _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.KeyClose)));
    }

    private void EmitKeyHandleStore(Target target, Action emitValue)
    {
        switch (target)
        {
            case VariableTarget variable:
                // Gli handle (long da Ldelem_I8) vanno in variabili implicite DOUBLE:
                // il percorso (int, Action) non applica la coerce — il long 1 stoccato
                // senza conv.r8 diventava un double denormale (~1E-324) e GetCryptoKey
                // falliva ('Handle chiave non valido').
                var keyLocal = GetOrDeclareLocal(variable.SymbolId);
                emitValue();
                if (keyLocal.LocalType == typeof(double)) _il.Emit(OpCodes.Conv_R8);
                _il.Emit(OpCodes.Stloc, keyLocal);
                return;
            case ArrayElementTarget arrayElement:
                EmitArrayElementStore(arrayElement, emitValue);
                return;
            case FieldTarget field:
                EmitFieldStore(field, emitValue);
                return;
            default:
                throw new NotSupportedException($"Backend IL: target KEYGEN '{target.GetType().Name}' non supportato.");
        }
    }

    private void EmitFieldStore(FieldTarget target, Action emitValue)
    {
        var fieldSymbolId = ResolveFieldTargetSymbolId(target);
        if (_classFields.TryGetValue(fieldSymbolId, out var fieldBuilder))
        {
            if (fieldBuilder.IsStatic) { emitValue(); _il.Emit(OpCodes.Stsfld, fieldBuilder); return; }
            if (fieldBuilder.DeclaringType!.IsValueType) EmitInstanceAddress(target.Structure);
            else EmitInstanceValue(target.Structure);
            emitValue();
            _il.Emit(OpCodes.Stfld, fieldBuilder);
            return;
        }
        if (_classProperties.TryGetValue(fieldSymbolId, out var propInfo) && propInfo.Setter is not null)
        {
            EmitInstanceValue(target.Structure);
            emitValue();
            EmitPropertyAccessorCall(propInfo.Setter);
            return;
        }
        throw new NotSupportedException($"Backend IL: campo/proprietà '{target.OriginalToken.Lexeme}' non trovato/a, o proprietà senza SET.");
    }

    private int GetDataPositionAtOrAfterLine(int targetLine)
    {
        var offset = 0;
        foreach (var lineNumber in _program.MainModule!.ExecutionOrder)
        {
            foreach (var statement in _program.MainModule.Statements[lineNumber])
            {
                if (statement is DataStmt data)
                {
                    if (lineNumber >= targetLine) return offset;
                    offset += data.Values.Length;
                }
            }
        }
        return offset;
    }

    private void EmitReadDataValue(DataType dataType)
    {
        var method = NormalizeInferredType(dataType) switch
        {
            DoubleType => IlRuntimeReadDataDoubleMethod,
            // READ ... INTO una variabile SINGLE: se SINGLE è mappato a double (parità C) la
            // lettura deve produrre un double (ReadDataSingle restituisce float → stloc double
            // = IL invalido).
            SingleType when DataTypeMapper.Map(dataType, _types, _classTypes) == _types.Double
                => IlRuntimeReadDataDoubleMethod,
            SingleType => IlRuntimeReadDataSingleMethod,
            LongType => IlRuntimeReadDataLongMethod,
            ULongType => IlRuntimeReadDataULongMethod,
            IntegerType => IlRuntimeReadDataIntegerMethod,
            UIntegerType => IlRuntimeReadDataUIntegerMethod,
            ByteType => IlRuntimeReadDataByteMethod,
            SByteType => IlRuntimeReadDataSByteMethod,
            BitType => IlRuntimeReadDataBitMethod,
            CurrencyType => IlRuntimeReadDataCurrencyMethod,
            DateType => IlRuntimeReadDataDateMethod,
            StringType => IlRuntimeReadDataStringMethod,
            _ => throw new NotSupportedIlDataTypeException(dataType)
        };
        _il.Emit(OpCodes.Call, method);
    }

    /// <summary>
    /// Emette <c>SWAP a, b</c> per due variabili: salva il valore sinistro in un vero
    /// locale CLR tipizzato, poi effettua le due scritture. Non usa un'assegnazione
    /// temporanea BASIC perché quella potrebbe alterare la semantica di BYREF o di una
    /// variabile catturata; <see cref="EmitLoadVariable"/> e
    /// <see cref="EmitStoreVariable(int,Action)"/> preservano invece entrambi i casi.
    ///
    /// Gli array-element, i campi e i target indicizzati richiedono un emitter comune di
    /// lettura/scrittura di target (per non rivalutare gli indici durante lo scambio) e
    /// restano esplicitamente fuori da questo primo passo.
    /// </summary>
    private void EmitSwapStatement(SwapStmt stmt)
    {
        // L'ordine e' importante: in BASIC la location sinistra viene risolta e letta
        // PRIMA che il lato destro sia anche solo valutato. Catturare entrambi i target
        // prima della lettura sinistra cambierebbe `SWAP a(f()), a(g())` se g() ha effetti
        // collaterali. Per questo il temporaneo viene caricato fra le due catture.
        var left = CaptureSwapLocation(stmt.Left);
        var temporary = _il.DeclareLocal(left.Type);
        left.EmitLoad();
        _il.Emit(OpCodes.Stloc, temporary);

        var right = CaptureSwapLocation(stmt.Right);
        if (left.Type != right.Type)
            throw new NotSupportedException("Backend IL: SWAP richiede due variabili dello stesso tipo CLR.");

        left.EmitStore(right.EmitLoad);
        right.EmitStore(() => _il.Emit(OpCodes.Ldloc, temporary));
    }

    /// <summary>
    /// Risolve una destinazione di SWAP in una location IL riusabile. Un elemento array
    /// cattura i suoi indici gia' normalizzati nei propri locali: nessuna espressione di
    /// indice viene rivalutata durante lo store successivo. I campi e gli indexed target
    /// di campo verranno aggiunti con lo stesso contratto, dopo avere introdotto la cattura
    /// dell'istanza ricevente.
    /// </summary>
    private (Type Type, Action EmitLoad, Action<Action> EmitStore) CaptureSwapLocation(Target target)
    {
        switch (target)
        {
            case VariableTarget variable:
            {
                var type = DataTypeMapper.Map(_program.Symbols.GetSymbol(variable.SymbolId).DataType, _types, _classTypes);
                return (type,
                    () => EmitLoadVariable(variable.SymbolId),
                    emitValue => EmitStoreVariable(variable.SymbolId, emitValue));
            }

            case ArrayElementTarget arrayElement:
            {
                var info = GetArrayInfo(arrayElement.SymbolId, arrayElement.Indices.Length);
                var indices = new LocalBuilder[info.Rank];
                for (var axis = 0; axis < info.Rank; axis++)
                {
                    indices[axis] = _il.DeclareLocal(_types.Int32);
                    EmitInt32Arg(arrayElement.Indices[axis]);
                    if (info.LowerBounds[axis] != 0)
                    {
                        _il.Emit(OpCodes.Ldc_I4, info.LowerBounds[axis]);
                        _il.Emit(OpCodes.Sub);
                    }
                    _il.Emit(OpCodes.Stloc, indices[axis]);
                }

                void EmitAddress()
                {
                    _il.Emit(OpCodes.Ldloc, info.Local);
                    for (var axis = 0; axis < info.Rank; axis++)
                    {
                        _il.Emit(OpCodes.Ldloc, indices[axis]);
                        if (axis < info.Rank - 1 && info.ClrArrayType.GetElementType()!.IsArray)
                            _il.Emit(OpCodes.Ldelem_Ref);
                    }
                }

                void EmitLoad()
                {
                    EmitAddress();
                    if (info.Rank == 1) EmitArrayElementLoad(info.ElementDataType);
                    else if (info.ClrArrayType.GetElementType()!.IsArray) EmitArrayElementLoad(info.ElementDataType);
                    else _il.Emit(OpCodes.Call, info.ClrArrayType.GetMethod("Get")!);
                }

                void EmitStore(Action emitValue)
                {
                    EmitAddress();
                    emitValue();
                    if (info.Rank == 1) EmitArrayElementStore(info.ElementDataType);
                    else if (info.ClrArrayType.GetElementType()!.IsArray) EmitArrayElementStore(info.ElementDataType);
                    else _il.Emit(OpCodes.Call, info.ClrArrayType.GetMethod("Set")!);
                }

                return (DataTypeMapper.Map(info.ElementDataType, _types, _classTypes), EmitLoad, EmitStore);
            }

            case FieldTarget fieldTarget:
            {
                var fieldSymbolId = ResolveFieldTargetSymbolId(fieldTarget);
                if (_classFields.TryGetValue(fieldSymbolId, out var field))
                {
                    if (field.IsStatic)
                    {
                        return (field.FieldType,
                            () => _il.Emit(OpCodes.Ldsfld, field),
                            emitValue => { emitValue(); _il.Emit(OpCodes.Stsfld, field); });
                    }

                    // Un campo di UDT richiede un indirizzo del contenitore; catturare il
                    // valore in un locale lo trasformerebbe in una copia e lo SWAP non
                    // modificherebbe l'originale. Il caso CLASS, reference type, e' invece
                    // sicuro: il riferimento catturato identifica lo stesso oggetto per
                    // tutta l'operazione.
                    if (field.DeclaringType!.IsValueType)
                        throw new NotSupportedException("Backend IL: SWAP su campo di UDT non ancora supportato.");

                    var receiver = _il.DeclareLocal(field.DeclaringType);
                    EmitInstanceValue(fieldTarget.Structure);
                    _il.Emit(OpCodes.Stloc, receiver);
                    return (field.FieldType,
                        () => { _il.Emit(OpCodes.Ldloc, receiver); _il.Emit(OpCodes.Ldfld, field); },
                        emitValue => { _il.Emit(OpCodes.Ldloc, receiver); emitValue(); _il.Emit(OpCodes.Stfld, field); });
                }

                if (_classProperties.TryGetValue(fieldSymbolId, out var property) && property.Getter is not null && property.Setter is not null)
                {
                    var receiver = _il.DeclareLocal(property.Getter.DeclaringType!);
                    EmitInstanceValue(fieldTarget.Structure);
                    _il.Emit(OpCodes.Stloc, receiver);
                    return (property.Getter.ReturnType,
                        () => { _il.Emit(OpCodes.Ldloc, receiver); EmitPropertyAccessorCall(property.Getter); },
                        emitValue => { _il.Emit(OpCodes.Ldloc, receiver); emitValue(); EmitPropertyAccessorCall(property.Setter); });
                }

                throw new NotSupportedException($"Backend IL: SWAP richiede un campo o una PROPERTY leggibile e scrivibile ('{fieldTarget.OriginalToken.Lexeme}').");
            }

            default:
                throw new NotSupportedException($"Backend IL: SWAP non supporta ancora il target '{target.GetType().Name}'.");
        }
    }

    /// <summary>
    /// RETURN: usata sia per l'uscita esplicita da una FUNCTION (RETURN espr) sia da una
    /// SUB (RETURN nudo), sia — l'unico caso in cui <see cref="_onErrorDispatch"/> è attivo
    /// qui, dato che GosubStmt "nudo" resta non supportato (nessun case in EmitStatement) —
    /// come terminatore del corpo di un handler ON ERROR GOSUB.
    ///
    /// Nel caso normale (FUNCTION/SUB/costruttore/lambda, mai a livello top-level) `ret` è
    /// il vero return CIL: non serve nessun meccanismo di indirizzo di ritorno hand-rolled,
    /// lo stack di chiamata lo gestisce già il CLR.
    ///
    /// Quando invece <see cref="_onErrorDispatch"/> è attivo (RETURN a livello top-level,
    /// dentro uno dei chunk try-per-riga di EmitEntryPointWithErrorHandling) un `ret` nudo
    /// sarebbe IL NON VALIDO (non si può uscire da un blocco try con `ret`, solo con
    /// `leave`) — e comunque semanticamente sbagliato, "torna al chiamante CIL" non è mai
    /// il significato di un RETURN BASIC qui. La semantica di riferimento (unica fonte
    /// trovata: il commento di Interpreter/Tests/Exceptions.cs::SampleOnErrorResume — sia
    /// il p-code Compiler/VM sia l'interprete tree-walker hanno il campo
    /// "ErrorHandlerIsGosub"/IsGosub predisposto ma MAI cablato a un comportamento reale,
    /// quindi non c'è un riferimento FUNZIONANTE da replicare 1:1) è: RETURN dentro un
    /// handler ON ERROR GOSUB si comporta ESATTAMENTE come RESUME NEXT (riprende alla riga
    /// successiva a quella che ha originato l'errore, vedi EmitResumeNextTarget) — non
    /// richiede un RESUME esplicito. Se IsGosub è false (nessun handler GOSUB
    /// correntemente attivo — es. RETURN raggiunto senza un ON ERROR GOSUB, o dopo che un
    /// GOTO ha installato l'ultimo handler), tratta il RETURN come un END/STOP: comportamento
    /// analogo al p-code (OP_RETURN con Frame nullo e IpStack vuoto termina il task).
    /// </summary>
    private void EmitReturnStatement(ReturnStmt stmt)
    {
        // GOSUB RETURN: nessun valore, torna dinamicamente a dopo l'ultimo GOSUB.
        if (stmt.Value is null && _onErrorDispatch is null && _gosubReturnStackLocal is not null)
        {
            var noPendingGosub = _il.DefineLabel();
            _il.Emit(OpCodes.Ldloc, _gosubReturnStackLocal);
            _il.Emit(OpCodes.Callvirt, GosubStackCountGetter);
            _il.Emit(OpCodes.Brfalse, noPendingGosub);
            _il.Emit(OpCodes.Ldloc, _gosubReturnStackLocal);
            _il.Emit(OpCodes.Callvirt, GosubStackPopMethod);
            _il.Emit(OpCodes.Switch, _gosubReturnLabels);
            _il.Emit(OpCodes.Br, noPendingGosub); // ID corrotto: termina come RETURN senza GOSUB.
            _il.MarkLabel(noPendingGosub);
        }

        if (_onErrorDispatch is { } dispatch)
        {
            // Dentro una PROCEDURA (SUB/FUNCTION — _currentReturnType non nullo) il RETURN è
            // un'uscita VERA della procedura, non il terminatore di un handler ON ERROR GOSUB
            // dell'entry point: il valore va sul return della procedura. Il corpo però è
            // decomposto in un try per chunk (EmitProcedureBodyWithErrorHandling) e un `ret`
            // grezzo in una protected region è IL invalido → valore in un local tipizzato +
            // `leave` al punto di uscita comune (fuori dai try), che emette il vero ret.
            if (_currentReturnType is not null && _onErrorReturnExitLabel is { } procedureExit)
            {
                if (stmt.Value is not null)
                {
                    EmitExpression(stmt.Value);
                    if (_onErrorReturnValue is { } returnValueLocal)
                    {
                        EmitReturnValueConversion(stmt.Value);
                        _il.Emit(OpCodes.Stloc, returnValueLocal);
                    }
                    else
                    {
                        _il.Emit(OpCodes.Pop); // RETURN valore in una SUB: nessun valore da restituire
                    }
                }
                _il.Emit(OpCodes.Leave, procedureExit);
                return;
            }

            if (stmt.Value is not null)
                throw new NotSupportedException("Backend IL: RETURN con un valore non supportato a livello top-level (solo dentro una FUNCTION/SUB/costruttore/lambda compilata come un vero metodo CIL).");

            if (_gosubReturnStackLocal is not null && _gosubResumeSiteLocal is not null)
            {
                var noPendingOrdinaryGosub = _il.DefineLabel();
                _il.Emit(OpCodes.Ldloc, _gosubReturnStackLocal);
                _il.Emit(OpCodes.Callvirt, GosubStackCountGetter);
                _il.Emit(OpCodes.Brfalse, noPendingOrdinaryGosub);
                _il.Emit(OpCodes.Ldloc, _gosubReturnStackLocal);
                _il.Emit(OpCodes.Callvirt, GosubStackPopMethod);
                _il.Emit(OpCodes.Stloc, _gosubResumeSiteLocal);
                foreach (var site in _dispatchedGosubSites.Values)
                {
                    var nextSite = _il.DefineLabel();
                    _il.Emit(OpCodes.Ldloc, _gosubResumeSiteLocal);
                    _il.Emit(OpCodes.Ldc_I4, site.Id);
                    _il.Emit(OpCodes.Bne_Un, nextSite);
                    EmitSetStateAndLeave(dispatch.State, dispatch.LeaveTarget, site.OwnerState);
                    _il.MarkLabel(nextSite);
                }
                _il.Emit(OpCodes.Ldc_I4_M1);
                _il.Emit(OpCodes.Stloc, _gosubResumeSiteLocal);
                _il.MarkLabel(noPendingOrdinaryGosub);
            }

            var isGosubLabel = _il.DefineLabel();
            _il.Emit(OpCodes.Ldloc, dispatch.IsGosub);
            _il.Emit(OpCodes.Brtrue, isGosubLabel);
            EmitSetStateAndLeave(dispatch.State, dispatch.LeaveTarget, OnErrorDoneState);
            _il.MarkLabel(isGosubLabel);
            EmitResumeNextTarget(dispatch);
            _il.Emit(OpCodes.Leave, dispatch.LeaveTarget);
            return;
        }

        if (stmt.Value is not null)
        {
            EmitExpression(stmt.Value);
            EmitReturnValueConversion(stmt.Value);
        }
        if (_activeTryScopes.Count > 0 || _currentDefers.Count > 0)
        {
            // RETURN dentro un TRY/FINALLY (`ret` grezzo in una protected region è IL
            // invalido, ReturnFromTry) OPPURE con DEFER pendenti (parità Go: i body vanno
            // eseguiti LIFO PRIMA del ret reale/PCode/VM-C/C99Native) — in
            // entrambi i casi: salva il valore (se c'è) in un local tipizzato e salta al
            // punto di uscita comune (marcato a fine metodo in EmitStatementBody, dove i
            // DEFER vengono eseguiti prima del ret vero). Leave è OBBLIGATORIO per uscire
            // da una protected region; Br è l'unico valido quando il salto non attraversa
            // alcun TRY/CATCH/FINALLY (un DEFER fuori da qualunque TRY).
            if (stmt.Value is not null && _currentReturnType is not null and not VoidType)
            {
                var clr = DataTypeMapper.Map(_currentReturnType, _types, _classTypes);
                _returnExitValue ??= _il.DeclareLocal(clr);
                _il.Emit(OpCodes.Stloc, _returnExitValue);
            }
            _il.Emit(_activeTryScopes.Count > 0 ? OpCodes.Leave : OpCodes.Br, _returnExitLabel ??= _il.DefineLabel());
            _returnExitPending = true;
            return;
        }
        EmitDynamicScopeExit();
        _il.Emit(OpCodes.Ret);
    }

    /// <summary>
    /// Conversione del valore di ritorno (già sullo stack IL) al tipo dichiarato della
    /// FUNCTION corrente. Estratto da EmitReturnStatement perché usato anche dal RETURN
    /// dentro il dispatch ON ERROR di una procedura (EmitProcedureBodyWithErrorHandling),
    /// che salva il valore in un local tipizzato prima del `leave`.
    ///
    /// EmitCoerceTypes gestisce SOLO numerico→numerico (no-op silenzioso per qualunque
    /// altra combinazione) — RETURN di un valore numerico da una FUNCTION dichiarata AS
    /// STRING (fissa o no) lasciava quindi il double grezzo sullo stack dove la firma CIL
    /// del metodo dichiara `string`: un mismatch di tipo che il verificatore a volte
    /// tollera silenziosamente, producendo poi un riferimento oggetto "stringa" che in
    /// realtà sono i bit grezzi di un double — AccessViolationException al primo uso reale
    /// (es. PRINT) quando il runtime prova a dereferenziarlo come se fosse un vero
    /// puntatore a un oggetto String. Porting identico da GMBasic (vedi
    /// AiWiki IL-OnError-Loop-Bug.md, riprodotto con Types/TypesFixedLengthStrings.bas).
    /// Fix: converti esplicitamente in stringa PRIMA di Coerce/Ret quando il tipo di
    /// ritorno è stringa ma il valore non lo è.
    /// </summary>
    private void EmitReturnValueConversion(Expr value)
    {
        if (_currentReturnType is null) return;
        var valueType = NormalizeInferredType(InferType(value));
        if (_currentReturnType is StringType returnStrType)
        {
            if (valueType is not StringType)
                EmitConvertToString(valueType);
            // STRING * N: applica padding/troncamento anche al valore di ritorno —
            // mancava, RETURN da una FUNCTION dichiarata AS STRING * N restituiva la
            // stringa così com'era, senza la regola di lunghezza fissa. Porting da
            // GMBasic (commit 24e8d24e).
            if (returnStrType.FixedLength is > 0 and var retFixedLen)
            {
                _il.Emit(OpCodes.Ldc_I4, retFixedLen);
                _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.AdjustFixedString)));
            }
        }
        else if (_currentReturnType is AnyType)
        {
            if (!ExpressionEmitsObject(value))
            {
                var valueClr = DataTypeMapper.Map(valueType, _types, _classTypes);
                if (valueClr.IsValueType) _il.Emit(OpCodes.Box, valueClr);
            }
        }
        else if (valueType is BitType && IlConversion.IsNumeric(_currentReturnType)
                 && _currentReturnType is not BitType && !EmitsClrBoolValue(value))
        {
            // Variabile BIT (local/global int32, già -1/0) restituita da una FUNCTION
            // dichiarata numerica: la conversione standard tratta BitType come bool CLR e
            // moltiplica per -1, portando -1 a +1. Qui il valore sullo stack è numerico.
            var retClr = DataTypeMapper.Map(NormalizeInferredType(_currentReturnType), _types, _classTypes);
            IlConversion.EmitConvert(_il, _types, valueType, _types.Int32, retClr);
        }
        else
            EmitCoerceTypes(valueType, _currentReturnType);
    }

    /// <summary>
    /// Determina se un'espressione produce già una reference object nonostante
    /// l'inferenza statica conservativa indichi un tipo scalare.
    /// </summary>
    /// <param name="expression">Espressione appena emessa nello stack IL.</param>
    /// <returns>True quando non deve essere applicato un ulteriore opcode box.</returns>
    private bool ExpressionEmitsObject(Expr expression) => expression switch
    {
        BinaryExpr { Op.Kind: TokenKind.Plus } binary
            when NormalizeInferredType(InferType(binary.Left)) is AnyType
                || NormalizeInferredType(InferType(binary.Right)) is AnyType => true,
        _ => DataTypeMapper.Map(NormalizeInferredType(InferType(expression)), _types, _classTypes) == _types.Object
    };

    /// <summary>
    /// END SUB/END FUNCTION: implica sempre un `ret`. Per una FUNCTION, se il flusso
    /// raggiunge la fine SENZA una RETURN esplicita, il metodo CIL non può comunque fare
    /// `ret` senza valore (firma non-void) — si spinge un valore di default del tipo di
    /// ritorno dichiarato (0/0.0/""/false), stesso comportamento "innocuo" di un default
    /// mai inizializzato. _currentReturnType è null per Main (nessuna FUNCTION in corso).
    /// </summary>
    /// <summary>Finalizzazione DETERMINISTICA a fine procedura: per ogni LOCAL di tipo
    /// CLASSE con DELETE/DESTRUCTOR, chiama esplicitamente `__Destructor` (metodo pubblico)
    /// + `GC.SuppressFinalize` (evita la doppia esecuzione via GC). Parità PCode
    /// `EmitFinalizeBodyLocals` (OP_FINALIZE_SCOPE su ogni local del body a END SUB, in
    /// ordine inverso, esclusi parametri e return-slot). No-op se il compiler non ha il
    /// registro dei finalizer (GC-only).</summary>
    private void EmitFinalizeClassLocals()
    {
        bool hasClass = _classFinalizers is { Count: > 0 };
        bool hasUdt = _udtFinalizers is { Count: > 0 };
        if (!hasClass && !hasUdt) return;
        var suppress = typeof(GC).GetMethod(nameof(GC.SuppressFinalize), [typeof(object)])!;
        // Ordine inverso di dichiarazione (LIFO, come PCode): _locals è un Dictionary senza
        // ordine garantito → si finalizza in ordine di SymbolId decrescente (approssimazione
        // stabile; il campione ha un solo local).
        foreach (var kv in _locals.OrderByDescending(k => k.Key))
        {
            if (_parameters.ContainsKey(kv.Key)) continue;   // i parametri NON si finalizzano
            if (kv.Key == _currentReturnSymbolId) continue;  // return-slot
            // GLOBALI/STATIC NON si finalizzano qui: la finalizzazione è una sola, a fine
            // PROGRAMMA (EmitFinalizeGlobalObjects). Un UDT globale è materializzato come
            // local per-method (GetOrDeclareLocal) — senza questo skip verrebbe finalizzato
            // in OGNI procedura che lo legge (buffer fantasma id=0, parità PCode/VMC).
            if (IsGlobalSymbol(kv.Key) || IsStaticSymbol(kv.Key)) continue;
            var sym = kv.Key >= 0 ? _program.Symbols.GetSymbol(kv.Key) : null;
            if (sym is null) continue;
            var dt = NormalizeInferredType(sym.DataType);
            if (dt is ClassType ct)
            {
                // Reference type: null-check, call __Destructor, GC.SuppressFinalize.
                if (_classFinalizers is null || !_classFinalizers.TryGetValue(ct.Definition.SymbolId, out var fin)) continue;
                var skip = _il.DefineLabel();
                _il.Emit(OpCodes.Ldloc, kv.Value);
                _il.Emit(OpCodes.Brfalse, skip);
                _il.Emit(OpCodes.Ldloc, kv.Value);
                _il.Emit(OpCodes.Call, fin);
                _il.Emit(OpCodes.Ldloc, kv.Value);
                _il.Emit(OpCodes.Call, suppress);
                _il.MarkLabel(skip);
            }
            else if (dt is UserDefinedType udt && _udtFinalizers is { } udts
                     && udts.TryGetValue(udt.SymbolId, out var ufin))
            {
                // Value type: nessun finalizer CLR (niente SuppressFinalize) — Ldloca + call.
                // Il local è la struct stessa (address-of per un metodo d'istanza su value type).
                _il.Emit(OpCodes.Ldloca, kv.Value);
                _il.Emit(OpCodes.Call, ufin);
            }
        }
    }

    private void EmitEndProcedureStatement()
    {
        EmitFinalizeClassLocals();
        // END SUB/FUNCTION DENTRO il dispatch ON ERROR (un chunk): un `ret` nel try è
        // ILLEGALE ("Return out of try block" → InvalidProgramException, verificato con
        // ilverify — ExceptionsOnErrorResume). Il flusso corretto: stato DONE + leave al
        // target del chunk; il ret finale (fuori dai try) è emesso dal dispatch loop.
        if (_onErrorDispatch is { } dispatch)
        {
            _il.Emit(OpCodes.Ldc_I4, OnErrorDoneState);
            _il.Emit(OpCodes.Stloc, dispatch.State);
            _il.Emit(OpCodes.Leave, dispatch.LeaveTarget);
            return;
        }
        if (_currentReturnType is null or VoidType)
        {
            EmitDeferredBodiesIfAny();
            EmitDynamicScopeExit();
            _il.Emit(OpCodes.Ret);
            return;
        }

        if (_currentReturnSymbolId >= 0)
        {
            EmitLoadVariable(_currentReturnSymbolId);
            if (_currentDefers.Count > 0)
            {
                // Il valore va salvato PRIMA di eseguire i DEFER (potrebbero sovrascrivere
                // lo stesso local/campo di ritorno, come in Go — parità: il valore letto
                // qui, all'ingresso di END FUNCTION, è quello "congelato" per il chiamante).
                var clr = DataTypeMapper.Map(_currentReturnType, _types, _classTypes);
                _returnExitValue ??= _il.DeclareLocal(clr);
                _il.Emit(OpCodes.Stloc, _returnExitValue);
                EmitDeferredBodiesIfAny();
                _il.Emit(OpCodes.Ldloc, _returnExitValue);
            }
            EmitDynamicScopeExit();
            _il.Emit(OpCodes.Ret);
            return;
        }

        EmitDefaultValue(_currentReturnType);
        if (_currentDefers.Count > 0)
        {
            var clr = DataTypeMapper.Map(_currentReturnType, _types, _classTypes);
            _returnExitValue ??= _il.DeclareLocal(clr);
            _il.Emit(OpCodes.Stloc, _returnExitValue);
            EmitDeferredBodiesIfAny();
            _il.Emit(OpCodes.Ldloc, _returnExitValue);
        }
        EmitDynamicScopeExit();
        _il.Emit(OpCodes.Ret);
    }

    private void EmitExitStatement(ExitStmt stmt)
    {
        var loopName = stmt.TargetLabel?.Lexeme ?? stmt.TargetLoop.Lexeme;
        // First try named exit label (FOR loops)
        if (_loopExitLabels.TryGetValue(loopName, out var exitLabel))
        {
            // Con ON ERROR attivo il corpo è decomposto in un try SEPARATO per ogni
            // riga (vedi EmitEntryPointWithErrorHandling): un Br diretto dal chunk del
            // body verso il label di exit del loop (in un chunk DIVERSO — la riga del
            // LOOP) attraverserebbe il confine di una regione protetta senza Leave =
            // InvalidProgramException. Il dispatch imposta lo stato alla riga del label
            // di exit (quella del chunk LOOP, da _namedLabelToLine) e fa Leave verso il
            // dispatcher, che rientrerà lì: ExitLoopStmt marca il label e si prosegue.
            // NB: il token del label è stmt.TargetLabel (per EXIT DO anonimo è il label
            // di exit; stmt.TargetLoop è invece il token del DO/FOR).
            if (_onErrorDispatch is { } dispatch)
            {
                var exitToken = stmt.TargetLabel ?? stmt.TargetLoop;
                var exitLine = ResolveJumpTargetLine(exitToken)
                    ?? throw new NotSupportedException(
                        $"Backend IL: EXIT '{loopName}' con ON ERROR attivo — riga del label di uscita non risolvibile.");
                EmitSetStateAndLeave(dispatch.State, dispatch.LeaveTarget, exitLine);
                return;
            }
            // Stesso principio di EmitContinueStatement: il label di EXIT corretto è quello
            // del LabelStmt(loopExitLabel) (dopo il jump-back); _loopExitLabels marca exit e
            // continue insieme subito dopo il jump-back (posizione coincidente per l'exit,
            // ma per coerenza usiamo il label del LabelStmt quando disponibile).
            if (_namedLabels.TryGetValue(loopName, out var namedExitLabel))
            {
                _il.Emit(OpCodes.Leave, namedExitLabel);   // leave: valido anche dentro TRY/CATCH (br fuori da un handler = IL invalido)
                return;
            }
            _il.Emit(OpCodes.Leave, exitLabel);
            return;
        }
        // For DO loops without explicit name, use the active loop's exit label.
        // EXIT SUB / EXIT FUNCTION non devono essere assorbiti dal loop attivo:
        // il target e' la procedura (TokenKind.Function), non il ciclo.
        if (stmt.TargetLoop.Kind != TokenKind.Function && _activeLoopStack.Count > 0)
        {
            var (_, activeExitLabel) = _activeLoopStack.Peek();
            if (_onErrorDispatch is { } dispatch2)
            {
                var exitToken2 = stmt.TargetLabel ?? stmt.TargetLoop;
                var exitLine2 = ResolveJumpTargetLine(exitToken2)
                    ?? throw new NotSupportedException(
                        $"Backend IL: EXIT '{loopName}' con ON ERROR attivo — riga del label di uscita non risolvibile.");
                EmitSetStateAndLeave(dispatch2.State, dispatch2.LeaveTarget, exitLine2);
                return;
            }
            _il.Emit(OpCodes.Leave, activeExitLabel);
            return;
        }
        // EXIT SUB / EXIT FUNCTION (non in loop): ritorna dalla procedura corrente.
        if (_currentReturnType is not null)
        {
            // Con ON ERROR GOTO/RESUME NEXT attivo il corpo è decomposto in un try
            // SEPARATO per chunk (vedi EmitProcedureBodyWithErrorHandling): un `ret`
            // diretto qui uscirebbe da una regione protetta senza Leave — IL invalido
            // (ilverify: "ReturnFromTry"; a runtime InvalidProgramException). Le altre
            // due varianti di EXIT sopra (verso un loop) già gestiscono questo caso
            // instradando verso il dispatcher via EmitSetStateAndLeave; questo ramo
            // (EXIT SUB/FUNCTION fuori da un loop) mancava lo stesso guard — bug
            // riprodotto con `SUB Foo: ON ERROR GOTO H: ... : EXIT SUB: H: ...`. Fix:
            // stesso pattern usato da EmitReturnStatement per RETURN con ON ERROR
            // attivo — imposta lo stato a OnErrorDoneState e Leave verso il
            // dispatcher, che marca doneLabel (fuori da qualunque try) e lì emette il
            // vero Ret (con eventuale default value per una FUNCTION — vedi
            // EmitProcedureBodyWithErrorHandling).
            if (_onErrorDispatch is { } exitDispatch)
            {
                EmitSetStateAndLeave(exitDispatch.State, exitDispatch.LeaveTarget, OnErrorDoneState);
                return;
            }
            if (_currentReturnType is not VoidType)
            {
                // FUNCTION: EXIT FUNCTION deve restituire il valore gia' assegnato al
                // return-slot (convenzione BASIC `NomeFunzione = ...`), non un default.
                // Stessa semantica di END FUNCTION (EmitEndProcedureStatement).
                if (_currentReturnSymbolId >= 0)
                {
                    EmitLoadVariable(_currentReturnSymbolId);
                    if (_currentDefers.Count > 0)
                    {
                        var clr = DataTypeMapper.Map(_currentReturnType, _types, _classTypes);
                        _returnExitValue ??= _il.DeclareLocal(clr);
                        _il.Emit(OpCodes.Stloc, _returnExitValue);
                        EmitDeferredBodiesIfAny();
                        _il.Emit(OpCodes.Ldloc, _returnExitValue);
                    }
                }
                else
                {
                    EmitDefaultValue(_currentReturnType);
                }
                EmitDynamicScopeExit();
                _il.Emit(OpCodes.Ret);
            }
            else
            {
                EmitDynamicScopeExit();
                _il.Emit(OpCodes.Ret);
            }
            return;
        }
        throw new NotSupportedException($"Backend IL: EXIT '{loopName}' non supportato.");
    }

    private void EmitContinueStatement(ContinueStmt stmt)
    {
        var loopName = stmt.TargetLabel.Lexeme;
        if (_forLoopState.TryGetValue(loopName, out _))
        {
            // CONTINUE FOR: NEXT handles increment/check — no branch needed
            return;
        }
        // Il label di CONTINUE è il LabelStmt(loopContinueLabel) emesso dal parser alla
        // LOOP, PRIMA del jump-back (posizione CORRETTA). _loopExitLabels/ExitLoopStmt
        // marcano invece continue E exit nello stesso punto DOPO il jump-back — per i loop
        // DO/WHILE annidati con label, CONTINUE <loop-esterno> puntava lì (= exit) → il
        // ciclo usciva tutto (o loop infinito nel sample completo). Preferisci il label
        // del LabelStmt; se il nome non è registrato (es. FOR), ripiega su _loopExitLabels.
        if (_namedLabels.TryGetValue(loopName, out var continueLabel))
        {
            _il.Emit(OpCodes.Leave, continueLabel);
            return;
        }
        if (_loopExitLabels.TryGetValue(loopName, out var legacyContinueLabel))
        {
            _il.Emit(OpCodes.Leave, legacyContinueLabel);
            return;
        }
        // Fallback: use active loop's continue label (unnamed loops)
        if (_activeLoopStack.Count > 0)
        {
            var (activeContinue, _) = _activeLoopStack.Peek();
            _il.Emit(OpCodes.Leave, activeContinue);
            return;
        }
        throw new NotSupportedException($"Backend IL: CONTINUE '{stmt.TargetLabel.Lexeme}' non supportato.");
    }

    private void EmitGosubStatement(GosubStmt stmt)
    {
        if (_onErrorDispatch is not null)
        {
            if (_gosubReturnStackLocal is null || _gosubResumeSiteLocal is null ||
                !_dispatchedGosubSites.TryGetValue(stmt, out var dispatchedSite))
                throw new NotSupportedException("Backend IL: GOSUB dentro FINALLY o TRY annidato non è ancora supportato insieme a ON ERROR.");

            _il.Emit(OpCodes.Ldloc, _gosubReturnStackLocal);
            _il.Emit(OpCodes.Ldc_I4, dispatchedSite.Id);
            _il.Emit(OpCodes.Callvirt, GosubStackPushMethod);
            EmitBranch(stmt.Target);
            _il.MarkLabel(dispatchedSite.ReturnLabel);
            _il.Emit(OpCodes.Ldc_I4_M1);
            _il.Emit(OpCodes.Stloc, _gosubResumeSiteLocal);
            return;
        }

        if (_gosubReturnStackLocal is null || !_gosubSites.TryGetValue(stmt, out var site))
            throw new NotSupportedException("Backend IL: sito GOSUB privo di continuazione runtime.");

        _il.Emit(OpCodes.Ldloc, _gosubReturnStackLocal);
        _il.Emit(OpCodes.Ldc_I4, site.Id);
        _il.Emit(OpCodes.Callvirt, GosubStackPushMethod);
        EmitBranch(stmt.Target);
        _il.MarkLabel(site.ReturnLabel);
    }

}
