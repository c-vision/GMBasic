// FILE: IlBackendRuntime/FfiRefCallbackTypes.cs
// Delegate PUBBLICI per i callback FFI con FUNCTION NOMINATE (2026-11-02).
// I metodi BASIC dichiarano gli array come `T[]&` (BYREF): il delegate deve avere parametri
// `ref` per combaciare esattamente con la firma del metodo. Devono essere PUBBLICI perché è
// il codice IL generato (in un altro assembly) a costruirli con Ldftn + Newobj.
namespace Basic.IlBackend;

public static class FfiRefCallbackTypes
{
    public delegate double CbRef1D(ref double[] a);
    public delegate double CbRef1S1D(ref double[] a, double s);
    public delegate double CbRef2D(ref double[] a, ref double[] b);
    public delegate long CbRef1I(ref double[] a);
    public delegate long CbRef1S1I(ref double[] a, double s);
    public delegate long CbRef2I(ref double[] a, ref double[] b);
    public delegate double[] CbRef1ArrD(ref double[] a);
    public delegate double[] CbRef2ArrD(ref double[] a, ref double[] b);
    // 148 (2026-11-07): f(v(), x) AS ARRAY — array + scalare, ritorno array.
    public delegate double[] CbRef1S1ArrD(ref double[] a, double s);
}
