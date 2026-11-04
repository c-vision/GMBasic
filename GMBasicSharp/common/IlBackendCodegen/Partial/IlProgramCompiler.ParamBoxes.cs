// FILE: IlBackendCodegen/Partial/IlProgramCompiler.ParamBoxes.cs
// PARTIAL — CATTURE PER RIFERIMENTO DI PARAMETRI di procedura.
//
// Una closure cattura la VARIABILE, non il valore: VM/C e C99Native mettono ogni
// locale/parametro catturato in una cella heap condivisa. Per i LOCALI il backend IL usa
// gia' ByRefBox<T> (vedi EmitLambdaCreation/BoxedSymbols); per i PARAMETRI di procedura
// mancava: il campo della closure era un ByRefBox<T> ma il "box sorgente" non esisteva
// (il parametro e' un argomento CIL, non un local) → NotSupportedException oppure, prima
// del box, uno SNAPSHOT del valore (una riassegnazione del parametro dopo la creazione
// della closure non si vedeva: `Mk(p): f={||p}: p=99: EVAL(f)` → 5 invece di 99).
//
// Qui il parametro catturato viene MATERIALIZZATO in un ByRefBox<T> all'ingresso del
// metodo (ldarg + newobj ByRefBox<T>) e il simbolo passa da "_parameters" a "_locals":
// da quel momento letture/scritture/EVAL/BYREF passano dal box, e la closure ne condivide
// l'identita' (parita' con le gambe C). Solo parametri BYVAL (quelli BYREF hanno gia' il
// percorso Fase 4 con ParameterInfo.IsBox) e solo fuori dalle lambda (il parametro di una
// lambda ha ABI delegate: i numerici viaggiano come double).

using System.Collections.Immutable;
using System.Reflection.Emit;
using Basic.Runtime.Gpm;

namespace Basic.IlBackend;

public sealed partial class IlProgramCompiler
{
    /// <summary>
    /// Materializza in un <c>ByRefBox&lt;T&gt;</c> ogni parametro BYVAL della procedura
    /// corrente che una lambda del corpo cattura. Va invocata PRIMA di emettere gli
    /// statement (il box deve esistere da subito: qualunque uso del parametro passa da lì).
    /// </summary>
    private void BoxCapturedParameters(ImmutableArray<Stmt> body)
    {
        if (_currentReturnSymbolId < 0 || _parameters.Count == 0) return;
        var lambdas = new List<LambdaExpr>();
        CollectLambdaExpressions(body, lambdas);
        if (lambdas.Count == 0) return;
        var captured = lambdas.SelectMany(CollectLambdaCaptures).ToHashSet();
        foreach (var symbolId in captured.ToList())
        {
            if (!_parameters.TryGetValue(symbolId, out var param)) continue;
            if (param.IsByRef || param.IsBox) continue;      // percorso Fase 4 (byref/box)
            if (_program.Symbols.GetSymbol(symbolId).DataType is ArrayType) continue; // ref-type: nessun box
            var clr = param.DataType is BitType
                ? _types.Int32
                : MapProcedureType(param.DataType, _types, _classTypes);
            var boxType = typeof(ByRefBox<>).MakeGenericType(clr);
            var boxCtor = boxType.GetConstructor([clr]);
            if (boxCtor is null) continue;
            var boxLocal = _il.DeclareLocal(boxType);
            _il.Emit(OpCodes.Ldarg, param.ArgIndex);
            _il.Emit(OpCodes.Newobj, boxCtor);
            _il.Emit(OpCodes.Stloc, boxLocal);
            _locals[symbolId] = boxLocal;
            _parameters.Remove(symbolId);
        }
    }
}
