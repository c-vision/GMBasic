// FILE: CommonLib.Numerics.Special.cs
// Nucleo `scipy.special` per GMBasic: tutti i metodi delegano ai kernel del runtime C
// (`gmb_math.c`) tramite la lib `gmb_special_flat` (P/Invoke, inoltro puro).
//
// PERCHÉ. Prima erano implementati qui (MathNet + serie): le gambe managed avevano una
// sorgente numerica diversa da VM/C e C99Native, che usano `gmb_math.c`. Ora la sorgente è una
// sola e le 4 gambe danno gli stessi valori.
//
// Dominio: input fuori dominio → NaN (convenzione GMBasic: mai eccezione), come nei kernel.

namespace CommonLib;

public static partial class Numerics
{
    // Nomi con prefisso Special* per non ombreggiare i tipi di distribuzione MathNet
    // (es. Beta/StudentT/... usati come classi in Stats.cs).
    public static double SpecialGamma(double x)
        => double.IsNaN(x) ? double.NaN
            : double.IsInfinity(x) ? (x > 0 ? double.PositiveInfinity : double.NaN)
            : SpecialNative.Gamma(x);

    public static double SpecialLGamma(double x)
        => double.IsNaN(x) || x <= 0 ? double.NaN
            : SpecialNative.LGamma(x);

    public static double SpecialBeta(double a, double b)
        => double.IsNaN(a) || double.IsNaN(b) || a <= 0 || b <= 0 ? double.NaN
            : SpecialNative.Beta(a, b);

    public static double SpecialErf(double x)
        => double.IsNaN(x) ? double.NaN
            : double.IsInfinity(x) ? (x > 0 ? 1.0 : -1.0)
            : SpecialNative.Erf(x);

    public static double SpecialErfc(double x)
        => double.IsNaN(x) ? double.NaN
            : double.IsInfinity(x) ? (x > 0 ? 0.0 : 2.0)
            : SpecialNative.Erfc(x);

    // --- Estensioni (scipy.special) — incomplete gamma/beta, digamma, Bessel ---
    // gammainc(a,x) = P(a,x) = γ(a,x)/Γ(a)  (regolarizzata inferiore)
    public static double SpecialGammaInc(double a, double x)
        => double.IsNaN(a) || double.IsNaN(x) || a <= 0 || x < 0 ? double.NaN
            : SpecialNative.GammaInc(a, x);

    // gammaincc(a,x) = Q(a,x) = 1 - P(a,x)  (regolarizzata superiore)
    public static double SpecialGammaIncc(double a, double x)
        => double.IsNaN(a) || double.IsNaN(x) || a <= 0 || x < 0 ? double.NaN
            : SpecialNative.GammaIncc(a, x);

    // betainc(a,b,x) = I_x(a,b)  (regolarizzata incompleta)
    public static double SpecialBetaInc(double a, double b, double x)
        => double.IsNaN(a) || double.IsNaN(b) || double.IsNaN(x)
            || a <= 0 || b <= 0 || x is < 0 or > 1 ? double.NaN
            : SpecialNative.BetaInc(a, b, x);

    // digamma(x) = ψ(x) = Γ'(x)/Γ(x)
    public static double SpecialDigamma(double x)
        => double.IsNaN(x) || x <= 0 ? double.NaN
            : SpecialNative.Digamma(x);

    // Bessel: J_v(x), Y_v(x), I_v(x), K_v(x)
    public static double SpecialBesselJ(double v, double x)
        => double.IsNaN(v) || double.IsNaN(x) ? double.NaN : SpecialNative.BesselJ(v, x);

    public static double SpecialBesselY(double v, double x)
        => double.IsNaN(v) || double.IsNaN(x) ? double.NaN : SpecialNative.BesselY(v, x);

    public static double SpecialBesselI(double v, double x)
        => double.IsNaN(v) || double.IsNaN(x) ? double.NaN : SpecialNative.BesselI(v, x);

    public static double SpecialBesselK(double v, double x)
        => double.IsNaN(v) || double.IsNaN(x) || x < 0 ? double.NaN : SpecialNative.BesselK(v, x);

    // --- Inversi e log (scipy.special) ---
    // betaln(a,b) = log B(a,b)
    public static double SpecialBetaLn(double a, double b)
        => double.IsNaN(a) || double.IsNaN(b) || a <= 0 || b <= 0 ? double.NaN
            : SpecialNative.BetaLn(a, b);

    // erfinv(x): inversa della funzione errore, x in [-1,1]
    public static double SpecialErfInv(double x)
        => double.IsNaN(x) || x < -1 || x > 1 ? double.NaN
            : SpecialNative.ErfInv(x);

    // erfcinv(x): inversa di erfc, x in [0,2]
    public static double SpecialErfcInv(double x)
        => double.IsNaN(x) || x < 0 || x > 2 ? double.NaN
            : SpecialNative.ErfcInv(x);

    // gammaincinv(a, y): x tale che gammainc(a, x) = y
    public static double SpecialGammaIncInv(double a, double y)
        => double.IsNaN(a) || double.IsNaN(y) || a <= 0 || y < 0 || y > 1 ? double.NaN
            : SpecialNative.GammaIncInv(a, y);

    // gammainccinv(a, y): x tale che gammaincc(a, x) = y  (Q(a,x)=y ⇔ P(a,x)=1-y)
    public static double SpecialGammaInccInv(double a, double y)
        => double.IsNaN(a) || double.IsNaN(y) || a <= 0 || y < 0 || y > 1 ? double.NaN
            : SpecialNative.GammaInccInv(a, y);

    // betaincinv(a, b, y): x tale che betainc(a, b, x) = y
    public static double SpecialBetaIncInv(double a, double b, double y)
    {
        if (double.IsNaN(a) || double.IsNaN(b) || double.IsNaN(y) || a <= 0 || b <= 0 || y < 0 || y > 1)
            return double.NaN;
        if (y == 0) return 0.0;
        if (y == 1) return 1.0;
        return SpecialNative.BetaIncInv(a, b, y);
    }

    // --- gm_special2: funzioni speciali avanzate (kernel in gmb_math.c) ---
    public static double SpecialZeta(double s) => SpecialNative.Zeta(s);

    public static double SpecialZetaHurwitz(double s, double a) => SpecialNative.ZetaHurwitz(s, a);

    public static double SpecialPolygamma(double nIn, double x) => SpecialNative.Polygamma(nIn, x);

    public static double SpecialHypergeom2F1(double a, double b, double c, double x)
        => SpecialNative.Hypergeom2F1(a, b, c, x);

    public static double SpecialHypergeom1F1(double a, double b, double x)
        => SpecialNative.Hypergeom1F1(a, b, x);

    public static double SpecialLambertW(double x) => SpecialNative.LambertW(x);

    public static double SpecialMittagLeffler(double alpha, double beta, double x)
        => SpecialNative.MittagLeffler(alpha, beta, x);

    public static double SpecialStruveH(double v, double x) => SpecialNative.StruveH(v, x);

    public static double SpecialKelvinBer(double x) => SpecialNative.KelvinBer(x);

    public static double SpecialKelvinBei(double x) => SpecialNative.KelvinBei(x);

    public static double SpecialParabolicCylinder(double a, double x) => SpecialNative.ParabolicCylinder(a, x);
}
