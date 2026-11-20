// FILE: CommonLib.Numerics.InterpolationNative.cs
// INTERPOLAZIONI: le gambe MANAGED (IL/PCodeVM) chiamano il runtime C via P/Invoke.
//
// PERCHÉ. `Interpolation.cs` calcolava in C# (MathNet per linear/akima/pchip, barycentric per
// polynomial, sistema denso not-a-knot per la spline) le stesse interpolazioni che `gmb_math.c`
// implementa per VM/C e C99Native. Qui il managed chiama la lib `gmb_interp_flat` (inoltro puro
// ai kernel): una sola fonte numerica, stessi valori sulle 4 gambe.
//
// ERRORI. Le validazioni (ordinate crescenti, niente duplicati, lunghezze) restano nel chiamante;
// lib assente → `DllNotFoundException` (nessun fallback al C#).

using System.Runtime.InteropServices;

namespace CommonLib;

internal static class InterpolationNative
{
    private const string Lib = "gmb_interp_flat";

    [DllImport(Lib, EntryPoint = "gmb_ipf_linear", CallingConvention = CallingConvention.Cdecl)]
    internal static extern double Linear(double[] x, double[] y, int n, double targetX);
    [DllImport(Lib, EntryPoint = "gmb_ipf_step", CallingConvention = CallingConvention.Cdecl)]
    internal static extern double Step(double[] x, double[] y, int n, double targetX);
    [DllImport(Lib, EntryPoint = "gmb_ipf_polynomial", CallingConvention = CallingConvention.Cdecl)]
    internal static extern double Polynomial(double[] x, double[] y, int n, double targetX);
    [DllImport(Lib, EntryPoint = "gmb_ipf_akima", CallingConvention = CallingConvention.Cdecl)]
    internal static extern double Akima(double[] x, double[] y, int n, double targetX);
    [DllImport(Lib, EntryPoint = "gmb_ipf_pchip", CallingConvention = CallingConvention.Cdecl)]
    internal static extern double Pchip(double[] x, double[] y, int n, double targetX);
    [DllImport(Lib, EntryPoint = "gmb_ipf_cubic_spline", CallingConvention = CallingConvention.Cdecl)]
    internal static extern double CubicSpline(double[] x, double[] y, int n, double targetX);
}
