// FILE: IlBackendRuntime/PCode/PCodeProcessor.FfiCallback.cs
// PARTIAL — trampolino nativo per Callback come parametro FFI (2026-09-29, v1: arità 0-4,
// firma interna omogenea per kind, BYVAL — vedi AiWiki Bugs/(open) - FFI-Estensioni-Possibili-
// BYREF-Struct-Callback.md §3bis e Compiler.Expressions.cs). Usato da OP_CALL_FFI_T
// (PCodeProcessor.cs) per convertire una ClosureData BASIC in un puntatore nativo C-ABI reale
// prima di passarla a una funzione FFI che si aspetta un function pointer (es. qsort-style
// comparator/callback). I 19 tipi di delegate taggati UnmanagedFunctionPointer sono CONDIVISI
// con il backend IL (IlRuntime.FfiCallback.cs) — vedi FfiCallbackTrampolineTypes.cs.

using Basic.Runtime.Gpm;
using System.Runtime.InteropServices;
using static Basic.IlBackend.FfiCallbackTrampolineTypes;

namespace Basic.IlBackend.PCode;

internal sealed partial class PCodeProcessor
{
    /// <summary>
    /// Costruisce un delegate taggato UnmanagedFunctionPointer che, quando invocato da codice
    /// nativo, richiama sincronamente <c>cd</c> (via CallFnSync — stesso meccanismo già usato
    /// da AMAP/ASORT/OPTIMIZE.* per invocare una lambda BASIC, qui riusato per un rientro da
    /// NATIVO invece che da un builtin C#) e ne marshalla il risultato nel tipo nativo atteso.
    /// Il chiamante DEVE mantenere vivo (GC-rooted) il delegate restituito per tutta la durata
    /// della chiamata nativa sottostante (GC.KeepAlive) — il puntatore nativo da solo non basta
    /// a impedire che il GC raccolga il delegate.
    /// </summary>
    private (Delegate KeepAlive, IntPtr Ptr) BuildFfiCallbackTrampoline(PCodeTask task, ClosureData cd, int arity, bool isInt, bool hasReturn)
    {
        Delegate d = (hasReturn, isInt, arity) switch
        {
            (true, true, 0) => new CbFnI0(() => ToI64(CallFnSync(task, cd))),
            (true, true, 1) => new CbFnI1(a0 => ToI64(CallFnSync(task, cd, a0))),
            (true, true, 2) => new CbFnI2((a0, a1) => ToI64(CallFnSync(task, cd, a0, a1))),
            (true, true, 3) => new CbFnI3((a0, a1, a2) => ToI64(CallFnSync(task, cd, a0, a1, a2))),
            (true, true, 4) => new CbFnI4((a0, a1, a2, a3) => ToI64(CallFnSync(task, cd, a0, a1, a2, a3))),
            (true, false, 0) => new CbFnD0(() => ToF64(CallFnSync(task, cd))),
            (true, false, 1) => new CbFnD1(a0 => ToF64(CallFnSync(task, cd, a0))),
            (true, false, 2) => new CbFnD2((a0, a1) => ToF64(CallFnSync(task, cd, a0, a1))),
            (true, false, 3) => new CbFnD3((a0, a1, a2) => ToF64(CallFnSync(task, cd, a0, a1, a2))),
            (true, false, 4) => new CbFnD4((a0, a1, a2, a3) => ToF64(CallFnSync(task, cd, a0, a1, a2, a3))),
            (false, _, 0) => new CbSub0(() => { CallFnSync(task, cd); }),
            (false, true, 1) => new CbSubI1(a0 => { CallFnSync(task, cd, a0); }),
            (false, true, 2) => new CbSubI2((a0, a1) => { CallFnSync(task, cd, a0, a1); }),
            (false, true, 3) => new CbSubI3((a0, a1, a2) => { CallFnSync(task, cd, a0, a1, a2); }),
            (false, true, 4) => new CbSubI4((a0, a1, a2, a3) => { CallFnSync(task, cd, a0, a1, a2, a3); }),
            (false, false, 1) => new CbSubD1(a0 => { CallFnSync(task, cd, a0); }),
            (false, false, 2) => new CbSubD2((a0, a1) => { CallFnSync(task, cd, a0, a1); }),
            (false, false, 3) => new CbSubD3((a0, a1, a2) => { CallFnSync(task, cd, a0, a1, a2); }),
            (false, false, 4) => new CbSubD4((a0, a1, a2, a3) => { CallFnSync(task, cd, a0, a1, a2, a3); }),
            _ => throw new InvalidOperationException($"arità callback non supportata nella v1 del supporto FFI: {arity}")
        };
        return (d, Marshal.GetFunctionPointerForDelegate(d));
    }
}
