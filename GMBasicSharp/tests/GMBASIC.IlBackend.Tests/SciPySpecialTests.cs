// Nucleo scipy.special (SPECIAL namespace) — verifica end-to-end sul runtime C#:
// 1) unit su CommonLib.Numerics (SpecialGamma/LGamma/Beta/Erf/Erfc) con oracolo SciPy;
// 2) dispatch BASIC (P-Code VM) di SPECIAL.GAMMA/LGAMMA/BETA/ERF/ERFC.
// Oracoli: scipy.special.{gamma,lgamma,beta,erf,erfc} 1.17 (valori noti).
using System.IO;
using Xunit;

namespace Basic.IlBackend.Tests;

[Collection("GpmRuntimeState")]
public class SciPySpecialTests
{
    private static void AssertClose(double expected, double actual, double tol = 1e-12)
        => Assert.True(Math.Abs(expected - actual) <= tol,
            $"atteso {expected} ottenuto {actual}");

    [Fact]
    public void CommonLib_Special_Matches_SciPy_Oracle()
    {
        AssertClose(24.0, CommonLib.Numerics.SpecialGamma(5));
        AssertClose(3.1780538303479458, CommonLib.Numerics.SpecialLGamma(5));
        AssertClose(0.08333333333333333, CommonLib.Numerics.SpecialBeta(2, 3));
        AssertClose(0.5204998778130465, CommonLib.Numerics.SpecialErf(0.5));
        AssertClose(0.4795001221869535, CommonLib.Numerics.SpecialErfc(0.5));
        // casi limite
        Assert.True(double.IsNaN(CommonLib.Numerics.SpecialLGamma(-1)));
        Assert.True(double.IsNaN(CommonLib.Numerics.SpecialBeta(0, 1)));
        AssertClose(1.0, CommonLib.Numerics.SpecialErf(double.PositiveInfinity));
        AssertClose(0.0, CommonLib.Numerics.SpecialErfc(double.PositiveInfinity));
    }

    [Fact]
    public void CommonLib_Special_Extensions_Match_SciPy_Oracle()
    {
        // scipy.special: gammainc/gammaincc/betainc/digamma/jv/yv/iv/kv
        AssertClose(0.8008517265, CommonLib.Numerics.SpecialGammaInc(2, 3), 1e-10);
        AssertClose(0.1991482735, CommonLib.Numerics.SpecialGammaIncc(2, 3), 1e-10);
        AssertClose(0.6875, CommonLib.Numerics.SpecialBetaInc(2, 3, 0.5), 1e-12);
        AssertClose(0.9227843351, CommonLib.Numerics.SpecialDigamma(3), 1e-10);
        AssertClose(0.5767248078, CommonLib.Numerics.SpecialBesselJ(1, 2), 1e-10);
        AssertClose(-0.1070324315, CommonLib.Numerics.SpecialBesselY(1, 2), 1e-10);
        AssertClose(1.5906368546, CommonLib.Numerics.SpecialBesselI(1, 2), 1e-10);
        AssertClose(0.1398658818, CommonLib.Numerics.SpecialBesselK(1, 2), 1e-10);
        // casi limite
        Assert.True(double.IsNaN(CommonLib.Numerics.SpecialGammaInc(-1, 1)));
        Assert.True(double.IsNaN(CommonLib.Numerics.SpecialBetaInc(1, 1, 1.5)));
        Assert.True(double.IsNaN(CommonLib.Numerics.SpecialBesselK(0, -1)));
    }

    [Fact]
    public void Vector_Metrics_Match_SciPy_Distance_Oracle()
    {
        // scipy.spatial.distance: manhattan/chebyshev/cosine/minkowski su [1,2,3] vs [4,6,8]
        var a = new double[] { 1, 2, 3 };
        var b = new double[] { 4, 6, 8 };
        AssertClose(12.0, CommonLib.Numerics.VectorManhattan(a, b));                          // 3+4+5
        AssertClose(5.0, CommonLib.Numerics.VectorChebyshev(a, b));                           // max(3,4,5)
        AssertClose(1 - 40 / (Math.Sqrt(14) * Math.Sqrt(116)), CommonLib.Numerics.VectorCosineDistance(a, b), 1e-12);
        AssertClose(6.0, CommonLib.Numerics.VectorMinkowski(a, b, 3));                        // (3^3+4^3+5^3)^(1/3)=216^(1/3)
        // casi limite
        Assert.True(double.IsNaN(CommonLib.Numerics.VectorManhattan(a, new[] { 1.0, 2.0 })));
        Assert.True(double.IsNaN(CommonLib.Numerics.VectorCosineDistance(new[] { 0.0 }, new[] { 0.0 })));
        Assert.True(double.IsNaN(CommonLib.Numerics.VectorMinkowski(a, b, 0)));
    }

    [Fact]
    public void PCode_Dispatches_Vector_Metrics()
    {
        var src = Path.Combine(Path.GetTempPath(), "gmb_vmetric_" + Guid.NewGuid().ToString("N") + ".bas");
        File.WriteAllText(src, """
            IMPORT VECTOR
            SUB Main()
              h1 = VECTOR.CREATE(1, 2, 3)
              h2 = VECTOR.CREATE(4, 6, 8)
              PRINT VECTOR.MANHATTAN(h1, h2)
              PRINT VECTOR.CHEBYSHEV(h1, h2)
              PRINT VECTOR.COSINE(h1, h2)
              PRINT VECTOR.MINKOWSKI(h1, h2, 3)
            END SUB
            """);
        try
        {
            var program = Basic.Orchestrator.Compile(src, null, false, "il");
            var chunk = new global::Compiler.Compiler().Compile(program);
            var originalOut = Console.Out;
            var sw = new StringWriter();
            try
            {
                Console.SetOut(sw);
                Basic.IlBackend.IlRuntime.SetCurrent(new Runtime.HeadlessRuntimeHost(Console.Out, new Runtime.RuntimeConfig()));
                var r = global::Basic.IlBackend.PCode.PCodeVmRunner.Run(chunk);
                Assert.True(r.Success, "VECTOR metrics dispatch fallito:\n" + r.Output);
                Assert.Contains("12", r.Output);
                Assert.Contains("5", r.Output);
                Assert.Contains("0.0074", r.Output); // 1 - 40/sqrt(14*116)
                Assert.Contains("6", r.Output);
            }
            finally { Console.SetOut(originalOut); }
        }
        finally { try { File.Delete(src); } catch { } }
    }

    [Fact]
    public void CommonLib_Special_Inverses_Match_SciPy_Oracle()
    {
        // scipy.special: betaln/erfinv/erfcinv/gammaincinv/gammainccinv/betaincinv
        AssertClose(-2.4849066497880004, CommonLib.Numerics.SpecialBetaLn(2, 3), 1e-12);
        AssertClose(0.4769362762044699, CommonLib.Numerics.SpecialErfInv(0.5), 1e-12);
        AssertClose(0.4769362762044699, CommonLib.Numerics.SpecialErfcInv(0.5), 1e-12);
        AssertClose(2.994308347002123, CommonLib.Numerics.SpecialGammaIncInv(2, 0.8), 1e-9);
        AssertClose(2.9943083470021223, CommonLib.Numerics.SpecialGammaInccInv(2, 0.2), 1e-9);
        AssertClose(0.3857275681323895, CommonLib.Numerics.SpecialBetaIncInv(2, 3, 0.5), 1e-9);
        // casi limite
        Assert.True(double.IsNaN(CommonLib.Numerics.SpecialErfInv(1.5)));
        Assert.True(double.IsNaN(CommonLib.Numerics.SpecialErfcInv(-1)));
        Assert.True(double.IsNaN(CommonLib.Numerics.SpecialGammaIncInv(-1, 0.5)));
        Assert.True(double.IsNaN(CommonLib.Numerics.SpecialBetaIncInv(1, 1, 1.5)));
        AssertClose(0.0, CommonLib.Numerics.SpecialBetaIncInv(2, 3, 0.0));
        AssertClose(1.0, CommonLib.Numerics.SpecialBetaIncInv(2, 3, 1.0));
    }

    [Fact]
    public void PCode_Dispatches_Special_Namespace()    {
        var src = Path.Combine(Path.GetTempPath(), "gmb_special_" + Guid.NewGuid().ToString("N") + ".bas");
        File.WriteAllText(src, """
            IMPORT SPECIAL
            SUB Main()
              PRINT SPECIAL.GAMMA(5)
              PRINT SPECIAL.LGAMMA(5)
              PRINT SPECIAL.BETA(2, 3)
              PRINT SPECIAL.ERF(0.5)
              PRINT SPECIAL.ERFC(0.5)
            END SUB
            """);
        try
        {
            var program = Basic.Orchestrator.Compile(src, null, false, "il");
            var chunk = new global::Compiler.Compiler().Compile(program);
            var originalOut = Console.Out;
            var sw = new StringWriter();
            try
            {
                Console.SetOut(sw);
                Basic.IlBackend.IlRuntime.SetCurrent(new Runtime.HeadlessRuntimeHost(Console.Out, new Runtime.RuntimeConfig()));
                var r = global::Basic.IlBackend.PCode.PCodeVmRunner.Run(chunk);
                Assert.True(r.Success, "SPECIAL dispatch fallito:\n" + r.Output);
                Assert.Contains("24", r.Output);
                Assert.Contains("3.1780538303", r.Output);
                Assert.Contains("0.0833333333", r.Output);
                Assert.Contains("0.5204998778", r.Output);
                Assert.Contains("0.4795001221", r.Output);
            }
            finally { Console.SetOut(originalOut); }
        }
        finally { try { File.Delete(src); } catch { } }
    }
}
