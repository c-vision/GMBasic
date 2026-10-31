// FILE: IlProgramCompiler.EvalLocals.cs
// PARTIAL — snapshot/restore delle LOCALI di procedura attorno a EVAL/macro (S_eval Gap B).
// Il backend IL tiene le locali in LocalBuilder CLR (nessuno store runtime): prima di
// chiamare IlRuntime.MacroEval*WithLocals si fotografano TUTTE le locali/parametri/array
// della procedura corrente in object?[] (la stringa EVAL è dinamica: il compilatore non
// può sapere quali nomi referenzia) e dopo la chiamata si ricaricano (write-back).
// Riuso simmetrico di EmitLoadVariable/EmitStoreVariable: mai toccati i loro percorsi.
// Costo solo ai siti EVAL (raro), zero a regime. Limiti: campi closure catturati esclusi
// (fase 2, come upvalue altrove); scritture incompatibili lanciano InvalidCastException
// esplicito (mai corruzione silenziosa).

using System;
using System.Collections.Generic;
using System.Reflection;
using System.Reflection.Emit;
using Basic;
using Basic.Runtime.Gpm;

namespace Basic.IlBackend;

public sealed partial class IlProgramCompiler
{
    private enum EvalLocalKind { Plain, Boxed, ParamVal, ParamBox, ParamByRef, Array }

    private sealed record EvalLocalEntry(int SymbolId, string Name, EvalLocalKind Kind,
        LocalBuilder? Local, ParameterInfo? Param, Type ClrType, Type? InnerType);

    private static string EvalStripSuffix(string s)
    {
        if (string.IsNullOrEmpty(s)) return s;
        var last = s[^1];
        if (last is '$' or '%' or '#' or '!' or '&' or '@') return s[..^1];
        return s;
    }

    /// <summary>Raccoglie le variabili di scope-procedura visibili all'EVAL: SOLO IsLocal /
    /// IsParameter, MAI globali/statiche/costanti (quelle restano sul percorso bag, già
    /// funzionante — un backing local UDT-globale omonimo NON deve ombreggiarlo qui).
    /// `_locals`/`_parameters`/`_arrays` sono fresh per istanza = per procedura.</summary>
    private List<EvalLocalEntry> CollectEvalLocals()
    {
        var list = new List<EvalLocalEntry>();
        var seen = new HashSet<int>();
        foreach (var (sid, lb) in _locals)
        {
            var sym = _program.Symbols.GetSymbol(sid);
            var f = sym.Flags;
            if (f.HasFlag(SymbolFlags.IsGlobal) || f.HasFlag(SymbolFlags.IsStaticStorage) || f.HasFlag(SymbolFlags.IsConst)) continue;
            if (!f.HasFlag(SymbolFlags.IsLocal) && !f.HasFlag(SymbolFlags.IsParameter)) continue;
            var kind = EvalLocalKind.Plain;
            Type? inner = null;
            if (lb.LocalType.IsGenericType && IsByRefBoxFieldType(lb.LocalType))
            {
                kind = EvalLocalKind.Boxed;
                inner = lb.LocalType.GetGenericArguments()[0];
            }
            list.Add(new EvalLocalEntry(sid, EvalStripSuffix(_program.Symbols.GetSymbol(sid).OriginalName),
                kind, lb, null, lb.LocalType, inner));
            seen.Add(sid);
        }
        foreach (var (sid, p) in _parameters)
        {
            if (!seen.Add(sid)) continue;
            var kind = p.IsBox ? EvalLocalKind.ParamBox
                : p.IsByRef ? EvalLocalKind.ParamByRef : EvalLocalKind.ParamVal;
            var t = MapProcedureType(p.DataType, _types, _classTypes);
            Type boxType = t;
            Type? inner = null;
            if (p.IsBox || p.IsByRef)
            {
                if (p.DataType is BitType) t = _types.Int32;
                if (p.IsBox) { boxType = typeof(ByRefBox<>).MakeGenericType(t); inner = t; }
            }
            list.Add(new EvalLocalEntry(sid, EvalStripSuffix(_program.Symbols.GetSymbol(sid).OriginalName),
                kind, null, p, kind == EvalLocalKind.ParamBox ? boxType : t, inner));
        }
        foreach (var (sid, ai) in _arrays)
        {
            if (!seen.Add(sid)) continue;
            var f = _program.Symbols.GetSymbol(sid).Flags;
            if (f.HasFlag(SymbolFlags.IsGlobal) || f.HasFlag(SymbolFlags.IsStaticStorage) || f.HasFlag(SymbolFlags.IsConst)) continue;
            if (!f.HasFlag(SymbolFlags.IsLocal) && !f.HasFlag(SymbolFlags.IsParameter)) continue;
            list.Add(new EvalLocalEntry(sid, EvalStripSuffix(_program.Symbols.GetSymbol(sid).OriginalName),
                EvalLocalKind.Array, ai.Local, null, ai.ClrArrayType, null));
        }
        return list;
    }

    /// <summary>Tipo CLR effettivo del valore caricato per lo snapshot (dopo Ldind/Ldfld):
    /// serve a decidere Box/Unbox. Per i byref segue l'opcode di indirezione (I1/I2/U1/U2
    /// spingono int32 nativo).</summary>
    private static Type EvalLoadedType(EvalLocalEntry e)
    {
        return e.Kind switch
        {
            EvalLocalKind.Boxed => e.InnerType!,
            EvalLocalKind.ParamBox => e.InnerType!,
            EvalLocalKind.ParamByRef => IndirectOpCodes(e.Param!.DataType).Load == OpCodes.Ldind_Ref
                ? typeof(object) : EvalIndirectStackType(e.Param.DataType),
            EvalLocalKind.Array => e.ClrType,
            _ => e.ClrType,
        };
    }

    private static Type EvalIndirectStackType(DataType dt) => dt switch
    {
        SingleType => typeof(float),
        DoubleType => typeof(double),
        LongType => typeof(long),
        ULongType => typeof(ulong),
        _ => typeof(int),
    };

    private void EmitEvalBoxIfNeeded(Type t)
    {
        if (t.IsValueType) _il.Emit(OpCodes.Box, t);
    }

    private void EmitEvalUnbox(Type t)
    {
        // S_eval Gap B: prima coerce (77.0 double in locale Int16), poi unbox esatto.
        // Senza coerce, qualunque scrittura EVAL con tipo runtime diverso abortirebbe
        // in InvalidCastException (semantica BASIC = coerenza, mai abort).
        _il.Emit(OpCodes.Ldtoken, t);
        _il.Emit(OpCodes.Call, GetTypeFromHandleMethod);
        _il.Emit(OpCodes.Call, IlRuntimeCoerceEvalLocalMethod);
        if (t.IsValueType) _il.Emit(OpCodes.Unbox_Any, t);
        else if (t != typeof(object)) _il.Emit(OpCodes.Castclass, t);
    }

    /// <summary>Carica il valore corrente della variabile come object (per values[]).</summary>
    private void EmitEvalLocalLoad(EvalLocalEntry e)
    {
        switch (e.Kind)
        {
            case EvalLocalKind.Plain:
                _il.Emit(OpCodes.Ldloc, e.Local!);
                EmitEvalBoxIfNeeded(e.ClrType);
                break;
            case EvalLocalKind.Boxed:
                _il.Emit(OpCodes.Ldloc, e.Local!);
                _il.Emit(OpCodes.Ldfld, e.ClrType.GetField("Value")!);
                EmitEvalBoxIfNeeded(e.InnerType!);
                break;
            case EvalLocalKind.ParamVal:
                _il.Emit(OpCodes.Ldarg, e.Param!.ArgIndex);
                if (e.Param.IsByRef) _il.Emit(IndirectOpCodes(e.Param.DataType).Load);
                EmitEvalBoxIfNeeded(EvalLoadedType(e));
                break;
            case EvalLocalKind.ParamBox:
                _il.Emit(OpCodes.Ldarg, e.Param!.ArgIndex);
                _il.Emit(OpCodes.Ldfld, e.ClrType.GetField("Value")!);
                EmitEvalBoxIfNeeded(e.InnerType!);
                break;
            case EvalLocalKind.ParamByRef:
                _il.Emit(OpCodes.Ldarg, e.Param!.ArgIndex);
                _il.Emit(IndirectOpCodes(e.Param.DataType).Load);
                EmitEvalBoxIfNeeded(EvalLoadedType(e));
                break;
            case EvalLocalKind.Array:
                EmitArrayRefFromInfo(_arrays[e.SymbolId]);
                break;
        }
    }

    /// <summary>Riscrive values[i] nella home (specchio del load: box/indirizzo preservati).</summary>
    private void EmitEvalLocalStore(EvalLocalEntry e, LocalBuilder valuesArr, int index)
    {
        switch (e.Kind)
        {
            case EvalLocalKind.Plain:
                _il.Emit(OpCodes.Ldloc, valuesArr);
                _il.Emit(OpCodes.Ldc_I4, index);
                _il.Emit(OpCodes.Ldelem_Ref);
                EmitEvalUnbox(e.ClrType);
                _il.Emit(OpCodes.Stloc, e.Local!);
                break;
            case EvalLocalKind.Boxed:
                _il.Emit(OpCodes.Ldloc, e.Local!);
                _il.Emit(OpCodes.Ldloc, valuesArr);
                _il.Emit(OpCodes.Ldc_I4, index);
                _il.Emit(OpCodes.Ldelem_Ref);
                EmitEvalUnbox(e.InnerType!);
                _il.Emit(OpCodes.Stfld, e.ClrType.GetField("Value")!);
                break;
            case EvalLocalKind.ParamVal:
                if (e.Param!.IsByRef)
                {
                    _il.Emit(OpCodes.Ldarg, e.Param.ArgIndex);
                    _il.Emit(OpCodes.Ldloc, valuesArr);
                    _il.Emit(OpCodes.Ldc_I4, index);
                    _il.Emit(OpCodes.Ldelem_Ref);
                    EmitEvalUnbox(EvalLoadedType(e));
                    _il.Emit(IndirectOpCodes(e.Param.DataType).Store);
                }
                else
                {
                    _il.Emit(OpCodes.Ldloc, valuesArr);
                    _il.Emit(OpCodes.Ldc_I4, index);
                    _il.Emit(OpCodes.Ldelem_Ref);
                    EmitEvalUnbox(e.ClrType);
                    _il.Emit(OpCodes.Starg, e.Param.ArgIndex);
                }
                break;
            case EvalLocalKind.ParamBox:
                _il.Emit(OpCodes.Ldarg, e.Param!.ArgIndex);
                _il.Emit(OpCodes.Ldloc, valuesArr);
                _il.Emit(OpCodes.Ldc_I4, index);
                _il.Emit(OpCodes.Ldelem_Ref);
                EmitEvalUnbox(e.InnerType!);
                _il.Emit(OpCodes.Stfld, e.ClrType.GetField("Value")!);
                break;
            case EvalLocalKind.ParamByRef:
                _il.Emit(OpCodes.Ldarg, e.Param!.ArgIndex);
                _il.Emit(OpCodes.Ldloc, valuesArr);
                _il.Emit(OpCodes.Ldc_I4, index);
                _il.Emit(OpCodes.Ldelem_Ref);
                EmitEvalUnbox(EvalLoadedType(e));
                _il.Emit(IndirectOpCodes(e.Param.DataType).Store);
                break;
            case EvalLocalKind.Array:
                _il.Emit(OpCodes.Ldloc, valuesArr);
                _il.Emit(OpCodes.Ldc_I4, index);
                _il.Emit(OpCodes.Ldelem_Ref);
                EmitEvalUnbox(e.ClrType);
                _il.Emit(OpCodes.Stloc, e.Local!);
                break;
        }
    }

    /// <summary>Emette la chiamata EVAL con snapshot/restore locali. Stack in ingresso:
    /// la stringa-codice (già valutata dal chiamante come oggi). Stack in uscita: il
    /// risultato object (l'expression-form lo consuma, la statement-form fa Pop).</summary>
    private void EmitMacroCallWithLocals(MethodInfo target)
    {
        var entries = CollectEvalLocals();
        var codeTemp = _il.DeclareLocal(typeof(string));
        _il.Emit(OpCodes.Stloc, codeTemp);
        var namesTemp = _il.DeclareLocal(typeof(string[]));
        _il.Emit(OpCodes.Ldc_I4, entries.Count);
        _il.Emit(OpCodes.Newarr, typeof(string));
        _il.Emit(OpCodes.Stloc, namesTemp);
        for (var i = 0; i < entries.Count; i++)
        {
            _il.Emit(OpCodes.Ldloc, namesTemp);
            _il.Emit(OpCodes.Ldc_I4, i);
            _il.Emit(OpCodes.Ldstr, entries[i].Name);
            _il.Emit(OpCodes.Stelem_Ref);
        }
        var valuesTemp = _il.DeclareLocal(typeof(object));
        _il.Emit(OpCodes.Ldc_I4, entries.Count);
        _il.Emit(OpCodes.Newarr, typeof(object));
        _il.Emit(OpCodes.Stloc, valuesTemp);
        for (var i = 0; i < entries.Count; i++)
        {
            _il.Emit(OpCodes.Ldloc, valuesTemp);
            _il.Emit(OpCodes.Ldc_I4, i);
            EmitEvalLocalLoad(entries[i]);
            _il.Emit(OpCodes.Stelem_Ref);
        }
        _il.Emit(OpCodes.Ldloc, codeTemp);
        _il.Emit(OpCodes.Ldloc, namesTemp);
        _il.Emit(OpCodes.Ldloc, valuesTemp);
        _il.Emit(OpCodes.Call, target);
        for (var i = 0; i < entries.Count; i++)
            EmitEvalLocalStore(entries[i], valuesTemp, i);
    }
}
