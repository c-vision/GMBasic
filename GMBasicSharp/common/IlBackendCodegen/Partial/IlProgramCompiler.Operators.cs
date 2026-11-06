// FILE: IlBackend/Partial/IlProgramCompiler.Operators.cs
// PARTIAL — dominio OPERATORS/VARIABLES (load/store/coerce/comparison/indirect).
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

/// <summary>Partial del transpiler IL: dominio OPERATORS/VARIABLES.</summary>
public sealed partial class IlProgramCompiler
{
    /// <summary>Builtin ARRAYS.* dichiarate ArrayType(InferredType) in StdLib (l'elemento non
    /// è staticamente noto) il cui runtime IlRuntime effettivo ritorna SEMPRE object?[]
    /// (ACHUNK: sotto-array; AZIP: coppie {a,b}) — mai un array omogeneo riducibile al mapping
    /// generico InferredType→Double di DataTypeMapper. Usata da EmitCoerceToTarget
    /// (Castclass del valore memorizzato) e RegisterDynamicArrayLocal (ClrArrayType del
    /// loader per le letture successive) per evitare un Castclass a double[] su un valore
    /// realmente object[] — vedi i commenti sui due call site per il bug reale.</summary>
    private static readonly System.Collections.Generic.HashSet<TokenKind> ObjectArrayReturningBuiltins =
        new() { TokenKind.AChunk, TokenKind.AZip };

    private void EmitStringArg(Expr expr)
    {
        EmitExpression(expr);
        EmitConvertToString(NormalizeInferredType(InferType(expr)));
    }

    /// <summary>
    /// Un parametro di procedura non è mai un vero LocalBuilder (è un argomento CIL,
    /// Ldarg/Starg — vedi EmitLoadVariable/EmitStoreVariable): chiamare questo metodo su
    /// un SymbolId di parametro fallisce esplicito invece di crearne silenziosamente un
    /// locale scollegato (es. un contatore FOR che fosse anche un parametro smetterebbe
    /// di aggiornare l'argomento reale — bug silenzioso, non ancora un caso gestito).
    /// </summary>
    private bool IsGlobalSymbol(int symbolId)
    {
        var symbol = _program.Symbols.GetSymbol(symbolId);
        var flags = symbol.Flags;
        if (!flags.HasFlag(SymbolFlags.IsGlobal) || flags.HasFlag(SymbolFlags.IsConst)) return false;
        // ERR e simili oggetti di runtime non passano dal bag GLOBAL.
        if (symbol.DataType is ErrType) return false;
        return true;
    }

    /// <summary>
    /// Piano Closure-ByRef-Box-Linker-Definitivo, Fase 4: un campo di __ClosureCtx per un
    /// parametro BYREF catturato è tipizzato ByRefBox&lt;T&gt; (mai il T nudo — un campo non
    /// può essere T&amp;, vedi Linker.Rewrites.cs). Controllo per riflessione sul TIPO CLR del
    /// campo, non un dizionario a parte: sempre corretto per costruzione, zero stato da
    /// tenere sincronizzato col Linker.
    /// </summary>
    private static bool IsByRefBoxFieldType(Type t) =>
        t.IsGenericType && t.GetGenericTypeDefinition() == typeof(ByRefBox<>);

    /// <summary>Resolver del tipo dichiarato di un simbolo (symbolId → DataType) per la
    /// detection del FOR EACH su canale nel may-park/lowering v2.</summary>
    private Basic.DataType? SymbolTypeResolver(int symbolId)
        => _program.Symbols.GetSymbol(symbolId).DataType;

    /// <summary>Vero se il simbolo è una variabile STATIC (persistenza tra chiamate,
    /// spec VARIABLES #3) — il linker marca i simboli STATIC con IsStaticStorage.</summary>
    private bool IsStaticSymbol(int symbolId)
        => _program.Symbols.GetSymbol(symbolId).Flags.HasFlag(SymbolFlags.IsStaticStorage);

    private Type StaticClrType(int symbolId) => GlobalClrType(symbolId);

    // ── DYNAMIC SCOPE (variabili DYNAMIC dichiarate dall'utente) ────────────────────────
    /// <summary>Vero se l'accesso riguarda una variabile DYNAMIC: si indicizza per NOME perché una
    /// lettura "a cascata" (una procedura chiamata che usa un nome dichiarato DYNAMIC da un
    /// chiamante, senza averlo dichiarato) NON risolve al simbolo IsDynamicScope della
    /// dichiarazione, ma a un simbolo globale/implicito con lo stesso nome. Stessa scelta del
    /// backend C99 (chiave per nome). L'insieme è precomputato una volta sola.</summary>
    private bool IsDynamicSymbol(int symbolId)
        => symbolId >= 0 && DynamicSymbolNames.Contains(_program.Symbols.GetSymbol(symbolId).OriginalToken.Lexeme);

    private HashSet<string>? _dynamicSymbolNames;

    private HashSet<string> DynamicSymbolNames
    {
        get
        {
            if (_dynamicSymbolNames is not null) return _dynamicSymbolNames;
            var set = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
            var all = _program.Symbols.AllSymbols;
            for (var i = 0; i < all.Count; i++)
                if (all[i].Flags.HasFlag(SymbolFlags.IsDynamicScope))
                    set.Add(all[i].OriginalToken.Lexeme);
            _dynamicSymbolNames = set;
            return set;
        }
    }

    /// <summary>Vero se la procedura correntemente emessa dichiara almeno una DYNAMIC: solo
    /// allora apre/chiude un frame dello scope dinamico (una procedura che non dichiara nulla
    /// non ne ha bisogno: le sue letture cercano verso l'esterno).</summary>
    private bool _currentProcDeclaresDynamic;

    /// <summary>Entra nello scope dinamico di una procedura (push di un frame se la procedura
    /// dichiara DYNAMIC). Da chiamare all'inizio dell'emissione del corpo.</summary>
    private void EmitDynamicScopeEnter(ImmutableArray<Stmt> body)
    {
        _currentProcDeclaresDynamic = BodyDeclaresDynamic(body);
        if (_currentProcDeclaresDynamic)
            _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.DynamicVarPush)));
    }

    /// <summary>Esce dallo scope dinamico della procedura corrente (pop del frame). No-op se
    /// la procedura non dichiara DYNAMIC. Da chiamare su OGNI percorso di uscita.</summary>
    private void EmitDynamicScopeExit()
    {
        if (_currentProcDeclaresDynamic)
            _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.DynamicVarPop)));
    }

    /// <summary>Vero se il corpo (ricorsivamente) contiene una dichiarazione DYNAMIC.</summary>
    private static bool BodyDeclaresDynamic(ImmutableArray<Stmt> body)
    {
        foreach (var stmt in body)
        {
            var found = false;
            WalkAst(stmt, node =>
            {
                if (node is DeclarationStmt decl && decl.Keyword.Kind == TokenKind.Dynamic)
                {
                    found = true;
                    return false;
                }
                return true;
            });
            if (found) return true;
        }
        return false;
    }

    /// <summary>Dichiara una variabile DYNAMIC nel frame corrente all'esecuzione della
    /// dichiarazione (DIM/let DYNAMIC). Il valore è memorizzato BOXED.</summary>
    private void EmitDynamicDeclaration(VariableDeclaration declaration)
    {
        var name = _program.Symbols.GetSymbol(declaration.SymbolId).OriginalToken.Lexeme;
        var clr = GlobalClrType(declaration.SymbolId);
        _il.Emit(OpCodes.Ldstr, name);
        if (declaration.InitialValue is not null)
        {
            EmitStoredValue(declaration.SymbolId, declaration.InitialValue);
            if (clr.IsValueType) _il.Emit(OpCodes.Box, clr);
        }
        else
        {
            // DYNAMIC non inizializzata → NIL (parità: una lettura restituisce il default).
            _il.Emit(OpCodes.Ldnull);
        }
        _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.DynamicVarDeclare), typeof(string), typeof(object)));
    }

    /// <summary>Legge una variabile DYNAMIC dallo scope dinamico (per NOME, con shadowing). Il
    /// valore è boxed nel frame: si dereferenzia al tipo CLR del simbolo, con coercizione
    /// permissiva per i tipi comuni (STRING/DOUBLE), come il runtime C condiviso (gmb_dscope).</summary>
    private void EmitLoadDynamicVar(int symbolId)
    {
        var name = _program.Symbols.GetSymbol(symbolId).OriginalToken.Lexeme;
        var clr = GlobalClrType(symbolId);
        _il.Emit(OpCodes.Ldstr, name);
        if (clr == _types.String)
        {
            _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.DynamicVarGetString), typeof(string)));
        }
        else if (clr == _types.Double)
        {
            _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.DynamicVarGetDouble), typeof(string)));
        }
        else if (clr == _types.Single)
        {
            _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.DynamicVarGetDouble), typeof(string)));
            _il.Emit(OpCodes.Conv_R4);
        }
        else if (clr == _types.Int32)
        {
            _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.DynamicVarGetDouble), typeof(string)));
            _il.Emit(OpCodes.Conv_I4);
        }
        else if (clr == _types.Int64)
        {
            _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.DynamicVarGetDouble), typeof(string)));
            _il.Emit(OpCodes.Conv_I8);
        }
        else
        {
            // Altri tipi (classi/UDT/object/bool): valore boxed con null-guard → default se assente
            // (stesso pattern di TryEmitLoadGlobal).
            _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.DynamicVarGet), typeof(string)));
            var has = _il.DefineLabel();
            var done = _il.DefineLabel();
            _il.Emit(OpCodes.Dup);
            _il.Emit(OpCodes.Brtrue, has);
            _il.Emit(OpCodes.Pop);
            EmitDefaultValue(NormalizeInferredType(_program.Symbols.GetSymbol(symbolId).DataType));
            _il.Emit(OpCodes.Br, done);
            _il.MarkLabel(has);
            if (clr.IsValueType) _il.Emit(OpCodes.Unbox_Any, clr);
            else if (clr != _types.Object) _il.Emit(OpCodes.Castclass, clr);
            _il.MarkLabel(done);
        }
    }

    /// <summary>Scrive una variabile DYNAMIC nello scope dinamico (per NOME, colpendo il frame
    /// più interno che la definisce). Il valore è valutato PRIMA in un local temp (poi boxed) —
    /// stesso accorgimento del ramo GLOBALE per non lasciare valori sotto eventuali loop.</summary>
    private void EmitStoreDynamicVar(int symbolId, Action emitValue)
    {
        var name = _program.Symbols.GetSymbol(symbolId).OriginalToken.Lexeme;
        var clr = GlobalClrType(symbolId);
        var temp = _il.DeclareLocal(clr);
        emitValue();
        _il.Emit(OpCodes.Stloc, temp);
        _il.Emit(OpCodes.Ldstr, name);
        _il.Emit(OpCodes.Ldloc, temp);
        if (clr.IsValueType) _il.Emit(OpCodes.Box, clr);
        _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.DynamicVarSet), typeof(string), typeof(object)));
    }

    private Type GlobalClrType(int symbolId)
    {
        var symbol = _program.Symbols.GetSymbol(symbolId);
        // Simbolo con tipo RIDEFINITO (`DIM S AS SINGLE` + `DIM S$ AS STRING`: stesso
        // SymbolId, tipi CLR diversi): nel bag globale il valore è BOXED, qualunque sia
        // il tipo dell'ultima dichiarazione. Senza questo, la lettura emetteva
        // `castclass System.String` su un Single boxed → InvalidCastException
        // (sample TypesConversions.bas). Gemello del ramo LOCAL (vedi
        // EmitLocalClrType/`clrType = typeof(object)`).
        if (_redefinedTypeSymbols.Contains(symbolId)) return typeof(object);
        if (symbol.DataType is BitType) return _types.Int32;
        return DataTypeMapper.Map(symbol.DataType, _types, _classTypes);
    }

    private bool TryEmitLoadGlobal(int symbolId)
    {
        if (!IsGlobalSymbol(symbolId)) return false;
        var clr = GlobalClrType(symbolId);
        _il.Emit(OpCodes.Ldc_I4, symbolId);
        _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.LoadGlobal), typeof(int)));
        if (clr.IsValueType)
        {
            var has = _il.DefineLabel();
            var done = _il.DefineLabel();
            _il.Emit(OpCodes.Dup);
            _il.Emit(OpCodes.Brtrue, has);
            _il.Emit(OpCodes.Pop);
            EmitDefaultValue(NormalizeInferredType(_program.Symbols.GetSymbol(symbolId).DataType));
            _il.Emit(OpCodes.Br, done);
            _il.MarkLabel(has);
            _il.Emit(OpCodes.Unbox_Any, clr);
            _il.MarkLabel(done);
        }
        else if (clr != typeof(object))
        {
            _il.Emit(OpCodes.Castclass, clr);
        }
        return true;
    }

    /// <summary>Carica una variabile STATIC dal bag (parità TryEmitLoadGlobal): ritorna false
    /// se il simbolo non è STATIC. Se mai inizializzata → default del tipo dichiarato.</summary>
    private bool TryEmitLoadStatic(int symbolId)
    {
        if (!IsStaticSymbol(symbolId)) return false;
        var clr = StaticClrType(symbolId);
        _il.Emit(OpCodes.Ldc_I4, symbolId);
        _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.LoadStatic), typeof(int)));
        if (clr.IsValueType)
        {
            var has = _il.DefineLabel();
            var done = _il.DefineLabel();
            _il.Emit(OpCodes.Dup);
            _il.Emit(OpCodes.Brtrue, has);
            _il.Emit(OpCodes.Pop);
            EmitDefaultValue(NormalizeInferredType(_program.Symbols.GetSymbol(symbolId).DataType));
            _il.Emit(OpCodes.Br, done);
            _il.MarkLabel(has);
            _il.Emit(OpCodes.Unbox_Any, clr);
            _il.MarkLabel(done);
        }
        else if (clr != typeof(object))
        {
            _il.Emit(OpCodes.Castclass, clr);
        }
        return true;
    }

    private LocalBuilder GetOrDeclareLocal(int symbolId)
    {
        if (_locals.TryGetValue(symbolId, out var existing)) return existing;
        if (_parameters.ContainsKey(symbolId))
            throw new NotSupportedException(
                "Backend IL: uso di un parametro di procedura in una posizione che richiede un vero locale (es. contatore FOR) non ancora supportato.");

        var symbol = _program.Symbols.GetSymbol(symbolId);
        var clrType = DataTypeMapper.Map(symbol.DataType, _types, _classTypes);
        // BIT SCALARE → int32 (parità VM: i valori logici sono 1/0/-1, NON bool -1/0 —
        // un confronto assegna 1, un'esplicita -1; PrintFormatValue(int32) li distingue,
        // PrintFormatValue(bool) no). Gli ARRAY BIT restano byte[]/bool[] (DataTypeMapper).
        if (symbol.DataType is BitType)
            clrType = _types.Int32;
        // Symbol con tipo RIDEFINITO (DIM S AS SINGLE poi DIM S$ AS STRING): local object
        // (boxed) — il tipo statico cambia tra DIM multiple, object li accomoda tutti.
        if (_redefinedTypeSymbols.Contains(symbolId))
            clrType = typeof(object);
        // Stepper mode: target di GO FUNCTION ... INTO x → local OBJECT (contiene un
        // PCodeFutureValue finché non risolto, poi il valore reale) — §11.5.
        if (_gpmStepperMode && _gpmFutureTargets.Contains(symbolId))
            clrType = typeof(object);
        // BYREF catturato da una closure di un'ALTRA procedura (Fase 4, piano
        // Closure-ByRef-Box-Linker-Definitivo — vedi IlProgramCompiler.ByRefEscapeBox.cs): la
        // variabile viene passata BYREF in una call che la cattura in una closure che sfugge
        // (RETURN di una FUNCTION/delegate) — un T&/local CLR normale non sopravviverebbe al
        // ritorno di QUESTA procedura, quindi il local è un ByRefBox<T> (reference type) invece
        // del tipo nudo. Letture/scritture instradate su .Value (vedi EmitLoadVariable/
        // EmitStoreVariable, ramo LOCAL). No-op per la stragrande maggioranza delle procedure
        // (BoxedSymbols è vuoto se non c'è questo pattern).
        if (BoxedSymbols.Contains(symbolId))
            clrType = typeof(ByRefBox<>).MakeGenericType(clrType);
        var local = _il.DeclareLocal(clrType);
        _locals[symbolId] = local;
        _localByName[symbol.OriginalName.ToUpperInvariant()] = local;
        // Stepper mode (§15): ogni locale CIL deve avere uno slot nel frame, altrimenti
        // non viene salvato/ricaricato ai confini chunk (es. il contatore FOR I, introdotto
        // da ForStmt e non presente nella prescan symbolIds → si perde a ogni tick → loop
        // infinito in GoRoutinesMultiplexing). Alloca lo slot al primo accesso; la capacity
        // del frame è già coperta dal prologo (margine per i contatori FOR annidati).
        if (_gpmStepperMode && _gpmFrameSlots is { } slots && !slots.ContainsKey(symbolId))
            slots[symbolId] = _gpmNextForSlot++;
        return local;
    }
    /// <summary>
    /// Carica sullo stack il valore della variabile (parametro o locale) identificata da
    /// symbolId. Un parametro BYVAL è un argomento CIL diretto (Ldarg); un parametro BYREF
    /// è un vero puntatore CIL (T&amp;) — Ldarg dà l'INDIRIZZO, serve un Ldind.Xxx in più per
    /// leggere il valore puntato.
    /// </summary>
    private void EmitLoadVariable(int symbolId)
    {
        // DYNAMIC (scope dinamico per NOME): il valore vive in un frame runtime, NON in un
        // local CIL (un local sarebbe visibile solo alla procedura dichiarante → i chiamati
        // leggevano sempre vuoto). Ramo per PRIMO: nessun'altra categoria di simbolo qui.
        if (IsDynamicSymbol(symbolId))
        {
            EmitLoadDynamicVar(symbolId);
            return;
        }
        // Variabile catturata da una lambda con chiusura (vedi LambdaClosureInfo): il
        // corpo referenzia ancora il SymbolId ORIGINALE dell'outer, mai riscritto — va
        // sempre controllato per PRIMO, prima di SELF/parametri/locali (una cattura non è
        // mai nessuna delle tre cose per COSTRUZIONE, ma un simbolo SELF/ME di una classe
        // esterna coesistente teoricamente potrebbe collidere per ID in scenari non
        // ancora esercitati — questo ordine è quindi anche difensivo).
        if (_capturedFields.TryGetValue(symbolId, out var capturedField))
        {
            _il.Emit(OpCodes.Ldarg_0);
            _il.Emit(OpCodes.Ldfld, capturedField);
            // BYREF catturato (piano Closure-ByRef-Box-Linker-Definitivo, Fase 4): il campo
            // è un ByRefBox<T> (Linker.Rewrites.cs marca il campo IsByRef quando la cattura
            // è un parametro BYREF — vedi la costruzione del campo in
            // IlProgramCompiler.Host.Compile.cs), MAI il valore nudo: un ulteriore Ldfld
            // sul suo campo Value dereferenzia il box (specchio del deref già fatto da
            // PCodeVM/nativo su OP_LOAD_MEMBER — vedi quel commit).
            if (IsByRefBoxFieldType(capturedField.FieldType))
                _il.Emit(OpCodes.Ldfld, capturedField.FieldType.GetField("Value")!);
            return;
        }
        // Contatore FOR ATTIVO: il local è l'AUTORITÀ mentre il loop è aperto, anche se il
        // simbolo è (o è aliasato per nome a) un GLOBALE/STATIC — controllato PRIMA di
        // static/global perché l'incremento del FOR (EmitForCheckAndJumpStatement/NextStmt)
        // scrive SOLO nel local via Stloc, MAI in StoreGlobal/StoreStatic. Senza questo
        // controllo anticipato, TryEmitLoadGlobal (sotto) vinceva SEMPRE per un contatore
        // dichiarato a livello MODULO (quindi globale): il bag non viene mai scritto dal
        // loop, quindi ogni lettura di `i` nel corpo restituiva il default (0) invece del
        // valore incrementato — bug: `FOR i = 1 TO 5 ... NEXT i` a livello modulo con `i`
        // usato nel corpo (es. `t = t + i`) sommava sempre 0.
        var forCounterName = _program.Symbols.GetSymbol(symbolId).OriginalToken.Lexeme;
        if (_forLoopState.TryGetValue(forCounterName, out var activeForState)
            && _activeForCounters.Contains(activeForState.SymbolId))
        {
            _il.Emit(OpCodes.Ldloc, GetOrDeclareLocal(activeForState.SymbolId));
            return;
        }
        // UDT GLOBALE (struct): il valore vive BOXED nel bag globale (StoreGlobal/LoadGlobal),
        // come ogni altro globale — così è CONDIVISO tra i metodi (il vecchio storage in un
        // local CIL per-metodo perdeva il valore quando la SUB che leggeva era diversa da
        // quella che aveva inizializzato: bug `cache id=0`). Le scritture di campo passano da
        // EmitInstanceAddress (Unbox del box → modifica in place), quindi il bag resta coerente.
        // Nessuno special-case: si lascia fare a TryEmitLoadGlobal (che per un value type fa
        // null→default + Unbox_Any).
        // Un array (DIM locale o parametro-array materializzato, vedi RegisterParameters)
        // vive SOLO in _arrays, mai in _locals — controllato PRIMA dei path static/global e
        // di _parameters: un array TOP-LEVEL (simbolo a visibilità di modulo) finirebbe
        // altrimenti nel bag globale (TryEmitLoadGlobal) che non materializza mai l'array
        // (→ PIXELBUFFER riceveva null e non disegnava nulla). Coerente con
        // indicizzazione/FOR EACH/UBOUND/REDIM, che leggono tutti _arrays direttamente.
        // EmitArrayRefFromInfo (non un Ldloc diretto): un array STATIC/GLOBALE (es. `DIM t =
        // FROM n IN nums ... SELECT n` a livello modulo) vive nel bag (RegisterDynamicArrayLocal
        // imposta ArrayInfo.LoadRef in quel caso) — il local qui non viene mai scritto, un
        // Ldloc diretto leggerebbe sempre null (bug: `t.Join(",")` su un DIM globale).
        if (_arrays.TryGetValue(symbolId, out var arrayInfo))
        {
            EmitArrayRefFromInfo(arrayInfo);
            return;
        }
        if (TryEmitLoadStatic(symbolId))
            return;
        if (TryEmitLoadGlobal(symbolId))
            return;
        if (TryResolveSelfArgIndex(symbolId, out var selfArgIndex))
        {
            _il.Emit(OpCodes.Ldarg, selfArgIndex);
            return;
        }
        if (_parameters.TryGetValue(symbolId, out var param))
        {
            _il.Emit(OpCodes.Ldarg, param.ArgIndex);
            if (param.IsBox)
            {
                // Fase 4: parametro logicamente BYREF ma rappresentato come ByRefBox<T>
                // (BYVAL a livello CLR) — un Ldfld su Value in più dereferenzia, stesso
                // schema di _capturedFields/local box.
                var boxClrType = typeof(ByRefBox<>).MakeGenericType(MapProcedureType(param.DataType, _types, _classTypes));
                _il.Emit(OpCodes.Ldfld, boxClrType.GetField("Value")!);
                return;
            }
            if (param.IsByRef)
            {
                // Parametro BYREF a un UDT STRUCT (value type CLR): Ldind_Ref leggerebbe
                // 8 byte (un pointer) da una struct di N byte → garbage (AccessViolation
                // in WritePrinter). Il managed pointer (T&) è già sullo stack: il campo
                // access (Ldfld) lo usa direttamente. Class/Interface (reference) restano
                // sul Ldind_Ref corretto.
                if (param.DataType is UserDefinedType) return;
                _il.Emit(IndirectOpCodes(param.DataType).Load);
            }
            return;
        }
        var loadedLocal = GetOrDeclareLocal(symbolId);
        // Variabile di iterazione FOR: l'uso nel corpo (es. `LINE (i*40,...), i, BF`) può
        // avere un SymbolId DIVERSO dal contatore del FOR (gap del Linker) → GetOrDeclareLocal
        // allocherebbe un local separato MAI aggiornato (i=0 nel loop). Se il NOME coincide
        // con un contatore FOR attivo, carica il local del contatore (aggiornato dal Next).
        var symName = _program.Symbols.GetSymbol(symbolId).OriginalToken.Lexeme;
        // Reindirizza SOLO se il FOR è ATTIVO (ancora aperto): _forLoopState resta popolato
        // per tutta la procedura e senza questo guard una variabile successiva omonima (es.
        // J in un DO dopo un `FOR J`) verrebbe letta dal local del vecchio contatore FOR.
        if (_forLoopState.TryGetValue(symName, out var forState)
            && forState.SymbolId != symbolId
            && _activeForCounters.Contains(forState.SymbolId))
        {
            loadedLocal = GetOrDeclareLocal(forState.SymbolId);
        }
        _il.Emit(OpCodes.Ldloc, loadedLocal);
        // BYREF catturato da una closure di un'altra procedura (Fase 4): il local è
        // ByRefBox<T> (vedi GetOrDeclareLocal) — un Ldfld su Value in più dereferenzia,
        // stesso schema del ramo _capturedFields sopra.
        if (loadedLocal.LocalType.IsGenericType && IsByRefBoxFieldType(loadedLocal.LocalType))
            _il.Emit(OpCodes.Ldfld, loadedLocal.LocalType.GetField("Value")!);
        // Symbol con tipo RIDEFINITO (local object boxed): il LOAD lascia object sullo
        // stack — le conversioni (EmitConvert/EmitNumericConversionCall) lo gestiscono via
        // IlRuntime.ToDouble/ToInt (un boxed SINGLE o una stringa "1.2" diventano double).
        // Niente castclass/unbox qui: il tipo del symbol cambia tra le DIM e il valore
        // boxed reale può essere di un tipo diverso (es. SINGLE da S = 1.2, poi STRING).
    }

    /// <summary>
    /// True se symbolId è il simbolo sintetico "SELF"/"ME" (per nome, non per ID — vedi
    /// <see cref="_isInstanceMember"/> per il perché) registrato dal Linker nello scope
    /// del metodo/costruttore correntemente compilato da questa istanza.
    /// </summary>
    private bool TryResolveSelfArgIndex(int symbolId, out int argIndex)
    {
        argIndex = 0;
        if (!_isInstanceMember || _parameters.ContainsKey(symbolId)) return false;
        // SELF non legato dal Linker negli accessor PROPERTY (SymbolId -1 = placeholder
        // __dummy): dentro un membro d'istanza l'unica variabile non-param non risolta è
        // SELF/ME — risolvila a `this` (arg 0).
        if (symbolId < 0) return true;
        var name = _program.Symbols.GetSymbol(symbolId).OriginalName;
        // __context (heap promotion del Linker per FUNCTION locali con catture): dentro il
        // metodo della closure, la variabile contesto È l'istanza stessa → SELF (arg 0).
        return name.Equals("SELF", StringComparison.OrdinalIgnoreCase)
            || name.Equals("ME", StringComparison.OrdinalIgnoreCase)
            || name.StartsWith("__context", StringComparison.OrdinalIgnoreCase);
    }

    /// <summary>
    /// Emette <paramref name="value"/> e lo memorizza in symbolId — thin wrapper su
    /// <see cref="EmitStoreVariable(int, Action)"/> per il caso comune (il valore viene da
    /// un'espressione utente vera e propria).
    /// </summary>
    private void EmitStoreVariable(int symbolId, Expr value) => EmitStoreVariable(symbolId, () => EmitStoredValue(symbolId, value));

    /// <summary>Valuta il valore da memorizzare (push sullo stack), con tutta la logica di
    /// conversione al tipo target (letterali tipizzati, coerce, ConvertArray). Estratto da
    /// EmitStoreVariable(int,Expr) per essere riusato dall'init UNA-SOLTA-VOLTA delle
    /// variabili STATIC (StaticInitOnce), che valuta ma NON scrive direttamente.</summary>
    private void EmitStoredValue(int symbolId, Expr value)
        {
            var targetType = NormalizeInferredType(_program.Symbols.GetSymbol(symbolId).DataType);
            // Target NULLABLE (scalare `INTEGER?`/`DOUBLE?`...): il local è OBJECT boxed
            // (DataTypeMapper: NullableType → object) → il valore va BOXATO, mai convertito al
            // tipo scalare (uno Stloc di un Int16 raw in un local object = AccessViolation).
            // NIL → Ldnull già sullo stack (un reference null è valido per il local object).
            if (_program.Symbols.GetSymbol(symbolId).DataType is NullableType)
            {
                EmitExpression(value);
                if (NormalizeInferredType(InferType(value)) is not NilType) EmitDynamicValueBox(value);
                return;
            }

            switch (value)
            {
                case ListLiteralExpr list when targetType is ListType expectedList:
                    EmitListLiteral(list, expectedList);
                    break;
                case ListLiteralExpr list when targetType is HashSetType or QueueType or StackType or LinkedListType
                    or DoublyLinkedListType or CircularLinkedListType or RingBufferType:
                    EmitTypedCollectionLiteral(list, targetType);
                    break;
                case ListLiteralExpr list when targetType is TrieType:
                    // DIM x AS TRIE = ["car","cat"]: il letterale produce List<string>, ma il
                    // local è CommonLib.Trie → builda il Trie dagli elementi (stloc in un
                    // local Trie con una List<string> = IL invalido).
                    EmitExpression(list);
                    _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.TrieFromItems), typeof(System.Collections.IEnumerable)));
                    break;
                case ListLiteralExpr list when targetType is BinaryHeapType heapLit:
                    // DIM x AS HEAP/MAXHEAP<T> = [4,1,7,3]: il letterale produce List<T>, ma il
                    // local è BinaryHeap<object> → impila con heapify (Peek/Extract = RADICE).
                    EmitExpression(list);
                    _il.Emit(OpCodes.Ldc_I4, heapLit.IsMaxHeap ? 1 : 0);
                    _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.HeapFromItems),
                        typeof(System.Collections.IEnumerable), typeof(bool)));
                    break;
                case ListLiteralExpr list when targetType is BagType bagType:
                    // DIM x AS BAG<T> = ["a","b","a"]: il letterale produce List<T>, ma il
                    // local è Dictionary<T,long> → conteggia le occorrenze (stloc in un local
                    // Dictionary con una List<string> = IL invalido / InvalidProgram con ILOptim).
                    EmitExpression(list);
                    var bagElemClr = DataTypeMapper.Map(CollectionElementType(bagType), _types, _classTypes);
                    _il.Emit(OpCodes.Call, BagFromItemsMethod.MakeGenericMethod(bagElemClr));
                    break;

                // Comprehension di dict/set con DIM tipizzato: l'emissione usa il tipo DICHIARATO (stringhe, interi, ...), non double fisso.
                case DictComprehensionExpr dictComp when targetType is DictType dictCompType:
                    EmitDictComprehension(dictComp, dictCompType);
                    break;
                case SetComprehensionExpr setComp when targetType is HashSetType setCompType:
                    EmitSetComprehension(setComp, setCompType);
                    break;
                case DictionaryLiteralExpr dictionary when targetType is DictType or SortedDictType:
                    EmitDictionaryLiteral(dictionary, targetType);
                    break;
                case DictionaryLiteralExpr dictionary when targetType is TreeType or AVLTreeType or RedBlackTreeType or BPlusTreeType:
                    // DIM x AS TREE<K,V> = {k:v} → SortedDictionary<object,object?> popolato
                    // (materializzazione delle strutture avanzate; prima il letterale non era
                    // gestito e `x.Add(...)` passava un int come istanza → IL invalido).
                    EmitDictionaryLiteral(dictionary, targetType);
                    break;
                // Il parser produce SEMPRE ArrayLiteralExpr per un letterale {...} (mai
                // ListLiteralExpr, riservato a [...]) — un `DIM x AS LIST OF T = {...}` o un
                // array a dimensione implicita (`DIM x() AS T = {...}`, Dimensions vuoto quindi
                // instradato qui invece che in EmitArrayDeclaration) arriva quindi con
                // value:ArrayLiteralExpr, MAI intercettato dai case sopra — cadeva nel default
                // sotto, che chiama EmitExpression semplice → EmitArrayLiteralExpression, la
                // quale ASSUME sempre double[] (nessuna nozione del tipo target). Bug scoperto
                // con `DIM colors AS LIST OF STRING = {...}` e `DIM names() AS STRING = {...}`:
                // string letterali salvati via stelem.r8 in un double[], corruzione silenziosa
                // (nessun errore a compile-time, valori spazzatura/crash a runtime).
                case ArrayLiteralExpr arrLit when targetType is ListType expectedList2:
                    EmitListLiteralElements(arrLit.Elements, expectedList2);
                    break;
                case ArrayLiteralExpr arrLit when targetType is ArrayType arrType:
                    EmitTypedArrayLiteral(arrLit.Elements, arrType.ElementType);
                    break;
                // DIM x AS ARRAY OF DOUBLE = [1,2,3,4]: le parentesi QUADRE producono un
                // ListLiteralExpr (List<T>), ma il tipo dichiarato è un ARRAY → senza questa
                // conversione la List veniva scritta nel local double[] (o restava una List
                // letta come array) → InvalidCastException a runtime su LEN/indice/slice
                // (bug reale: `DIM a AS ARRAY OF DOUBLE = [...]` + `a[1:3]`). Rank 1.
                case ListLiteralExpr listLit when targetType is ArrayType { ElementType: not ArrayType } arrFromList:
                {
                    var elemT = arrFromList.ElementType;
                    EmitListLiteralElements(listLit.Elements, new ListType(elemT));
                    var listClr = typeof(List<>).MakeGenericType(DataTypeMapper.Map(elemT, _types, _classTypes));
                    _il.Emit(OpCodes.Callvirt, listClr.GetMethod(nameof(List<int>.ToArray), Type.EmptyTypes)!);
                    break;
                }
                // DIM x() = AMAP/AFILTER/AREDUCE con callback tipizzata: il risultato è
                // tipizzato sul PARAMETRO della callback (es. short[] da FN(n AS INTEGER)),
                // il target del linker è double[] — castclass incompatibile (IL invalido).
                // ConvertArray del risultato al tipo del target.
                case BuiltinFunctionCallExpr bce when targetType is ArrayType targetArrType
                    && (bce.Name.Kind is TokenKind.AMap or TokenKind.AFilter or TokenKind.AReduce):
                {
                    EmitExpression(bce);
                    // AMAP: il risultato è tipizzato sul RETURN della callback
                    // (decimal[] da FN(o) => o.Quantita*o.PrezzoUnitario), NON sul parametro
                    // (Udt_498) — ConvertArray<Udt_498,decimal> su un decimal[] già pronto
                    // era un cast impossibile (InvalidCastException "Object must implement
                    // IConvertible"). AFILTER: il risultato è l'elemento (il parametro).
                    var srcClr = bce.Name.Kind == TokenKind.AMap
                        ? DataTypeMapper.Map(GetCallbackReturnType(bce.Args[1]), _types, _classTypes)
                        : GetCallbackParamClrType(bce.Args[1])
                          ?? DataTypeMapper.Map(targetArrType.ElementType, _types, _classTypes);
                    var dstClr = DataTypeMapper.Map(targetArrType.ElementType, _types, _classTypes);
                    if (srcClr != dstClr)
                        _il.Emit(OpCodes.Call, ArrayHelpersConvertArrayMethod.MakeGenericMethod(srcClr, dstClr));
                    break;
                }
                // DIM CB() AS INTEGER = COLLIDEBATCH(X, Y, w, h): il runtime ritorna
                // object[] (indici boxed); il target può essere un array tipizzato (int16[]).
                case BuiltinFunctionCallExpr cbce when targetType is ArrayType cbArrType
                    && cbce.Name.Kind is TokenKind.CollideBatch:
                {
                    EmitExpression(cbce);
                    var cbDst = DataTypeMapper.Map(cbArrType.ElementType, _types, _classTypes);
                    if (cbDst != _types.Object)
                        _il.Emit(OpCodes.Call, ArrayHelpersConvertArrayMethod.MakeGenericMethod(_types.Object, cbDst));
                    break;
                }
                default:
                    EmitExpression(value);
                    EmitCoerceToTarget(symbolId, value);
                    break;
            }
        }

    /// <summary>
    /// Emette il valore tramite <paramref name="emitValue"/> e lo memorizza in symbolId.
    /// Prende un delegato (non un valore già sullo stack, né solo un'Expr) perché un
    /// parametro BYREF richiede un ordine diverso: prima l'INDIRIZZO (Ldarg), POI il
    /// valore, poi Stind.Xxx — a differenza di Stloc/Starg (BYVAL/locale), che vogliono
    /// solo il valore; stesso discorso per un campo catturato (Ldarg_0 prima, poi il
    /// valore, poi Stfld). Il chiamante non può quindi limitarsi a pushare il valore PRIMA
    /// di chiamare questo metodo — da cui il delegato, invocato al punto giusto della
    /// sequenza per ciascun caso. Usato sia dal wrapper Expr sopra (il caso comune) sia da
    /// FOR EACH (EmitForEachAdvanceAndAssignStatement), dove il valore non viene da
    /// un'espressione utente ma da un ldelem sull'array sorgente.
    /// </summary>
    private void EmitStoreVariable(int symbolId, Action emitValue)
    {
        // DYNAMIC (scope dinamico per NOME): la scrittura colpisce il frame runtime più interno
        // che definisce il nome (shadowing), o lo dichiara nel frame corrente se assente.
        if (IsDynamicSymbol(symbolId))
        {
            EmitStoreDynamicVar(symbolId, emitValue);
            return;
        }
        if (_capturedFields.TryGetValue(symbolId, out var capturedField))
        {
            // BYREF catturato (Fase 4): scrivi ATTRAVERSO il box (Ldfld il campo per
            // ottenere il box, POI Stfld su Value) invece di sovrascrivere il campo stesso
            // — altrimenti si perderebbe l'identità/collegamento col box del chiamante.
            if (IsByRefBoxFieldType(capturedField.FieldType))
            {
                _il.Emit(OpCodes.Ldarg_0);
                _il.Emit(OpCodes.Ldfld, capturedField);
                emitValue();
                _il.Emit(OpCodes.Stfld, capturedField.FieldType.GetField("Value")!);
                return;
            }
            _il.Emit(OpCodes.Ldarg_0);
            emitValue();
            _il.Emit(OpCodes.Stfld, capturedField);
            return;
        }
        if (IsGlobalSymbol(symbolId))
        {
            // UDT GLOBALE (struct): si memorizza BOXED nel bag globale (ramo comune sotto,
            // con `if (clr.IsValueType) Box clr`) — storage condiviso tra metodi. Il vecchio
            // store nel local CIL per-metodo rendeva il globale invisibile alle altre SUB.
            // Bug corretto (2026-08-31, audit generator expressions): valuta PRIMA il
            // valore in un local temp, poi pusha chiave+valore per StoreGlobal — non
            // l'ordine originale (chiave PRIMA, poi emitValue()), che lascia la chiave
            // "sotto" tutto quello che emitValue() emette. Innocuo quando emitValue() è
            // una singola espressione semplice, ma quando contiene un CICLO con un
            // branch all'indietro (es. una generator expression `(x*x FOR x IN nums)`
            // come RHS di `DIM g1 = ...` a livello globale), l'ottimizzatore post-IL
            // (ILOptim, common/ILOptim/Frontend/BlockState.MergePredStacks) assume che il
            // blocco della TESTA del loop (raggiunto SOLO tramite il branch all'indietro)
            // erediti profondità 0 dal suo predecessore non ancora visitato — un limite
            // strutturale del suo importer a singola passata in ordine di indirizzo — e
            // scarta la profondità 1 dovuta alla chiave lasciata sotto, con conseguente
            // assertion failure ("Inconsistent evaluation stack between basic blocks",
            // crash del COMPILATORE stesso, non un'eccezione a runtime). Un local temp
            // valutato PRIMA elimina qualunque valore residuo sotto il loop.
            var clr = GlobalClrType(symbolId);
            var tempLocal = _il.DeclareLocal(clr);
            emitValue();
            _il.Emit(OpCodes.Stloc, tempLocal);
            // RIASSEGNAZIONE di un GLOBALE CLASS con DESTRUCTOR: finalizza la VECCHIA istanza
            // (parità PCode/VM-C/C99: `g = NEW X()` chiama il distruttore del vecchio g).
            // Il nuovo valore è già stato valutato (tempLocal) → l'ordine è nuovo-ctor poi
            // vecchio-dtor, come PCode. Null-check per la prima assegnazione/DIM (old = null).
            EmitFinalizeReassignedGlobalInstance(symbolId, clr);
            _il.Emit(OpCodes.Ldc_I4, symbolId);
            _il.Emit(OpCodes.Ldloc, tempLocal);
            if (clr.IsValueType) _il.Emit(OpCodes.Box, clr);
            _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.StoreGlobal), typeof(int), typeof(object)));
            return;
        }
        if (IsStaticSymbol(symbolId))
        {
            // Stesso fix del ramo globale sopra (vedi il commento lì): valuta PRIMA il
            // valore in un local temp, evitando che la chiave resti sotto un eventuale
            // loop con branch all'indietro emesso da emitValue().
            var clr = StaticClrType(symbolId);
            var tempLocal = _il.DeclareLocal(clr);
            emitValue();
            _il.Emit(OpCodes.Stloc, tempLocal);
            _il.Emit(OpCodes.Ldc_I4, symbolId);
            _il.Emit(OpCodes.Ldloc, tempLocal);
            if (clr.IsValueType) _il.Emit(OpCodes.Box, clr);
            _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.StoreStatic), typeof(int), typeof(object)));
            return;
        }
        if (TryResolveSelfArgIndex(symbolId, out _))
            throw new NotSupportedException("Backend IL: non è possibile riassegnare SELF/ME.");
        if (_parameters.TryGetValue(symbolId, out var param))
        {
            if (param.IsBox)
            {
                // Fase 4: scrivi ATTRAVERSO il box (Ldarg il parametro per ottenere il box,
                // POI Stfld su Value) — mai Starg (sovrascriverebbe il RIFERIMENTO al box,
                // perdendo il collegamento col chiamante).
                var boxClrType = typeof(ByRefBox<>).MakeGenericType(MapProcedureType(param.DataType, _types, _classTypes));
                _il.Emit(OpCodes.Ldarg, param.ArgIndex);
                emitValue();
                _il.Emit(OpCodes.Stfld, boxClrType.GetField("Value")!);
                return;
            }
            if (param.IsByRef)
            {
                _il.Emit(OpCodes.Ldarg, param.ArgIndex); // indirizzo, PRIMA del valore (ordine richiesto da stind)
                emitValue();
                _il.Emit(IndirectOpCodes(param.DataType).Store);
                return;
            }
            emitValue();
            _il.Emit(OpCodes.Starg, param.ArgIndex);
            return;
        }
        // Riassegnazione dell'INTERO riferimento array (es. `LET B = ACLONE(A)`, non un
        // elemento singolo — quello passa da EmitArrayElementAssignment, non da qui): il
        // locale corretto è quello già registrato in _arrays (creato da EmitArrayDeclaration
        // e riusato ovunque per l'indicizzazione), MAI un nuovo locale da GetOrDeclareLocal
        // (che userebbe un _locals separato, disconnesso, lasciando l'array acceduto tramite
        // ArrayInfo.Local sempre col vecchio riferimento) — stesso locale riusato da REDIM
        // (EmitRedimStatement) per lo stesso motivo.
        if (_arrays.TryGetValue(symbolId, out var arrayInfo))
        {
            emitValue();
            _il.Emit(OpCodes.Stloc, arrayInfo.Local);
            return;
        }
        var targetLocal = GetOrDeclareLocal(symbolId);
        // Fase 4: riassegnazione (non la PRIMA scrittura, quella la gestisce
        // EmitScalarDeclaration con Newobj) di un local box-typed — scrivi ATTRAVERSO il box
        // esistente (Ldloc + Stfld Value), mai Stloc (sovrascriverebbe il RIFERIMENTO al box,
        // perdendo il collegamento con la closure che lo ha già catturato).
        if (targetLocal.LocalType.IsGenericType && IsByRefBoxFieldType(targetLocal.LocalType))
        {
            _il.Emit(OpCodes.Ldloc, targetLocal);
            emitValue();
            _il.Emit(OpCodes.Stfld, targetLocal.LocalType.GetField("Value")!);
            return;
        }
        emitValue();
        _il.Emit(OpCodes.Stloc, targetLocal);
    }

    /// <summary>
    /// Coerce il valore già sullo stack al tipo CLR del simbolo target (solo scalari
    /// numerici) — risolve il bug preesistente dello store-var (vedi nota Obsidian
    /// IlBackend-Patterns-from-XSharp.md): senza coerce, <c>LET A = CLNG(2.7)</c> con A
    /// Double pushava l'Int64 di CLNG nello Stloc Double senza conversione → IL invalido,
    /// valore riletto come denormal (~1E-323) al PRINT. Wrapper (int,Expr) sopra accoda
    /// questo DOPO il push del valore e PRIMA dello store (Stloc/Starg/Stind/Stfld leggono
    /// tutte il valore in cima allo stack → la coerce resta applicata in ognuno). Nessuna
    /// istruzione se i tipi coincidono, altrimenti widening/narrowing (arrotonda).
    /// </summary>
    private void EmitCoerceToTarget(int symbolId, Expr value)
    {
        var targetType = NormalizeInferredType(_program.Symbols.GetSymbol(symbolId).DataType);
        // `DIM entries AS LIST` + `entries = IO.DIR(...)`: IO.DIR/READLINES... ritornano un ARRAY (string[]), ma una variabile LIST
        // senza elemento tipizzato e' una List<object>: lo store (castclass sul campo/local) lanciava InvalidCastException
        // "String[] -> List<object>" per le variabili globali (registry PackageServer.bas). Si converte l'array in List<object>.
        if (targetType is ListType && NormalizeInferredType(InferType(value)) is ArrayType
            && DataTypeMapper.Map(targetType, _types, _classTypes) == typeof(System.Collections.Generic.List<object>))
        {
            _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.ArrayToObjectList), typeof(System.Collections.IEnumerable)));
            return;
        }
        // Stepper mode — il valore è il RISULTATO di un future (GO FUNCTION ... INTO x,
        // §11.5): EmitLoadVariable ha lasciato un OBJECT (il valore boxed, non più il
        // future). Se il target è numerico/stringa, converti dall'object reale (dispatch
        // su boxed) invece di fidarti del tipo statico del RHS (Long → già Int64, ma a
        // runtime è un object boxed → stloc Long invalido).
        if (_gpmStepperMode && value is VariableExpr fv && _gpmFutureTargets.Contains(fv.SymbolId))
        {
            // Il valore è un object (risultato del future). Per un target NUMERICO, dispatch
            // sul boxed reale via IlRuntime.ToDouble poi conv al tipo target. (Il caso
            // String/altro richiede la sua conversione — nessun sample lo usa oggi.)
            if (IlConversion.IsNumeric(targetType))
            {
                _il.Emit(OpCodes.Call, _types.IlRuntimeToDoubleMethod);
                var dynToClr = DataTypeMapper.Map(targetType, _types, _classTypes);
                IlConversion.EmitConvert(_il, _types, new DoubleType(), _types.Double, dynToClr);
            }
            return;
        }
        // Simbolo con local object (ridefinizione `$` o SafeCast `AS?`): il valore emesso
        // non è necessariamente già object. Per esempio `DIM S AS SINGLE` seguito più
        // avanti da `DIM S$ AS STRING` condivide lo stesso SymbolId; `S = 1.2` lascia un
        // float32 grezzo sullo stack e uno `stloc object` senza BOX genera IL invalido
        // (il successivo PRINT interpreta i bit del float come un riferimento e può
        // terminare con AccessViolationException). Usa il tipo CLR realmente emesso:
        // value type vengono boxed, mentre SafeCast/variabili già object restano invariati.
        if (_redefinedTypeSymbols.Contains(symbolId))
        {
            // Il chiamante ha già convertito il valore al tipo numerico ORIGINALE del
            // simbolo (es. conv.r4 per un SINGLE): per un valore NUMERICO il box deve
            // essere .Single (non .Double come EmitDynamicValueBox — che boxava col tipo
            // inferito dell'espressione, lasciando S = 1.234567 come double 1.23456704616547
            // e facendo sì che CDBL(S) estendesse invece di usare la stringa). Golden
            // TypesConversions 'CDBL(S) = 1.234567'. Valori non numerici (String, Any...)
            // → box del valore come prima.
            var reSourceType = NormalizeInferredType(InferType(value));
            if (reSourceType is not StringType and not AnyType and not NullableType and not DateType)
            {
                // Box col tipo CLR REALE del valore (SINGLE scalare = double quando mappato a
                // double, parità C; prima si boxava sempre float → InvalidCastException sul
                // successivo unbox double di LoadGlobal/GlobalClrType).
                var reClr = DataTypeMapper.Map(reSourceType, _types, _classTypes);
                if (reClr.IsValueType) _il.Emit(OpCodes.Box, reClr);
                else EmitDynamicValueBox(value);
            }
            else
            {
                EmitDynamicValueBox(value);
            }
            return;
        }
        // Target DINAMICO (AnyType — es. `DIM x = AREDUCE(...)` il cui ReturnType è
        // InferredType): il local è object — un valore numerico va BOXATO (senza, il double
        // raw letto come reference in JsonValueToString → AccessViolation).
        if (targetType is AnyType)
        {
            var dynValClr = DataTypeMapper.Map(NormalizeInferredType(InferType(value)), _types, _classTypes);
            if (dynValClr.IsValueType) _il.Emit(OpCodes.Box, dynValClr);
            return;
        }
        if (value is BuiltinFunctionCallExpr { Name.Kind: TokenKind.CType, Args: [_, StringExpr requested, ..] }
            && requested.Value.ToUpperInvariant().Replace(" ", string.Empty) == OperatorReturnKey(targetType))
            return;
        // Assegnazione a una variabile/parametro STRING (fissa o no): un valore non-stringa
        // (es. un numero) va convertito implicitamente come da CSTR() PRIMA di applicare
        // l'eventuale padding/troncamento STRING * N (stesso principio già applicato a RETURN
        // — vedi il commento in EmitReturnStatement — qui mancava del tutto per LET/assegnazione
        // a variabile: EmitCoerceToTarget usciva subito perché StringType non è IsNumeric,
        // lasciando SEMPRE la stringa non paddata/troncata anche quando il target dichiarava
        // STRING * N esplicitamente).
        if (targetType is StringType targetStr)
        {
            var strValueType = NormalizeInferredType(InferType(value));
            if (strValueType is not StringType)
                EmitConvertToString(strValueType);
            if (targetStr.FixedLength is > 0 and var fixedLen)
            {
                _il.Emit(OpCodes.Ldc_I4, fixedLen);
                _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.AdjustFixedString)));
            }
            return;
        }
        // BIT SCALARE → int32 (parità VM: valori 1/0/-1). Il valore numerico (confronto
        // int32, literal double -1/0) va convertito a int32 — prima il local era bool e
        // la coerce usciva qui, lasciando tipi incoerenti con il nuovo local int32.
        if (targetType is BitType)
        {
            var bitValueType = NormalizeInferredType(InferType(value));
            if (bitValueType is not BitType)
            {
                var bitClr = DataTypeMapper.Map(bitValueType, _types, _classTypes);
                if (bitClr == _types.Int32) { /* già int32 (confronto) */ }
                else if (bitClr.IsValueType) _il.Emit(OpCodes.Conv_I4);
                else _il.Emit(OpCodes.Call, _types.IlRuntimeToDoubleMethod);
            }
            else if (EmitsClrBoolValue(value))
            {
                // Il local BIT scalare è int32 (GetOrDeclareLocal) ma TRUE/confronti/chiamate
                // BIT lasciano sullo stack un bool CLR 1/0: la convenzione BASIC è -1 per vero
                // (parità PCode/VM-C/C99). Senza questa normalizzazione `b = TRUE : PRINT b`
                // stampava " 1 " solo su IL e `b = TRUE` risultava falso. Una variabile BIT
                // (locale/globale) è già int32 e non va toccata: EmitsClrBoolValue la esclude.
                _il.Emit(OpCodes.Ldc_I4_M1);
                _il.Emit(OpCodes.Mul);
            }
            return;
        }
        // BUG REALE trovato 2026-09-07 (AccessViolationException, due FOR EACH su RANGE(...)
        // nella stessa SUB): `DIM a = RANGE(1,3)` (o REVERSE/altri builtin "generici" non
        // coperti dai case dedicati sopra in EmitStoredValue, es. AMAP/AFILTER/AREDUCE/
        // COLLIDEBATCH) emette `call IlRuntime.Range` che ritorna `object` a livello di
        // firma CLR (il tipo reale dipende dall'elemento, risolto solo a runtime) --
        // ARRIVAVA QUI senza mai un castclass, e lo `stloc` nel local del vero tipo array
        // (es. double[], gia' dichiarato correttamente da RegisterDynamicArrayLocal) e'
        // IL non verificabile: funziona per caso quando l'esecuzione non tocca la memoria
        // in modo distruttivo, va in AccessViolationException (memoria corrotta) quando
        // cambia la pressione di allocazione (es. un secondo ciclo FOR EACH nella stessa
        // SUB). Il commento sopra "array/UDT..." documentava questo skip come voluto --
        // era una lacuna reale, mai esercitata prima da un caso con `object` staticamente
        // dichiarato dalla firma CLR del builtin. Scoped a BuiltinFunctionCallExpr (la
        // categoria che produce `object`): un array-literal o una variabile già del tipo
        // giusto arrivano qui raramente e un castclass su un valore già corretto è comunque
        // innocuo (nessuna regressione), ma restringere il caso minimizza il raggio d'azione
        // del fix. Stesso idiom già usato per il caricamento STATIC/GLOBAL in
        // RegisterDynamicArrayLocal (Castclass dopo LoadStatic/LoadGlobal).
        if (targetType is ArrayType && value is BuiltinFunctionCallExpr objArrBce
            && ObjectArrayReturningBuiltins.Contains(objArrBce.Name.Kind))
        {
            // ACHUNK/AZIP ritornano SEMPRE object?[] a runtime (IlRuntime.Chunk/ZipArrays:
            // ogni elemento è a sua volta un sotto-array/coppia — vedi ArrayType(InferredType)
            // in StdLib.Arrays.cs, MAI riducibile a un array omogeneo). Il Castclass generico
            // sotto usa DataTypeMapper.Map(targetType), che mappa InferredType → Double e
            // quindi proverebbe un Castclass a double[] — InvalidCastException a runtime,
            // "mascherato" da ILOptim in un semplice store di null (pop+ldnull nel dump IL),
            // che poi esplodeva più avanti come NullReferenceException al primo uso
            // (UBOUND/stampa/ecc.): bug reale, ArraysUncoveredCoverage.bas sezioni
            // ACHUNK/AZIP. Stessa correzione richiesta in RegisterDynamicArrayLocal
            // (ClrArrayType del loader, altrimenti la LETTURA successiva fallisce identico).
            _il.Emit(OpCodes.Castclass, typeof(object[]));
            return;
        }
        if (targetType is ArrayType && value is BuiltinFunctionCallExpr)
        {
            // Array target + RHS builtin: se il builtin ritorna un array di elemento DIVERSO
            // (es. `DIM pos(1) AS INTEGER` + `pos = CONSOLE.GETCURSORPOS()` → double[2]),
            // converti elemento-per-elemento (ConvertArray), non `castclass` (double[]→short[]
            // → InvalidCastException, mascherata da ILOptim in pop+ldnull → NRE al primo uso:
            // bug 2026-10-01, ConsoleSetAndGet.bas). Stesso helper del caso AMap.
            var srcArrType = NormalizeInferredType(InferType(value));
            if (srcArrType is ArrayType { ElementType: not ArrayType } srcArr
                && targetType is ArrayType { ElementType: not ArrayType } dstArr)
            {
                var srcElemClr = DataTypeMapper.Map(srcArr.ElementType, _types, _classTypes);
                var dstElemClr = DataTypeMapper.Map(dstArr.ElementType, _types, _classTypes);
                if (srcElemClr != dstElemClr)
                {
                    _il.Emit(OpCodes.Call, ArrayHelpersConvertArrayMethod.MakeGenericMethod(srcElemClr, dstElemClr));
                    return;
                }
            }
            _il.Emit(OpCodes.Castclass, DataTypeMapper.Map(targetType, _types, _classTypes));
            return;
        }
        var valueType = NormalizeInferredType(InferType(value));
        // Valore DINAMICO (JsonType/AnyType): a runtime lo stack ha un OBJECT boxed (una
        // lettura di campo JSON/ANY via JsonFieldGet). Il boxed va RISOLTO al tipo del
        // target: DATE → data (seriale OADate o stringa ISO/parse-abile); numerico →
        // numero (dispatch sul boxed, con fallback "atof" per stringhe). Prima per JsonType
        // non c'era alcuna conversione → un reference finiva in stloc/stfld di un campo
        // numerico (garbage o IL invalido). Bug reale: mapping JSON→UDT del report LEDGER.
        if (DataTypeMapper.Map(valueType, _types, _classTypes) == _types.Object)
        {
            if (targetType is DateType)
            {
                _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.CoerceDynamicDate), typeof(object)));
                return;
            }
            if (IlConversion.IsNumeric(targetType))
            {
                _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.CoerceDynamicNumber), typeof(object)));
                var dynClrD = DataTypeMapper.Map(targetType, _types, _classTypes);
                IlConversion.EmitConvert(_il, _types, new DoubleType(), _types.Double, dynClrD);
                return;
            }
        }
        // Skip se target NON è numerico: LambdaExpr → FunctionType, array/UDT...
        // Non chiamiamo InferType su tipi non-numerici per evitare NotSupportedIlExpressionException.
        if (!IlConversion.IsNumeric(targetType)) return;
        // Valore DINAMICO (AnyType/object — es. il risultato di IIF/CHOOSE): il boxed può
        // essere di qualunque tipo (double, string, ...). Se il target è numerico, passa
        // da IlRuntime.ToDouble (dispatch sul boxed reale), poi conv al tipo target.
        if (valueType is AnyType && IlConversion.IsNumeric(targetType))
        {
            _il.Emit(OpCodes.Call, _types.IlRuntimeToDoubleMethod);
            var dynToClr = DataTypeMapper.Map(targetType, _types, _classTypes);
            IlConversion.EmitConvert(_il, _types, new DoubleType(), _types.Double, dynToClr);
            return;
        }
        // Data/ora (DateTime CLR) → target NUMERICO (OADate): senza, lo store salva i bit
        // grezzi della struct DateTime in un double (garbage tipo -6e-266) o stloc invalido.
        if (valueType is DateType or TimeType or DateTimeType && IlConversion.IsNumeric(targetType))
        {
            _il.Emit(OpCodes.Call, IlRuntimeToOADateMethod);
            var dynToClr = DataTypeMapper.Map(targetType, _types, _classTypes);
            IlConversion.EmitConvert(_il, _types, new DoubleType(), _types.Double, dynToClr);
            return;
        }
        if (!IlConversion.IsNumeric(valueType)) return;
        var fromClr = DataTypeMapper.Map(valueType, _types, _classTypes);
        var toClr = DataTypeMapper.Map(targetType, _types, _classTypes);
        IlConversion.EmitConvert(_il, _types, valueType, fromClr, toClr);
    }

    private void EmitCoerceTypes(DataType valueType, DataType targetType)
    {
        /* Numero/bool → STRING (letterali misti in una lista di stringhe, es. le colonne
         * TBROWSE ["Name", 20, "C", "L", TRUE, TRUE]): senza conversione si emetteva
         * List<string>.Add(double) → InvalidProgram. */
        if (targetType is StringType && valueType is not StringType)
        {
            var fromClrS = DataTypeMapper.Map(valueType, _types, _classTypes);
            if (fromClrS.IsValueType) _il.Emit(OpCodes.Box, fromClrS);
            _il.Emit(OpCodes.Call, IlRuntimeObjToStrMethod);
            return;
        }
        if (!IlConversion.IsNumeric(targetType)) return;
        // Valore DINAMICO (AnyType — CHOOSE/IIF/letture da dict ritornano un object boxed):
        // per convertirlo a un target numerico va prima unbox-to-double (IlRuntime.ToDouble
        // gestisce qualunque boxed/stringa numerica). Prima: nessuna istruzione (IsNumeric
        // fallisce su AnyType) e l'object finiva su stfld/stelem/conv di un campo numerico
        // → IL invalido ("Expected I8, but got O" su IL_00f0) poi ArrayTypeMismatchException
        // a runtime (es. Sprites(i).Color = CHOOSE(&hFFFFFFFF, ...) nel sample grafico).
        if (valueType is AnyType)
        {
            _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.ToDouble), typeof(object)));
            valueType = new DoubleType();
        }
        if (!IlConversion.IsNumeric(valueType)) return;
        var fromClr = DataTypeMapper.Map(valueType, _types, _classTypes);
        var toClr = DataTypeMapper.Map(targetType, _types, _classTypes);
        IlConversion.EmitConvert(_il, _types, valueType, fromClr, toClr);
    }

    /// <summary>
    /// Vero quando l'espressione lascia sullo stack un <c>bool</c> CLR (0/1) invece di un
    /// intero -1/0. Serve al solo store su un BIT scalare (local int32): una variabile BIT
    /// (locale o globale) è già int32 — <c>EmitLoadVariable</c>/<c>LoadGlobal</c> la
    /// restituiscono numerica — mentre letterali booleani, confronti e chiamate BIT
    /// producono il bool CLR, normalizzato per BASIC nella conversione a -1.
    /// </summary>
    private bool EmitsClrBoolValue(Expr e) => e switch
    {
        VariableExpr => false,
        // Un'assegnazione-espressione restituisce il valore già memorizzato (int32).
        AssignmentExpr => false,
        GroupingExpr g => EmitsClrBoolValue(g.Expression),
        _ => DataTypeMapper.Map(NormalizeInferredType(InferType(e)), _types, _classTypes) == _types.Boolean,
    };

    /// <summary>
    /// Coppia (ldind, stind) per la lettura/scrittura indiretta attraverso un puntatore
    /// CIL (T&amp;, tipico di un parametro BYREF) — stessa idea di ArrayElementOpCodes ma
    /// per gli opcode Ldind/Stind invece di Ldelem/Stelem.
    /// </summary>
    // NB: statico (lo usa anche EvalIndirectLocalClrType, statica). SINGLE resta R4: gli ARRAY
    // SINGLE sono float[] (ABI FFI) e il passaggio BYREF di un SINGLE scalare, quando lo scalare
    // è mappato a double (parità C), va allineato insieme a quella mappatura.
    private static (OpCode Load, OpCode Store) IndirectOpCodes(DataType type) => type switch
    {
        DoubleType => (OpCodes.Ldind_R8, OpCodes.Stind_R8),
        SingleType => (OpCodes.Ldind_R4, OpCodes.Stind_R4),
        LongType => (OpCodes.Ldind_I8, OpCodes.Stind_I8),
        ULongType => (OpCodes.Ldind_I8, OpCodes.Stind_I8),
        // INTEGER/UINTEGER = 64 bit nel dialetto (parità C, vedi DataTypeMapper): il BYREF punta
        // a un Int64/UInt64. Prima Ldind_I2/Stind_I2 su un Int64& leggeva i 2 byte bassi
        // (divergenza col valore vero).
        IntegerType or UIntegerType => (OpCodes.Ldind_I8, OpCodes.Stind_I8),
        ByteType => (OpCodes.Ldind_U1, OpCodes.Stind_I1),
        SByteType => (OpCodes.Ldind_I1, OpCodes.Stind_I1),
        BitType => (OpCodes.Ldind_I1, OpCodes.Stind_I1),
        StringType => (OpCodes.Ldind_Ref, OpCodes.Stind_Ref),
        ArrayType => (OpCodes.Ldind_Ref, OpCodes.Stind_Ref), // un array è un reference type CLR, come una stringa
        // CLASS/INTERFACE: reference type CLR (TypeBuilder) — lettura/scrittura indiretta
        // di un riferimento. NOTA: NIENTE UserDefinedType qui — un UDT è una STRUCT (value
        // type): Ldind_Ref/Stind_Ref leggerebbero 8 byte da una struct di N byte. Il load
        // di un BYREF UDT è gestito direttamente in EmitLoadVariable (Ldarg = pointer).
        ClassType or InterfaceType => (OpCodes.Ldind_Ref, OpCodes.Stind_Ref),
        _ => throw new NotSupportedIlDataTypeException(type)
    };

    /// <summary>
    /// Normalizza il placeholder pre-inferenza del Linker (variabili dichiarate
    /// implicitamente senza tipo esplicito, es. contatore FOR mai visto da un
    /// LET/DIM tipizzato) allo stesso default double usato ovunque in questo
    /// backend per literal/variabili numeriche non suffissate — vedi
    /// DataTypeMapper.Map per lo stesso ragionamento lato mappatura CLR.
    /// </summary>
    private FunctionDefinition? ResolveGenericSpecialization(FunctionExpr expr)
    {
        var name = expr.OriginalToken.Lexeme;
        var retType = expr.ReturnType;
        return _program.ResolvedSpecializations.Values.FirstOrDefault(s =>
            s.OriginalName.Lexeme.Equals(name, StringComparison.OrdinalIgnoreCase)
            && s.Parameters.Length == expr.Args.Length
            && (s.ReturnType.Equals(retType)));
    }

    private void EmitIndirectFunctionCall(FunctionExpr expr)
    {
        /* Il tipo effettivo del local può essere un delegate `ref T[]` (CbRef…) scelto
         * per una FUNCTION nominata con parametri array, diverso dal Func<…> del tipo
         * simbolo. Usa il local quando è un delegate. */
        Type delegateType;
        if (_locals.TryGetValue(expr.SymbolId, out var dloc) && typeof(Delegate).IsAssignableFrom(dloc.LocalType))
            delegateType = dloc.LocalType;
        else
            delegateType = _parameters.TryGetValue(expr.SymbolId, out var param)
                ? DataTypeMapper.Map(param.DataType, _types, _classTypes)
                : DataTypeMapper.Map(_program.Symbols.GetSymbol(expr.SymbolId).DataType, _types, _classTypes);
        var invokeMethod = delegateType.GetMethod("Invoke")
            ?? throw new NotSupportedException($"Backend IL: metodo Invoke non trovato sul delegate '{delegateType}'.");

        EmitLoadVariable(expr.SymbolId);
        var ps = invokeMethod.GetParameters();
        for (var i = 0; i < expr.Args.Length; i++)
        {
            var arg = expr.Args[i];
            if (i < ps.Length && ps[i].ParameterType.IsByRef)
            {
                /* Delegate con parametri ARRAY `ref T[]` (CbRef1D/…): l'Invoke vuole
                 * l'INDIRIZZO, non il valore (prima si passava l'array byval → il callee
                 * leggeva un riferimento non valido e l'indicizzazione falliva). */
                if (arg is VariableExpr)
                {
                    EmitAddressOf(arg);
                }
                else
                {
                    var elem = ps[i].ParameterType.GetElementType()!;
                    var tmp = _il.DeclareLocal(elem);
                    EmitExpression(arg);
                    _il.Emit(OpCodes.Stloc, tmp);
                    _il.Emit(OpCodes.Ldloca, tmp);
                }
            }
            else EmitExpression(arg);
        }
        _il.Emit(OpCodes.Callvirt, invokeMethod);
    }
}
