// FILE: StdLib.Numerics.StatsInverse.cs
// ReSharper disable CheckNamespace

namespace CommonLib;

/// <summary>
/// Inverse CDF (ppf) e ISF delle distribuzioni continue, con parità a scipy.stats
/// (norm/lognorm/chi/chi2/beta/t/f/expon/weibull/logistic/uniform/pareto/triang).
/// Le formule vivono nel runtime C (`gmb_dist_*_ppf/isf`): qui solo l'API managed.
/// NOTA: l'API pubblica ha `p` come PRIMO argomento; i kernel lo hanno per ultimo, quindi
/// ogni inoltro riordina esplicitamente.
/// </summary>
public static partial class Numerics
{
    /// <summary>Quantile normale (scipy norm.ppf).</summary>
    public static double NormalPpf(double p, double mean, double sd)
        => StatsDistNative.NormalPpf(mean, sd, p);

    public static double NormalIsf(double p, double mean, double sd)
        => StatsDistNative.NormalIsf(mean, sd, p);

    /// <summary>Quantile lognormale (scipy lognorm.ppf).</summary>
    public static double LognormalPpf(double p, double mu, double sd)
        => StatsDistNative.LognormalPpf(mu, sd, p);

    /// <summary>Quantile chi-quadrato (scipy chi2.ppf).</summary>
    public static double ChiSquaredPpf(double p, double df)
        => StatsDistNative.Chi2Ppf(df, p);

    public static double ChiSquaredIsf(double p, double df)
        => StatsDistNative.Chi2Isf(df, p);

    /// <summary>Quantile chi (scipy chi.ppf).</summary>
    public static double ChiPpf(double p, double df)
        => StatsDistNative.ChiPpf(df, p);

    /// <summary>Quantile beta (scipy beta.ppf).</summary>
    public static double BetaPpf(double p, double a, double b)
        => StatsDistNative.BetaPpf(a, b, p);

    /// <summary>Quantile F (scipy f.ppf).</summary>
    public static double FPpf(double p, double d1, double d2)
        => StatsDistNative.FPpf(d1, d2, p);

    public static double FIsf(double p, double d1, double d2)
        => StatsDistNative.FIsf(d1, d2, p);

    /// <summary>Quantile Student-t (scipy t.ppf).</summary>
    public static double StudentTPpf(double p, double df)
        => StatsDistNative.StudentTPpf(df, p);

    public static double StudentTIsf(double p, double df)
        => StatsDistNative.StudentTIsf(df, p);

    /// <summary>Quantile esponenziale (scipy expon.ppf).</summary>
    public static double ExponentialPpf(double p, double rate)
        => StatsDistNative.ExpPpf(rate, p);

    public static double ExponentialIsf(double p, double rate)
        => StatsDistNative.ExpIsf(rate, p);

    /// <summary>Quantile Weibull (scipy weibull_min.ppf).</summary>
    public static double WeibullPpf(double p, double shape, double scale)
        => StatsDistNative.WeibullPpf(shape, scale, p);

    /// <summary>Quantile logistico (scipy logistic.ppf).</summary>
    public static double LogisticPpf(double p, double loc, double scale)
        => StatsDistNative.LogisticPpf(loc, scale, p);

    /// <summary>Quantile uniforme (scipy uniform.ppf).</summary>
    public static double UniformPpf(double p, double a, double b)
        => StatsDistNative.UniformPpf(a, b, p);

    public static double UniformCdf(double a, double b, double x)
    {
        // Il kernel normalizza b<=a a 0; il C# pubblico è sempre stato NaN: guardia conservata.
        if (b <= a) return double.NaN;
        return StatsDistNative.UniformCdf(a, b, x);
    }

    public static double UniformSf(double a, double b, double x) => 1.0 - UniformCdf(a, b, x);

    public static double UniformIsf(double p, double a, double b)
        => StatsDistNative.UniformIsf(a, b, p);

    public static double WeibullIsf(double p, double shape, double scale)
        => StatsDistNative.WeibullIsf(shape, scale, p);

    /// <summary>Quantile Pareto (scipy pareto.ppf).</summary>
    public static double ParetoPpf(double p, double scale, double shape)
        => StatsDistNative.ParetoPpf(scale, shape, p);

    public static double ParetoIsf(double p, double scale, double shape)
        => StatsDistNative.ParetoIsf(scale, shape, p);

    /// <summary>Quantile triangolare (scipy triang.ppf).</summary>
    public static double TriangularPpf(double p, double min, double mode, double max)
        => StatsDistNative.TriangularPpf(min, max, mode, p);

    public static double ChiIsf(double p, double k) => StatsDistNative.ChiIsf(k, p);
    public static double BetaIsf(double p, double a, double b) => StatsDistNative.BetaIsf(a, b, p);
    public static double LogisticIsf(double p, double loc, double scale) => StatsDistNative.LogisticIsf(loc, scale, p);
    public static double LognormalIsf(double p, double mu, double sd) => StatsDistNative.LognormalIsf(mu, sd, p);
    public static double TriangularIsf(double p, double min, double mode, double max)
        => StatsDistNative.TriangularIsf(min, max, mode, p);
}
