// Nucleo scipy.signal (SIGNAL namespace): convoluzione/correlazione 1D + finestre.
// Oracoli: scipy.signal / numpy (convolve, correlate, hann, hamming, blackman).
using System.IO;
using Xunit;

namespace Basic.IlBackend.Tests;

[Collection("GpmRuntimeState")]
public class SciPySignalTests
{
    private static void AssertArr(double[] expected, double[] actual, double tol = 1e-12)
    {
        Assert.Equal(expected.Length, actual.Length);
        for (var i = 0; i < expected.Length; i++)
            Assert.True(Math.Abs(expected[i] - actual[i]) <= tol,
                $"pos {i}: atteso {expected[i]} ottenuto {actual[i]}");
    }

    [Fact]
    public void CommonLib_Signal_Matches_SciPy()
    {
        AssertArr([0, 1, 2.5, 4, 1.5], CommonLib.Numerics.SignalConvolve([1, 2, 3], [0, 1, 0.5]));
        AssertArr([0.5, 2, 3.5, 3, 0], CommonLib.Numerics.SignalCorrelate([1, 2, 3], [0, 1, 0.5]));
        AssertArr([0, 0.75, 0.75, 0], CommonLib.Numerics.SignalHann(4));
        AssertArr([0.08, 0.77, 0.77, 0.08], CommonLib.Numerics.SignalHamming(4));
        AssertArr([0, 0.63, 0.63, 0], CommonLib.Numerics.SignalBlackman(4), 1e-14);
        // casi limite
        AssertArr([], CommonLib.Numerics.SignalConvolve([], [1, 2]));
        AssertArr([1.0], CommonLib.Numerics.SignalHann(1));
    }

    [Fact]
    public void PCode_Dispatches_Signal_Namespace()
    {
        var src = Path.Combine(Path.GetTempPath(), "gmb_signal_" + Guid.NewGuid().ToString("N") + ".bas");
        File.WriteAllText(src, """
            IMPORT SIGNAL
            SUB Main()
              DIM x(2) AS DOUBLE = {1, 2, 3}
              DIM k(2) AS DOUBLE = {0, 1, 0.5}
              DIM c() = SIGNAL.CONVOLVE(x, k)
              DIM r() = SIGNAL.CORRELATE(x, k)
              DIM h() = SIGNAL.HANN(4)
              PRINT c(0)
              PRINT c(2)
              PRINT c(4)
              PRINT r(1)
              PRINT h(1)
              PRINT h(2)
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
                var res = global::Basic.IlBackend.PCode.PCodeVmRunner.Run(chunk);
                Assert.True(res.Success, "SIGNAL dispatch fallito:\n" + res.Output);
                Assert.Contains("0", res.Output);   // c(0)=0
                Assert.Contains("2.5", res.Output); // c(2)=2.5
                Assert.Contains("1.5", res.Output); // c(4)=1.5
                Assert.Contains("2", res.Output);   // r(1)=2
                Assert.Contains("0.75", res.Output);// h(1)=h(2)=0.75
            }
            finally { Console.SetOut(originalOut); }
        }
        finally { try { File.Delete(src); } catch { } }
    }

    // ---- scipy.signal P0: firwin / find_peaks ----
    [Fact]
    public void Firwin_Matches_SciPy()
    {
        var taps = CommonLib.Numerics.Firwin(11, [0.2], "lowpass", "hann");
        Assert.True(Math.Abs(taps[5] - 0.4) < 1e-9, $"tap centrale {taps[5]}");
        Assert.True(Math.Abs(taps[4] - 0.273822482781629) < 1e-9, $"tap 4 {taps[4]}");
        Assert.True(Math.Abs(taps[0]) < 1e-9, $"tap 0 {taps[0]}");
    }

    [Fact]
    public void FindPeaks_Matches_SciPy()
    {
        var pk = CommonLib.Numerics.FindPeaks([0, 1, 0, 3, 0, 2, 0], 1);
        Assert.Equal(new double[] { 1, 3, 5 }, pk);
    }

    [Fact]
    public void SolveIvp_Matches_SciPy()
    {
        // y' = y, y(0)=1 → y(1) = e
        var sol = CommonLib.Numerics.SolveIvpk45((t, y) => [y[0]], [1.0], 0, 1);
        var rows = sol.GetLength(0);
        Assert.True(Math.Abs(sol[rows - 1, 1] - Math.E) < 1e-5, $"y_end={sol[rows - 1, 1]}");
    }
}
