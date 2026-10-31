// Inverse CDF (ppf) e ISF delle distribuzioni continue — oracoli scipy.stats 1.18.
using Xunit;

namespace Basic.IlBackend.Tests;

[Collection("GpmRuntimeState")]
public class SciPyStatsInverseTests
{
    private static void AssertVal(double expected, double actual, double tol = 1e-9)
        => Assert.True(Math.Abs(expected - actual) <= tol, $"atteso {expected} ottenuto {actual}");

    [Fact]
    public void Normal_Ppf_Isf_Matches_SciPy()
    {
        AssertVal(4.919927969080108, CommonLib.Numerics.NormalPpf(0.975, 1, 2));
        AssertVal(-1.6448536269514729, CommonLib.Numerics.NormalPpf(0.05, 0, 1));
        AssertVal(4.919927969080108, CommonLib.Numerics.NormalIsf(0.025, 1, 2));
        AssertVal(0.0, CommonLib.Numerics.NormalPpf(0.5, 0, 1));
    }

    [Fact]
    public void Lognormal_Ppf_Matches_SciPy()
    {
        AssertVal(5.159170355622591, CommonLib.Numerics.LognormalPpf(0.9, 1, 0.5));
        AssertVal(1.432217893498685, CommonLib.Numerics.LognormalPpf(0.1, 1, 0.5));
    }

    [Fact]
    public void Chi2_And_Chi_Ppf_Isf_Matches_SciPy()
    {
        AssertVal(11.070497693516351, CommonLib.Numerics.ChiSquaredPpf(0.95, 5));
        AssertVal(0.2971094805065319, CommonLib.Numerics.ChiSquaredPpf(0.01, 4));
        AssertVal(11.070497693516353, CommonLib.Numerics.ChiSquaredIsf(0.05, 5));
        AssertVal(3.466559767807347, CommonLib.Numerics.ChiPpf(0.9, 7));
        AssertVal(0.5931663491376357, CommonLib.Numerics.ChiPpf(0.05, 3));
    }

    [Fact]
    public void Beta_Ppf_Matches_SciPy()
    {
        AssertVal(0.3857275681323895, CommonLib.Numerics.BetaPpf(0.5, 2, 3));
        AssertVal(0.5103163065514917, CommonLib.Numerics.BetaPpf(0.9, 2, 5));
    }

    [Fact]
    public void F_Ppf_Isf_Matches_SciPy()
    {
        AssertVal(4.0661805514, CommonLib.Numerics.FPpf(0.95, 3, 8));
        AssertVal(4.1026846306, CommonLib.Numerics.FPpf(0.99, 5, 20));
        AssertVal(4.0661805513511595, CommonLib.Numerics.FIsf(0.05, 3, 8));
        AssertVal(1.0, CommonLib.Numerics.FPpf(0.5, 2, 2));
    }

    [Fact]
    public void StudentT_Ppf_Isf_Matches_SciPy()
    {
        AssertVal(2.2281388520, CommonLib.Numerics.StudentTPpf(0.975, 10));
        AssertVal(3.3649299989, CommonLib.Numerics.StudentTPpf(0.99, 5));
        AssertVal(-1.8595480375, CommonLib.Numerics.StudentTPpf(0.05, 8));
        AssertVal(0.0, CommonLib.Numerics.StudentTPpf(0.5, 3));
        AssertVal(2.228138851986275, CommonLib.Numerics.StudentTIsf(0.025, 10));
        AssertVal(3.364929998907218, CommonLib.Numerics.StudentTIsf(0.01, 5));
    }

    [Fact]
    public void ClosedForm_Ppf_Matches_SciPy()
    {
        AssertVal(4.605170185988092, CommonLib.Numerics.ExponentialPpf(0.9, 0.5));
        AssertVal(0.2107210313156526, CommonLib.Numerics.ExponentialIsf(0.9, 0.5));
        AssertVal(2.746710033739222, CommonLib.Numerics.WeibullPpf(0.8, 1.5, 2));
        AssertVal(0.7357883197815892, CommonLib.Numerics.WeibullPpf(0.2, 1.5, 2));
        AssertVal(2.694595720774407, CommonLib.Numerics.LogisticPpf(0.7, 1, 2));
        AssertVal(3.6, CommonLib.Numerics.UniformPpf(0.4, 2, 6));
        AssertVal(9.486832980505138, CommonLib.Numerics.ParetoPpf(0.9, 3, 2));
        AssertVal(3.872983346207417, CommonLib.Numerics.TriangularPpf(0.3, 0, 5, 10));
        AssertVal(6.837722339831621, CommonLib.Numerics.TriangularPpf(0.8, 0, 5, 10));
    }

    // ---- scipy.stats P0 (StatsCommon.cs) ----
    [Fact]
    public void Gamma_Matches_SciPy()
    {
        AssertVal(0.1493612051035918, CommonLib.Numerics.GammaPdf(2, 1, 3));
        AssertVal(0.8008517265285442, CommonLib.Numerics.GammaCdf(2, 1, 3));
        AssertVal(1.6783469900166605, CommonLib.Numerics.GammaPpf(2, 1, 0.5), 1e-6);
        AssertVal(0.3678794411714423, CommonLib.Numerics.GammaPdf(1, 1, 1));
    }

    [Fact]
    public void PearsonSpearmanLinRegress_Matches_SciPy()
    {
        double[] x = [1, 2, 3, 4, 5], y = [2, 4, 6, 8, 10], r = [5, 4, 3, 2, 1];
        var pr = CommonLib.Numerics.PearsonR(x, y);
        AssertVal(1.0, pr.Statistic);
        AssertVal(0.0, pr.PValue);
        AssertVal(-1.0, CommonLib.Numerics.PearsonR(x, r).Statistic);
        AssertVal(1.0, CommonLib.Numerics.SpearmanR(x, y).Statistic);
        var lr = CommonLib.Numerics.LinRegress(x, y);
        AssertVal(2.0, lr.Slope);
        AssertVal(0.0, lr.Intercept);
        AssertVal(1.0, lr.R);
    }

    [Fact]
    public void Tests_Matches_SciPy()
    {
        var ct = CommonLib.Numerics.Chi2Contingency([new double[] { 10, 20 }, new double[] { 30, 40 }]);
        AssertVal(0.7936507936507936, ct.Statistic);
        AssertVal(0.3729984836134773, ct.PValue, 1e-9);
        AssertVal(1.0, ct.DegreesOfFreedom);
        var mw = CommonLib.Numerics.MannWhitneyU([1, 2, 3, 4], [5, 6, 7, 8]);
        AssertVal(0.0, mw.Statistic);
        var wl = CommonLib.Numerics.Wilcoxon([1, 2, 3, 4, 5], [2, 1, 4, 3, 6]);
        AssertVal(6.0, wl.Statistic);
    }

    [Fact]
    public void NormalityTests_Matches_SciPy()
    {
        double[] z = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20];
        var sk = CommonLib.Numerics.SkewTest(z);
        // z è perfettamente simmetrico (skewness campionaria = 0): scipy.stats.skewtest NON dà
        // Z=0 per questo caso — sostituisce y=1 prima della trasformata asinh (np.where(y==0,1,y)
        // nel sorgente scipy) invece di lasciare y=0. Valori verificati con una chiamata reale
        // scipy.stats.skewtest(np.arange(1,21)) (bug precedente: SkewTest usava un'approssimazione
        // grezza `skew/sqrt(6/n)` che dava 0/1.0, mai stata scipy-conforme per questo caso).
        AssertVal(1.0403494689426542, sk.Statistic);
        AssertVal(0.298177568989332, sk.PValue);
        var jb = CommonLib.Numerics.JarqueBeraTest(z);
        AssertVal(4.8482409030094, jb.Statistic);
        AssertVal(0.0885559740721757, jb.PValue, 1e-9);
    }

    [Fact]
    public void Descriptive_Matches_SciPy()
    {
        AssertVal(3.0, CommonLib.Numerics.Mode([1, 2, 2, 3, 3, 3]));
        AssertVal(2.0, CommonLib.Numerics.Moment([1, 2, 3, 4, 5], 2));
        AssertVal(4.5, CommonLib.Numerics.Iqr([1, 2, 3, 4, 5, 6, 7, 8, 9, 10]));
        var d = CommonLib.Numerics.Describe([1, 2, 3, 4, 5]);
        AssertVal(5.0, d.Nobs);
        AssertVal(3.0, d.Mean);
        AssertVal(2.5, d.Variance);
    }
}
