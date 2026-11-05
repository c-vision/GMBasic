// FILE: StdLib.Numerics.StatsInverse.cs
// ReSharper disable CheckNamespace

namespace CommonLib;

/// <summary>
/// Inverse CDF (ppf) e ISF delle distribuzioni continue, con parità a scipy.stats
/// (norm/lognorm/chi/chi2/beta/t/f/expon/weibull/logistic/uniform/pareto/triang).
/// Basate sulle inverse SPECIAL (erfinv/gammaincinv/betaincinv) già verificate.
/// </summary>
public static partial class Numerics
{
    /// <summary>Quantile normale (scipy norm.ppf): mu + sd·√2·erfinv(2p−1).</summary>
    public static double NormalPpf(double p, double mean, double sd)
        => mean + sd * Math.Sqrt(2.0) * SpecialErfInv(2.0 * p - 1.0);

    public static double NormalIsf(double p, double mean, double sd)
        => NormalPpf(1.0 - p, mean, sd);

    /// <summary>Quantile lognormale (scipy lognorm.ppf): exp(mu + sd·ppf_norm(p)).</summary>
    public static double LognormalPpf(double p, double mu, double sd)
        => Math.Exp(NormalPpf(p, mu, sd));

    /// <summary>Quantile chi-quadrato (scipy chi2.ppf): 2·gammaincinv(df/2, p).</summary>
    public static double ChiSquaredPpf(double p, double df)
        => 2.0 * SpecialGammaIncInv(df / 2.0, p);

    public static double ChiSquaredIsf(double p, double df)
        => 2.0 * SpecialGammaIncInv(df / 2.0, 1.0 - p);

    /// <summary>Quantile chi (scipy chi.ppf): √(chi2.ppf(p, df)).</summary>
    public static double ChiPpf(double p, double df)
        => Math.Sqrt(ChiSquaredPpf(p, df));

    /// <summary>Quantile beta (scipy beta.ppf): betaincinv(a, b, p).</summary>
    public static double BetaPpf(double p, double a, double b)
        => SpecialBetaIncInv(a, b, p);

    /// <summary>Quantile F (scipy f.ppf): x = d2·z/(d1·(1−z)) con z = betaincinv(d1/2, d2/2, p).</summary>
    public static double FPpf(double p, double d1, double d2)
    {
        var z = SpecialBetaIncInv(d1 / 2.0, d2 / 2.0, p);
        return d2 * z / (d1 * (1.0 - z));
    }

    public static double FIsf(double p, double d1, double d2)
        => FPpf(1.0 - p, d1, d2);

    /// <summary>Quantile Student-t (scipy t.ppf): sign(p−½)·√(F.ppf(|2p−1|, 1, df)).</summary>
    public static double StudentTPpf(double p, double df)
    {
        if (p == 0.5) return 0.0;
        var sign = p > 0.5 ? 1.0 : -1.0;
        return sign * Math.Sqrt(FPpf(Math.Abs(2.0 * p - 1.0), 1.0, df));
    }

    public static double StudentTIsf(double p, double df)
        => StudentTPpf(1.0 - p, df);

    /// <summary>Quantile esponenziale (scipy expon.ppf): −ln(1−p)/rate.</summary>
    public static double ExponentialPpf(double p, double rate)
        => -Math.Log(1.0 - p) / rate;

    public static double ExponentialIsf(double p, double rate)
        => -Math.Log(p) / rate;

    /// <summary>Quantile Weibull (scipy weibull_min.ppf): scale·(−ln(1−p))^(1/shape).</summary>
    public static double WeibullPpf(double p, double shape, double scale)
        => scale * Math.Pow(-Math.Log(1.0 - p), 1.0 / shape);

    /// <summary>Quantile logistico (scipy logistic.ppf): loc + scale·ln(p/(1−p)).</summary>
    public static double LogisticPpf(double p, double loc, double scale)
        => loc + scale * Math.Log(p / (1.0 - p));

    /// <summary>Quantile uniforme (scipy uniform.ppf): a + p·(b−a).</summary>
    public static double UniformPpf(double p, double a, double b)
        => a + p * (b - a);

    public static double UniformCdf(double a, double b, double x)
    {
        if (b <= a) return double.NaN;
        if (x <= a) return 0.0;
        if (x >= b) return 1.0;
        return (x - a) / (b - a);
    }

    public static double UniformSf(double a, double b, double x) => 1.0 - UniformCdf(a, b, x);

    public static double UniformIsf(double p, double a, double b) => UniformPpf(1.0 - p, a, b);

    public static double WeibullIsf(double p, double shape, double scale) => WeibullPpf(1.0 - p, shape, scale);

    /// <summary>Quantile Pareto (scipy pareto.ppf): scale·(1−p)^(−1/shape).</summary>
    public static double ParetoPpf(double p, double scale, double shape)
        => scale / Math.Pow(1.0 - p, 1.0 / shape);

    public static double ParetoIsf(double p, double scale, double shape) => ParetoPpf(1.0 - p, scale, shape);

    /// <summary>Quantile triangolare (scipy triang.ppf): forma standard [0,1] con mode
    /// c=(mode−min)/(max−min), poi a + (b−a)·x.</summary>
    public static double TriangularPpf(double p, double min, double mode, double max)
    {
        var c = (mode - min) / (max - min);
        double xStd;
        if (p < c) xStd = Math.Sqrt(c * p);
        else xStd = 1.0 - Math.Sqrt((1.0 - c) * (1.0 - p));
        return min + (max - min) * xStd;
    }
}
