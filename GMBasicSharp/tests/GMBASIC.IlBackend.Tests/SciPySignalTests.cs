// scipy.signal: il dominio SIGNAL è migrato alla lib FFI `signal`
// (shared/libs/signal + shared/include/signal.inc). I test dei nuclei migrati
// (convoluzione/correlazione, finestre, firwin, find_peaks, filtri IIR) vivono
// ora nel self-test C del canonico (`make test` in gmbsuite/gmbsignal, 93 check)
// e nei MiniTest del dominio; qui resta il nucleo scipy.signal NON migrato.
using Xunit;

namespace Basic.IlBackend.Tests;

[Collection("GpmRuntimeState")]
public class SciPySignalTests
{
    [Fact]
    public void SolveIvp_Matches_SciPy()
    {
        // y' = y, y(0)=1 → y(1) = e
        var sol = CommonLib.Numerics.SolveIvpk45((t, y) => [y[0]], [1.0], 0, 1);
        var rows = sol.GetLength(0);
        Assert.True(Math.Abs(sol[rows - 1, 1] - Math.E) < 1e-5, $"y_end={sol[rows - 1, 1]}");
    }
}
