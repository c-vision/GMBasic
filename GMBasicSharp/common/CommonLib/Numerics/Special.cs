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

    // --- gm_special2: funzioni speciali avanzate (port fedele di gm_special2.c) ---
    private static double Sp2Lgam(double x)
    {
        double[] c =
        {
            0.99999999999980993, 676.5203681218851, -1259.1392167224028,
            771.32342877765313, -176.61502916214059, 12.507343278686905,
            -0.13857109526572012, 9.9843695780195716e-6, 1.5056327351493116e-7
        };
        if (x < 0.5) return Math.Log(Math.PI / Math.Sin(Math.PI * x)) - Sp2Lgam(1.0 - x);
        x -= 1.0;
        var a = c[0];
        var t = x + 7.5;
        for (var i = 1; i < 9; i++) a += c[i] / (x + i);
        return 0.5 * Math.Log(2.0 * Math.PI) + (x + 0.5) * Math.Log(t) - t + Math.Log(a);
    }

    private static double Sp2Gam(double x)
    {
        if (x == Math.Floor(x) && x <= 0.0) return double.NaN;
        if (x < 0.5) return Math.PI / (Math.Sin(Math.PI * x) * Sp2Gam(1.0 - x));
        return Math.Exp(Sp2Lgam(x));
    }

    private static readonly double[] Sp2Bern =
    {
        1.0 / 6.0, -1.0 / 30.0, 1.0 / 42.0, -1.0 / 30.0, 5.0 / 66.0,
        -691.0 / 2730.0, 7.0 / 6.0, -3617.0 / 510.0, 43867.0 / 798.0, -174611.0 / 330.0
    };

    private static double Sp2ZetaEm(double s, double a)
    {
        int N = 20, M = 10, k, j;
        double sum = 0.0;
        for (k = 0; k < N; k++) sum += Math.Pow(k + a, -s);
        var Np = N + a;
        sum += Math.Pow(Np, 1.0 - s) / (s - 1.0) + 0.5 * Math.Pow(Np, -s);
        for (j = 1; j <= M; j++)
        {
            double fact = 1.0, rising = 1.0;
            var m2 = 2 * j;
            for (k = 1; k <= m2; k++) fact *= k;
            for (k = 0; k < m2 - 1; k++) rising *= s + k;
            sum += Sp2Bern[j - 1] / fact * rising * Math.Pow(Np, -s - m2 + 1.0);
        }
        return sum;
    }

    public static double SpecialZeta(double s)
    {
        if (s == 1.0) return double.PositiveInfinity;
        if (s > 1.0) return Sp2ZetaEm(s, 1.0);
        return Math.Pow(2.0, s) * Math.Pow(Math.PI, s - 1.0) * Math.Sin(Math.PI * s / 2.0)
               * Sp2Gam(1.0 - s) * SpecialZeta(1.0 - s);
    }

    public static double SpecialZetaHurwitz(double s, double a)
    {
        if (a <= 0.0 && a == Math.Floor(a)) return double.PositiveInfinity;
        return Sp2ZetaEm(s, a);
    }

    public static double SpecialPolygamma(double nIn, double x)
    {
        var n = (int)nIn;
        double r = 0.0;
        int i;
        if (n < 0 || x <= 0.0) return double.NaN;
        if (n == 0)
        {
            double res = 0.0;
            while (x < 6.0) { res -= 1.0 / x; x += 1.0; }
            var inv = 1.0 / x;
            var inv2 = inv * inv;
            var p = inv2;
            res += Math.Log(x) - 0.5 * inv;
            for (i = 1; i <= 10; i++) { res -= Sp2Bern[i - 1] / (2 * i) * p; p *= inv2; }
            return res;
        }
        {
            double fact = 1.0, factn1 = 1.0, res;
            var sign = (n % 2 == 1) ? 1 : -1;
            for (i = 1; i <= n; i++) fact *= i;
            for (i = 1; i <= n - 1; i++) factn1 *= i;
            while (x < 10.0) { r += sign * fact / Math.Pow(x, n + 1); x += 1.0; }
            res = factn1 / Math.Pow(x, n) + 0.5 * fact / Math.Pow(x, n + 1);
            for (i = 1; i <= 10; i++)
            {
                var k2 = 2 * i;
                double ratio = 1.0;
                for (var m = k2 + 1; m <= k2 + n - 1; m++) ratio *= m;
                res += Sp2Bern[i - 1] * ratio / Math.Pow(x, k2 + n);
            }
            r += ((n % 2 == 1) ? 1.0 : -1.0) * res;
        }
        return r;
    }

    public static double SpecialHypergeom2F1(double a, double b, double c, double x)
    {
        double s = 1.0, term = 1.0;
        int n;
        if (c == 0.0 || (c == Math.Floor(c) && c <= 0.0)) return double.NaN;
        if (x < -1.0)
        {
            var y = x / (x - 1.0);
            return Math.Pow(1.0 - x, -a) * SpecialHypergeom2F1(a, c - b, c, y);
        }
        if (x < 0.0 || x < 1.0)
        {
            for (n = 0; n < 10000; n++)
            {
                term *= (a + n) * (b + n) / ((c + n) * (n + 1.0)) * x;
                s += term;
                if (Math.Abs(term) < 1e-16 * Math.Abs(s)) break;
            }
            return s;
        }
        if (x == 1.0) return Sp2Gam(c) * Sp2Gam(c - a - b) / (Sp2Gam(c - a) * Sp2Gam(c - b));
        return double.NaN;
    }

    public static double SpecialHypergeom1F1(double a, double b, double x)
    {
        double s, term, u;
        int n;
        if (b == 0.0 || (b == Math.Floor(b) && b <= 0.0)) return double.NaN;
        if (a <= 0.0 && a == Math.Floor(a))
        {
            s = 1.0; term = 1.0;
            for (n = 0; n <= (int)(-a); n++)
            {
                term *= (a + n) / ((b + n) * (n + 1.0)) * x;
                s += term;
                if (!(Math.Abs(s) < 1e300)) return double.NaN;
            }
            return s;
        }
        if (x > 15.0)
        {
            double lv;
            var sign = (Sp2Gam(a) < 0.0) ? -1.0 : 1.0;
            int k;
            if (Math.Abs(a) > 60.0 || Math.Abs(b) > 60.0 || x > 700.0) lv = 0.0;
            else lv = SpecialFunctions.GammaLn(b) - SpecialFunctions.GammaLn(a) + x + (a - b) * Math.Log(Math.Abs(x));
            if (lv > 700.0) return sign * 1e308;
            if (lv < -700.0) return 0.0;
            s = 1.0; term = 1.0;
            for (k = 0; k < 12; k++)
            {
                term *= (b - a + k) * (1.0 - a + k) / (k + 1.0) / x;
                s += term;
            }
            return sign * Math.Exp(lv) * s;
        }
        if (x < -20.0)
        {
            var sign = ((Sp2Gam(b) < 0.0) ? -1.0 : 1.0) * ((Sp2Gam(b - a) < 0.0) ? -1.0 : 1.0);
            double lv;
            int cap;
            u = -x;
            if (Math.Abs(a) > 60.0 || Math.Abs(b - a) > 60.0) lv = 0.0;
            else lv = SpecialFunctions.GammaLn(b) - SpecialFunctions.GammaLn(b - a) - a * Math.Log(u);
            if (lv > 700.0) return sign * 1e308;
            if (lv < -700.0) return 0.0;
            cap = (int)u + 8;
            if (cap < 16) cap = 16;
            if (cap > 300) cap = 300;
            s = 1.0; term = 1.0;
            var best = 1.0;
            var minterm = 1.0;
            for (n = 1; n <= cap; n++)
            {
                term *= (a + (n - 1)) * (a - b + n) / n * (-1.0 / u);
                s += term;
                if (Math.Abs(term) < minterm) { minterm = Math.Abs(term); best = s; }
            }
            return sign * Math.Exp(lv) * best;
        }
        s = 1.0; term = 1.0;
        for (n = 0; n < 100000; n++)
        {
            term *= (a + n) / ((b + n) * (n + 1.0)) * x;
            s += term;
            if (!(Math.Abs(s) < 1e300)) return double.NaN;
            if (Math.Abs(term) < 1e-16 * Math.Abs(s)) break;
        }
        return s;
    }

    public static double SpecialLambertW(double x)
    {
        const double em1 = -1.0 / 2.71828182845904523536;
        if (x < em1) return double.NaN;
        if (x == 0.0) return 0.0;
        if (x == em1) return -1.0;
        var w = x > 1.0 ? Math.Log(x) - Math.Log(Math.Log(x) + 1e-300) : x;
        for (var i = 0; i < 100; i++)
        {
            var ew = Math.Exp(w);
            var f = w * ew - x;
            var wp1 = w + 1.0;
            var d = ew * wp1 - (w + 2.0) * f / (2.0 * wp1);
            var dw = f / d;
            w -= dw;
            if (Math.Abs(dw) < 1e-16 * (1.0 + Math.Abs(w))) break;
        }
        return w;
    }

    public static double SpecialMittagLeffler(double alpha, double beta, double x)
    {
        double s = 0.0;
        for (var k = 0; k < 2000; k++)
        {
            var term = Math.Pow(x, k) / Sp2Gam(alpha * k + beta);
            s += term;
            if (k > 0 && Math.Abs(term) < 1e-16 * Math.Abs(s)) break;
        }
        return s;
    }

    public static double SpecialStruveH(double v, double x)
    {
        double s = 0.0;
        var h = x / 2.0;
        for (var k = 0; k < 500; k++)
        {
            var term = Math.Pow(h, 2.0 * k + v + 1.0) / (Sp2Gam(k + 1.5) * Sp2Gam(k + v + 1.5));
            if (k % 2 != 0) term = -term;
            s += term;
            if (Math.Abs(term) < 1e-16 * Math.Abs(s)) break;
        }
        return s;
    }

    public static double SpecialKelvinBer(double x)
    {
        double s = 0.0;
        var h = x / 2.0;
        for (var k = 0; k < 200; k++)
        {
            var term = Math.Pow(h, 4.0 * k) / (Sp2Gam(2.0 * k + 1.0) * Sp2Gam(2.0 * k + 1.0));
            if (k % 2 != 0) term = -term;
            s += term;
            if (Math.Abs(term) < 1e-16 * Math.Abs(s)) break;
        }
        return s;
    }

    public static double SpecialKelvinBei(double x)
    {
        double s = 0.0;
        var h = x / 2.0;
        for (var k = 0; k < 200; k++)
        {
            var term = Math.Pow(h, 4.0 * k + 2.0) / (Sp2Gam(2.0 * k + 2.0) * Sp2Gam(2.0 * k + 2.0));
            if (k % 2 != 0) term = -term;
            s += term;
            if (Math.Abs(term) < 1e-16 * Math.Abs(s)) break;
        }
        return s;
    }

    public static double SpecialParabolicCylinder(double a, double x)
    {
        var nu = -a - 0.5;
        var t1 = Math.Sqrt(Math.PI) / Sp2Gam(0.5 * (1.0 - nu)) * SpecialHypergeom1F1(-0.5 * nu, 0.5, 0.5 * x * x);
        var t2 = Math.Sqrt(2.0 * Math.PI) * x / Sp2Gam(-0.5 * nu) * SpecialHypergeom1F1(0.5 * (1.0 - nu), 1.5, 0.5 * x * x);
        return Math.Pow(2.0, 0.5 * nu) * Math.Exp(-0.25 * x * x) * (t1 - t2);
    }

}
