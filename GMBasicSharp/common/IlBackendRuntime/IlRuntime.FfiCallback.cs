// FILE: IlBackendRuntime/IlRuntime.FfiCallback.cs
// Callback come parametro FFI per il backend IL (2026-09-29, v1: arità 0-4, firma interna
// omogenea per kind, BYVAL — vedi AiWiki Bugs/(open) - FFI-Estensioni-Possibili-BYREF-Struct-
// Callback.md §3bis). A differenza di PCodeVM (dove un riferimento a funzione è una
// ClosureData reinterpretata da CallFnSync) qui un parametro callback arriva GIÀ come un vero
// delegate CLR (Func&lt;&gt;/Action&lt;&gt; — EmitFunctionReference in
// IlProgramCompiler.BuiltinsCore.cs costruisce il delegate reale a compile-time). Il trampolino
// nativo si limita quindi a invocare quel delegate con Delegate.DynamicInvoke — nessun
// "rientro nel loop di dispatch" necessario come su PCodeVM/VM-C. Stessi 19 tipi di delegate
// UnmanagedFunctionPointer condivisi con PCodeVM — vedi FfiCallbackTrampolineTypes.cs.

using System.Runtime.InteropServices;
using static Basic.IlBackend.FfiCallbackTrampolineTypes;

namespace Basic.IlBackend;

public static partial class IlRuntime
{
    /// <summary>Invoca un delegate BASIC reale con argomenti nativi (long/double) già pronti,
    /// convertendoli al tipo CLR ESATTO atteso da ciascun parametro del delegate (un parametro
    /// dichiarato <c>AS INTEGER</c> mappa a <c>short</c>/<c>int</c> CLR, non <c>long</c> —
    /// <see cref="Delegate.DynamicInvoke"/> non fa conversioni numeriche implicite da solo,
    /// serve convertire esplicitamente prima). Il tipo di ritorno CLR del delegate (o void per
    /// una SUB/Action) viene ignorato qui — la conversione al tipo nativo atteso dal chiamante
    /// nativo (long/double) è fatta dal chiamante (vedi ToI64/ToF64 in
    /// FfiCallbackTrampolineTypes).</summary>
    private static object? InvokeFfiCallbackDelegate(Delegate d, object?[] rawArgs)
    {
        var pars = d.Method.GetParameters();
        var converted = new object?[rawArgs.Length];
        for (var i = 0; i < rawArgs.Length; i++)
        {
            var pt = pars[i].ParameterType;
            if (pt.IsInstanceOfType(rawArgs[i])) { converted[i] = rawArgs[i]; continue; }
            /* Parametro BYREF (le FUNCTION nominate dichiarano gli array `T[]&`): si passa
             * il valore cosi' com'e' — DynamicInvoke lo lega per riferimento. */
            if (pt.IsByRef && pt.GetElementType() is { } et && et.IsInstanceOfType(rawArgs[i]))
            { converted[i] = rawArgs[i]; continue; }
            /* Un array passato dal trampolino e' double[]; una FUNCTION nominata puo' pero'
             * dichiarare il parametro come List(Of ...)/object() (stessa convenzione delle
             * closure): si costruisce la collezione del tipo atteso, elemento per elemento. */
            if (rawArgs[i] is double[] da && typeof(System.Collections.IList).IsAssignableFrom(pt))
            {
                var list = (System.Collections.IList)Activator.CreateInstance(pt)!;
                foreach (var v in da) list.Add(v);
                converted[i] = list;
                continue;
            }
            converted[i] = Convert.ChangeType(rawArgs[i], pt);
        }
        return d.DynamicInvoke(converted);
    }

    /// <summary>Scrive in `out` un risultato array di callback (double[] o IList boxed).</summary>
    /// <summary>Numero di elementi scritti in `out` (2026-11-03): il valore di ritorno del
    /// trampolino è la lunghezza scritta dalla callback che ritorna array (i chiamanti void la ignorano).</summary>
    private static double OutLength(object? r)
        => r switch { double[] a => a.Length, System.Collections.IList l => l.Count, _ => 0.0 };

    private static void WriteOutArray(IntPtr outp, object? r)
    {
        if (r is double[] a) { if (a.Length > 0) Marshal.Copy(a, 0, outp, a.Length); return; }
        if (r is System.Collections.IList l)
            for (var i = 0; i < l.Count; i++) Marshal.WriteInt64(outp, i * 8, BitConverter.DoubleToInt64Bits(Convert.ToDouble(l[i] ?? 0.0)));
    }

    private static (Delegate KeepAlive, IntPtr Ptr) BuildIlFfiCallbackTrampoline(Delegate cb, int arity, bool isInt, bool hasReturn,
        bool[]? sigKinds = null, int sigRetKind = -1)
    {
        // Firma per-slot (2026-11-11): kind MISTO e arità 0-64 via delegate dinamico.
        if (sigKinds != null && arity < 128)
            return FfiCallbackDynamicThunk.Create(sigKinds, sigRetKind, a => InvokeFfiCallbackDelegate(cb, a));
        // Callback VETTORIALE (2026-11-02): arity >= 128, descriptor = arity-128:
        // bit0-1 nArray-1, bit2-3 nScalari, bit4 ritorno ARRAY. ABI nativa:
        // (double* a0, int64_t n0[, double* a1, int64_t n1][, double s0[, double s1]][, double* out]).
        // 192+arità (2026-11-07): SOLO scalari -> array, f(x[, y[, z]]) AS ARRAY.
        // ABI nativa: (double a0[, double a1[, double a2]], double* out).
        if (arity >= 193 && arity <= 195)
        {
            var ns = arity - 192;
            if (ns == 1)
            {
                var dv = new CbS1OutD((a0, outp) => { var r = InvokeFfiCallbackDelegate(cb, [a0]); WriteOutArray(outp, r); return OutLength(r); });
                return (dv, Marshal.GetFunctionPointerForDelegate(dv));
            }
            if (ns == 2)
            {
                var dv = new CbS2OutD((a0, a1, outp) => { var r = InvokeFfiCallbackDelegate(cb, [a0, a1]); WriteOutArray(outp, r); return OutLength(r); });
                return (dv, Marshal.GetFunctionPointerForDelegate(dv));
            }
            var dv3 = new CbS3OutD((a0, a1, a2, outp) => { var r = InvokeFfiCallbackDelegate(cb, [a0, a1, a2]); WriteOutArray(outp, r); return OutLength(r); });
            return (dv3, Marshal.GetFunctionPointerForDelegate(dv3));
        }
        if (arity >= 128)
        {
            var code = arity - 128;
            var nArr = (code & 3) + 1;
            var nSca = (code >> 2) & 3;
            var arrRet = (code & 16) != 0;
            double[] CopyD(IntPtr p, long n) { var a = new double[n]; if (n > 0) Marshal.Copy(p, a, 0, (int)n); return a; }
            long[] CopyI(IntPtr p, long n) { var a = new long[n]; if (n > 0) Marshal.Copy(p, a, 0, (int)n); return a; }
            if (nArr == 1 && nSca == 0 && !arrRet)
            {
                if (isInt)
                {
                    var dv = new CbVecI((p, n) => ToI64(InvokeFfiCallbackDelegate(cb, [CopyI(p, n)])));
                    return (dv, Marshal.GetFunctionPointerForDelegate(dv));
                }
                var dv2 = new CbVecD((p, n) => ToF64(InvokeFfiCallbackDelegate(cb, [CopyD(p, n)])));
                return (dv2, Marshal.GetFunctionPointerForDelegate(dv2));
            }
            if (nArr == 1 && nSca == 1 && !arrRet && !isInt)
            {
                var dv = new CbV1S1D((p, n, s) => ToF64(InvokeFfiCallbackDelegate(cb, [CopyD(p, n), s])));
                return (dv, Marshal.GetFunctionPointerForDelegate(dv));
            }
            if (nArr == 1 && nSca == 1 && arrRet && !isInt)
            {
                /* 148 (2026-11-07): f(v(), x) AS ARRAY — array + scalare, ritorno array. */
                var dv = new CbV1S1OutD((p, n, s, outp) =>
                {
                    var r1 = InvokeFfiCallbackDelegate(cb, [CopyD(p, n), s]);
                    WriteOutArray(outp, r1);
                    return OutLength(r1);
                });
                return (dv, Marshal.GetFunctionPointerForDelegate(dv));
            }
            if (nArr == 1 && nSca == 0 && arrRet)
            {
                var dv = new CbV1OutD((p, n, outp) =>
                {
                    var r1 = InvokeFfiCallbackDelegate(cb, [CopyD(p, n)]);
                    WriteOutArray(outp, r1);
                    return OutLength(r1);
                });
                return (dv, Marshal.GetFunctionPointerForDelegate(dv));
            }
            if (nArr == 2 && nSca == 0 && !arrRet)
            {
                var dv = new CbV2D((p0, n0, p1, n1) => ToF64(InvokeFfiCallbackDelegate(cb, [CopyD(p0, n0), CopyD(p1, n1)])));
                return (dv, Marshal.GetFunctionPointerForDelegate(dv));
            }
            if (nArr == 2 && nSca == 0 && arrRet)
            {
                var dv = new CbV2OutD((p0, n0, p1, n1, outp) =>
                {
                    var r2 = InvokeFfiCallbackDelegate(cb, [CopyD(p0, n0), CopyD(p1, n1)]);
                    WriteOutArray(outp, r2);
                    return OutLength(r2);
                });
                return (dv, Marshal.GetFunctionPointerForDelegate(dv));
            }
            throw new InvalidOperationException($"callback vettoriale non supportato: array={nArr} scalari={nSca} ritornoArray={arrRet}");
        }
        Delegate d = (hasReturn, isInt, arity) switch
        {
            (true, true, 0) => new CbFnI0(() => ToI64(InvokeFfiCallbackDelegate(cb, []))),
            (true, true, 1) => new CbFnI1(a0 => ToI64(InvokeFfiCallbackDelegate(cb, [a0]))),
            (true, true, 2) => new CbFnI2((a0, a1) => ToI64(InvokeFfiCallbackDelegate(cb, [a0, a1]))),
            (true, true, 3) => new CbFnI3((a0, a1, a2) => ToI64(InvokeFfiCallbackDelegate(cb, [a0, a1, a2]))),
            (true, true, 4) => new CbFnI4((a0, a1, a2, a3) => ToI64(InvokeFfiCallbackDelegate(cb, [a0, a1, a2, a3]))),
            (true, false, 0) => new CbFnD0(() => ToF64(InvokeFfiCallbackDelegate(cb, []))),
            (true, false, 1) => new CbFnD1(a0 => ToF64(InvokeFfiCallbackDelegate(cb, [a0]))),
            (true, false, 2) => new CbFnD2((a0, a1) => ToF64(InvokeFfiCallbackDelegate(cb, [a0, a1]))),
            (true, false, 3) => new CbFnD3((a0, a1, a2) => ToF64(InvokeFfiCallbackDelegate(cb, [a0, a1, a2]))),
            (true, false, 4) => new CbFnD4((a0, a1, a2, a3) => ToF64(InvokeFfiCallbackDelegate(cb, [a0, a1, a2, a3]))),
            (false, _, 0) => new CbSub0(() => InvokeFfiCallbackDelegate(cb, [])),
            (false, true, 1) => new CbSubI1(a0 => InvokeFfiCallbackDelegate(cb, [a0])),
            (false, true, 2) => new CbSubI2((a0, a1) => InvokeFfiCallbackDelegate(cb, [a0, a1])),
            (false, true, 3) => new CbSubI3((a0, a1, a2) => InvokeFfiCallbackDelegate(cb, [a0, a1, a2])),
            (false, true, 4) => new CbSubI4((a0, a1, a2, a3) => InvokeFfiCallbackDelegate(cb, [a0, a1, a2, a3])),
            (false, false, 1) => new CbSubD1(a0 => InvokeFfiCallbackDelegate(cb, [a0])),
            (false, false, 2) => new CbSubD2((a0, a1) => InvokeFfiCallbackDelegate(cb, [a0, a1])),
            (false, false, 3) => new CbSubD3((a0, a1, a2) => InvokeFfiCallbackDelegate(cb, [a0, a1, a2])),
            (false, false, 4) => new CbSubD4((a0, a1, a2, a3) => InvokeFfiCallbackDelegate(cb, [a0, a1, a2, a3])),
            (true, true, 5) => new CbFnI5((a0, a1, a2, a3, a4) => ToI64(InvokeFfiCallbackDelegate(cb, [a0, a1, a2, a3, a4]))),
            (true, true, 6) => new CbFnI6((a0, a1, a2, a3, a4, a5) => ToI64(InvokeFfiCallbackDelegate(cb, [a0, a1, a2, a3, a4, a5]))),
            (true, true, 7) => new CbFnI7((a0, a1, a2, a3, a4, a5, a6) => ToI64(InvokeFfiCallbackDelegate(cb, [a0, a1, a2, a3, a4, a5, a6]))),
            (true, true, 8) => new CbFnI8((a0, a1, a2, a3, a4, a5, a6, a7) => ToI64(InvokeFfiCallbackDelegate(cb, [a0, a1, a2, a3, a4, a5, a6, a7]))),
            (true, false, 5) => new CbFnD5((a0, a1, a2, a3, a4) => ToF64(InvokeFfiCallbackDelegate(cb, [a0, a1, a2, a3, a4]))),
            (true, false, 6) => new CbFnD6((a0, a1, a2, a3, a4, a5) => ToF64(InvokeFfiCallbackDelegate(cb, [a0, a1, a2, a3, a4, a5]))),
            (true, false, 7) => new CbFnD7((a0, a1, a2, a3, a4, a5, a6) => ToF64(InvokeFfiCallbackDelegate(cb, [a0, a1, a2, a3, a4, a5, a6]))),
            (true, false, 8) => new CbFnD8((a0, a1, a2, a3, a4, a5, a6, a7) => ToF64(InvokeFfiCallbackDelegate(cb, [a0, a1, a2, a3, a4, a5, a6, a7]))),
            (false, true, 5) => new CbSubI5((a0, a1, a2, a3, a4) => InvokeFfiCallbackDelegate(cb, [a0, a1, a2, a3, a4])),
            (false, true, 6) => new CbSubI6((a0, a1, a2, a3, a4, a5) => InvokeFfiCallbackDelegate(cb, [a0, a1, a2, a3, a4, a5])),
            (false, true, 7) => new CbSubI7((a0, a1, a2, a3, a4, a5, a6) => InvokeFfiCallbackDelegate(cb, [a0, a1, a2, a3, a4, a5, a6])),
            (false, true, 8) => new CbSubI8((a0, a1, a2, a3, a4, a5, a6, a7) => InvokeFfiCallbackDelegate(cb, [a0, a1, a2, a3, a4, a5, a6, a7])),
            (false, false, 5) => new CbSubD5((a0, a1, a2, a3, a4) => InvokeFfiCallbackDelegate(cb, [a0, a1, a2, a3, a4])),
            (false, false, 6) => new CbSubD6((a0, a1, a2, a3, a4, a5) => InvokeFfiCallbackDelegate(cb, [a0, a1, a2, a3, a4, a5])),
            (false, false, 7) => new CbSubD7((a0, a1, a2, a3, a4, a5, a6) => InvokeFfiCallbackDelegate(cb, [a0, a1, a2, a3, a4, a5, a6])),
            (false, false, 8) => new CbSubD8((a0, a1, a2, a3, a4, a5, a6, a7) => InvokeFfiCallbackDelegate(cb, [a0, a1, a2, a3, a4, a5, a6, a7])),
            _ => throw new InvalidOperationException($"arità callback non supportata nella v1 del supporto FFI: {arity}")
        };
        return (d, Marshal.GetFunctionPointerForDelegate(d));
    }

    /// <summary>Risolve OGNI slot callback di <paramref name="args"/> (posizioni con
    /// <paramref name="callbackArity"/>[i] != 255) in un vero puntatore nativo (trampolino),
    /// sostituendo <c>args[i]</c> IN PLACE con quel puntatore (long) — da quel momento lo slot
    /// è indistinguibile da un normale argomento puntatore Int64 già risolto (array/STRING/
    /// BYREF/UDT), nessuna modifica agli overload FfiCallTyped* esistenti è necessaria.
    /// Ritorna la lista dei delegate trampolino creati: il chiamante (IL emesso da
    /// EmitFfiCall) DEVE tenerla viva (in un locale + <see cref="KeepAliveFfiCallback"/> dopo
    /// la chiamata nativa) per tutta la durata della chiamata FFI sottostante — il GC
    /// potrebbe altrimenti raccogliere un delegate il cui puntatore nativo è ancora in uso.</summary>
    public static List<Delegate> PrepareFfiCallbackArgs(object?[] args, int[] callbackArity, int callbackKindMask, int callbackHasRetMask,
        string cbSigs = "")
    {
        // Firma per-slot (2026-11-11): "slot:kinds>ret;..." — kind I/D, ritorno I/D/V.
        var sigKinds = new Dictionary<int, bool[]>();
        var sigRet = new Dictionary<int, int>();
        if (!string.IsNullOrEmpty(cbSigs))
        {
            foreach (var rec in cbSigs.Split(';'))
            {
                if (rec.Length == 0) continue;
                var colon = rec.IndexOf(':');
                if (colon <= 0) continue;
                var slot = int.Parse(rec[..colon]);
                var rest = rec[(colon + 1)..];
                var arrow = rest.IndexOf('>');
                if (arrow < 0) continue;
                var ks = rest[..arrow];
                var rk = rest[(arrow + 1)..];
                sigKinds[slot] = Array.ConvertAll(ks.ToCharArray(), ch => ch == 'I');
                sigRet[slot] = rk == "V" ? 2 : rk == "I" ? 1 : 0;
            }
        }
        var keepAlive = new List<Delegate>();
        for (var i = 0; i < args.Length && i < 64; i++)
        {
            if (callbackArity[i] == 255) continue;
            if (args[i] is not Delegate cb)
                throw new InvalidOperationException($"FFI: il parametro {i + 1} atteso come callback (AS FUNCTION/AS SUB) non è un riferimento a funzione valido.");
            var isInt = (callbackKindMask & (1 << i)) != 0;
            var hasRet = (callbackHasRetMask & (1 << i)) != 0;
            var (d, ptr) = BuildIlFfiCallbackTrampoline(cb, callbackArity[i], isInt, hasRet,
                sigKinds.TryGetValue(i, out var sk) ? sk : null,
                sigRet.TryGetValue(i, out var sr) ? sr : -1);
            keepAlive.Add(d);
            args[i] = ptr.ToInt64();
        }
        return keepAlive;
    }

    /// <summary>No-op esplicito — vedi <see cref="GC.KeepAlive"/>. Emesso da EmitFfiCall SUBITO
    /// dopo la chiamata FFI nativa, per impedire che il GC raccolga i delegate trampolino
    /// (<paramref name="keepAlive"/>, la lista ritornata da <see cref="PrepareFfiCallbackArgs"/>)
    /// PRIMA che la chiamata nativa sottostante sia terminata.</summary>
    public static void KeepAliveFfiCallback(object keepAlive) => GC.KeepAlive(keepAlive);
}
