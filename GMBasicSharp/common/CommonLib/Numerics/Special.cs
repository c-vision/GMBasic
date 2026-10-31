// FILE: CommonLib.Numerics.Special.cs
// Nucleo `scipy.special` per GMBasic (runtime C#): aggiunto alla classe statica
// CommonLib.Numerics (partial). Implementazione via MathNet SpecialFunctions; il
// runtime C (gmb_math.c) ha i corrispondenti gmb_gamma_p, gmb_lgamma, gmb_beta_fn,
// gmb_erf_c, gmb_incbeta per il porting C.
//
// Funzioni (parità scipy.special 1.17):
//   gamma(x)  — funzione Gamma completa Γ(x)
//   lgamma(x) — log|Γ(x)|
//   beta(a,b) — funzione Beta completa B(a,b) = Γ(a)Γ(b)/Γ(a+b)
//   erf(x)    — funzione errore
//   erfc(x)   — complementare: 1 - erf(x)
//
// Dominio: input fuori dominio → NaN (convenzione GMBasic: mai eccezione).

using MathNet.Numerics;

namespace CommonLib;

public static partial class Numerics
{
    // Nomi con prefisso Special* per non ombreggiare i tipi di distribuzione MathNet
    // (es. Beta/StudentT/... usati come classi in Stats.cs).
    public static double SpecialGamma(double x)
        => double.IsNaN(x) ? double.NaN
            : double.IsInfinity(x) ? (x > 0 ? double.PositiveInfinity : double.NaN)
            : SpecialFunctions.Gamma(x);

    public static double SpecialLGamma(double x)
        => double.IsNaN(x) || x <= 0 ? double.NaN
            : SpecialFunctions.GammaLn(x);

    public static double SpecialBeta(double a, double b)
        => double.IsNaN(a) || double.IsNaN(b) || a <= 0 || b <= 0 ? double.NaN
            : SpecialFunctions.Beta(a, b);

    public static double SpecialErf(double x)
        => double.IsNaN(x) ? double.NaN
            : double.IsInfinity(x) ? (x > 0 ? 1.0 : -1.0)
            : SpecialFunctions.Erf(x);

    public static double SpecialErfc(double x)
        => double.IsNaN(x) ? double.NaN
            : double.IsInfinity(x) ? (x > 0 ? 0.0 : 2.0)
            : SpecialFunctions.Erfc(x);

    // --- Estensioni (scipy.special) — incomplete gamma/beta, digamma, Bessel ---
    // gammainc(a,x) = P(a,x) = γ(a,x)/Γ(a)  (regolarizzata inferiore)
    public static double SpecialGammaInc(double a, double x)
        => double.IsNaN(a) || double.IsNaN(x) || a <= 0 || x < 0 ? double.NaN
            : SpecialFunctions.GammaLowerRegularized(a, x);

    // gammaincc(a,x) = Q(a,x) = 1 - P(a,x)  (regolarizzata superiore)
    public static double SpecialGammaIncc(double a, double x)
        => double.IsNaN(a) || double.IsNaN(x) || a <= 0 || x < 0 ? double.NaN
            : SpecialFunctions.GammaUpperRegularized(a, x);

    // betainc(a,b,x) = I_x(a,b)  (regolarizzata incompleta)
    public static double SpecialBetaInc(double a, double b, double x)
        => double.IsNaN(a) || double.IsNaN(b) || double.IsNaN(x)
            || a <= 0 || b <= 0 || x is < 0 or > 1 ? double.NaN
            : SpecialFunctions.BetaRegularized(a, b, x);

    // digamma(x) = ψ(x) = Γ'(x)/Γ(x)
    public static double SpecialDigamma(double x)
        => double.IsNaN(x) || x <= 0 ? double.NaN
            : SpecialFunctions.DiGamma(x);

    // Bessel: J_v(x), Y_v(x), I_v(x), K_v(x)
    public static double SpecialBesselJ(double v, double x)
        => double.IsNaN(v) || double.IsNaN(x) ? double.NaN : SpecialFunctions.BesselJ(v, x);

    public static double SpecialBesselY(double v, double x)
        => double.IsNaN(v) || double.IsNaN(x) ? double.NaN : SpecialFunctions.BesselY(v, x);

    public static double SpecialBesselI(double v, double x)
        => double.IsNaN(v) || double.IsNaN(x) ? double.NaN : SpecialFunctions.BesselI(v, x);

    public static double SpecialBesselK(double v, double x)
        => double.IsNaN(v) || double.IsNaN(x) || x < 0 ? double.NaN : SpecialFunctions.BesselK(v, x);

    // --- Inversi e log (scipy.special) ---
    // betaln(a,b) = log B(a,b)
    public static double SpecialBetaLn(double a, double b)
        => double.IsNaN(a) || double.IsNaN(b) || a <= 0 || b <= 0 ? double.NaN
            : SpecialFunctions.BetaLn(a, b);

    // erfinv(x): inversa della funzione errore, x in [-1,1]
    public static double SpecialErfInv(double x)
        => double.IsNaN(x) || x < -1 || x > 1 ? double.NaN
            : SpecialFunctions.ErfInv(x);

    // erfcinv(x): inversa di erfc, x in [0,2]
    public static double SpecialErfcInv(double x)
        => double.IsNaN(x) || x < 0 || x > 2 ? double.NaN
            : SpecialFunctions.ErfcInv(x);

    // gammaincinv(a, y): x tale che gammainc(a, x) = y
    public static double SpecialGammaIncInv(double a, double y)
        => double.IsNaN(a) || double.IsNaN(y) || a <= 0 || y < 0 || y > 1 ? double.NaN
            : SpecialFunctions.GammaLowerRegularizedInv(a, y);

    // gammainccinv(a, y): x tale che gammaincc(a, x) = y  (Q(a,x)=y ⇔ P(a,x)=1-y)
    public static double SpecialGammaInccInv(double a, double y)
        => double.IsNaN(a) || double.IsNaN(y) || a <= 0 || y < 0 || y > 1 ? double.NaN
            : SpecialFunctions.GammaLowerRegularizedInv(a, 1 - y);

    // betaincinv(a, b, y): x tale che betainc(a, b, x) = y — bisection su [0,1]
    // (BetaRegularized è monotona crescente in x; MathNet non ha l'inversa diretta)
    public static double SpecialBetaIncInv(double a, double b, double y)
    {
        if (double.IsNaN(a) || double.IsNaN(b) || double.IsNaN(y) || a <= 0 || b <= 0 || y < 0 || y > 1)
            return double.NaN;
        if (y == 0) return 0.0;
        if (y == 1) return 1.0;
        double lo = 0, hi = 1;
        for (var i = 0; i < 60; i++)
        {
            var mid = (lo + hi) / 2;
            if (SpecialFunctions.BetaRegularized(a, b, mid) < y) lo = mid; else hi = mid;
        }
        return (lo + hi) / 2;
    }
}
