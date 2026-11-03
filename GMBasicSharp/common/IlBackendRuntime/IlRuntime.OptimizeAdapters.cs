// FILE: IlBackendRuntime/IlRuntime.OptimizeAdapters.cs
// Adattatori per i callback dei builtin OPTIMIZE quando l'argomento è una FUNCTION
// NOMINATA con parametri array (2026-11-03). La FUNCTION nominata produce un delegate con
// parametri `ref` (FfiRefCallbackTypes); i builtin OPTIMIZE accettano Func<> by-value.
// Questi metodi ricostruiscono la Func<> attesa, invocando il delegate `ref`.
namespace Basic.IlBackend;

public static partial class IlRuntime
{
    public static Func<double[], double> OptVecFromRef(FfiRefCallbackTypes.CbRef1D d)
        => a => { var t = a; return d(ref t); };

    public static Func<double[], double> OptVecFromRef(FfiRefCallbackTypes.CbRef1I d)
        => a => { var t = a; return d(ref t); };

    public static Func<double[], double[]> OptVecVecFromRef(FfiRefCallbackTypes.CbRef1ArrD d)
        => a => { var t = a; return d(ref t); };

    public static Func<double[], double, double> OptModelFromRef(FfiRefCallbackTypes.CbRef1S1D d)
        => (a, x) => { var t = a; return d(ref t, x); };

    public static Func<double[], double, double> OptModelFromRef(FfiRefCallbackTypes.CbRef1S1I d)
        => (a, x) => { var t = a; return d(ref t, x); };

    public static Func<double[], double[], double[]> OptVModelFromRef(FfiRefCallbackTypes.CbRef2ArrD d)
        => (a, b) => { var t = a; var u = b; return d(ref t, ref u); };
}
