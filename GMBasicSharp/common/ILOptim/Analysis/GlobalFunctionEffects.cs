namespace ILOptim.Analysis;

public class GlobalFunctionEffects : IGlobalAnalysis
{
    readonly Dictionary<MethodDef, FunctionEffects> _cache = new();
    readonly Compilation _comp;

    private GlobalFunctionEffects(Compilation comp) => _comp = comp;

    static IGlobalAnalysis IGlobalAnalysis.Create(Compilation comp)
        => new GlobalFunctionEffects(comp);
    
    public FunctionEffects GetEffects(MethodDesc method)
    {
        if (method is not MethodDefOrSpec def) {
            return FunctionEffects.Unknown;
        }

        var target = def.Definition;

        // Placeholder (default = Pure) inserito PRIMA di ricorrere: spezza i cicli di
        // ricorsione indiretta (A chiama B, B chiama A) — se la chiave esiste già
        // (ricorsione ciclica in corso, o valore già calcolato), esce subito con
        // quello che c'è.
        if (!_cache.TryAdd(target, default)) {
            return _cache[target];
        }

        // NB: NON tenere un `ref` al valore in cache attraverso questa chiamata.
        // ComputeEffects ricorre in GetEffects per ogni CallInst nel corpo (via
        // ComputeEffectsFromBody), il che può inserire NUOVE chiavi nello STESSO
        // _cache e quindi farlo ridimensionare (rehash) internamente. La versione
        // precedente usava `ref var entry = ref _cache.GetOrAddRef(...)` e scriveva
        // `entry = ComputeEffects(...)` DOPO la ricorsione: se nel frattempo il
        // dizionario si era ridimensionato, quel ref puntava ormai a un vecchio array
        // di backing abbandonato — la scrittura del valore calcolato andava persa e lo
        // slot REALMENTE vivo restava al placeholder default(FunctionEffects) =
        // {None,None} = IsPure TRUE. Falso positivo di purezza: DeadCodeElim
        // eliminava chiamate con side-effect reali (bug osservato: `CALL Foo` a una
        // SUB void spariva silenziosamente non appena InlineMethods la appiattiva nel
        // metodo chiamante — nessuna eccezione, solo output perso). Fix: calcola in
        // una variabile locale, poi scrivi con l'indicizzatore (rilegge/riscrive lo
        // slot vivo, qualunque sia il backing array corrente).
        var computed = ComputeEffects(target);
        _cache[target] = computed;
        return computed;
    }

    public bool SetDirty(MethodDef method) => _cache.Remove(method);
    public void SetAllDirty()
    {
        _cache.Clear();
    }

    private FunctionEffects ComputeEffects(MethodDef method)
    {
        if (method.DeclaringType.IsCorelibType() && GetEffectsFromCorelibMethod(method) is { } effects) {
            return effects;
        }
        if (method.Body != null && (method.Module == _comp.Module || _comp.Settings.AllowCrossAssemblyIPO)) {
            return ComputeEffectsFromBody(method.Body);
        }
        // We can't deduce anything about this method, assume the worst.
        return FunctionEffects.Unknown;
    }

    // TODO: compute info for inlining heuristics
    private FunctionEffects ComputeEffectsFromBody(MethodBody body)
    {
        var memEffects = MemoryEffects.None;
        var traits = FunctionTraits.None;
        int numReturns = 0, numThrows = 0;

        foreach (var block in body) {
            foreach (var inst in block.NonPhis()) {
                switch (inst) {
                    case CallInst { Method: MethodDefOrSpec target } call: {
                        if (target.Definition != body.Definition) {
                            var callEffects = GetEffects(target.Definition);
                            memEffects |= callEffects.MemEffects;
                            traits |= callEffects.Traits & FunctionTraits.CallTransferMask;
                        } else {
                            traits |= FunctionTraits.Recursive;
                        }
                        break;
                    }
                    default: {
                        if (inst.MayReadFromMemory) memEffects |= MemoryEffects.Read;
                        if (inst.MayWriteToMemory) memEffects |= MemoryEffects.Write;
                        if (inst.MayThrow) traits |= FunctionTraits.MayThrow;
                        break;
                    }
                }
            }

            switch (block.Last) {
                case ReturnInst: numReturns++; break;
                case ThrowInst: numThrows++; break;
            }
        }

        if (numReturns == 0 && numThrows > 0) {
            traits |= FunctionTraits.DoesNotReturn;
        }
        
        return new FunctionEffects() {
            MemEffects = memEffects,
            Traits = traits,
        };
    }


    /*private FunctionEffects ComputeEffectsFromIL(ILMethodBody body)
    {
        var memEffects = MemoryEffects.None;
        var traits = FunctionTraits.MayThrow;

        foreach (var inst in body.Instructions) {
            switch (inst.OpCode) {
                case ILCode.Ldfld or ILCode.Ldsfld or ILCode.Ldflda:
                case ILCode.Ldind_I or (>= ILCode.Ldind_I1 and <= ILCode.Ldind_Ref):
                case ILCode.Ldelem or (>= ILCode.Ldelem_I1 and <= ILCode.Ldelem_Ref):
                case ILCode.Ldobj:
                case ILCode.Unbox or ILCode.Unbox_Any: {
                    memEffects |= MemoryEffects.Read;
                    break;
                }
                case ILCode.Stfld or ILCode.Stsfld: 
                case ILCode.Stind_I or (>= ILCode.Stind_Ref and <= ILCode.Stind_R8): 
                case ILCode.Stelem or (>= ILCode.Stelem_I and <= ILCode.Stelem_Ref):
                case ILCode.Stobj: {
                    memEffects |= MemoryEffects.Write;
                    break;
                }
                case ILCode.Localloc: {
                    traits |= FunctionTraits.HasStackAllocs | FunctionTraits.MayThrow;
                    break;
                }
            }
        }

        return new FunctionEffects() {
            MemEffects = memEffects,
            Traits = traits
        };
    }*/

    // TODO: consider moving this to a more easily editable config file or something
    private static FunctionEffects? GetEffectsFromCorelibMethod(MethodDef method)
    {
        var type = method.DeclaringType;

        if (type.Namespace == "System.Collections.Generic" && type.Name is "Dictionary`2" or "List`1" or "HashSet`1") {
            bool isAccessor = method.Name is "get_Item" or "get_Count" or "ContainsKey";

            return new FunctionEffects(
                isAccessor ? MemoryEffects.Read : MemoryEffects.ReadWrite,
                FunctionTraits.MayThrow
            );
        }

        switch (method.DeclaringType.Name, method.Name) {
            // Pure functions
            case ("String", "op_Equality" or "op_Inequality" or "get_Length"):
            case ("Object", "GetType"):
            case ("Type", "op_Equality" or "op_Inequality" or "GetTypeFromHandle"):
            case ("RuntimeHelpers", "IsReferenceOrContainsReferences"):
            case ("Environment", "get_CurrentManagedThreadId"):
            case ("Math" or "MathF", _) when IsPureMathFunc(method):
            case ("Vector128`1" or "Vector256`1" or "Vector512`1", _) when method.Name.StartsWith("op_"): {
                return new FunctionEffects(MemoryEffects.None, FunctionTraits.None);
            }
            // Throwing functions (some of these are actually pure, but it doesn't matter much)
            case ("String", "IndexOf" or "LastIndexOf" or "Contains" or "Substring" or "Replace" or
                            "ToLower" or "ToUpper" or "ToLowerInvariant" or "ToUpperInvariant"
            ): {
                return new FunctionEffects(MemoryEffects.None, FunctionTraits.MayThrow);
            }
            // TODO: Include MemoryExtensions and span stuff
            //       also check if GVN can handle things like "span.IndexOf(1); span[0] = 0; span.IndexOf(1)" properly.
        }
        return null;
    }

    private static bool IsPureMathFunc(MethodDef method)
    {
        if (method.Params.All(p => p.Type.StackType == StackType.Float)) {
            ReadOnlySpan<string> names = [
                "Floor", "Ceiling", "Round", "Truncate",
                "Abs", "Sqrt", "Cbrt", "Log", "Log2", "Log10", "Exp",
                "Sin", "Cos", "Tan", "SinCos",
                "Sinh", "Cosh", "Tanh",
                "Asin", "Acos", "Atan",
                "Sinh", "Cosh", "Atanh",
                "ReciprocalEstimate", "ReciprocalSqrtEstimate",

                "Pow", "Log", "Atan2",
                "Min", "Max", "MaxMagnitude", "MinMagnitude",
                "FusedMultiplyAdd",
            ];
            return names.Contains(method.Name);
        }
        return false;
    }
}

public readonly struct FunctionEffects
{
    public static FunctionEffects Unknown => new(MemoryEffects.ReadWrite, FunctionTraits.Unknown);

    public MemoryEffects MemEffects { get; init; }
    public FunctionTraits Traits { get; init; }

    public bool IsUnknown => Traits == FunctionTraits.Unknown;
    public bool IsPure => Traits == FunctionTraits.None && MemEffects == MemoryEffects.None;

    public bool MayThrow => (Traits & FunctionTraits.MayThrow) != 0;
    public bool MayReadMem => (MemEffects & MemoryEffects.Read) != 0;
    public bool MayWriteMem => (MemEffects & MemoryEffects.Write) != 0;
    public bool MayOnlyThrowOrReadMem => (Traits & ~FunctionTraits.MayThrow) == 0 && (MemEffects & ~MemoryEffects.Read) == 0;

    public FunctionEffects(MemoryEffects memEffects, FunctionTraits traits)
    {
        MemEffects = memEffects;
        Traits = traits;
    }

    public override string ToString() => $"Mem={MemEffects}, Traits={Traits}";
}
[Flags]
public enum FunctionTraits
{
    None = 0,
    Unknown         = ~0,
    CallTransferMask = MayThrow,

    MayThrow        = 1 << 0,
    Recursive       = 1 << 1,
    DoesNotReturn   = 1 << 3, // e.g. ThrowHelper

}
public enum MemoryEffects : byte
{
    /// <summary> Memory is neither read nor written. </summary>
    None = 0,

    /// <summary> Memory may be read. </summary>
    Read = 1 << 0,

    /// <summary> Memory may be written. </summary>
    Write = 1 << 1,

    /// <summary> Memory may be both read and written. </summary>
    ReadWrite = Read | Write,
}