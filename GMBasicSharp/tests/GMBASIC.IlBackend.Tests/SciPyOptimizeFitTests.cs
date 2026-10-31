// scipy.optimize: curve_fit e differential_evolution — oracoli scipy 1.18.
using System;
using Xunit;

namespace Basic.IlBackend.Tests;

[Collection("GpmRuntimeState")]
public class SciPyOptimizeFitTests
{
    [Fact]
    public void CurveFit_Matches_SciPy()
    {
        // y = a*exp(b*x)+c, dati = scipy curve_fit oracle
        double[] x = [0.0, 0.25, 0.5, 0.75, 1.0, 1.25, 1.5, 1.75, 2.0];
        double[] y = [3.504967, 4.052124, 4.736039, 5.570527, 6.561511, 7.793363, 9.316084, 11.145674, 13.377886];
        var p = CommonLib.Numerics.CurveFit((par, xi) => par[0] * Math.Exp(par[1] * xi) + par[2], x, y, [1.0, 1.0, 1.0]);
        Assert.Equal(3, p.Length);
        Assert.True(Math.Abs(p[0] - 2.51623969) < 1e-4, $"a={p[0]}");
        Assert.True(Math.Abs(p[1] - 0.79729828) < 1e-4, $"b={p[1]}");
        Assert.True(Math.Abs(p[2] - 0.98585173) < 1e-4, $"c={p[2]}");
    }

    [Fact]
    public void CurveFit_Linear_Exact()
    {
        double[] x = [0.0, 0.25, 0.5, 0.75, 1.0, 1.25, 1.5, 1.75, 2.0];
        var y = new double[x.Length];
        for (var i = 0; i < x.Length; i++) y[i] = 2.0 * x[i] + 0.5;
        var p = CommonLib.Numerics.CurveFit((par, xi) => par[0] * xi + par[1], x, y, [0.0, 0.0]);
        Assert.True(Math.Abs(p[0] - 2.0) < 1e-6, $"a={p[0]}");
        Assert.True(Math.Abs(p[1] - 0.5) < 1e-6, $"b={p[1]}");
    }

    [Fact]
    public void DifferentialEvolution_Rosenbrock_Finds_Global_Min()
    {
        // Rosenbrock n=3, minimo globale [1,1,1] → f=0 (scipy.optimize.rosen).
        var bounds = new double[3, 2];
        for (var i = 0; i < 3; i++) { bounds[i, 0] = -5; bounds[i, 1] = 5; }
        var best = CommonLib.Numerics.DifferentialEvolution(Rosenbrock, bounds);
        var f = Rosenbrock(best);
        Assert.True(f < 1e-4, $"f(best)={f}");
        Assert.True(Math.Abs(best[0] - 1.0) < 1e-2 && Math.Abs(best[1] - 1.0) < 1e-2 && Math.Abs(best[2] - 1.0) < 1e-2,
            $"x={string.Join(",", best)}");
    }

    private static double Rosenbrock(double[] x)
    {
        double s = 0;
        for (var i = 0; i < x.Length - 1; i++)
            s += 100 * Math.Pow(x[i + 1] - x[i] * x[i], 2) + Math.Pow(1 - x[i], 2);
        return s;
    }

    // ---- scipy.optimize completato: radici scalari ----
    [Fact]
    public void Brentq_Matches_SciPy()
        => Assert.True(Math.Abs(CommonLib.Numerics.Brentq(x => x * x - 2, 0, 2) - Math.Sqrt(2)) < 1e-9);

    [Fact]
    public void BrenthRidder_Matches_SciPy()
    {
        Assert.True(Math.Abs(CommonLib.Numerics.Brenth(x => x * x * x - 27, 2, 4) - 3) < 1e-9);
        Assert.True(Math.Abs(CommonLib.Numerics.Ridder(x => x * x * x - 27, 2, 4) - 3) < 1e-9);
    }

    [Fact]
    public void Toms748_Matches_SciPy()
        => Assert.True(Math.Abs(CommonLib.Numerics.Toms748(x => Math.Sin(x) - 0.5, 0, 1) - Math.PI / 6) < 1e-9);

    [Fact]
    public void BrentMin_MinimizeScalar_Matches_SciPy()
    {
        Assert.True(Math.Abs(CommonLib.Numerics.BrentMinimize(x => (x - 3) * (x - 3), 0, 5) - 3) < 1e-6);
        Assert.True(Math.Abs(CommonLib.Numerics.MinimizeScalar(x => (x + 1) * (x + 1) + 2, -5, 5, "brent") + 1) < 1e-5);
    }

    // ---- minimize: powell/tnc/cobyla/slsqp su Rosenbrock → [1,1] ----
    private static double Rosen2(double[] x) => (1 - x[0]) * (1 - x[0]) + 100 * (x[1] - x[0] * x[0]) * (x[1] - x[0] * x[0]);
    private static MathNet.Numerics.LinearAlgebra.Vector<double> V(double[] a)
        => MathNet.Numerics.LinearAlgebra.Double.DenseVector.OfArray(a);

    [Fact]
    public void Powell_Matches_SciPy()
    {
        var r = CommonLib.Numerics.PowellMinimize(v => Rosen2(v.ToArray()), V([-1.2, 1.0]));
        Assert.True(Math.Abs(r[0] - 1) < 1e-5 && Math.Abs(r[1] - 1) < 1e-5, $"{r[0]},{r[1]}");
    }

    [Fact]
    public void TncSlsqp_Matches_SciPy()
    {
        var bounds = new double[,] { { -5, 5 }, { -5, 5 } };
        var tn = CommonLib.Numerics.TruncatedNewton(v => Rosen2(v.ToArray()), V([-1.2, 1.0]), bounds);
        Assert.True(Math.Abs(tn[0] - 1) < 1e-5 && Math.Abs(tn[1] - 1) < 1e-5, $"tnc {tn[0]},{tn[1]}");
        var sl = CommonLib.Numerics.SlsqpMinimize(v => Rosen2(v.ToArray()), V([-1.2, 1.0]), bounds);
        Assert.True(Math.Abs(sl[0] - 1) < 1e-5 && Math.Abs(sl[1] - 1) < 1e-5, $"slsqp {sl[0]},{sl[1]}");
    }

    [Fact]
    public void Cobyla_Matches_SciPy()
    {
        var bounds = new double[,] { { -5, 5 }, { -5, 5 } };
        var cb = CommonLib.Numerics.CobylaMinimize(v => (v[0] + 2) * (v[0] + 2) + (v[1] + 1) * (v[1] + 1), V([-1.2, 1.0]), bounds);
        Assert.True(Math.Abs(cb[0] + 2) < 1e-4 && Math.Abs(cb[1] + 1) < 1e-4, $"{cb[0]},{cb[1]}");
    }

    [Fact]
    public void Minimize_Dispatcher_Matches_SciPy()
    {
        var r = CommonLib.Numerics.Minimize(v => Rosen2(v.ToArray()), V([-1.2, 1.0]), "powell", null);
        Assert.True(Math.Abs(r[0] - 1) < 1e-5, $"minimize {r[0]}");
    }

    // ---- fsolve / least_squares / linprog ----
    [Fact]
    public void Fsolve_Matches_SciPy()
    {
        var r = CommonLib.Numerics.Fsolve(v => MathNet.Numerics.LinearAlgebra.Double.DenseVector.OfArray(
            new[] { v[0] * v[0] + v[1] * v[1] - 1.0, v[0] - v[1] }), V([0.5, 0.5]));
        Assert.True(Math.Abs(r[0] - Math.Sqrt(2) / 2) < 1e-6 && Math.Abs(r[1] - Math.Sqrt(2) / 2) < 1e-6, $"{r[0]},{r[1]}");
    }

    [Fact]
    public void LeastSquaresNonlinear_Matches_SciPy()
    {
        var r = CommonLib.Numerics.LeastSquaresNonlinear(v => MathNet.Numerics.LinearAlgebra.Double.DenseVector.OfArray(
            new[] { v[0] * 1 + v[1] - 2.0, v[0] * 2 + v[1] - 3.0, v[0] * 3 + v[1] - 5.0 }), V([0.0, 0.0]), null);
        Assert.True(Math.Abs(r[0] - 1.5) < 1e-6 && Math.Abs(r[1] - 1.0 / 3) < 1e-6, $"{r[0]},{r[1]}");
    }

    [Fact]
    public void LinProg_Matches_SciPy()
    {
        var r = CommonLib.Numerics.LinProg([-1, -1],
            [new double[] { 1, 2 }, new double[] { 3, 2 }], [4, 6], null, null, null);
        Assert.True(Math.Abs(r[0] - 1) < 1e-6 && Math.Abs(r[1] - 1.5) < 1e-6, $"{r[0]},{r[1]}");
        Assert.True(-r[0] - r[1] <= -2.5 + 1e-6, "obiettivo");
    }

    [Fact]
    public void Global_Optimizers_Find_Optimum()
    {
        var ranges = new double[,] { { -3, 3 }, { -3, 3 } };
        var br = CommonLib.Numerics.Brute(v => (v[0] - 1) * (v[0] - 1) + (v[1] + 2) * (v[1] + 2), ranges, 41);
        Assert.True(Math.Abs(br[0] - 1) < 0.2 && Math.Abs(br[1] + 2) < 0.2, $"brute {br[0]},{br[1]}");
        var bh = CommonLib.Numerics.BasinHopping(v => Rosen2(v.ToArray()), V([-1.2, 1.0]), 30, new Random(7));
        Assert.True(Math.Abs(bh[0] - 1) < 1e-3 && Math.Abs(bh[1] - 1) < 1e-3, $"basin {bh[0]},{bh[1]}");
        var da = CommonLib.Numerics.DualAnnealing(v => (v[0] - 1) * (v[0] - 1) + (v[1] + 2) * (v[1] + 2), new double[,] { { -10, 10 }, { -10, 10 } }, 2000, new Random(3));
        Assert.True(Math.Abs(da[0] - 1) < 1e-3 && Math.Abs(da[1] + 2) < 1e-3, $"dual {da[0]},{da[1]}");
    }
}
