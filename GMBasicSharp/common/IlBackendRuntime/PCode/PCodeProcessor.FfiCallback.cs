// FILE: IlBackendRuntime/PCode/PCodeProcessor.FfiCallback.cs
// PARTIAL — trampolino nativo per Callback come parametro FFI (2026-09-29; arità 0-8 dal
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
        // Callback VETTORIALE (2026-11-02): descriptor = arity-128 (array/scalari/ritorno array).
        if (arity >= 128)
        {
            var code = arity - 128;
            var nArr = (code & 3) + 1;
            var nSca = (code >> 2) & 3;
            var arrRet = (code & 16) != 0;
            List<object?> Ld(IntPtr p, long n)
            {
                var l = new List<object?>((int)Math.Max(0, n));
                for (long i = 0; i < n; i++) l.Add(BitConverter.Int64BitsToDouble(Marshal.ReadInt64(p, (int)(i * 8))));
                return l;
            }
            List<object?> Li(IntPtr p, long n)
            {
                var l = new List<object?>((int)Math.Max(0, n));
                for (long i = 0; i < n; i++) l.Add(Marshal.ReadInt64(p, (int)(i * 8)));
                return l;
            }
            double OutCount(object? r)
                => r is System.Collections.IList l2 ? l2.Count : 0.0;
            void WriteOut(IntPtr outp, object? r)
            {
                if (r is not System.Collections.IList l) return;
                for (var i = 0; i < l.Count; i++)
                    Marshal.WriteInt64(outp, i * 8, BitConverter.DoubleToInt64Bits(Convert.ToDouble(l[i] ?? 0.0)));
            }
            // 192+arità (2026-11-07): SOLO scalari -> array, f(x[, y[, z]]) AS ARRAY.
            if (arity >= 193 && arity <= 195)
            {
                var ns = arity - 192;
                if (ns == 1)
                {
                    var dv = new CbS1OutD((a0, outp) => { var r = CallFnSync(task, cd, a0); WriteOut(outp, r); return OutCount(r); });
                    return (dv, Marshal.GetFunctionPointerForDelegate(dv));
                }
                if (ns == 2)
                {
                    var dv = new CbS2OutD((a0, a1, outp) => { var r = CallFnSync(task, cd, a0, a1); WriteOut(outp, r); return OutCount(r); });
                    return (dv, Marshal.GetFunctionPointerForDelegate(dv));
                }
                var dv3 = new CbS3OutD((a0, a1, a2, outp) => { var r = CallFnSync(task, cd, a0, a1, a2); WriteOut(outp, r); return OutCount(r); });
                return (dv3, Marshal.GetFunctionPointerForDelegate(dv3));
            }
            if (nArr == 1 && nSca == 0 && !arrRet)
            {
                if (isInt)
                {
                    var dv = new CbVecI((p, n) => ToI64(CallFnSync(task, cd, Li(p, n))));
                    return (dv, Marshal.GetFunctionPointerForDelegate(dv));
                }
                var dv2 = new CbVecD((p, n) => ToF64(CallFnSync(task, cd, Ld(p, n))));
                return (dv2, Marshal.GetFunctionPointerForDelegate(dv2));
            }
            if (nArr == 1 && nSca == 1 && !arrRet && !isInt)
            {
                var dv = new CbV1S1D((p, n, s) => ToF64(CallFnSync(task, cd, Ld(p, n), s)));
                return (dv, Marshal.GetFunctionPointerForDelegate(dv));
            }
            if (nArr == 1 && nSca == 1 && arrRet && !isInt)
            {
                /* 148 (2026-11-07): f(v(), x) AS ARRAY — array + scalare, ritorno array. */
                var dv = new CbV1S1OutD((p, n, s, outp) => { var r1 = CallFnSync(task, cd, Ld(p, n), s); WriteOut(outp, r1); return OutCount(r1); });
                return (dv, Marshal.GetFunctionPointerForDelegate(dv));
            }
            if (nArr == 1 && nSca == 0 && arrRet)
            {
                var dv = new CbV1OutD((p, n, outp) => { var r1 = CallFnSync(task, cd, Ld(p, n)); WriteOut(outp, r1); return OutCount(r1); });
                return (dv, Marshal.GetFunctionPointerForDelegate(dv));
            }
            if (nArr == 2 && nSca == 0 && !arrRet)
            {
                var dv = new CbV2D((p0, n0, p1, n1) => ToF64(CallFnSync(task, cd, Ld(p0, n0), Ld(p1, n1))));
                return (dv, Marshal.GetFunctionPointerForDelegate(dv));
            }
            if (nArr == 2 && nSca == 0 && arrRet)
            {
                var dv = new CbV2OutD((p0, n0, p1, n1, outp) => { var r2 = CallFnSync(task, cd, Ld(p0, n0), Ld(p1, n1)); WriteOut(outp, r2); return OutCount(r2); });
                return (dv, Marshal.GetFunctionPointerForDelegate(dv));
            }
            throw new InvalidOperationException($"callback vettoriale non supportato: array={nArr} scalari={nSca} ritornoArray={arrRet}");
        }
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
            (true, true, 5) => new CbFnI5((a0, a1, a2, a3, a4) => ToI64(CallFnSync(task, cd, a0, a1, a2, a3, a4))),
            (true, true, 6) => new CbFnI6((a0, a1, a2, a3, a4, a5) => ToI64(CallFnSync(task, cd, a0, a1, a2, a3, a4, a5))),
            (true, true, 7) => new CbFnI7((a0, a1, a2, a3, a4, a5, a6) => ToI64(CallFnSync(task, cd, a0, a1, a2, a3, a4, a5, a6))),
            (true, true, 8) => new CbFnI8((a0, a1, a2, a3, a4, a5, a6, a7) => ToI64(CallFnSync(task, cd, a0, a1, a2, a3, a4, a5, a6, a7))),
            (true, false, 5) => new CbFnD5((a0, a1, a2, a3, a4) => ToF64(CallFnSync(task, cd, a0, a1, a2, a3, a4))),
            (true, false, 6) => new CbFnD6((a0, a1, a2, a3, a4, a5) => ToF64(CallFnSync(task, cd, a0, a1, a2, a3, a4, a5))),
            (true, false, 7) => new CbFnD7((a0, a1, a2, a3, a4, a5, a6) => ToF64(CallFnSync(task, cd, a0, a1, a2, a3, a4, a5, a6))),
            (true, false, 8) => new CbFnD8((a0, a1, a2, a3, a4, a5, a6, a7) => ToF64(CallFnSync(task, cd, a0, a1, a2, a3, a4, a5, a6, a7))),
            (false, true, 5) => new CbSubI5((a0, a1, a2, a3, a4) => { CallFnSync(task, cd, a0, a1, a2, a3, a4); }),
            (false, true, 6) => new CbSubI6((a0, a1, a2, a3, a4, a5) => { CallFnSync(task, cd, a0, a1, a2, a3, a4, a5); }),
            (false, true, 7) => new CbSubI7((a0, a1, a2, a3, a4, a5, a6) => { CallFnSync(task, cd, a0, a1, a2, a3, a4, a5, a6); }),
            (false, true, 8) => new CbSubI8((a0, a1, a2, a3, a4, a5, a6, a7) => { CallFnSync(task, cd, a0, a1, a2, a3, a4, a5, a6, a7); }),
            (false, false, 5) => new CbSubD5((a0, a1, a2, a3, a4) => { CallFnSync(task, cd, a0, a1, a2, a3, a4); }),
            (false, false, 6) => new CbSubD6((a0, a1, a2, a3, a4, a5) => { CallFnSync(task, cd, a0, a1, a2, a3, a4, a5); }),
            (false, false, 7) => new CbSubD7((a0, a1, a2, a3, a4, a5, a6) => { CallFnSync(task, cd, a0, a1, a2, a3, a4, a5, a6); }),
            (false, false, 8) => new CbSubD8((a0, a1, a2, a3, a4, a5, a6, a7) => { CallFnSync(task, cd, a0, a1, a2, a3, a4, a5, a6, a7); }),
            _ => throw new InvalidOperationException($"arità callback non supportata nella v1 del supporto FFI: {arity}")
        };
        return (d, Marshal.GetFunctionPointerForDelegate(d));
    }
}
