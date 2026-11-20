// FILE: CommonLib.Numerics.SpecialNative.cs
// FUNZIONI SPECIALI: le gambe MANAGED (IL/PCodeVM) chiamano il runtime C via P/Invoke.
//
// PERCHÉ. `Special.cs` implementava in C# (MathNet + serie) le stesse funzioni che
// `gmb_math.c` implementa per VM/C e C99Native: due implementazioni indipendenti. Qui il managed
// chiama la lib `gmb_special_flat` (inoltro puro alle `gmb_*` del runtime, come `gmb_matrix_flat`
// e `gmb_ts_flat`): una sola fonte numerica, stessi valori sulle 4 gambe.
//
// ERRORI. I kernel ritornano NaN fuori dominio (convenzione GMBasic, mai eccezione); se la lib
// manca si ha `DllNotFoundException` — nessun fallback al C#.

using System.Runtime.InteropServices;

namespace CommonLib;

internal static class SpecialNative
{
    private const string Lib = "gmb_special_flat";

    [DllImport(Lib, EntryPoint = "gmb_spf_gamma", CallingConvention = CallingConvention.Cdecl)]
    internal static extern double Gamma(double x);
    [DllImport(Lib, EntryPoint = "gmb_spf_lgamma", CallingConvention = CallingConvention.Cdecl)]
    internal static extern double LGamma(double x);
    [DllImport(Lib, EntryPoint = "gmb_spf_beta", CallingConvention = CallingConvention.Cdecl)]
    internal static extern double Beta(double a, double b);
    [DllImport(Lib, EntryPoint = "gmb_spf_erf", CallingConvention = CallingConvention.Cdecl)]
    internal static extern double Erf(double x);
    [DllImport(Lib, EntryPoint = "gmb_spf_erfc", CallingConvention = CallingConvention.Cdecl)]
    internal static extern double Erfc(double x);
    [DllImport(Lib, EntryPoint = "gmb_spf_gamma_inc", CallingConvention = CallingConvention.Cdecl)]
    internal static extern double GammaInc(double a, double x);
    [DllImport(Lib, EntryPoint = "gmb_spf_gamma_incc", CallingConvention = CallingConvention.Cdecl)]
    internal static extern double GammaIncc(double a, double x);
    [DllImport(Lib, EntryPoint = "gmb_spf_beta_inc", CallingConvention = CallingConvention.Cdecl)]
    internal static extern double BetaInc(double a, double b, double x);
    [DllImport(Lib, EntryPoint = "gmb_spf_digamma", CallingConvention = CallingConvention.Cdecl)]
    internal static extern double Digamma(double x);
    [DllImport(Lib, EntryPoint = "gmb_spf_bessel_j", CallingConvention = CallingConvention.Cdecl)]
    internal static extern double BesselJ(double v, double x);
    [DllImport(Lib, EntryPoint = "gmb_spf_bessel_y", CallingConvention = CallingConvention.Cdecl)]
    internal static extern double BesselY(double v, double x);
    [DllImport(Lib, EntryPoint = "gmb_spf_bessel_i", CallingConvention = CallingConvention.Cdecl)]
    internal static extern double BesselI(double v, double x);
    [DllImport(Lib, EntryPoint = "gmb_spf_bessel_k", CallingConvention = CallingConvention.Cdecl)]
    internal static extern double BesselK(double v, double x);
    [DllImport(Lib, EntryPoint = "gmb_spf_beta_ln", CallingConvention = CallingConvention.Cdecl)]
    internal static extern double BetaLn(double a, double b);
    [DllImport(Lib, EntryPoint = "gmb_spf_erf_inv", CallingConvention = CallingConvention.Cdecl)]
    internal static extern double ErfInv(double x);
    [DllImport(Lib, EntryPoint = "gmb_spf_erfc_inv", CallingConvention = CallingConvention.Cdecl)]
    internal static extern double ErfcInv(double x);
    [DllImport(Lib, EntryPoint = "gmb_spf_gamma_inc_inv", CallingConvention = CallingConvention.Cdecl)]
    internal static extern double GammaIncInv(double a, double y);
    [DllImport(Lib, EntryPoint = "gmb_spf_gamma_incc_inv", CallingConvention = CallingConvention.Cdecl)]
    internal static extern double GammaInccInv(double a, double y);
    [DllImport(Lib, EntryPoint = "gmb_spf_beta_inc_inv", CallingConvention = CallingConvention.Cdecl)]
    internal static extern double BetaIncInv(double a, double b, double y);
    [DllImport(Lib, EntryPoint = "gmb_spf_zeta", CallingConvention = CallingConvention.Cdecl)]
    internal static extern double Zeta(double s);
    [DllImport(Lib, EntryPoint = "gmb_spf_zeta_hurwitz", CallingConvention = CallingConvention.Cdecl)]
    internal static extern double ZetaHurwitz(double s, double a);
    [DllImport(Lib, EntryPoint = "gmb_spf_polygamma", CallingConvention = CallingConvention.Cdecl)]
    internal static extern double Polygamma(double n, double x);
    [DllImport(Lib, EntryPoint = "gmb_spf_hypergeom_2f1", CallingConvention = CallingConvention.Cdecl)]
    internal static extern double Hypergeom2F1(double a, double b, double c, double x);
    [DllImport(Lib, EntryPoint = "gmb_spf_hypergeom_1f1", CallingConvention = CallingConvention.Cdecl)]
    internal static extern double Hypergeom1F1(double a, double b, double x);
    [DllImport(Lib, EntryPoint = "gmb_spf_lambert_w", CallingConvention = CallingConvention.Cdecl)]
    internal static extern double LambertW(double x);
    [DllImport(Lib, EntryPoint = "gmb_spf_mittag_leffler", CallingConvention = CallingConvention.Cdecl)]
    internal static extern double MittagLeffler(double alpha, double beta, double x);
    [DllImport(Lib, EntryPoint = "gmb_spf_struve_h", CallingConvention = CallingConvention.Cdecl)]
    internal static extern double StruveH(double v, double x);
    [DllImport(Lib, EntryPoint = "gmb_spf_kelvin_ber", CallingConvention = CallingConvention.Cdecl)]
    internal static extern double KelvinBer(double x);
    [DllImport(Lib, EntryPoint = "gmb_spf_kelvin_bei", CallingConvention = CallingConvention.Cdecl)]
    internal static extern double KelvinBei(double x);
    [DllImport(Lib, EntryPoint = "gmb_spf_parabolic_cylinder", CallingConvention = CallingConvention.Cdecl)]
    internal static extern double ParabolicCylinder(double a, double x);
}
