// Nuclei scipy.integrate / scipy.interpolate / scipy.fft completati in CommonLib.Numerics:
// IntegrateQuad (Gauss-Kronrod adattivo), IntegrateSimpson, IntegrateTrapezoid,
// IntegrateCumulativeTrapezoid, SolveOde (RK4), InterpolateCubicSpline (not-a-knot),
// FftFreq, FftShift, IfftShift. Oracoli: scipy 1.18.
using Xunit;

namespace Basic.IlBackend.Tests;

[Collection("GpmRuntimeState")]
public class SciPyIntegrateInterpFftTests
{
    private static void AssertArr(double[] expected, double[] actual, double tol = 1e-12)
    {
        Assert.Equal(expected.Length, actual.Length);
        for (var i = 0; i < expected.Length; i++)
            Assert.True(Math.Abs(expected[i] - actual[i]) <= tol,
                $"pos {i}: atteso {expected[i]} ottenuto {actual[i]}");
    }

    [Fact]
    public void Quad_Matches_SciPy()
    {
        Assert.Equal(1.0 / 3.0, CommonLib.Numerics.IntegrateQuad(x => x * x, 0, 1), 8);
        Assert.Equal(2.0, CommonLib.Numerics.IntegrateQuad(Math.Sin, 0, Math.PI), 8);
        Assert.Equal(0.8820813907624216, CommonLib.Numerics.IntegrateQuad(x => Math.Exp(-x * x), 0, 2), 10);
        Assert.Equal(0.0, CommonLib.Numerics.IntegrateQuad(x => x, 1, 1));
    }

    [Fact]
    public void Simpson_Matches_SciPy()
    {
        Assert.Equal(1.0 / 3.0, CommonLib.Numerics.IntegrateSimpson(x => x * x, 0, 1), 10);
        Assert.Equal(2.0, CommonLib.Numerics.IntegrateSimpson(Math.Sin, 0, Math.PI, 1024), 10);
        Assert.Equal(0.8820813907624216, CommonLib.Numerics.IntegrateSimpson(x => Math.Exp(-x * x), 0, 2, 512), 10);
    }

    [Fact]
    public void Trapezoid_And_Cumulative_Matches_SciPy()
    {
        double[] y = [0, 1, 4, 9];
        double[] x = [0, 1, 2, 3];
        Assert.Equal(9.5, CommonLib.Numerics.IntegrateTrapezoid(y, x), 12);
        AssertArr([0.5, 3.0, 9.5], CommonLib.Numerics.IntegrateCumulativeTrapezoid(y, x));
    }

    [Fact]
    public void SolveOde_Matches_Solve_Ivp()
    {
        // dy/dt = -y, y(0)=1 → y(t)=e^-t. RK4 a passo 0.1 vs scipy solve_ivp RK45.
        var ys = CommonLib.Numerics.SolveOde((t, yv) => -yv, 1.0, 0.0, 1.0, 10);
        Assert.Equal(11, ys.Length);
        Assert.Equal(1.0, ys[0], 15);
        Assert.True(Math.Abs(ys[^1] - 0.36809008) < 5e-3, $"y(1)={ys[^1]} atteso ~0.36809008");
        Assert.True(Math.Abs(ys[^1] - Math.Exp(-1.0)) < 1e-3, $"y(1)={ys[^1]} vs e^-1={Math.Exp(-1)}");
        // monotona decrescente
        for (var i = 1; i < ys.Length; i++) Assert.True(ys[i] < ys[i - 1]);
    }

    [Fact]
    public void CubicSpline_NotAKnot_Matches_SciPy()
    {
        double[] x = [0, 1, 2, 3];
        Assert.Equal(1.3125, CommonLib.Numerics.InterpolateCubicSpline(x, [0, 0.5, 2, 1.5], 1.5), 12);
        Assert.Equal(4.84, CommonLib.Numerics.InterpolateCubicSpline(x, [0, 1, 4, 9], 2.2), 12);
        // interpolazione esatta sui nodi
        Assert.Equal(2.0, CommonLib.Numerics.InterpolateCubicSpline(x, [0, 0.5, 2, 1.5], 2.0), 12);
    }

    [Fact]
    public void FftFreq_Matches_SciPy()
    {
        AssertArr([0, 1.0 / 6, 1.0 / 3, -0.5, -1.0 / 3, -1.0 / 6], CommonLib.Numerics.FftFreq(6));
        AssertArr([0, 2, 4, -4, -2], CommonLib.Numerics.FftFreq(5, 0.1));
    }

    [Fact]
    public void FftShift_Matches_SciPy()
    {
        AssertArr([3, 4, 5, 0, 1, 2], CommonLib.Numerics.FftShift([0, 1, 2, 3, 4, 5]));
        AssertArr([3, 4, 5, 0, 1, 2], CommonLib.Numerics.IfftShift([0, 1, 2, 3, 4, 5]));
        AssertArr([3, 4, 0, 1, 2], CommonLib.Numerics.FftShift([0, 1, 2, 3, 4]));
        AssertArr([2, 3, 4, 0, 1], CommonLib.Numerics.IfftShift([0, 1, 2, 3, 4]));
    }
}
