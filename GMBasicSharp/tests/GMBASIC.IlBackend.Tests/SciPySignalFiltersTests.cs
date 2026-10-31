// Design e applicazione filtri IIR con parità a scipy.signal 1.18
// (butter/cheby1/cheby2/ellip + lfilter/filtfilt/freqz). Oracoli calcolati con scipy.
using Xunit;

namespace Basic.IlBackend.Tests;

[Collection("GpmRuntimeState")]
public class SciPySignalFiltersTests
{
    private static void AssertArr(double[] expected, double[] actual, double tol = 1e-9)
    {
        Assert.Equal(expected.Length, actual.Length);
        for (var i = 0; i < expected.Length; i++)
            Assert.True(Math.Abs(expected[i] - actual[i]) <= tol,
                $"pos {i}: atteso {expected[i]} ottenuto {actual[i]}");
    }

    private static void AssertCoeffs(double[] eb, double[] ea, double[] ab, double[] aa, double tol = 1e-9)
    {
        Assert.Equal(eb.Length, ab.Length);
        Assert.Equal(ea.Length, aa.Length);
        for (var i = 0; i < eb.Length; i++)
            Assert.True(Math.Abs(eb[i] - ab[i]) <= tol, $"b[{i}]: atteso {eb[i]} ottenuto {ab[i]}");
        for (var i = 0; i < ea.Length; i++)
            Assert.True(Math.Abs(ea[i] - aa[i]) <= tol, $"a[{i}]: atteso {ea[i]} ottenuto {aa[i]}");
    }

    [Fact]
    public void Butter_Lowpass_Matches_SciPy()
    {
        var f = CommonLib.Numerics.Butter(4, [0.3], "low");
        AssertCoeffs(
            [0.018563010627, 0.074252042508, 0.111378063761, 0.074252042508, 0.018563010627],
            [1, -1.570398851228, 1.275613324983, -0.484403368335, 0.07619706461],
            f.B, f.A);
    }

    [Fact]
    public void Butter_Highpass_Matches_SciPy()
    {
        var f = CommonLib.Numerics.Butter(2, [0.5], "high");
        AssertCoeffs(
            [0.292893218813, -0.585786437627, 0.292893218813],
            [1, 0.0, 0.171572875254],
            f.B, f.A);
    }

    [Fact]
    public void Butter_Bandpass_Matches_SciPy()
    {
        var f = CommonLib.Numerics.Butter(4, [0.2, 0.5], "bandpass");
        AssertCoeffs(
            [0.018563010627, 0, -0.074252042508, 0, 0.111378063761, 0, -0.074252042508, 0, 0.018563010627],
            [1, -2.838259978535, 4.682368686456, -5.254272031931, 4.497680679556, -2.818125271767, 1.311542039904, -0.40211321837, 0.07619706461],
            f.B, f.A);
    }

    [Fact]
    public void Butter_Bandstop_Matches_SciPy()
    {
        var f = CommonLib.Numerics.Butter(3, [0.2, 0.5], "bandstop");
        AssertCoeffs(
            [0.37445269259, -1.144759059038, 2.289925826009, -2.685782088935, 2.289925826009, -1.144759059038, 0.37445269259],
            [1, -2.120602876627, 2.724749202768, -2.289518118077, 1.46624653317, -0.565179212308, 0.13776130126],
            f.B, f.A);
    }

    [Fact]
    public void Cheby1_Lowpass_And_Bandpass_Matches_SciPy()
    {
        var f = CommonLib.Numerics.Cheby1(3, 1.0, [0.4], "low");
        AssertCoeffs(
            [0.073597089929, 0.220791269786, 0.220791269786, 0.073597089929],
            [1, -0.97612747634, 0.856761775713, -0.291857579944],
            f.B, f.A);

        var fb = CommonLib.Numerics.Cheby1(3, 1.0, [0.2, 0.6], "bandpass");
        AssertCoeffs(
            [0.073597089929, 0, -0.220791269786, 0, 0.220791269786, 0, -0.073597089929],
            [1, -1.51874555236, 1.823651595035, -1.574338966023, 1.376920913285, -0.661692905053, 0.291857579944],
            fb.B, fb.A);
    }

    [Fact]
    public void Cheby2_Matches_SciPy()
    {
        var f = CommonLib.Numerics.Cheby2(3, 20.0, [0.4], "low");
        AssertCoeffs(
            [0.139656895836, 0.091102719748, 0.091102719748, 0.139656895836],
            [1, -1.145436191211, 0.727458002699, -0.12050258032],
            f.B, f.A);
    }

    [Fact]
    public void Ellip_Matches_SciPy()
    {
        var f = CommonLib.Numerics.Ellip(3, 1.0, 20.0, [0.4], "low");
        AssertCoeffs(
            [0.186758989168, 0.203601742773, 0.203601742773, 0.186758989168],
            [1, -0.873879102017, 0.921720013198, -0.267119447298],
            f.B, f.A);

        var fh = CommonLib.Numerics.Ellip(3, 1.0, 20.0, [0.3], "highpass");
        AssertCoeffs(
            [0.445168806206, -1.137349433024, 1.137349433024, -0.445168806206],
            [1, -1.177581474344, 0.897467403329, -0.089987600785],
            fh.B, fh.A);
    }

    [Fact]
    public void Elliptic_Intermediates_Probe()
    {
        var epsSq = Math.Pow(10, 0.1 * 1.0) - 1.0;
        var ck1Sq = epsSq / (Math.Pow(10, 2.0) - 1.0);
        Assert.Equal(1.5718249089323806, CommonLib.Numerics.EllipticK(ck1Sq), 10);
        // _ellipdeg(3, ck1Sq) = 0.5846852904478214 — esposto indirettamente; testiamo sn:
        CommonLib.Numerics.EllipSnCnDn(1.2889043025, 0.5846852904478214, out var sn, out var cn, out var dn, out _);
        Assert.True(Math.Abs(sn - 0.908245734013) < 1e-9, $"sn={sn}");
        Assert.True(Math.Abs(cn - 0.418437195583) < 1e-9, $"cn={cn}");
        Assert.True(Math.Abs(dn - 0.719504742058) < 1e-9, $"dn={dn}");
    }

    [Fact]
    public void Lfilter_Matches_SciPy()
    {
        var f = CommonLib.Numerics.Butter(2, [0.3], "low");
        double[] x = [1, 0, 0, 0, 0, 0, 0, 0, 0, 0];
        AssertArr(
            [0.13110644, 0.36025286, 0.3648105, 0.17473513, 0.03135817, -0.02411621, -0.02657, -0.01330397, -0.00271581, 0.00159068],
            CommonLib.Numerics.LFilter(f, x), 1e-6);
    }

    [Fact]
    public void Filtfilt_Matches_SciPy()
    {
        var f = CommonLib.Numerics.Butter(2, [0.3], "low");
        double[] x = [1, 0, 0, 0, 0, 0, 0, 0, 0, 0];
        AssertArr(
            [0.99999793, 0.43878922, 0.07309755, -0.06477432, -0.06831519, -0.03342968, -0.00641404, 0.00418549, 0.00459553, 0.00196136],
            CommonLib.Numerics.FiltFilt(f, x), 1e-5);
    }

    [Fact]
    public void Freqz_Matches_SciPy()
    {
        var f = CommonLib.Numerics.Butter(2, [0.3], "low");
        AssertArr(
            [1.0, 0.98858522, 0.83427508, 0.50268536, 0.25128586, 0.11513809, 0.04449897, 0.01027147],
            CommonLib.Numerics.Freqz(f, 8), 1e-6);
    }
}
